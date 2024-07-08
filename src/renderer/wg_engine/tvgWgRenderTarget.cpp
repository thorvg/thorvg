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

//*****************************************************************************
// render storage
//*****************************************************************************

 void WgRenderStorage::initialize(WgContext& context, uint32_t w, uint32_t h, uint32_t samples, WGPUTextureFormat format)
 {
    release(context);
    // store target storage size
    width = w * samples;
    height = h * samples;
    workgroupsCountX = (width  + WG_COMPUTE_WORKGROUP_SIZE_X - 1) / WG_COMPUTE_WORKGROUP_SIZE_X; // workgroup size x == 8
    workgroupsCountY = (height + WG_COMPUTE_WORKGROUP_SIZE_Y - 1) / WG_COMPUTE_WORKGROUP_SIZE_Y; // workgroup size y == 8
    // create color and stencil textures
    texColor = context.createTexture2d(
        WGPUTextureUsage_CopySrc |
        WGPUTextureUsage_CopyDst |
        WGPUTextureUsage_TextureBinding |
        WGPUTextureUsage_StorageBinding |
        WGPUTextureUsage_RenderAttachment,
        format, width, height, "The target texture color");
    texStencil = context.createTexture2d(
        WGPUTextureUsage_RenderAttachment,
        WGPUTextureFormat_Stencil8,
        width, height, "The target texture stencil");
    assert(texColor);
    assert(texStencil);
    texViewColor = context.createTextureView2d(texColor, "The target texture view color");
    texViewStencil = context.createTextureView2d(texStencil, "The target texture view stencil");
    assert(texViewColor);
    assert(texViewStencil);
    // initialize bind group for blitting
    if (format == WGPUTextureFormat_RGBA8Unorm)
        bindGroupTexStorageRgba.initialize(context.device, context.queue, texViewColor);
    if (format == WGPUTextureFormat_BGRA8Unorm)
        bindGroupTexStorageBgra.initialize(context.device, context.queue, texViewColor);
    // initialize window binding groups
    WgShaderTypeMat4x4f viewMat(w, h);
    mBindGroupCanvas.initialize(context.device, context.queue, viewMat);
    mPipelines = context.pipelines;
 }


void WgRenderStorage::release(WgContext& context)
{
    mRenderPassEncoder = nullptr;
    mBindGroupCanvas.release();
    bindGroupTexStorageRgba.release();
    context.releaseTextureView(texViewStencil);
    context.releaseTextureView(texViewColor);
    context.releaseTexture(texStencil);
    context.releaseTexture(texColor);
    workgroupsCountX = 0;
    workgroupsCountY = 0;
    height = 0;
    width = 0;
}


void WgRenderStorage::renderShape(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    // draw strokes
    if (renderData->strokeFirst) {
        drawStroke(context, renderData, blendType);
        drawShape(context, renderData, blendType);
    } else {
        drawShape(context, renderData, blendType);
        drawStroke(context, renderData, blendType);
    }
}


void WgRenderStorage::renderPicture(WgContext& context, WgRenderDataPicture* renderData, WgPipelineBlendType blendType)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    uint8_t blend = (uint8_t)blendType;
    wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
    mPipelines->image[blend].use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, renderData->bindGroupPicture);
    renderData->meshData.drawImage(context, mRenderPassEncoder);
}


void WgRenderStorage::drawShape(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    assert(renderData->meshGroupShapes.meshes.count == renderData->meshGroupShapesBBox.meshes.count);
    if (renderData->renderSettingsShape.skip) return;
    // draw shape geometry
    wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
    // setup fill rule
    if (renderData->fillRule == FillRule::Winding)
        mPipelines->fillShapeWinding.use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint);
    else
        mPipelines->fillShapeEvenOdd.use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint);
    // draw to stencil (first pass)
    for (uint32_t i = 0; i < renderData->meshGroupShapes.meshes.count; i++)
        renderData->meshGroupShapes.meshes[i]->drawFan(context, mRenderPassEncoder);
    // fill shape geometry (second pass)
    uint8_t blend = (uint8_t)blendType;
    WgRenderSettings& settings = renderData->renderSettingsShape;
    if (settings.fillType == WgRenderSettingsType::Solid)
        mPipelines->solid[blend].use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupSolid);
    else if (settings.fillType == WgRenderSettingsType::Linear)
        mPipelines->linear[blend].use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupLinear);
    else if (settings.fillType == WgRenderSettingsType::Radial)
        mPipelines->radial[blend].use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupRadial);
    renderData->meshDataBBox.drawFan(context, mRenderPassEncoder);
}


void WgRenderStorage::drawStroke(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType)
{
    assert(renderData);
    assert(mRenderPassEncoder);
    assert(renderData->meshGroupStrokes.meshes.count == renderData->meshGroupStrokesBBox.meshes.count);
    if (renderData->renderSettingsStroke.skip) return;
    // draw stroke geometry
    uint8_t blend = (uint8_t)blendType;
    // draw strokes to stencil (first pass)
    for (uint32_t i = 0; i < renderData->meshGroupStrokes.meshes.count; i++) {
        // draw to stencil (first pass)
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 255);
        mPipelines->fillStroke.use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint);
        renderData->meshGroupStrokes.meshes[i]->draw(context, mRenderPassEncoder);
        // fill shape (second pass)
        wgpuRenderPassEncoderSetStencilReference(mRenderPassEncoder, 0);
        WgRenderSettings& settings = renderData->renderSettingsStroke;
        if (settings.fillType == WgRenderSettingsType::Solid)
            mPipelines->solid[blend].use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupSolid);
        else if (settings.fillType == WgRenderSettingsType::Linear)
            mPipelines->linear[blend].use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupLinear);
        else if (settings.fillType == WgRenderSettingsType::Radial)
            mPipelines->radial[blend].use(mRenderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupRadial);
        renderData->meshGroupStrokesBBox.meshes[i]->drawFan(context, mRenderPassEncoder);
    }
}


