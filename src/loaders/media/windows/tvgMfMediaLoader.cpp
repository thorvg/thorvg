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

#include <mfapi.h>
#include <shlwapi.h>

#include "tvgMath.h"
#include "tvgMfMediaLoader.h"

#ifdef __MINGW32__
// Exported by mfplat since Windows 7, but missing in the MinGW headers.
STDAPI MFCreateMFByteStreamOnStream(IStream* stream, IMFByteStream** byteStream);
#endif

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static constexpr auto TIME_EPSILON = 0.001;  // frame-time tolerance in seconds
static constexpr auto IDLE_POLLS = 10;       // allow delayed seek frames before sleeping
static constexpr auto TIMESCALE = 10000000.0;  // 100ns units per second
static constexpr auto POLL_INTERVAL = 8UL;     // milliseconds

// Receives the engine events on Media Foundation worker threads.
struct MfNotify : IMFMediaEngineNotify
{
    // open() waits for loaded, then for ready
    HANDLE loaded = CreateEventW(nullptr, TRUE, FALSE, nullptr);  // metadata ready
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);   // first frame ready
    std::atomic<ULONG> refCnt{1};
    std::atomic<bool> failed{false};

    virtual ~MfNotify()
    {
        CloseHandle(loaded);
        CloseHandle(ready);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** obj) override
    {
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

    HRESULT STDMETHODCALLTYPE EventNotify(DWORD event, DWORD_PTR param1, DWORD param2) override
    {
        switch (event) {
            case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA: SetEvent(loaded); break;
            case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY: SetEvent(ready); break;
            case MF_MEDIA_ENGINE_EVENT_ERROR: {
                TVGERR("MF", "Media engine error: %u (0x%08lx)", static_cast<uint32_t>(param1), param2);
                failed = true;
                // Release open() at once, rather than at its timeout.
                SetEvent(loaded);
                SetEvent(ready);
                break;
            }
            default: break;
        }
        return S_OK;
    }
};

// Share the platform and D3D device
// for the process lifetime to avoid repeated driver initialization.
static struct
{
    StrictKey key;
    IMFMediaEngineClassFactory* factory = nullptr;
    IMFDXGIDeviceManager* manager = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
} _mf;

template<typename T>
static void _release(T*& obj)
{
    if (!obj) return;
    obj->Release();
    obj = nullptr;
}

static bool _startup()
{
    ScopedLock lock(_mf.key);
    if (_mf.factory) return true;

    // The engines need COM: keep the MTA alive for the process, without choosing an apartment for the calling thread.
    // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coincrementmtausage
    CO_MTA_USAGE_COOKIE mta = nullptr;
    if (FAILED(CoIncrementMTAUsage(&mta))) return false;

    // LITE skips socket initialization, not Media Foundation work queues.
    // https://learn.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mfstartup
    if (SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        // Without a gpu, fall back to the WARP rasterizer: it can't decode video, so the engines decode on the cpu.
        auto created = SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, &_mf.device, nullptr, &_mf.context)) ||
            SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr, 0, D3D11_SDK_VERSION, &_mf.device, nullptr, &_mf.context));
        ID3D10Multithread* mt = nullptr;
        if (created && SUCCEEDED(_mf.device->QueryInterface(IID_PPV_ARGS(&mt)))) {
            // The engines and the loader threads use the device concurrently; it must be multithread-protected.
            // https://learn.microsoft.com/en-us/windows/win32/medfound/supporting-direct3d-11-video-decoding-in-media-foundation
            mt->SetMultithreadProtected(TRUE);
            mt->Release();

            UINT token = 0;
            if (SUCCEEDED(MFCreateDXGIDeviceManager(&token, &_mf.manager)) &&
                SUCCEEDED(_mf.manager->ResetDevice(_mf.device, token)) &&
                SUCCEEDED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_mf.factory)))) {
                return true;
            }
        }
        _release(_mf.manager);
        _release(_mf.context);
        _release(_mf.device);
        MFShutdown();
    }
    CoDecrementMTAUsage(mta);
    return false;
}

