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

class WgRenderTarget {
private:
    // command buffer
    WGPURenderPassEncoder mRenderPassEncoder{};
    // fill and blit data
    WgBindGroupCanvas mBindGroupCanvasWnd;
    // composition handles
    WgMeshData mMeshDataCanvasWnd;
    // gpu buffers
    WGPUTexture mTextureColor{};
    WGPUTexture mTextureStencil{};
    WgPipelines* mPipelines{}; // external handle
public:
    WGPUSampler sampler{};
    WGPUTextureView textureViewColor{};
    WGPUTextureView textureViewStencil{};
    WgBindGroupTexture bindGroupTex;
    WgBindGroupStorageTexture bindGroupStorageTex;
    WgBindGroupTextureSampled bindGroupTexSampled;
public:
    void initialize(WgContext& context, uint32_t w, uint32_t h);
    void release(WgContext& context);

    void beginRenderPass(WGPUCommandEncoder commandEncoder, WGPUTextureView colorAttachement, bool clear);
    void beginRenderPass(WGPUCommandEncoder commandEncoder, bool clear);
    void endRenderPass();

    void renderShape(WgRenderDataShape* renderData);
    void renderStroke(WgRenderDataShape* renderData);
    void renderPicture(WgRenderDataPicture* renderData);

    void blit(WgContext& context, WgRenderTarget* renderTargetSrc, WgBindGroupOpacity* bindGroupOpacity);
    void blitColor(WgContext& context, WgRenderTarget* renderTargetSrc);
    void compose(WgContext& context, WgRenderTarget* renderTargetSrc, WgRenderTarget* renderTargetMsk, CompositeMethod method);
};


class WgRenderTargetPool {
private:
   Array<WgRenderTarget*> mList;
   Array<WgRenderTarget*> mPool;
public:
   WgRenderTarget* allocate(WgContext& context, uint32_t w, uint32_t h);
   void free(WgContext& context, WgRenderTarget* renderTarget);
   void release(WgContext& context);
};


#endif
