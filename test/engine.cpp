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

#include <chrono>
#include <thread>
#include "engine.h"
#include "testGlHelpers.h"

using namespace tvg;

#if defined(THORVG_GL_ENGINE_SUPPORT) && defined(THORVG_GL_TEST_SUPPORT)

TvgGlTestEngine::~TvgGlTestEngine()
{
    release();
    eglReleaseThread();
}

bool TvgGlTestEngine::init(uint32_t w, uint32_t h, ColorSpace cs)
{
    release();
    width = w;
    height = h;
    colorSpace = cs;
    return setup();
}

bool TvgGlTestEngine::init(uint32_t w, uint32_t h)
{
    return init(w, h, ColorSpace::ABGR8888S);
}

std::unique_ptr<Canvas> TvgGlTestEngine::canvas()
{
    return std::unique_ptr<Canvas>(GlCanvas::gen());
}

Result TvgGlTestEngine::target(Canvas* canvas)
{
    if (!canvas) return Result::InvalidArguments;
    if (!makeCurrent()) return Result::Unknown;
    clear();
    return static_cast<GlCanvas*>(canvas)->target(display, surface, context, 0, width, height, colorSpace);
}

bool TvgGlTestEngine::rendered()
{
    if (!makeCurrent()) return false;
    REQUIRE_NO_GL_ERROR("before rendered");

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    REQUIRE_GL(glFinish());
    REQUIRE_GL(glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
    return std::any_of(pixels.begin(), pixels.end(), [](uint8_t value) { return value != 0; });
}

bool TvgGlTestEngine::setup()
{
    if (!initDisplay()) return false;
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) return false;
    if (!initConfig()) return false;
    if (!initSurface()) return false;
    if (!initContext()) return false;
    if (!makeCurrent()) return false;
    clear();
    return true;
}

void TvgGlTestEngine::release()
{
    if (display == EGL_NO_DISPLAY) return;

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    eglTerminate(display);
    display = EGL_NO_DISPLAY;
    config = nullptr;
}

bool TvgGlTestEngine::initDisplay()
{
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) return false;
    if (eglInitialize(display, nullptr, nullptr) == EGL_TRUE) return true;

    display = EGL_NO_DISPLAY;

    auto queryDevicesExt = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    if (!queryDevicesExt) return false;

    auto getPlatformDisplayExt = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!getPlatformDisplayExt) {
        getPlatformDisplayExt = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplay"));
    }

    if (!getPlatformDisplayExt) return false;

    EGLDeviceEXT device = nullptr;
    EGLint numDevices = 0;
    if (queryDevicesExt(1, &device, &numDevices) != EGL_TRUE || numDevices <= 0) return false;

    display = getPlatformDisplayExt(EGL_PLATFORM_DEVICE_EXT, device, nullptr);
    if (display == EGL_NO_DISPLAY) return false;
    if (eglInitialize(display, nullptr, nullptr) != EGL_TRUE) {
        display = EGL_NO_DISPLAY;
        return false;
    }

    return true;
}

bool TvgGlTestEngine::initConfig()
{
    const EGLint attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLint count = 0;
    return eglChooseConfig(display, attrs, &config, 1, &count) == EGL_TRUE && count > 0;
}

bool TvgGlTestEngine::initContext()
{
    const EGLint attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    return context != EGL_NO_CONTEXT;
}

bool TvgGlTestEngine::initSurface()
{
    const EGLint attrs[] = {
        EGL_WIDTH, static_cast<EGLint>(width),
        EGL_HEIGHT, static_cast<EGLint>(height),
        EGL_NONE
    };

    surface = eglCreatePbufferSurface(display, config, attrs);
    return surface != EGL_NO_SURFACE;
}

bool TvgGlTestEngine::makeCurrent()
{
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT) return false;
    return eglMakeCurrent(display, surface, surface, context) == EGL_TRUE;
}

void TvgGlTestEngine::clear()
{
    REQUIRE_NO_GL_ERROR("before clear");

    REQUIRE_GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    REQUIRE_GL(glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height)));
    REQUIRE_GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    REQUIRE_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
}

#endif

#if defined(THORVG_WG_ENGINE_SUPPORT) && defined(THORVG_WG_TEST_SUPPORT)

