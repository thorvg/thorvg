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
    WgRenderDataViewport* rdViewport;
};

class WgCompositor
{
private:
    // pipelines
    WgPipelines pipelines{};
    // stage buffers
    WgRenderDataStageBuffer stageBuffer{};
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
    WgRenderTarget* currentTarget{};
    // intermediate render targets
    WgRenderTarget targetTemp0;
    WgRenderTarget targetTemp1;
    WGPUBindGroup bindGroupStorageTemp{};
    // composition and blend geometries
    WgMeshData meshDataBlit;
    // render target dimensions
    uint32_t width{};
    uint32_t height{};
    
    // viewport utilities
    RenderRegion shrinkRenderRegion(RenderRegion& rect);
    void copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src);
    void copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src, const RenderRegion& region);

    // base meshes draw
    void drawMesh(WgContext& context, WgMeshData* meshData);
    void drawMeshFan(WgContext& context, WgMeshData* meshData);
    void drawMeshImage(WgContext& context, WgMeshData* meshData);

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
    void drawScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose);
    void blendScene(WgContext& context, WgRenderTarget* src, WgCompose* compose);

    // the renderer prioritizes clipping with the stroke over the shape's fill
    void markupClipPath(WgContext& context, WgRenderDataShape* renderData);
    void renderClipPath(WgContext& context, WgRenderDataPaint* paint);
    void clearClipPath(WgContext& context, WgRenderDataPaint* paint);

public:
    void initialize(WgContext& context, uint32_t width, uint32_t height);
    void initPools(WgContext& context);
    void release(WgContext& context);
    void releasePools(WgContext& context);
    void resize(WgContext& context, uint32_t width, uint32_t height);

    // render passes workflow
    void beginRenderPass(WGPUCommandEncoder encoder, WgRenderTarget* target, bool clear, WGPUColor clearColor = { 0.0, 0.0, 0.0, 0.0 });
    void endRenderPass();

    // request shapes for drawing (staging)
        // stage data
    void reset(WgContext& context);
    void flush(WgContext& context);

    void requestShape(WgRenderDataShape* renderData);
    void requestImage(WgRenderDataPicture* renderData);

    // render shapes, images and scenes
    void renderShape(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod);
    void renderImage(WgContext& context, WgRenderDataPicture* renderData, BlendMethod blendMethod);
    void renderScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose);
    void composeScene(WgContext& context, WgRenderTarget* src, WgRenderTarget* mask, WgCompose* compose);

    // blit render target to texture view (f.e. screen buffer)
    void blit(WgContext& context, WGPUCommandEncoder encoder, WgRenderTarget* src, WGPUTextureView dstView);

    // effects
    bool gaussianBlur(WgContext& context, WgRenderTarget* dst, const RenderEffectGaussianBlur* params, const WgCompose* compose);
    bool dropShadow(WgContext& context, WgRenderTarget* dst, const RenderEffectDropShadow* params, const WgCompose* compose);
    bool fillEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectFill* params, const WgCompose* compose);
    bool tintEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectTint* params, const WgCompose* compose);
    bool tritoneEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectTritone* params, const WgCompose* compose);
};

#endif // _TVG_WG_COMPOSITOR_H_
