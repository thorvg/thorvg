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

#ifndef _TVG_WG_RENDER_TARGET_H_
#define _TVG_WG_RENDER_TARGET_H_

#include "tvgWgRenderData.h"

class WgRenderStorage {
private:
    // texture buffers
    WgBindGroupCanvas mBindGroupCanvas;
    WGPURenderPassEncoder mRenderPassEncoder{};
    WgPipelines* mPipelines{}; // external handle
public:
    WGPUTexture texColor{};
    WGPUTexture texStencil{};
    WGPUTextureView texViewColor{};
    WGPUTextureView texViewStencil{};
    WgBindGroupTextureStorage bindGroupTexStorage;
    uint32_t width{};
    uint32_t height{};
    uint32_t workgroupsCountX{};
    uint32_t workgroupsCountY{};
public:
    void initialize(WgContext& context, uint32_t w, uint32_t h, uint32_t samples = 1);
    void release(WgContext& context);

    void beginRenderPass(WGPUCommandEncoder commandEncoder, bool clear);
    void endRenderPass();

    void renderShape(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType);
    void renderPicture(WgContext& context, WgRenderDataPicture* renderData, WgPipelineBlendType blendType);

    void clear(WGPUCommandEncoder commandEncoder);
    void blend(WGPUCommandEncoder commandEncoder, WgRenderStorage* targetSrc, WgBindGroupBlendMethod* blendMethod);
    void compose(WGPUCommandEncoder commandEncoder, WgRenderStorage* targetMsk, WgBindGroupCompositeMethod* composeMethod, WgBindGroupOpacity* opacity);
    void antialias(WGPUCommandEncoder commandEncoder, WgRenderStorage* targetSrc);
private:
    void drawShape(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType);
    void drawStroke(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType);

    void dispatchWorkgroups(WGPUComputePassEncoder computePassEncoder);

    WGPUComputePassEncoder beginComputePass(WGPUCommandEncoder commandEncoder);
    void endComputePass(WGPUComputePassEncoder computePassEncoder);
};


class WgRenderStoragePool {
private:
   Array<WgRenderStorage*> mList;
   Array<WgRenderStorage*> mPool;
public:
   WgRenderStorage* allocate(WgContext& context, uint32_t w, uint32_t h);
   void free(WgContext& context, WgRenderStorage* renderTarget);
   void release(WgContext& context);
};


#endif