namespace
{

class WgTextureReadback
{
public:
    WgTextureReadback(WGPUDevice device, WGPUTexture texture, uint32_t width, uint32_t height)
    {
        // WebGPU requires texture-to-buffer rows to be aligned to 256 bytes.
        uint32_t stride = (width * 4 + 255) & ~255u;
        size = static_cast<size_t>(stride) * height;
        buffer = copyTextureToBuffer(device, texture, width, height, stride);
    }

    ~WgTextureReadback()
    {
        if (data) wgpuBufferUnmap(buffer);
        releaseBuffer(buffer);
    }

    WgTextureReadback(const WgTextureReadback&) = delete;
    WgTextureReadback& operator=(const WgTextureReadback&) = delete;

    const uint8_t* map(WGPUInstance instance)
    {
        if (!buffer) return nullptr;
        if (!data) data = mapBuffer(instance);
        return data;
    }

    size_t mappedSize() const
    {
        return size;
    }

private:
    WGPUBuffer copyTextureToBuffer(WGPUDevice device, WGPUTexture texture, uint32_t width, uint32_t height, uint32_t stride)
    {
        WGPUBufferDescriptor bufferDesc = { .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead, .size = static_cast<uint64_t>(stride) * height };

        auto readback = wgpuDeviceCreateBuffer(device, &bufferDesc);
        if (!readback) return nullptr;

        WGPUCommandEncoderDescriptor encoderDesc = {};
        auto encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);
        if (!encoder) {
            releaseBuffer(readback);
            return nullptr;
        }

        WGPUTexelCopyTextureInfo src = { .texture = texture, .aspect = WGPUTextureAspect_All };
        WGPUTexelCopyBufferInfo dst = { .layout = { .bytesPerRow = stride, .rowsPerImage = height }, .buffer = readback };
        WGPUExtent3D copySize = { .width = width, .height = height, .depthOrArrayLayers = 1 };

        wgpuCommandEncoderCopyTextureToBuffer(encoder, &src, &dst, &copySize);

        WGPUCommandBufferDescriptor commandBufferDesc = {};
        auto commands = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
        wgpuCommandEncoderRelease(encoder);
        if (!commands) {
            releaseBuffer(readback);
            return nullptr;
        }

        auto queue = wgpuDeviceGetQueue(device);
        if (!queue) {
            wgpuCommandBufferRelease(commands);
            releaseBuffer(readback);
            return nullptr;
        }

        wgpuQueueSubmit(queue, 1, &commands);
        wgpuQueueRelease(queue);
        wgpuCommandBufferRelease(commands);

        return readback;
    }

    static void releaseBuffer(WGPUBuffer buffer)
    {
        if (!buffer) return;

        wgpuBufferDestroy(buffer);
        wgpuBufferRelease(buffer);
    }

    const uint8_t* mapBuffer(WGPUInstance instance)
    {
        WGPUMapAsyncStatus mapStatus = WGPUMapAsyncStatus_Unknown;

        WGPUBufferMapCallbackInfo callback = {};
        callback.mode = WGPUCallbackMode_AllowProcessEvents;
        callback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
            *((WGPUMapAsyncStatus*)userdata1) = status;
        };
        callback.userdata1 = &mapStatus;

        wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, callback);

        using clock = std::chrono::steady_clock;
        auto timeout = clock::now() + std::chrono::seconds(5);
        while (mapStatus == WGPUMapAsyncStatus_Unknown && clock::now() < timeout) {
            wgpuInstanceProcessEvents(instance);
            std::this_thread::yield();
        }

        if (mapStatus != WGPUMapAsyncStatus_Success) return nullptr;
        return static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(buffer, 0, size));
    }

    size_t size = 0;
    WGPUBuffer buffer = nullptr;
    const uint8_t* data = nullptr;
};

}

TvgWgTestEngine::~TvgWgTestEngine()
{
    release();
}

bool TvgWgTestEngine::init(uint32_t w, uint32_t h)
{
    return init(w, h, ColorSpace::ABGR8888S);
}

bool TvgWgTestEngine::init(uint32_t w, uint32_t h, ColorSpace cs)
{
    release();
    width = w;
    height = h;
    colorSpace = cs;
    if (setup()) return true;

    release();
    return false;
}

