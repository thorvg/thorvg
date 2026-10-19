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

struct WgRenderTargetState
{
    WgRenderTarget* target{};
    RenderRegion bounds{};       // Canvas coordinates.
    uint32_t transformOffset{};  // Byte offset in the geometry buffer.
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
    struct DepthStencilAttachment
    {
        uint32_t width, height, sampleCount;
        WGPUTexture texture;
        WGPUTextureView view;
    };
    Array<DepthStencilAttachment> depthStencilAttachments;
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
    const WgRenderTargetState* currentTargetState{};
    // intermediate render targets
    WgRenderTarget targetTemp0;
    WgRenderTarget targetTemp1;
    WGPUBindGroup bindGroupStorageTemp{};
    // composition and blend geometries
    WgMesh meshDataBlit;
    // render target dimensions
    uint32_t width{};
    uint32_t height{};
    
    // viewport utilities
    void setScissorRegion(const RenderRegion& region);
    WGPUTextureView getDepthStencilView(WgContext& context, uint32_t width, uint32_t height, uint32_t sampleCount);
    void requestQuad(WgMesh& mesh, const WgRenderTargetState& src, const WgRenderTargetState& dst, const WgRenderTargetState* mask = nullptr);
    void copyEffectTexture(WgContext& context, WgRenderTarget* dst, const WgRenderTargetState& src, bool clear = false);
    void copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src);

    // base meshes draw
    void drawMesh(WgContext& context, WgMesh* meshData);
    void drawMeshSolid(WgContext& context, WgMesh* meshData, uint32_t solidColorInd);
    void drawMeshImage(WgContext& context, WgMesh* meshData);

    // shapes
    void drawShape(WgContext& context, WgShape* rdata);
    void blendShape(WgContext& context, WgShape* rdata, BlendMethod blendMethod);
    void clipShape(WgContext& context, WgShape* rdata);

    // strokes
    void drawStrokes(WgContext& context, WgShape* rdata);
    void blendStrokes(WgContext& context, WgShape* rdata, BlendMethod blendMethod);

    // images
    void drawImage(WgContext& context, WgImage* rdata);
    void blendImage(WgContext& context, WgImage* rdata, BlendMethod blendMethod);
    void clipImage(WgContext& context, WgImage* rdata);

    // scenes
    void drawScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose, WgMesh& mesh);
    void blendScene(WgContext& context, WgRenderTarget* src, WgCompose* compose, WgMesh& mesh);

    // the renderer prioritizes clipping with the stroke over the shape's fill
    void markupClipPath(WgContext& context, WgShape* rdata);
    void renderClipPath(WgContext& context, WgPaint* paint);
    void clearClipPath(WgContext& context, WgPaint* paint);
    void updateViewMat(WgContext& context, uint32_t width, uint32_t height);
public:
    void initialize(WgContext& context, uint32_t width, uint32_t height);
    void initPools(WgContext& context);
    void release(WgContext& context);
    void releasePools(WgContext& context);
    void resize(WgContext& context, uint32_t width, uint32_t height);

    // render passes workflow
    void beginRenderPassMS(WgContext& context, WGPUCommandEncoder encoder, const WgRenderTargetState& targetState, bool clearBuffer);
    void beginRenderPass(WgContext& context, WGPUCommandEncoder encoder, WgRenderTarget* target, bool clearBuffer = false);
    void endRenderPass();

    // stage buffers operations
    void reset(WgContext& context);
    void flush(WgContext& context);

    // request shapes for drawing (staging)
    void requestShape(WgShape* shape);
    void requestImage(WgImage* image);
    void requestSolidBatch(const Array<WgShape*>& renderShapes, WgSolidBatchRange& range);
    void requestImageBatch(const Array<WgImage*>& renderImages, WgImageBatchRange& range);
    void requestStencilBatch(const Array<WgShape*>& renderShapes, WgStencilBatchRange& range);
    void requestScene(WgRenderTargetState& targetState, WgMesh& mesh, WgMesh& effectMesh, const WgRenderTargetState* dst, const WgRenderTargetState* mask, bool effects);

    // render shapes, images and scenes
    void renderShape(WgContext& context, WgShape* rdata, BlendMethod blendMethod);
    void renderSolidBatch(const WgSolidBatchRange& range);
    void renderImageBatch(WgImage* image, const WgImageBatchRange& range);
    void renderStencilBatch(const Array<WgShape*>& renderShapes, const WgStencilBatchRange& range);
    void renderImage(WgContext& context, WgImage* rdata, BlendMethod blendMethod);
    void renderScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose, WgMesh& mesh);
    void composeScene(WgContext& context, WgRenderTarget* src, WgRenderTarget* mask, WgCompose* compose, WgMesh& mesh);

    // blit render target to texture view (f.e. screen buffer)
    void blit(WgContext& context, WGPUCommandEncoder encoder, WgRenderTarget* src, WGPUTextureView dstView, bool premultiplied, bool clearBuffer);

    // effects
    bool gaussianBlur(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectGaussianBlur* blur, WgMesh& mesh);
    bool dropShadow(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectDropShadow* shadow, WgMesh& mesh);
    bool fillEffect(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectFill* fill, WgMesh& mesh);
    bool tintEffect(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectTint* tint, WgMesh& mesh);
    bool tritoneEffect(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectTritone* tritone, WgMesh& mesh);
    bool motionBlur(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectMotionBlur* params, WgMesh& mesh);
};

#endif // _TVG_WG_COMPOSITOR_H_
