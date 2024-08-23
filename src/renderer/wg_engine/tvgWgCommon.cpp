/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "tvgWgCommon.h"
#include "tvgArray.h"
#include <iostream>

void WgContext::initialize(WGPUInstance instance, WGPUSurface surface)
{
    assert(instance);
    assert(surface);
    
    // store global instance and surface
    this->instance = instance;
    this->surface = surface;

    // request adapter
    const WGPURequestAdapterOptions requestAdapterOptions { .nextInChain = nullptr, .compatibleSurface = surface, .powerPreference = WGPUPowerPreference_HighPerformance, .forceFallbackAdapter = false };
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) { *((WGPUAdapter*)pUserData) = adapter; };
    wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, onAdapterRequestEnded, &adapter);
    #ifdef __EMSCRIPTEN__
    while (!adapter) emscripten_sleep(10);
    #endif
    assert(adapter);

    // get adapter and surface properties
    WGPUFeatureName featureNames[32]{};
    size_t featuresCount = wgpuAdapterEnumerateFeatures(adapter, featureNames);
    preferredFormat = WGPUTextureFormat_BGRA8Unorm;

    // request device
    const WGPUDeviceDescriptor deviceDesc { .nextInChain = nullptr, .label = "The device", .requiredFeatureCount = featuresCount, .requiredFeatures = featureNames };
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) { *((WGPUDevice*)pUserData) = device; };
    wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceRequestEnded, &device);
    #ifdef __EMSCRIPTEN__
    while (!device) emscripten_sleep(10);
    #endif
    assert(device);

    // device uncaptured error callback
    auto onDeviceError = [](WGPUErrorType type, char const* message, void* pUserData) { std::cout << message << std::endl; };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);

    // get current queue
    queue = wgpuDeviceGetQueue(device);
    assert(queue);

    // create shared webgpu assets
    allocateBufferIndexFan(32768);
    samplerNearestRepeat = createSampler(WGPUFilterMode_Nearest, WGPUMipmapFilterMode_Nearest, WGPUAddressMode_Repeat);
    samplerLinearRepeat = createSampler(WGPUFilterMode_Linear, WGPUMipmapFilterMode_Linear, WGPUAddressMode_Repeat);
    samplerLinearMirror = createSampler(WGPUFilterMode_Linear, WGPUMipmapFilterMode_Linear, WGPUAddressMode_MirrorRepeat);
    samplerLinearClamp = createSampler(WGPUFilterMode_Linear, WGPUMipmapFilterMode_Linear, WGPUAddressMode_ClampToEdge);
    assert(samplerNearestRepeat);
    assert(samplerLinearRepeat);
    assert(samplerLinearMirror);
    assert(samplerLinearClamp);
}


void WgContext::release()
{
    releaseSampler(samplerLinearClamp);
    releaseSampler(samplerLinearMirror);
    releaseSampler(samplerLinearRepeat);
    releaseSampler(samplerNearestRepeat);
    releaseBuffer(bufferIndexFan);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
}


WGPUSampler WgContext::createSampler(WGPUFilterMode filter, WGPUMipmapFilterMode mipmapFilter, WGPUAddressMode addrMode)
{
    const WGPUSamplerDescriptor samplerDesc {
        .addressModeU = addrMode, .addressModeV = addrMode, .addressModeW = addrMode,
        .magFilter = filter, .minFilter = filter, .mipmapFilter = mipmapFilter,
        .lodMinClamp = 0.0f, .lodMaxClamp = 32.0f, .maxAnisotropy = 1
    };
    return wgpuDeviceCreateSampler(device, &samplerDesc);
}


bool WgContext::allocateTexture(WGPUTexture& texture, uint32_t width, uint32_t height, WGPUTextureFormat format, void* data)
{
    if ((texture) && (wgpuTextureGetWidth(texture) == width) && (wgpuTextureGetHeight(texture) == height)) {
        // update texture data
        const WGPUImageCopyTexture imageCopyTexture{ .texture = texture };
        const WGPUTextureDataLayout textureDataLayout{ .bytesPerRow = 4 * width, .rowsPerImage = height };
        const WGPUExtent3D writeSize{ .width = width, .height = height, .depthOrArrayLayers = 1 };
        wgpuQueueWriteTexture(queue, &imageCopyTexture, data, 4 * width * height, &textureDataLayout, &writeSize);
        wgpuQueueSubmit(queue, 0, nullptr);
    } else {
        releaseTexture(texture);
        texture = createTexture(width, height, format);
        // update texture data
        const WGPUImageCopyTexture imageCopyTexture{ .texture = texture };
        const WGPUTextureDataLayout textureDataLayout{ .bytesPerRow = 4 * width, .rowsPerImage = height };
        const WGPUExtent3D writeSize{ .width = width, .height = height, .depthOrArrayLayers = 1 };
        wgpuQueueWriteTexture(queue, &imageCopyTexture, data, 4 * width * height, &textureDataLayout, &writeSize);
        wgpuQueueSubmit(queue, 0, nullptr);
        return true;
    }
    return false;

}


