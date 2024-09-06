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

#ifndef _TVG_WG_COMMON_H_
#define _TVG_WG_COMMON_H_

#include <cassert>
#include <webgpu/webgpu.h>

#define WG_VERTEX_BUFFER_MIN_SIZE 2048
#define WG_INDEX_BUFFER_MIN_SIZE 2048

class WgPipelines;

struct WgContext {
    // external webgpu handles
    WGPUInstance instance{};
    WGPUAdapter adapter{};
    WGPUDevice device{};
    // common webgpu handles
    WGPUQueue queue{};
    WGPUTextureFormat preferredFormat{};
    // external handles (do not release)
    WgPipelines* pipelines{};
    // shared webgpu assets
    WGPUBuffer bufferIndexFan{};
    WGPUSampler samplerNearestRepeat{};
    WGPUSampler samplerLinearRepeat{};
    WGPUSampler samplerLinearMirror{};
    WGPUSampler samplerLinearClamp{};

    void initialize(WGPUInstance instance, WGPUDevice device);
    void release();
    
    // create common objects
    WGPUSampler createSampler(WGPUFilterMode filter, WGPUMipmapFilterMode mipmapFilter, WGPUAddressMode addrMode);
    WGPUTexture createTexture(uint32_t width, uint32_t height, WGPUTextureFormat format);
    WGPUTexture createTexStorage(uint32_t width, uint32_t height, WGPUTextureFormat format, uint32_t sc = 1);
    WGPUTexture createTexStencil(uint32_t width, uint32_t height, WGPUTextureFormat format, uint32_t sc = 1);
    WGPUTextureView createTextureView(WGPUTexture texture);
    bool allocateTexture(WGPUTexture& texture, uint32_t width, uint32_t height, WGPUTextureFormat format, void* data);

    // release common objects
    void releaseTextureView(WGPUTextureView& textureView);
    void releaseTexture(WGPUTexture& texture);
    void releaseSampler(WGPUSampler& sampler);

    // create buffer objects (return true, if buffer handle was changed)
    bool allocateBufferUniform(WGPUBuffer& buffer, const void* data, uint64_t size);
    bool allocateBufferVertex(WGPUBuffer& buffer, const float* data, uint64_t size);
    bool allocateBufferIndex(WGPUBuffer& buffer, const uint32_t* data, uint64_t size);
    bool allocateBufferIndexFan(uint64_t vertexCount);

    // release buffer objects
    void releaseBuffer(WGPUBuffer& buffer);
};

#endif // _TVG_WG_COMMON_H_
