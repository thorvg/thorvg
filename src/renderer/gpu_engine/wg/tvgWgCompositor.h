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
    CompositionFlag flags{};
    bool masked{}; // indicate if composition allocates more than one render target
};

class WgCompositor
{
private:
    // pipelines
    WgPipelines pipelines{};
    // stage buffers
    WgStageBufferGeometry stageBufferGeometry{};
    WgStageBufferSolidColor stageBufferSolidColor{};
    WgStageBufferUniform<WgShaderTypePaintSettings> stageBufferPaint;
    WgStageBufferUniform<WgShaderTypeMat4x4fBlock> stageBufferViewMat;
    // global stencil/depth buffer handles
    WGPUTexture texDepthStencil{};
    WGPUTextureView texViewDepthStencil{};
    WGPUTexture texDepthStencilMS{};
    WGPUTextureView texViewDepthStencilMS{};
    // global view matrix handles
    WGPUBuffer bufferViewMat{};
    WGPUBindGroup bindGroupViewMat{};
    uint32_t viewMatWidth{};
    uint32_t viewMatHeight{};
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
    RenderRegion shrinkRenderRegion(const RenderRegion& rect);
    void copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src);
    void copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src, const RenderRegion& region);

    // base meshes draw
    void drawMesh(WgContext& context, WgMeshData* meshData);
    void drawMeshSolid(WgContext& context, WgMeshData* meshData, uint32_t solidColorInd);
    void drawMeshImage(WgContext& context, WgMeshData* meshData);

    // shapes
    void drawShape(WgContext& context, WgRenderShape* rdata);
    void blendShape(WgContext& context, WgRenderShape* rdata, BlendMethod blendMethod);
    void clipShape(WgContext& context, WgRenderShape* rdata);

    // strokes
    void drawStrokes(WgContext& context, WgRenderShape* rdata);
    void blendStrokes(WgContext& context, WgRenderShape* rdata, BlendMethod blendMethod);
    void clipStrokes(WgContext& context, WgRenderShape* rdata);

    // images
    void drawImage(WgContext& context, WgRenderPicture* rdata);
    void blendImage(WgContext& context, WgRenderPicture* rdata, BlendMethod blendMethod);
    void clipImage(WgContext& context, WgRenderPicture* rdata);

    // scenes
    void drawScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose);
    void blendScene(WgContext& context, WgRenderTarget* src, WgCompose* compose);

    // the renderer prioritizes clipping with the stroke over the shape's fill
    void markupClipPath(WgContext& context, WgRenderShape* rdata);
    void renderClipPath(WgContext& context, WgRenderPaint* paint);
    void clearClipPath(WgContext& context, WgRenderPaint* paint);
    void updateViewMat(WgContext& context, uint32_t width, uint32_t height);
public:
    void initialize(WgContext& context, uint32_t width, uint32_t height);
    void initPools(WgContext& context);
    void release(WgContext& context);
    void releasePools(WgContext& context);
    void resize(WgContext& context, uint32_t width, uint32_t height);

    // render passes workflow
    void beginRenderPassMS(WGPUCommandEncoder encoder, WgRenderTarget* target, bool clear, WGPUColor clearColor = { 0.0, 0.0, 0.0, 0.0 });
    void beginRenderPass(WGPUCommandEncoder encoder, WgRenderTarget* target);
    void endRenderPass();

    // stage buffers operations
    void reset(WgContext& context);
    void flush(WgContext& context);

    // request shapes for drawing (staging)
    void requestShape(WgRenderShape* rdata);
    void requestImage(WgRenderPicture* rdata);
    void requestSolidBatch(const Array<WgRenderShape*>& renderShapes, WgSolidBatchRange& range);
    void requestStencilBatch(const Array<WgRenderShape*>& renderShapes, WgStencilBatchRange& range);

    // render shapes, images and scenes
    void renderShape(WgContext& context, WgRenderShape* rdata, BlendMethod blendMethod);
    void renderSolidBatch(const WgSolidBatchRange& range);
    void renderStencilBatch(const Array<WgRenderShape*>& renderShapes, const WgStencilBatchRange& range);
    void renderImage(WgContext& context, WgRenderPicture* rdata, BlendMethod blendMethod);
    void renderScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose);
    void composeScene(WgContext& context, WgRenderTarget* src, WgRenderTarget* mask, WgCompose* compose);

    // blit render target to texture view (f.e. screen buffer)
    void blit(WgContext& context, WGPUCommandEncoder encoder, WgRenderTarget* src, WGPUTextureView dstView, bool premultiplied);

    // effects
    bool gaussianBlur(WgContext& context, WgRenderTarget* dst, const RenderEffectGaussianBlur* params, const WgCompose* compose);
    bool dropShadow(WgContext& context, WgRenderTarget* dst, const RenderEffectDropShadow* params, const WgCompose* compose);
    bool fillEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectFill* params, const WgCompose* compose);
    bool tintEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectTint* params, const WgCompose* compose);
    bool tritoneEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectTritone* params, const WgCompose* compose);
};

#endif // _TVG_WG_COMPOSITOR_H_