Result MfMediaLoader::open(const wchar_t* url, IMFByteStream* stream)
{
    static constexpr auto LOAD_TIMEOUT = 10000UL;  // milliseconds

    // 1. Shared platform and d3d device (once)
    if (!_startup()) {
        TVGERR("MF", "Failed to start the media platform.");
        return Result::SystemError;
    }

    // 2. Engine in frame-server mode: no window, frames taken with TransferVideoFrame()
    auto abgr = BitmapLoader::cs == ColorSpace::ABGR8888 || BitmapLoader::cs == ColorSpace::ABGR8888S;
    auto format = abgr ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    notify = new MfNotify;
    IMFAttributes* attrs = nullptr;
    IMFMediaEngine* base = nullptr;
    auto ret = SUCCEEDED(MFCreateAttributes(&attrs, 3)) &&
        SUCCEEDED(attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, notify)) &&
        SUCCEEDED(attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, _mf.manager)) &&
        SUCCEEDED(attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, format)) &&
        SUCCEEDED(_mf.factory->CreateInstance(0, attrs, &base)) &&
        SUCCEEDED(base->QueryInterface(IID_PPV_ARGS(&engine)));
    _release(attrs);
    _release(base);
    if (!ret) {
        TVGERR("MF", "Failed to create a media engine.");
        return Result::SystemError;
    }

    // 3. Source: loads asynchronously; wait for the metadata, then the first frame
    auto wait = [](HANDLE event) {
        return WaitForSingleObject(event, LOAD_TIMEOUT) == WAIT_OBJECT_0;
    };
    auto source = SysAllocString(url);
    ret = source && SUCCEEDED(stream ? engine->SetSourceFromByteStream(stream, source) : engine->SetSource(source));
    SysFreeString(source);

    if (!ret || !wait(notify->loaded) || !engine->HasVideo() || !wait(notify->ready) || notify->failed) {
        TVGERR("MF", "No playable video found.");
        return Result::InvalidArguments;
    }

    // 4. Size and duration; the native size already includes the pixel aspect ratio
    DWORD width = 0, height = 0;
    auto duration = engine->GetDuration();
    if (FAILED(engine->GetNativeVideoSize(&width, &height)) || width == 0 || height == 0 ||
        !std::isfinite(duration) || duration <= 0.0) {
        TVGERR("MF", "Invalid media metadata.");
        return Result::InvalidArguments;
    }

    // 5. Textures for the conversion and the readback, in the canvas pixel order
    CD3D11_TEXTURE2D_DESC desc(format, width, height, 1, 1, D3D11_BIND_RENDER_TARGET);
    if (FAILED(_mf.device->CreateTexture2D(&desc, nullptr, &texture))) return Result::FailedAllocation;
    desc = CD3D11_TEXTURE2D_DESC(format, width, height, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);
    if (FAILED(_mf.device->CreateTexture2D(&desc, nullptr, &staging))) return Result::FailedAllocation;

    // 6. Metadata; alpha videos stay straight, opaque ones count as premultiplied
    auto alpha = false;
    DWORD count = 0;
    engine->GetNumberOfStreams(&count);
    for (auto i = 0UL; i < count && !alpha; ++i) {
        PROPVARIANT attr = {};
        alpha = SUCCEEDED(engine->GetStreamAttribute(i, MF_MEDIA_ENGINE_STREAM_CONTAINS_ALPHA_CHANNEL, &attr)) &&
            attr.vt == VT_BOOL && attr.boolVal;
        PropVariantClear(&attr);
    }
    if (alpha) cs = abgr ? ColorSpace::ABGR8888S : ColorSpace::ARGB8888S;
    else cs = abgr ? ColorSpace::ABGR8888 : ColorSpace::ARGB8888;

    w = static_cast<float>(width);
    h = static_cast<float>(height);
    totalTime = static_cast<float>(duration);
    return Result::Success;
}

