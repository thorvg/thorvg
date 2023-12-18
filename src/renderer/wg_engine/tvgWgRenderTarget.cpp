/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

void WgRenderTarget::initialize(WGPUDevice device, WGPUQueue queue, WgPipelines& pipelines, uint32_t w, uint32_t h)
{
    release();
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
    mSampler = wgpuDeviceCreateSampler(device, &samplerDesc);
    assert(mSampler);
    // texture descriptor
    WGPUTextureDescriptor textureDescColor{};
    textureDescColor.nextInChain = nullptr;
    textureDescColor.label = "The target texture color";
    textureDescColor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    textureDescColor.dimension = WGPUTextureDimension_2D;
    textureDescColor.size = { w, h, 1 };
    textureDescColor.format = WGPUTextureFormat_RGBA8Unorm;
    textureDescColor.mipLevelCount = 1;
    textureDescColor.sampleCount = 1;
    textureDescColor.viewFormatCount = 0;
    textureDescColor.viewFormats = nullptr;
    mTextureColor = wgpuDeviceCreateTexture(device, &textureDescColor);
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
    mTextureViewColor = wgpuTextureCreateView(mTextureColor, &textureViewDescColor);
    assert(mTextureViewColor);
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
    mTextureStencil = wgpuDeviceCreateTexture(device, &textureDescStencil);
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
    mTextureViewStencil = wgpuTextureCreateView(mTextureStencil, &textureViewDescStencil);
    assert(mTextureViewStencil);
    // initialize window binding groups
    WgShaderTypeMat4x4f viewMat(w, h);
    mBindGroupCanvasWnd.initialize(device, queue, viewMat);
    WgShaderTypeMat4x4f modelMat;
    WgShaderTypeBlendSettings blendSettings(ColorSpace::ABGR8888);
    mBindGroupPaintWnd.initialize(device, queue, modelMat, blendSettings);
    // update pipeline geometry data
    WgVertexList wnd;
    wnd.appendRect(WgPoint(0.0f, 0.0f), WgPoint(w, 0.0f), WgPoint(0.0f, h), WgPoint(w, h));
    mGeometryDataWnd.update(device, queue, &wnd);
    mPipelines = &pipelines;
}

void WgRenderTarget::release()
{
    mBindGroupCanvasWnd.release();
    mBindGroupPaintWnd.release();
    mGeometryDataWnd.release();
    if (mTextureStencil) {
        wgpuTextureDestroy(mTextureStencil);
        wgpuTextureRelease(mTextureStencil);
        mTextureStencil = nullptr;
    }
    if (mTextureViewStencil) wgpuTextureViewRelease(mTextureViewStencil);
    mTextureViewStencil = nullptr;
    if (mTextureColor) {
        wgpuTextureDestroy(mTextureColor);
        wgpuTextureRelease(mTextureColor);
        mTextureColor = nullptr;
    }
    if (mTextureViewColor) wgpuTextureViewRelease(mTextureViewColor);
    mTextureViewColor = nullptr;
    if (mSampler) wgpuSamplerRelease(mSampler);
    mSampler = nullptr;
}

void WgRenderTarget::beginRenderPass(WGPUCommandEncoder commandEncoder)
{
    beginRenderPass(commandEncoder, mTextureViewColor);
}

void WgRenderTarget::beginRenderPass(WGPUCommandEncoder commandEncoder, WGPUTextureView colorAttachement)
{
    assert(!mRenderPassEncoder);
    // render pass depth stencil attachment
    WGPURenderPassDepthStencilAttachment depthStencilAttachment{};
    depthStencilAttachment.view = mTextureViewStencil;
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
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.clearValue = {0, 0, 0, 0};
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
    renderPassDesc.timestampWriteCount = 0;
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
    for (uint32_t i = 0; i < renderData->mGeometryDataShape.count; i++) {
        // draw to stencil (first pass)
        mPipelines->mPipelineFillShape.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->mBindGroupPaint);
        renderData->mGeometryDataShape[i]->draw(mRenderPassEncoder);
        // fill shape (second pass)
        WgRenderDataShapeSettings& settings = renderData->mRenderSettingsShape;
        if (settings.mFillType == WgRenderDataShapeFillType::Solid)
            mPipelines->mPipelineSolid.use(mRenderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupSolid);
        else if (settings.mFillType == WgRenderDataShapeFillType::Linear)
            mPipelines->mPipelineLinear.use(mRenderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupLinear);
        else if (settings.mFillType == WgRenderDataShapeFillType::Radial)
            mPipelines->mPipelineRadial.use(mRenderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupRadial);
        mGeometryDataWnd.draw(mRenderPassEncoder);
    }
}

void WgRenderTarget::renderStroke(WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    // draw stroke geometry
    if (renderData->mGeometryDataStroke.count > 0) {
        // draw strokes to stencil (first pass)
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 255);
        for (uint32_t i = 0; i < renderData->mGeometryDataStroke.count; i++) {
            mPipelines->mPipelineFillStroke.use(mRenderPassEncoder, mBindGroupCanvasWnd, renderData->mBindGroupPaint);
            renderData->mGeometryDataStroke[i]->draw(mRenderPassEncoder);
        }
        // fill shape (second pass)
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
        WgRenderDataShapeSettings& settings = renderData->mRenderSettingsStroke;
        if (settings.mFillType == WgRenderDataShapeFillType::Solid)
            mPipelines->mPipelineSolid.use(mRenderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupSolid);
        else if (settings.mFillType == WgRenderDataShapeFillType::Linear)
            mPipelines->mPipelineLinear.use(mRenderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupLinear);
        else if (settings.mFillType == WgRenderDataShapeFillType::Radial)
            mPipelines->mPipelineRadial.use(mRenderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupRadial);
        mGeometryDataWnd.draw(mRenderPassEncoder);
    }
}

void WgRenderTarget::renderImage(WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    if (renderData->mGeometryDataImage.count > 0) {
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
        for (uint32_t j = 0; j < renderData->mGeometryDataImage.count; j++) {
            mPipelines->mPipelineImage.use(
                mRenderPassEncoder,
                mBindGroupCanvasWnd,
                renderData->mBindGroupPaint,
                renderData->mBindGroupPicture);
            renderData->mGeometryDataImage[j]->drawImage(mRenderPassEncoder);
        }
    }
}