std::unique_ptr<Canvas> TvgWgTestEngine::canvas()
{
    return std::unique_ptr<Canvas>(WgCanvas::gen());
}

Result TvgWgTestEngine::target(Canvas* canvas)
{
    if (!canvas) return Result::InvalidArguments;
    if (!instance || !device || !texture) return Result::Unknown;

    clear();
    return static_cast<WgCanvas*>(canvas)->target(device, instance, texture, width, height, colorSpace, 1);
}

bool TvgWgTestEngine::rendered()
{
    if (!instance || !device || !texture) return false;

    WgTextureReadback readback(device, texture, width, height);
    auto data = readback.map(instance);
    return data && std::any_of(data, data + readback.mappedSize(), [](uint8_t value) { return value != 0; });
}

bool TvgWgTestEngine::setup()
{
    instance = wgpuCreateInstance(nullptr);
    if (!instance) return false;

    // request adapter
    WGPUAdapter adapter = nullptr;
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus, WGPUAdapter adapter, WGPUStringView, void* userdata1, void*) { *((WGPUAdapter*)userdata1) = adapter; };
    const WGPURequestAdapterOptions requestAdapterOptions { .featureLevel = WGPUFeatureLevel_Compatibility, .powerPreference = WGPUPowerPreference_HighPerformance, .compatibleSurface = nullptr };
    const WGPURequestAdapterCallbackInfo requestAdapterCallback { .mode = WGPUCallbackMode_WaitAnyOnly, .callback = onAdapterRequestEnded, .userdata1 = &adapter };
    wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, requestAdapterCallback);
    if (!adapter) return false;

    // request device
    auto onDeviceError = [](WGPUDevice const*, WGPUErrorType, WGPUStringView, void*, void*) {};
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus, WGPUDevice device, WGPUStringView, void* userdata1, void*) { *((WGPUDevice*)userdata1) = device; };
    const WGPUDeviceDescriptor deviceDesc { .label = { "ThorVG Test Device", WGPU_STRLEN }, .uncapturedErrorCallbackInfo = { .callback = onDeviceError } };
    const WGPURequestDeviceCallbackInfo requestDeviceCallback { .mode = WGPUCallbackMode_WaitAnyOnly, .callback = onDeviceRequestEnded, .userdata1 = &device };
    wgpuAdapterRequestDevice(adapter, &deviceDesc, requestDeviceCallback);

    wgpuAdapterRelease(adapter);
    if (!device) return false;

    if (!recreateTarget()) return false;

    clear();
    return true;
}

bool TvgWgTestEngine::recreateTarget()
{
    releaseTexture();

    WGPUTextureDescriptor textureDesc = {};
    textureDesc.usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment;
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size.width = width;
    textureDesc.size.height = height;
    textureDesc.size.depthOrArrayLayers = 1;
    textureDesc.format = format;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;

    texture = wgpuDeviceCreateTexture(device, &textureDesc);
    return texture;
}

void TvgWgTestEngine::release()
{
    releaseTexture();

    if (device) {
        wgpuDeviceDestroy(device);
        wgpuDeviceRelease(device);
        device = nullptr;
    }
    if (instance) {
        wgpuInstanceRelease(instance);
        instance = nullptr;
    }
}

void TvgWgTestEngine::releaseTexture()
{
    if (!texture) return;

    wgpuTextureDestroy(texture);
    wgpuTextureRelease(texture);
    texture = nullptr;
}

void TvgWgTestEngine::clear()
{
    if (!device || !texture) return;

    auto queue = wgpuDeviceGetQueue(device);
    if (!queue) return;

    std::vector<uint8_t> zeros(static_cast<size_t>(width) * height * 4, 0);

    WGPUTexelCopyTextureInfo copyTextureInfo = {};
    copyTextureInfo.texture = texture;

    WGPUTexelCopyBufferLayout copyBufferLayout = {};
    copyBufferLayout.bytesPerRow = width * 4;
    copyBufferLayout.rowsPerImage = height;

    WGPUExtent3D writeSize = {};
    writeSize.width = width;
    writeSize.height = height;
    writeSize.depthOrArrayLayers = 1;

    wgpuQueueWriteTexture(queue, &copyTextureInfo, zeros.data(), zeros.size(), &copyBufferLayout, &writeSize);
    wgpuQueueRelease(queue);
}

#endif