void WgRenderStorage::clear(WGPUCommandEncoder commandEncoder)
{
    assert(commandEncoder);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    mPipelines->computeClear.use(computePassEncoder, bindGroupTexStorageRgba);
    dispatchWorkgroups(computePassEncoder);
    endComputePass(computePassEncoder);
}


void WgRenderStorage::blend(WGPUCommandEncoder commandEncoder, WgRenderStorage* targetSrc, WgPipelineBlend* pipeline, WgBindGroupBlendMethod* blendMethod, WgBindGroupOpacity* opacity) {
    assert(commandEncoder);
    assert(targetSrc);
    assert(pipeline);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    pipeline->use(computePassEncoder, targetSrc->bindGroupTexStorageRgba, bindGroupTexStorageRgba, *blendMethod, *opacity);
    dispatchWorkgroups(computePassEncoder);
    endComputePass(computePassEncoder);
}


void WgRenderStorage::composeBlend(
    WgContext& context,
    WGPUCommandEncoder commandEncoder,
    WgRenderStorage* texSrc,
    WgRenderStorage* texMsk,
    WgBindGroupCompositeMethod* composeMethod,
    WgBindGroupBlendMethod* blendMethod,
    WgBindGroupOpacity* opacity)
{
    assert(commandEncoder);
    assert(texSrc);
    assert(texMsk);
    WgBindGroupTexComposeBlend composeBlend;
    composeBlend.initialize(context.device, context.queue, texSrc->texViewColor, texMsk->texViewColor, texViewColor);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    mPipelines->computeComposeBlend.use(computePassEncoder, composeBlend, *composeMethod, *blendMethod, *opacity);
    dispatchWorkgroups(computePassEncoder);
    endComputePass(computePassEncoder);
    composeBlend.release();
}


void WgRenderStorage::antialias(WGPUCommandEncoder commandEncoder, WgRenderStorage* targetSrc)
{
    assert(commandEncoder);
    assert(targetSrc);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    mPipelines->computeAntiAliasing.use(computePassEncoder, targetSrc->bindGroupTexStorageRgba, bindGroupTexStorageBgra);
    dispatchWorkgroups(computePassEncoder);
    endComputePass(computePassEncoder);
}


void WgRenderStorage::dispatchWorkgroups(WGPUComputePassEncoder computePassEncoder)
{
    assert(computePassEncoder);
    wgpuComputePassEncoderDispatchWorkgroups(computePassEncoder, workgroupsCountX, workgroupsCountY, 1);
}


void WgRenderStorage::beginRenderPass(WGPUCommandEncoder commandEncoder, bool clear)
{
    assert(commandEncoder);
    // render pass depth stencil attachment
    WGPURenderPassDepthStencilAttachment depthStencilAttachment{};
    depthStencilAttachment.view = texViewStencil;
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Load;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Discard;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthReadOnly = false;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Discard;
    depthStencilAttachment.stencilClearValue = 0;
    depthStencilAttachment.stencilReadOnly = false;
    // render pass color attachment
    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view = texViewColor;
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


void WgRenderStorage::endRenderPass()
{
    assert(mRenderPassEncoder);
    wgpuRenderPassEncoderEnd(mRenderPassEncoder);
    wgpuRenderPassEncoderRelease(mRenderPassEncoder);
    mRenderPassEncoder = nullptr;
}


WGPUComputePassEncoder WgRenderStorage::beginComputePass(WGPUCommandEncoder commandEncoder)
{
    assert(commandEncoder);
    WGPUComputePassDescriptor computePassDesc{};
    computePassDesc.nextInChain = nullptr;
    computePassDesc.label = "The compute pass composition";
    computePassDesc.timestampWrites = nullptr;
    return wgpuCommandEncoderBeginComputePass(commandEncoder, &computePassDesc);
};


void WgRenderStorage::endComputePass(WGPUComputePassEncoder computePassEncoder)
{
    assert(computePassEncoder);
    wgpuComputePassEncoderEnd(computePassEncoder);
}

//*****************************************************************************
// render storage pool
//*****************************************************************************

WgRenderStorage* WgRenderStoragePool::allocate(WgContext& context, uint32_t w, uint32_t h, uint32_t samples)
{
   WgRenderStorage* renderStorage{};
   if (mPool.count > 0) {
      renderStorage = mPool.last();
      mPool.pop();
   } else {
      renderStorage = new WgRenderStorage;
      renderStorage->initialize(context, w, h, samples);
      mList.push(renderStorage);
   }
   return renderStorage;
};


void WgRenderStoragePool::free(WgContext& context, WgRenderStorage* renderStorage) {
   mPool.push(renderStorage);
};


void WgRenderStoragePool::release(WgContext& context)
{
   for (uint32_t i = 0; i < mList.count; i++) {
      mList[i]->release(context);
      delete mList[i];
   }
   mList.clear();
   mPool.clear();
};
