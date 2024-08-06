/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#ifndef _TVG_WG_COMPOSITOR_H_
#define _TVG_WG_COMPOSITOR_H_

#include "tvgWgRenderTarget.h"
#include "tvgWgRenderData.h"

struct WgCompose: public Compositor {
    BlendMethod blend;
};

class WgCompositor {
private:
    // pipelines (external handle, do not release)
    WgPipelines* pipelines{};
    // global stencil buffer handles
    WGPUTexture texStencil{};
    WGPUTextureView texViewStencil{};
    // global view matrix handles
    WGPUBuffer bufferViewMat{};
    WGPUBindGroup bindGroupViewMat{};
    // opacity value pool
    WGPUBuffer bufferOpacities[256]{};
    WGPUBindGroup bindGroupOpacities[256]{};
    // current render pass handles
    WGPURenderPassEncoder renderPassEncoder{};
    WGPUCommandEncoder commandEncoder{};
    WgRenderStorage* currentTarget{};
    // intermediate render storages
    WgRenderStorage storageInterm;
    WgRenderStorage storageClipPath;
    WgRenderStorage storageDstCopy;

    static WgPipelineBlendType blendMethodToBlendType(BlendMethod blendMethod);
public:
    // render target dimensions
    uint32_t width{};
    uint32_t height{};
public:
    void initialize(WgContext& context, uint32_t width, uint32_t height);
    void release(WgContext& context);

    void beginRenderPass(WGPUCommandEncoder encoder, WgRenderStorage* target, bool clear);
    void endRenderPass();

    void renderClipPath(WgContext& context, WgRenderDataPaint* renderData, WgRenderStorage* dst);

    void renderShape(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod);
    void renderImage(WgContext& context, WgRenderDataPicture* renderData, BlendMethod blendMethod);

    void blendShape(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod);
    void blendStrokes(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod);
    void blendImage(WgContext& context, WgRenderDataPicture* renderData, BlendMethod blendMethod);

    void composeShape(WgContext& context, WgRenderDataShape* renderData, WgRenderStorage* mask, CompositeMethod composeMethod);
    void composeStrokes(WgContext& context, WgRenderDataShape* renderData, WgRenderStorage* mask, CompositeMethod composeMethod);
    void composeImage(WgContext& context, WgRenderDataPicture* renderData, WgRenderStorage* mask, CompositeMethod composeMethod);

    void drawClipPath(WgContext& context, WgRenderDataShape* renderData);
    void drawShape(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType);
    void drawStrokes(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType);
    void drawImage(WgContext& context, WgRenderDataPicture* renderData, WgPipelineBlendType blendType);

    void mergeMasks(WGPUCommandEncoder encoder, WgRenderStorage* mask0, WgRenderStorage* mask1);
    void blend(WGPUCommandEncoder encoder, WgRenderStorage* src, WgRenderStorage* dst, uint8_t opacity, BlendMethod blendMethod, WgRenderRasterType rasterType);
    void compose(WGPUCommandEncoder encoder, WgRenderStorage* src, WgRenderStorage* mask, WgRenderStorage* dst, CompositeMethod composeMethod);
};

#endif // _TVG_WG_COMPOSITOR_H_