// Converts the current frame on the gpu and reads it back; the map waits until the gpu has finished it.
bool MfMediaLoader::readback(uint32_t* frame)
{
    auto width = static_cast<uint32_t>(w);
    auto height = static_cast<uint32_t>(h);
    RECT rect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    MFARGB border = {0, 0, 0, 255};
    if (FAILED(engine->TransferVideoFrame(texture, nullptr, &rect, &border))) {
        TVGERR("MF", "Failed to transfer video frame.");
        return false;
    }
    _mf.context->CopyResource(staging, texture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(_mf.context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        TVGERR("MF", "Failed to map video frame.");
        return false;
    }

    // The mapped rows may carry gpu alignment padding.
    auto src = static_cast<uint8_t*>(mapped.pData);
    for (auto y = 0U; y < height; ++y, src += mapped.RowPitch) {
        memcpy(frame + y * width, src, width * sizeof(uint32_t));
    }
    _mf.context->Unmap(staging, 0);
    return true;
}

// Keep GPU readback off the render thread.
void MfMediaLoader::run()
{
    auto updateState = [this](double target) {
        if (playing) {
            // A playing seek presents its frames as it plays on: no landing to watch.
            if (target >= 0.0) engine->SetCurrentTime(target);
            engine->Play();
            landing = false;
        } else {
            // Pause before seeking to allow frame delivery.
            engine->Pause();
            if (target >= 0.0) {
                auto from = engine->GetCurrentTime();
                engine->SetCurrentTime(target);
                landing = engine->GetCurrentTime() != from;
            }
        }
    };

    auto updateFrame = [this](double time, bool seeking) {
        shown = time;
        idle = 0;

        // Skip trailing frames beyond the reported duration.
        if (time > static_cast<double>(totalTime) - TIME_EPSILON) {
            if (engine->GetLoop() && !seeking) engine->SetCurrentTime(0.0);
            return;
        }

        // The write slot is separate from the latest slot consumed by sync().
        auto& frame = ring.frames[ring.write];
        if (!frame) frame = tvg::malloc<uint32_t>(static_cast<uint32_t>(w * h) * sizeof(uint32_t));
        if (!frame || !readback(frame)) return;

        // Preserve the requested position during seeking.
        ScopedLock lock(key);
        ring.time = seeking ? -1.0f : static_cast<float>(time);
        ring.latest = ring.write;
        ring.write = (ring.write + 1) % BUFFER_COUNT;
        ring.updated = true;
    };

    auto settle = [this](bool seeking) {
        // The engine pauses itself at the end.
        if (playing && engine->IsEnded()) {
            playing = false;
            ScopedLock lock(key);
            if (!request.pending) ended = true;
        }

        // Allow delayed seek frames before stepping once.
        if (playing || seeking) idle = 0;
        else if (++idle == IDLE_POLLS && landing) {
            landing = false;
            if (fabs(shown - engine->GetCurrentTime()) > TIME_EPSILON) {
                if (FAILED(engine->FrameStep(TRUE))) TVGERR("MF", "Failed to step video frame.");
                idle = 0;
            }
        }
    };

    while (true) {
        // Poll during playback and seek settling; controls wake an idle worker.
        WaitForSingleObject(wake, idle > IDLE_POLLS ? INFINITE : POLL_INTERVAL);
        if (quit) break;

        // Apply the controls once no seek is pending: the engine would queue the seek.
        auto seeking = engine->IsSeeking();
        auto target = NO_SEEK;
        auto pending = false;
        if (!seeking) {
            ScopedLock lock(key);
            pending = request.pending;
            if (pending) {
                std::swap(target, request.position);
                playing = request.running;
                request.pending = false;
            }
        }

        if (pending) {
            updateState(target);
            // A seek just started counts as pending for this round.
            seeking = target >= 0.0;
        }

        // Frame-server mode requires polling for new frames.
        LONGLONG pts = 0;
        if (engine->OnVideoStreamTick(&pts) == S_OK) updateFrame(pts / TIMESCALE, seeking);
        else settle(seeking);
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

MfMediaLoader::~MfMediaLoader()
{
    // Stop readback before releasing the engine and textures.
    if (worker.joinable()) {
        quit = true;
        SetEvent(wake);
        worker.join();
    }
    CloseHandle(wake);
    if (engine) engine->Shutdown();  // stops the playback and the engine threads
    _release(engine);
    _release(notify);
    _release(texture);
    _release(staging);
    for (auto frame : ring.frames) tvg::free(frame);
    tvg::free(surface.data);
}

Result MfMediaLoader::open(const char* data, uint32_t size, TVG_UNUSED const LoaderOps& ops)
{
    // The source stream keeps a copy because playback may outlive the given buffer.
    auto memory = SHCreateMemStream(reinterpret_cast<const BYTE*>(data), size);
    if (!memory) return Result::FailedAllocation;

    // The url only hints the container format of the stream.
    IMFByteStream* stream = nullptr;
    auto ret = SUCCEEDED(MFCreateMFByteStreamOnStream(memory, &stream)) ? open(L"memory.mp4", stream) : Result::SystemError;
    _release(stream);
    memory->Release();
    return ret;
}

Result MfMediaLoader::open(const char* path, TVG_UNUSED const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    auto len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (len <= 0) return Result::InvalidArguments;

    auto url = tvg::malloc<wchar_t>(len * sizeof(wchar_t));
    if (!url) return Result::FailedAllocation;
    MultiByteToWideChar(CP_UTF8, 0, path, -1, url, len);
    auto ret = open(url, nullptr);
    tvg::free(url);
    return ret;
#else
    return Result::NonSupport;
#endif
}

bool MfMediaLoader::read()
{
    if (!Loader::read()) return true;

    auto width = static_cast<uint32_t>(w);
    auto height = static_cast<uint32_t>(h);
    surface.setup(tvg::malloc<pixel_t>(width * height * sizeof(uint32_t)), width, width, height,
        sizeof(uint32_t), cs, cs == ColorSpace::ARGB8888 || cs == ColorSpace::ABGR8888);
    if (!surface.data) return false;

    // The first frame right away; the thread takes the next ones.
    LONGLONG pts = 0;
    engine->OnVideoStreamTick(&pts);
    if (!readback(surface.buf32)) return false;

    paused = true;
    worker = std::thread(&MfMediaLoader::run, this);
    return true;
}

bool MfMediaLoader::sync()
{
    if (notify->failed) return false;

    ScopedLock lock(key);
    if (ended) curTime = totalTime;

    // Return the canvas buffer to the ring while taking the latest frame.
    if (!ring.updated) return false;
    std::swap(surface.buf32, ring.frames[ring.latest]);
    ring.updated = false;

    // Preserve the requested time while paused or seeking.
    if (!paused && !ended && request.position < 0.0 && ring.time >= 0.0f) curTime = ring.time;

    // rasterConvertCS() and rasterPremultiply() may have updated the previous frame in place.
    surface.setup(surface.buf32, surface.stride, surface.w, surface.h, surface.channelSize, cs, surface.alphaIgnored);
    return true;
}

void MfMediaLoader::post(double position, bool run)
{
    if (position >= 0.0) request.position = position;
    request.running = run;
    request.pending = true;
    SetEvent(wake);
}

Result MfMediaLoader::play()
{
    if (notify->failed) return Result::Unknown;

    ScopedLock lock(key);
    // Playing a finished playback starts it over.
    post(ended ? 0.0 : NO_SEEK, true);
    if (ended) curTime = 0.0f;
    ended = false;
    started = true;
    paused = false;
    return Result::Success;
}

Result MfMediaLoader::pause()
{
    if (notify->failed) return Result::Unknown;

    ScopedLock lock(key);
    if (!started || ended) return Result::InsufficientCondition;
    paused = true;
    post(NO_SEEK, false);
    return Result::Success;
}

Result MfMediaLoader::stop()
{
    started = false;
    paused = true;
    return seek(0.0f);
}

Result MfMediaLoader::seek(float seconds)
{
    if (notify->failed) return Result::Unknown;

    ScopedLock lock(key);
    // Wrap or clamp seeks at the end.
    auto target = static_cast<double>(seconds);
    if (seconds >= totalTime) target = looping ? 0.0 : static_cast<double>(totalTime) - TIME_EPSILON;
    ended = seconds >= totalTime && !looping;
    curTime = ended ? totalTime : static_cast<float>(target);

    // The engine runs only while playing and not ended, so a seek after the end plays on.
    post(target, !paused && !ended);
    return Result::Success;
}

Result MfMediaLoader::loop(bool on)
{
    if (notify->failed) return Result::Unknown;
    if (FAILED(engine->SetLoop(on))) return Result::Unknown;
    looping = on;
    return Result::Success;
}

Result MfMediaLoader::volume(float volume)
{
    if (notify->failed) return Result::Unknown;
    // Lottie sets the volume at every frame: skip the engine, which may wait for a frame transfer.
    if (volume == audioVolume) return Result::Success;
    if (FAILED(engine->SetVolume(volume))) return Result::Unknown;
    audioVolume = volume;
    return Result::Success;
}

Result MfMediaLoader::mute(bool on)
{
    if (notify->failed) return Result::Unknown;
    if (FAILED(engine->SetMuted(on))) return Result::Unknown;
    muted = on;
    return Result::Success;
}

MediaLoader* MediaLoader::gen()
{
    return new MfMediaLoader;
}
