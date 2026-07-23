/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <math.h>
#include <atomic>
#include <dxgi1_2.h>

#include "tvgMfMediaLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr auto LOAD_TIMEOUT = 10000UL;

// Receives media engine events on Media Foundation worker threads.
struct MfNotify : IMFMediaEngineNotify
{
    // Manual-reset events: open() blocks on them like the gst preroll wait.
    HANDLE loadedEvt = CreateEventW(nullptr, TRUE, FALSE, nullptr);  // metadata (size/duration) is available
    HANDLE readyEvt = CreateEventW(nullptr, TRUE, FALSE, nullptr);   // enough data is available to render the first frame
    std::atomic<uint32_t> error{0};
    std::atomic<bool> failed{false};
    std::atomic<ULONG> refCnt{1};

    virtual ~MfNotify()
    {
        CloseHandle(loadedEvt);
        CloseHandle(readyEvt);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** obj) override
    {
        if (!obj) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFMediaEngineNotify)) {
            *obj = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *obj = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++refCnt;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        auto cnt = --refCnt;
        if (cnt == 0) delete (this);
        return cnt;
    }

    HRESULT STDMETHODCALLTYPE EventNotify(DWORD event, DWORD_PTR param1, TVG_UNUSED DWORD param2) override
    {
        switch (event) {
            case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA: SetEvent(loadedEvt); break;
            case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY: SetEvent(readyEvt); break;
            case MF_MEDIA_ENGINE_EVENT_ERROR: {
                error = static_cast<uint32_t>(param1);
                failed = true;
                SetEvent(loadedEvt);
                SetEvent(readyEvt);
                break;
            }
            default: break;
        }
        return S_OK;
    }
};

template<typename T>
static void _release(T*& obj)
{
    if (!obj) return;
    obj->Release();
    obj = nullptr;
}

static bool _createDevice(MfMediaLoader* loader)
{
    constexpr auto flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

    // Fall back to the WARP software rasterizer if hardware device creation fails.
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &loader->d3d.device, nullptr, &loader->d3d.context)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &loader->d3d.device, nullptr, &loader->d3d.context)))
        return false;

    // The engine decodes on its own threads; the device must be multithread-protected.
    // https://learn.microsoft.com/en-us/windows/win32/medfound/supporting-direct3d-11-video-decoding-in-media-foundation
    ID3D10Multithread* mt = nullptr;
    if (FAILED(loader->d3d.device->QueryInterface(__uuidof(ID3D10Multithread), reinterpret_cast<void**>(&mt)))) return false;
    mt->SetMultithreadProtected(TRUE);
    mt->Release();

    UINT token = 0;
    if (FAILED(MFCreateDXGIDeviceManager(&token, &loader->d3d.manager))) return false;
    return SUCCEEDED(loader->d3d.manager->ResetDevice(loader->d3d.device, token));
}

static bool _createTexture(MfMediaLoader* loader, uint32_t width, uint32_t height)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = loader->output.format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    if (FAILED(loader->d3d.device->CreateTexture2D(&desc, nullptr, &loader->d3d.texture))) return false;

    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    return SUCCEEDED(loader->d3d.device->CreateTexture2D(&desc, nullptr, &loader->d3d.staging));
}

static void _checkPlayback(MfMediaLoader* loader)
{
    auto finish = loader->mf.notify->failed.exchange(false);
    if (finish) TVGERR("MF", "Media engine error: %u", loader->mf.notify->error.load());
    else finish = loader->mf.engine->IsEnded() && !loader->paused;
    if (!finish) return;

    loader->mf.engine->Pause();
    loader->paused = true;
    loader->curTime = loader->totalTime;
}

static void _close(MfMediaLoader* loader)
{
    if (loader->mf.engine) {
        loader->mf.engine->Shutdown();  // stops playback and the engine threads
        loader->mf.engine->Release();
        loader->mf.engine = nullptr;
    }

    _release(loader->mf.notify);
    _release(loader->mf.stream);
    _release(loader->d3d.texture);
    _release(loader->d3d.staging);
    _release(loader->d3d.context);
    _release(loader->d3d.device);
    _release(loader->d3d.manager);
}

static BSTR _utf8ToBstr(const char* path)
{
    auto len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (len <= 0) return nullptr;

    auto wpath = tvg::malloc<wchar_t>(len * sizeof(wchar_t));
    if (!wpath) return nullptr;

    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, len);
    auto source = SysAllocString(wpath);
    tvg::free(wpath);
    return source;
}

