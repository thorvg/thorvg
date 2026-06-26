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

#ifndef _TVG_TEST_WG_ENGINE_H_
#define _TVG_TEST_WG_ENGINE_H_

#include "config.h"

#if defined(THORVG_WG_TEST_SUPPORT)

#include <thorvg.h>
#include <webgpu/webgpu.h>

using namespace tvg;

struct TestWgEngine
{
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUTexture texture = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_BGRA8Unorm;
    uint32_t width;
    uint32_t height;
    ColorSpace colorSpace;

    TestWgEngine(uint32_t w = 100, uint32_t h = 100, ColorSpace cs = ColorSpace::ABGR8888) : width(w), height(h), colorSpace(cs)
    {
        instance = wgpuCreateInstance(nullptr);

        // request adapter
        auto onAdapterRequestEnded = [](WGPURequestAdapterStatus, WGPUAdapter adapter, WGPUStringView, void* userdata1, void*) {
            *((WGPUAdapter*)userdata1) = adapter;
        };
        WGPURequestAdapterOptions requestAdapterOptions = {};
        requestAdapterOptions.featureLevel = WGPUFeatureLevel_Compatibility;
        requestAdapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

        WGPURequestAdapterCallbackInfo requestAdapterCallback = {};
        requestAdapterCallback.mode = WGPUCallbackMode_WaitAnyOnly;
        requestAdapterCallback.callback = onAdapterRequestEnded;
        requestAdapterCallback.userdata1 = &adapter;
        wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, requestAdapterCallback);

        // request device
        auto onDeviceError = [](WGPUDevice const*, WGPUErrorType, WGPUStringView, void*, void*) {
        };
        auto onDeviceRequestEnded = [](WGPURequestDeviceStatus, WGPUDevice device, WGPUStringView, void* userdata1, void*) {
            *((WGPUDevice*)userdata1) = device;
        };
        WGPUDeviceDescriptor deviceDesc = {};
        deviceDesc.label.data = "ThorVG Test Device";
        deviceDesc.label.length = WGPU_STRLEN;
        deviceDesc.uncapturedErrorCallbackInfo.callback = onDeviceError;

        WGPURequestDeviceCallbackInfo requestDeviceCallback = {};
        requestDeviceCallback.mode = WGPUCallbackMode_WaitAnyOnly;
        requestDeviceCallback.callback = onDeviceRequestEnded;
        requestDeviceCallback.userdata1 = &device;
        wgpuAdapterRequestDevice(adapter, &deviceDesc, requestDeviceCallback);

        wgpuAdapterRelease(adapter);

        // create texture
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
    }

    ~TestWgEngine()
    {
        wgpuTextureDestroy(texture);
        wgpuTextureRelease(texture);
        wgpuDeviceDestroy(device);
        wgpuDeviceRelease(device);
        wgpuInstanceRelease(instance);
    }

    Result target(WgCanvas* canvas)
    {
        return canvas->target({instance, adapter, device}, texture, width, height, colorSpace, 1);
    }
};

#endif

#endif
