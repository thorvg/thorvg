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

#include "tvgWgRenderTarget.h"

void WgRenderTarget::initialize(WgContext& context, uint32_t w, uint32_t h)
{
    release(context);
    // sampler descriptor
    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.nextInChain = nullptr;
    samplerDesc.label = "The target sampler";
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Nearest;
    samplerDesc.minFilter = WGPUFilterMode_Nearest;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.compare = WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;
    sampler = wgpuDeviceCreateSampler(context.device, &samplerDesc);
    assert(sampler);
    // texture descriptor
    WGPUTextureDescriptor textureDescColor{};
    textureDescColor.nextInChain = nullptr;
    textureDescColor.label = "The target texture color";
    textureDescColor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_StorageBinding;
    textureDescColor.dimension = WGPUTextureDimension_2D;
    textureDescColor.size = { w, h, 1 };
    textureDescColor.format = WGPUTextureFormat_RGBA8Unorm;
    textureDescColor.mipLevelCount = 1;
    textureDescColor.sampleCount = 1;
    textureDescColor.viewFormatCount = 0;
    textureDescColor.viewFormats = nullptr;
    mTextureColor = wgpuDeviceCreateTexture(context.device, &textureDescColor);
    assert(mTextureColor);
    // texture view descriptor
    WGPUTextureViewDescriptor textureViewDescColor{};
    textureViewDescColor.nextInChain = nullptr;
    textureViewDescColor.label = "The target texture view color";
    textureViewDescColor.format = WGPUTextureFormat_RGBA8Unorm;
    textureViewDescColor.dimension = WGPUTextureViewDimension_2D;
    textureViewDescColor.baseMipLevel = 0;
    textureViewDescColor.mipLevelCount = 1;
    textureViewDescColor.baseArrayLayer = 0;
    textureViewDescColor.arrayLayerCount = 1;
    textureViewDescColor.aspect = WGPUTextureAspect_All;
    textureViewColor = wgpuTextureCreateView(mTextureColor, &textureViewDescColor);
    assert(textureViewColor);
    // stencil texture
    WGPUTextureDescriptor textureDescStencil{};
    textureDescStencil.nextInChain = nullptr;
    textureDescStencil.label = "The target texture stencil";
    textureDescStencil.usage = WGPUTextureUsage_RenderAttachment;
    textureDescStencil.dimension = WGPUTextureDimension_2D;
    textureDescStencil.size = { w, h, 1 };
    textureDescStencil.format = WGPUTextureFormat_Stencil8;
    textureDescStencil.mipLevelCount = 1;
    textureDescStencil.sampleCount = 1;
    textureDescStencil.viewFormatCount = 0;
    textureDescStencil.viewFormats = nullptr;
    mTextureStencil = wgpuDeviceCreateTexture(context.device, &textureDescStencil);
    assert(mTextureStencil);
    // texture view descriptor
    WGPUTextureViewDescriptor textureViewDescStencil{};
    textureViewDescStencil.nextInChain = nullptr;
    textureViewDescStencil.label = "The target texture view stncil";
    textureViewDescStencil.format = WGPUTextureFormat_Stencil8;
    textureViewDescStencil.dimension = WGPUTextureViewDimension_2D;
    textureViewDescStencil.baseMipLevel = 0;
    textureViewDescStencil.mipLevelCount = 1;
    textureViewDescStencil.baseArrayLayer = 0;
    textureViewDescStencil.arrayLayerCount = 1;
    textureViewDescStencil.aspect = WGPUTextureAspect_All;
    textureViewStencil = wgpuTextureCreateView(mTextureStencil, &textureViewDescStencil);
    assert(textureViewStencil);
    // initialize bind group for blitting
    bindGroupTex.initialize(context.device, context.queue, textureViewColor);
    bindGroupStorageTex.initialize(context.device, context.queue, textureViewColor);
    bindGroupTexSampled.initialize(context.device, context.queue, sampler, textureViewColor);
    // initialize window binding groups
    WgShaderTypeMat4x4f viewMat(w, h);
    mBindGroupCanvasWnd.initialize(context.device, context.queue, viewMat);
    WgGeometryData geometryDataWnd;
    geometryDataWnd.appendBlitBox();
    mMeshDataCanvasWnd.update(context, &geometryDataWnd);
    mPipelines = context.pipelines;
}


void WgRenderTarget::release(WgContext& context)
{
    mMeshDataCanvasWnd.release(context);
    mBindGroupCanvasWnd.release();
    bindGroupTexSampled.release();
    bindGroupStorageTex.release();
    bindGroupTex.release();
    if (mTextureStencil) {
        wgpuTextureDestroy(mTextureStencil);
        wgpuTextureRelease(mTextureStencil);
        mTextureStencil = nullptr;
    }
    if (textureViewStencil) wgpuTextureViewRelease(textureViewStencil);
    textureViewStencil = nullptr;
    if (mTextureColor) {
        wgpuTextureDestroy(mTextureColor);
        wgpuTextureRelease(mTextureColor);
        mTextureColor = nullptr;
    }
    if (textureViewColor) wgpuTextureViewRelease(textureViewColor);
    textureViewColor = nullptr;
    if (sampler) wgpuSamplerRelease(sampler);
    sampler = nullptr;
}


void WgRenderTarget::beginRenderPass(WGPUCommandEncoder commandEncoder, bool clear)
{
    beginRenderPass(commandEncoder, textureViewColor, clear);
}