static bool _open(MfMediaLoader* loader, const char* path)
{
    if (!_createDevice(loader)) {
        TVGERR("MF", "Failed to create a D3D11 device: %s", path);
        return false;
    }

    IMFMediaEngineClassFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMFMediaEngineClassFactory), reinterpret_cast<void**>(&factory)))) {
        TVGERR("MF", "Missing media engine class factory: %s", path);
        return false;
    }

    loader->mf.notify = new MfNotify;

    IMFAttributes* attrs = nullptr;
    auto ok = SUCCEEDED(MFCreateAttributes(&attrs, 3)) &&
        SUCCEEDED(attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, loader->mf.notify)) &&
        SUCCEEDED(attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, loader->d3d.manager)) &&
        SUCCEEDED(attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, loader->output.format)) &&
        SUCCEEDED(factory->CreateInstance(0, attrs, &loader->mf.engine));

    _release(attrs);
    _release(factory);
    if (!ok) return false;

    auto source = _utf8ToBstr(path);
    if (!source) return false;

    // SetSource loads asynchronously; wait until metadata is available.
    ok = SUCCEEDED(loader->mf.engine->SetAutoPlay(FALSE)) && SUCCEEDED(loader->mf.engine->SetPreload(MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC));
    if (ok && loader->mf.stream) {
        IMFMediaEngineEx* engine = nullptr;
        ok = SUCCEEDED(loader->mf.engine->QueryInterface(__uuidof(IMFMediaEngineEx), reinterpret_cast<void**>(&engine))) && SUCCEEDED(engine->SetSourceFromByteStream(loader->mf.stream, source));
        _release(engine);
    } else if (ok) {
        ok = SUCCEEDED(loader->mf.engine->SetSource(source));
    }
    SysFreeString(source);
    if (!ok) return false;

    return loader->mf.engine && WaitForSingleObject(loader->mf.notify->loadedEvt, LOAD_TIMEOUT) == WAIT_OBJECT_0 &&
           !loader->mf.notify->failed && loader->mf.engine->HasVideo();
}

static void _updateAlpha(MfMediaLoader* loader)
{
    IMFMediaEngineEx* engine = nullptr;
    if (FAILED(loader->mf.engine->QueryInterface(__uuidof(IMFMediaEngineEx), reinterpret_cast<void**>(&engine)))) return;

    DWORD count = 0;
    engine->GetNumberOfStreams(&count);

    for (auto i = 0UL; i < count; ++i) {
        PROPVARIANT attr = {};
        auto alpha = SUCCEEDED(engine->GetStreamAttribute(i, MF_MEDIA_ENGINE_STREAM_CONTAINS_ALPHA_CHANNEL, &attr)) &&
                     attr.vt == VT_BOOL && attr.boolVal == VARIANT_TRUE;
        PropVariantClear(&attr);
        if (!alpha) continue;

        auto mode = DXGI_ALPHA_MODE_STRAIGHT;
        if (SUCCEEDED(engine->GetStreamAttribute(i, MF_MT_ALPHA_MODE, &attr)) && attr.vt == VT_UI4) {
            mode = static_cast<DXGI_ALPHA_MODE>(attr.ulVal);
        }
        PropVariantClear(&attr);

        loader->output.alphaIgnored = mode == DXGI_ALPHA_MODE_IGNORE;
        if (!loader->output.alphaIgnored && mode != DXGI_ALPHA_MODE_PREMULTIPLIED) {
            loader->output.cs = loader->output.cs == ColorSpace::ABGR8888 ? ColorSpace::ABGR8888S : ColorSpace::ARGB8888S;
        }
        break;
    }
    _release(engine);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

MfMediaLoader::~MfMediaLoader()
{
    _close(this);
    if (mf.started) MFShutdown();
    if (mf.mta) CoDecrementMTAUsage(mf.mta);
    tvg::free(surface.data);
}

bool MfMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
    auto targetCs = BitmapLoader::cs.load();
    if (targetCs == ColorSpace::ABGR8888 || targetCs == ColorSpace::ABGR8888S) {
        output.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        output.cs = ColorSpace::ABGR8888;
    } else {
        output.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        output.cs = ColorSpace::ARGB8888;
    }
    output.alphaIgnored = true;

    if (!mf.mta && FAILED(CoIncrementMTAUsage(&mf.mta))) return false;

    // ref-counts MFStartup()/MFShutdown() pairs; the last release tears the platform down.
    if (!mf.started) {
        if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) return false;
        mf.started = true;
    }

    if (!_open(this, path)) {
        TVGERR("MF", "Failed to open media: %s", path);
        return false;
    }

    if (WaitForSingleObject(mf.notify->readyEvt, LOAD_TIMEOUT) != WAIT_OBJECT_0 || mf.notify->failed) {
        TVGERR("MF", "Failed to prepare the first frame.");
        return false;
    }

    _updateAlpha(this);

    DWORD width = 0, height = 0;
    auto duration = mf.engine->GetDuration();
    if (FAILED(mf.engine->GetNativeVideoSize(&width, &height)) || width == 0 || height == 0 ||
        !isfinite(duration) || duration <= 0.0) {
        TVGERR("MF", "Invalid media metadata: %s", path);
        return false;
    }

    w = static_cast<float>(width);
    h = static_cast<float>(height);
    totalTime = static_cast<float>(duration);
    curTime = 0.0f;

    return true;
}

