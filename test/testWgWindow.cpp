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

#include <algorithm>
#include <vector>
#include "catch.hpp"
#include "testWgWindow.h"

using namespace tvg;

#if defined(THORVG_WG_ENGINE_SUPPORT) && defined(THORVG_WG_TEST_SUPPORT)

namespace
{

static WGPUAdapter _requestAdapter(WGPUInstance instance)
{
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    } state;

    const WGPURequestAdapterOptions options = {
        .featureLevel = WGPUFeatureLevel_Compatibility,
        .powerPreference = WGPUPowerPreference_HighPerformance,
        .compatibleSurface = nullptr
    };

    const WGPURequestAdapterCallbackInfo callback = {
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* userdata1, void*) {
            auto state = static_cast<UserData*>(userdata1);
            if (status == WGPURequestAdapterStatus_Success) state->adapter = adapter;
            state->requestEnded = true;
        },
        .userdata1 = &state
    };

    auto future = wgpuInstanceRequestAdapter(instance, &options, callback);
    (void) future;

    return state.requestEnded ? state.adapter : nullptr;
}

static WGPUDevice _requestDevice(WGPUAdapter adapter)
{
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    } state;

    const WGPUDeviceDescriptor descriptor = {
        .uncapturedErrorCallbackInfo = {
            .callback = [](WGPUDevice const*, WGPUErrorType, WGPUStringView, void*, void*) {}
        }
    };

    const WGPURequestDeviceCallbackInfo callback = {
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* userdata1, void*) {
            auto state = static_cast<UserData*>(userdata1);
            if (status == WGPURequestDeviceStatus_Success) state->device = device;
            state->requestEnded = true;
        },
        .userdata1 = &state
    };

    auto future = wgpuAdapterRequestDevice(adapter, &descriptor, callback);
    (void) future;

    return state.requestEnded ? state.device : nullptr;
}

}

WgWindow::~WgWindow()
{
    releaseTexture();

    if (context.queue) wgpuQueueRelease(context.queue);
    if (context.device) wgpuDeviceRelease(context.device);
    if (context.instance) wgpuInstanceRelease(context.instance);
}

void WgWindow::releaseTexture()
{
    if (texture) {
        wgpuTextureDestroy(texture);
        wgpuTextureRelease(texture);
        texture = nullptr;
    }
}

bool WgWindow::recreateTarget()
{
    releaseTexture();

    const WGPUTextureDescriptor textureDesc = {
        // RenderAttachment is the offscreen target, CopySrc is for readback,
        // CopyDst lets tests reset the target before drawing.
        .usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = { width, height, 1 },
        .format = context.format,
        .mipLevelCount = 1,
        .sampleCount = 1
    };
    texture = wgpuDeviceCreateTexture(context.device, &textureDesc);
    return texture;
}

void WgWindow::clear()
{
    if (!context.queue || !texture) return;

    std::vector<uint8_t> zeros(static_cast<size_t>(width) * height * 4, 0);
    const WGPUTexelCopyTextureInfo copyTextureInfo = { .texture = texture };
    const WGPUTexelCopyBufferLayout copyBufferLayout = { .bytesPerRow = width * 4, .rowsPerImage = height };
    const WGPUExtent3D writeSize = { .width = width, .height = height, .depthOrArrayLayers = 1 };
    wgpuQueueWriteTexture(context.queue, &copyTextureInfo, zeros.data(), zeros.size(), &copyBufferLayout, &writeSize);
}

bool WgWindow::init(uint32_t w, uint32_t h)
{
    width = w;
    height = h;

    context.instance = wgpuCreateInstance(nullptr);
    if (!context.instance) return false;

    auto adapter = _requestAdapter(context.instance);
    if (!adapter) return false;

    context.device = _requestDevice(adapter);
    if (adapter) wgpuAdapterRelease(adapter);
    if (!context.device) return false;

    context.queue = wgpuDeviceGetQueue(context.device);
    if (!context.queue) return false;

    if (!recreateTarget()) return false;

    clear();
    return true;
}

Result WgWindow::target(WgCanvas* canvas)
{
    if (!canvas || !context.instance || !context.device || !texture) return Result::InvalidArguments;

    clear();
    return canvas->target(context.device, context.instance, texture, width, height, ColorSpace::ABGR8888S, 1);
}

bool WgWindow::hasRenderedContent()
{
    if (!context.instance || !context.device || !context.queue || !texture) return false;

    // WebGPU requires bytesPerRow to be a multiple of 256 for texture-to-buffer copies.
    auto readbackStride = (width * 4 + 255) & ~255u;

    const WGPUBufferDescriptor bufferDesc = {
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
        .size = static_cast<uint64_t>(readbackStride) * height,
        .mappedAtCreation = false
    };
    auto readback = wgpuDeviceCreateBuffer(context.device, &bufferDesc);
    if (!readback) return false;

    auto releaseReadback = [&]() {
        wgpuBufferDestroy(readback);
        wgpuBufferRelease(readback);
        readback = nullptr;
    };

    WGPUCommandEncoderDescriptor encoderDesc{};
    auto encoder = wgpuDeviceCreateCommandEncoder(context.device, &encoderDesc);
    if (!encoder) {
        releaseReadback();
        return false;
    }

    const WGPUTexelCopyTextureInfo src = {
        .texture = texture,
        .aspect = WGPUTextureAspect_All
    };
    const WGPUTexelCopyBufferInfo dst = {
        .layout = {
            .bytesPerRow = readbackStride,
            .rowsPerImage = height
        },
        .buffer = readback
    };
    const WGPUExtent3D size = {
        .width = width,
        .height = height,
        .depthOrArrayLayers = 1
    };

    wgpuCommandEncoderCopyTextureToBuffer(encoder, &src, &dst, &size);

    const WGPUCommandBufferDescriptor commandBufferDesc{};
    auto commands = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
    wgpuCommandEncoderRelease(encoder);
    if (!commands) {
        releaseReadback();
        return false;
    }

    wgpuQueueSubmit(context.queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    struct MapState {
        bool done = false;
        bool success = false;
    } state;

    const WGPUBufferMapCallbackInfo callback = {
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
            auto state = static_cast<MapState*>(userdata1);
            state->success = status == WGPUMapAsyncStatus_Success;
            state->done = true;
        },
        .userdata1 = &state
    };

    auto future = wgpuBufferMapAsync(readback, WGPUMapMode_Read, 0, static_cast<size_t>(readbackStride) * height, callback);
    (void) future;
    while (!state.done) wgpuQueueSubmit(context.queue, 0, nullptr);
    if (!state.success) {
        releaseReadback();
        return false;
    }

    auto data = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(readback, 0, static_cast<size_t>(readbackStride) * height));
    if (!data) {
        wgpuBufferUnmap(readback);
        releaseReadback();
        return false;
    }

    bool hasContent = false;
    for (uint32_t y = 0; y < height && !hasContent; ++y) {
        auto row = data + static_cast<size_t>(y) * readbackStride;
        hasContent = std::any_of(row, row + static_cast<size_t>(width) * 4, [](uint8_t value) { return value != 0; });
    }

    wgpuBufferUnmap(readback);
    releaseReadback();
    return hasContent;
}

std::unique_ptr<WgWindow> genWgWindow(uint32_t w, uint32_t h)
{
    auto window = std::make_unique<WgWindow>();
    INFO("Failed to create an offscreen WebGPU context.");
    REQUIRE(window->init(w, h));
    return window;
}

#endif
