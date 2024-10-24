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

struct WgCompose: RenderCompositor
{
    BlendMethod blend{};
    RenderRegion aabb{};
};

class WgCompositor
{
private:
    // pipelines (external handle, do not release)
    WgPipelines* pipelines{};
    // global stencil/depth buffer handles
    WGPUTexture texDepthStencil{};
    WGPUTextureView texViewDepthStencil{};
    WGPUTexture texDepthStencilMS{};
    WGPUTextureView texViewDepthStencilMS{};
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
    WgRenderStorage storageDstCopy;
    // composition and blend geometries
    WgMeshData meshData;
    // render target dimensions
    uint32_t width{};
    uint32_t height{};
    
    // viewport utilities
    RenderRegion shrinkRenderRegion(RenderRegion& rect);
public:
    void initialize(WgContext& context, uint32_t width, uint32_t height);
    void release(WgContext& context);

    // render passes workflow
    void beginRenderPass(WGPUCommandEncoder encoder, WgRenderStorage* target, bool clear);
    void endRenderPass();

    // render shapes, images and scenes
    void renderShape(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod);
    void renderImage(WgContext& context, WgRenderDataPicture* renderData, BlendMethod blendMethod);
    void renderScene(WgContext& context, WgRenderStorage* scene, WgCompose* compose);
    void composeScene(WgContext& context, WgRenderStorage* src, WgRenderStorage* mask, WgCompose* compose);

    // blit render storage to texture view (f.e. screen buffer)
    void blit(WgContext& context, WGPUCommandEncoder encoder, WgRenderStorage* src, WGPUTextureView dstView);
private:
    // shapes
    void drawShape(WgContext& context, WgRenderDataShape* renderData);
    void blendShape(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod);
    void clipShape(WgContext& context, WgRenderDataShape* renderData);

    // strokes
    void drawStrokes(WgContext& context, WgRenderDataShape* renderData);
    void blendStrokes(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod);
    void clipStrokes(WgContext& context, WgRenderDataShape* renderData);

    // images
    void drawImage(WgContext& context, WgRenderDataPicture* renderData);
    void blendImage(WgContext& context, WgRenderDataPicture* renderData, BlendMethod blendMethod);
    void clipImage(WgContext& context, WgRenderDataPicture* renderData);

    // scenes
    void drawScene(WgContext& context, WgRenderStorage* scene, WgCompose* compose);
    void blendScene(WgContext& context, WgRenderStorage* src, WgCompose* compose);
private:
    void renderClipPath(WgContext& context, WgRenderDataPaint* paint);
    void clearClipPath(WgContext& context, WgRenderDataPaint* paint);
};

#endif // _TVG_WG_COMPOSITOR_H_