void WgRenderTarget::beginRenderPass(WGPUCommandEncoder commandEncoder, WGPUTextureView colorAttachement, bool clear)
{
    assert(!mRenderPassEncoder);
    // render pass depth stencil attachment
    WGPURenderPassDepthStencilAttachment depthStencilAttachment{};
    depthStencilAttachment.view = textureViewStencil;
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthReadOnly = false;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilClearValue = 0;
    depthStencilAttachment.stencilReadOnly = false;
    // render pass color attachment
    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view = colorAttachement;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = clear ? WGPULoadOp_Clear : WGPULoadOp_Load;
    colorAttachment.clearValue = { 0, 0, 0, 0 };
    colorAttachment.storeOp = WGPUStoreOp_Store;
    // render pass descriptor
    WGPURenderPassDescriptor renderPassDesc{};
    renderPassDesc.nextInChain = nullptr;
    renderPassDesc.label = "The render pass";
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
    //renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.occlusionQuerySet = nullptr;
    renderPassDesc.timestampWrites = nullptr;
    // begin render pass
    mRenderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
}


void WgRenderTarget::endRenderPass()
{
    assert(mRenderPassEncoder);
    wgpuRenderPassEncoderEnd(mRenderPassEncoder);
    wgpuRenderPassEncoderRelease(mRenderPassEncoder);
    mRenderPassEncoder = nullptr;
}


void WgRenderTarget::renderShape(WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    // draw shape geometry
    wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
    for (uint32_t i = 0; i < renderData->meshGroupShapes.meshes.count; i++) {
        // draw to stencil (first pass)
        mPipelines->fillShape.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint);
        renderData->meshGroupShapes.meshes[i]->draw(mRenderPassEncoder);
        // fill shape (second pass)
        WgRenderSettings& settings = renderData->renderSettingsShape;
        if (settings.fillType == WgRenderSettingsType::Solid)
            mPipelines->solid.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint, settings.bindGroupSolid);
        else if (settings.fillType == WgRenderSettingsType::Linear)
            mPipelines->linear.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint, settings.bindGroupLinear);
        else if (settings.fillType == WgRenderSettingsType::Radial)
            mPipelines->radial.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint, settings.bindGroupRadial);
        renderData->meshBBoxShapes.draw(mRenderPassEncoder);
    }
}


void WgRenderTarget::renderStroke(WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    // draw stroke geometry
    if (renderData->meshGroupStrokes.meshes.count > 0) {
        // draw strokes to stencil (first pass)
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 255);
        for (uint32_t i = 0; i < renderData->meshGroupStrokes.meshes.count; i++) {
            mPipelines->fillStroke.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint);
            renderData->meshGroupStrokes.meshes[i]->draw(mRenderPassEncoder);
        }
        // fill shape (second pass)
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
        WgRenderSettings& settings = renderData->renderSettingsStroke;
        if (settings.fillType == WgRenderSettingsType::Solid)
            mPipelines->solid.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint, settings.bindGroupSolid);
        else if (settings.fillType == WgRenderSettingsType::Linear)
            mPipelines->linear.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint, settings.bindGroupLinear);
        else if (settings.fillType == WgRenderSettingsType::Radial)
            mPipelines->radial.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->bindGroupPaint, settings.bindGroupRadial);
        renderData->meshBBoxStrokes.draw(mRenderPassEncoder);
    }
}


void WgRenderTarget::renderPicture(WgRenderDataPicture* renderData)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    if (renderData->meshData.bufferTexCoord) {
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
        mPipelines->image.use(
            mRenderPassEncoder,
            mBindGroupCanvasWnd,
            renderData->bindGroupPaint,
            renderData->bindGroupPicture);
        renderData->meshData.drawImage(mRenderPassEncoder);
    }
}


void WgRenderTarget::blit(WgContext& context, WgRenderTarget* renderTargetSrc, WgBindGroupOpacity* mBindGroupOpacity)
{
    assert(mRenderPassEncoder);
    mPipelines->blit.use(mRenderPassEncoder, renderTargetSrc->bindGroupTexSampled, *mBindGroupOpacity);
    mMeshDataCanvasWnd.drawImage(mRenderPassEncoder);
}


void WgRenderTarget::blitColor(WgContext& context, WgRenderTarget* renderTargetSrc)
{
    assert(mRenderPassEncoder);
    mPipelines->blitColor.use(mRenderPassEncoder, renderTargetSrc->bindGroupTexSampled);
    mMeshDataCanvasWnd.drawImage(mRenderPassEncoder);
}


void WgRenderTarget::compose(WgContext& context, WgRenderTarget* renderTargetSrc, WgRenderTarget* renderTargetMsk, CompositeMethod method)
{
    assert(mRenderPassEncoder);
    WgPipelineComposition* pipeline = mPipelines->getCompositionPipeline(method);
    assert(pipeline);
    pipeline->use(mRenderPassEncoder, renderTargetSrc->bindGroupTexSampled, renderTargetMsk->bindGroupTexSampled);
    mMeshDataCanvasWnd.drawImage(mRenderPassEncoder);
}

//*****************************************************************************
// render terget pool
//*****************************************************************************

WgRenderTarget* WgRenderTargetPool::allocate(WgContext& context, uint32_t w, uint32_t h)
{
   WgRenderTarget* renderTarget{};
   if (mPool.count > 0) {
      renderTarget = mPool.last();
      mPool.pop();
   } else {
      renderTarget = new WgRenderTarget;
      renderTarget->initialize(context, w, h);
      mList.push(renderTarget);
   }
   return renderTarget;
};


void WgRenderTargetPool::free(WgContext& context, WgRenderTarget* renderTarget) {
   mPool.push(renderTarget);
};


void WgRenderTargetPool::release(WgContext& context)
{
   for (uint32_t i = 0; i < mList.count; i++) {
      mList[i]->release(context);
      delete mList[i];
   }
   mList.clear();
   mPool.clear();
};