bool MfMediaLoader::open(const char* data, uint32_t size, const LoaderOps* ops, TVG_UNUSED bool copy)
{
    auto memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return false;

    auto dst = GlobalLock(memory);
    if (!dst) {
        GlobalFree(memory);
        return false;
    }
    memcpy(dst, data, size);
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return false;
    }

    auto length = ULARGE_INTEGER{};
    length.QuadPart = size;

    auto ret = SUCCEEDED(stream->SetSize(length)) && SUCCEEDED(MFCreateMFByteStreamOnStream(stream, &mf.stream)) && open("memory.mp4", ops);
    _release(stream);
    return ret;
}

bool MfMediaLoader::read()
{
    if (!Loader::read()) return true;

    auto width = static_cast<uint32_t>(w);
    auto height = static_cast<uint32_t>(h);
    surface.setup(tvg::malloc<pixel_t>(width * height * sizeof(uint32_t)), width, width, height, sizeof(uint32_t), output.cs, output.alphaIgnored);
    if (!surface.data) return false;

    if (!_createTexture(this, surface.w, surface.h)) return false;

    if (FAILED(mf.engine->SetVolume(static_cast<double>(audioVolume)))) TVGERR("MF", "Failed to set media volume.");
    if (FAILED(mf.engine->SetMuted(static_cast<BOOL>(muted)))) TVGERR("MF", "Failed to set media mute.");
    paused = true;

    sync();

    return true;
}

bool MfMediaLoader::sync()
{
    _checkPlayback(this);

    // Frame-server mode: poll for a new frame at render cadence.
    LONGLONG pts = 0;
    if (mf.engine->OnVideoStreamTick(&pts) != S_OK) return false;

    curTime = static_cast<float>(pts) / 10000000.0f;  // 100-nanosecond units per second

    // Copy the current frame into the texture, scaling it to the rectangle and filling letterbox regions with opaque black.
    RECT rect = {0, 0, static_cast<LONG>(surface.w), static_cast<LONG>(surface.h)};
    MFARGB border = {0, 0, 0, 255};
    if (FAILED(mf.engine->TransferVideoFrame(d3d.texture, nullptr, &rect, &border))) return false;

    // GPU -> CPU readback through the staging texture.
    d3d.context->CopyResource(d3d.staging, d3d.texture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(d3d.context->Map(d3d.staging, 0, D3D11_MAP_READ, 0, &mapped))) return false;

    auto rowBytes = surface.stride * surface.channelSize;
    auto src = static_cast<uint8_t*>(mapped.pData);
    auto dst = reinterpret_cast<uint8_t*>(surface.data);

    // The mapped rows may carry gpu alignment padding (RowPitch >= rowBytes).
    for (auto y = 0U; y < surface.h; ++y, dst += rowBytes, src += mapped.RowPitch) {
        memcpy(dst, src, rowBytes);
    }
    surface.cs = output.cs;  // rasterConvertCS() can update this.
    surface.premultiplied = output.cs == ColorSpace::ABGR8888 || output.cs == ColorSpace::ARGB8888;

    d3d.context->Unmap(d3d.staging, 0);

    return true;
}

Result MfMediaLoader::play()
{
    // Restart from the beginning when playback already reached the end.
    if (!looping && curTime >= totalTime) {
        if (FAILED(mf.engine->SetCurrentTime(0.0))) TVGERR("MF", "Failed to rewind media.");
        curTime = 0.0f;
    }

    if (FAILED(mf.engine->Play())) TVGERR("MF", "Failed to play media.");
    paused = false;
    return Result::Success;
}

Result MfMediaLoader::pause()
{
    if (FAILED(mf.engine->Pause())) TVGERR("MF", "Failed to pause media.");
    paused = true;
    return Result::Success;
}

Result MfMediaLoader::stop()
{
    if (FAILED(mf.engine->Pause())) TVGERR("MF", "Failed to pause media.");
    paused = true;

    // The seeked still at 0 arrives via OnVideoStreamTick() on the next sync().
    if (FAILED(mf.engine->SetCurrentTime(0.0))) TVGERR("MF", "Failed to rewind media.");
    curTime = 0.0f;
    return Result::Success;
}

Result MfMediaLoader::seek(float seconds)
{
    if (FAILED(mf.engine->SetCurrentTime(static_cast<double>(seconds)))) TVGERR("MF", "Failed to seek media.");
    curTime = seconds;
    return Result::Success;
}

Result MfMediaLoader::loop(bool on)
{
    if (FAILED(mf.engine->SetLoop(static_cast<BOOL>(on)))) TVGERR("MF", "Failed to set media loop.");
    looping = on;
    return Result::Success;
}

Result MfMediaLoader::volume(float volume)
{
    if (FAILED(mf.engine->SetVolume(static_cast<double>(volume)))) TVGERR("MF", "Failed to set media volume.");
    audioVolume = volume;
    return Result::Success;
}

Result MfMediaLoader::mute(bool on)
{
    if (FAILED(mf.engine->SetMuted(static_cast<BOOL>(on)))) TVGERR("MF", "Failed to set media mute.");
    muted = on;
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new MfMediaLoader;
}