WGPUTexture WgContext::createTexture(uint32_t width, uint32_t height, WGPUTextureFormat format)
{
    const WGPUTextureDescriptor textureDesc {
        .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
        .dimension = WGPUTextureDimension_2D, .size = { width, height, 1 },
        .format = format, .mipLevelCount = 1, .sampleCount = 1
    };
    return wgpuDeviceCreateTexture(device, &textureDesc);
}


WGPUTexture WgContext::createTexStorage(uint32_t width, uint32_t height, WGPUTextureFormat format, uint32_t sc)
{
    const WGPUTextureDescriptor textureDesc {
        .usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_StorageBinding | WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D, .size = { width, height, 1 },
        .format = format, .mipLevelCount = 1, .sampleCount = sc
    };
    return wgpuDeviceCreateTexture(device, &textureDesc);
}


WGPUTexture WgContext::createTexStencil(uint32_t width, uint32_t height, WGPUTextureFormat format, uint32_t sc)
{
    const WGPUTextureDescriptor textureDesc {
        .usage = WGPUTextureUsage_RenderAttachment,
        .dimension = WGPUTextureDimension_2D, .size = { width, height, 1 },
        .format = format, .mipLevelCount = 1, .sampleCount = sc
    };
    return wgpuDeviceCreateTexture(device, &textureDesc);
}


WGPUTextureView WgContext::createTextureView(WGPUTexture texture)
{
    const WGPUTextureViewDescriptor textureViewDesc {
        .format = wgpuTextureGetFormat(texture),
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All
    };
    return wgpuTextureCreateView(texture, &textureViewDesc);
}


void WgContext::releaseTextureView(WGPUTextureView& textureView)
{
    if (textureView) {
        wgpuTextureViewRelease(textureView);
        textureView = nullptr;
    }
}


void WgContext::releaseTexture(WGPUTexture& texture)
{
    if (texture) {
        wgpuTextureDestroy(texture);
        wgpuTextureRelease(texture);
        texture = nullptr;
    }
    
}


void WgContext::releaseSampler(WGPUSampler& sampler)
{
    if (sampler) {
        wgpuSamplerRelease(sampler);
        sampler = nullptr;
    }
}


bool WgContext::allocateBufferUniform(WGPUBuffer& buffer, const void* data, uint64_t size)
{
    if ((buffer) && (wgpuBufferGetSize(buffer) >= size))
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    else {
        releaseBuffer(buffer);
        const WGPUBufferDescriptor bufferDesc { .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, .size = size };
        buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
        return true;
    }
    return false;
}


bool WgContext::allocateBufferVertex(WGPUBuffer& buffer, const float* data, uint64_t size)
{
    if ((buffer) && (wgpuBufferGetSize(buffer) >= size))
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    else {
        releaseBuffer(buffer);
        const WGPUBufferDescriptor bufferDesc {
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            .size = size > WG_VERTEX_BUFFER_MIN_SIZE ? size : WG_VERTEX_BUFFER_MIN_SIZE
        };
        buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
        return true;
    }
    return false;
}


bool WgContext::allocateBufferIndex(WGPUBuffer& buffer, const uint32_t* data, uint64_t size)
{
    if ((buffer) && (wgpuBufferGetSize(buffer) >= size))
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    else {
        releaseBuffer(buffer);
        const WGPUBufferDescriptor bufferDesc {
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
            .size = size > WG_INDEX_BUFFER_MIN_SIZE ? size : WG_INDEX_BUFFER_MIN_SIZE
        };
        buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
        return true;
    }
    return false;
}


bool WgContext::allocateBufferIndexFan(uint64_t vertexCount)
{
    uint64_t indexCount = (vertexCount - 2) * 3;
    if ((!bufferIndexFan) || (wgpuBufferGetSize(bufferIndexFan) < indexCount * sizeof(uint32_t))) {
        tvg::Array<uint32_t> indexes(indexCount);
        for (size_t i = 0; i < vertexCount - 2; i++) {
            indexes.push(0);
            indexes.push(i + 1);
            indexes.push(i + 2);
        }
        releaseBuffer(bufferIndexFan);
        WGPUBufferDescriptor bufferDesc{
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
            .size = indexCount * sizeof(uint32_t)
        };
        bufferIndexFan = wgpuDeviceCreateBuffer(device, &bufferDesc);
        wgpuQueueWriteBuffer(queue, bufferIndexFan, 0, &indexes[0], indexCount * sizeof(uint32_t));
        return true;
    }
    return false;
}


void WgContext::releaseBuffer(WGPUBuffer& buffer)
{
    if (buffer) { 
        wgpuBufferDestroy(buffer);
        wgpuBufferRelease(buffer);
        buffer = nullptr;
    }
}
