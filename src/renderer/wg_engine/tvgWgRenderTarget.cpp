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
// render target
//*****************************************************************************

void WgRenderTarget::initialize(WgContext& context, uint32_t w, uint32_t h)
{
    release(context);
    // create color and stencil textures
    texColor = context.createTexture2d(
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_StorageBinding,
        WGPUTextureFormat_RGBA8Unorm, 
        w, h, "The target texture color");
    texStencil = context.createTexture2d(
        WGPUTextureUsage_RenderAttachment,
        WGPUTextureFormat_Stencil8, 
        w, h, "The target texture stencil");
    assert(texColor);
    assert(texStencil);
    texViewColor = context.createTextureView2d(texColor, "The target texture view color");
    texViewStencil = context.createTextureView2d(texStencil, "The target texture view stencil");
    assert(texViewColor);
    assert(texViewStencil);
    // initialize bind group for blitting
    bindGroupTexStorage.initialize(context.device, context.queue, texViewColor);
    // initialize window binding groups
    WgShaderTypeMat4x4f viewMat(w, h);
    mBindGroupCanvas.initialize(context.device, context.queue, viewMat);
    mPipelines = context.pipelines;
}


void WgRenderTarget::release(WgContext& context)
{
    mBindGroupCanvas.release();
    bindGroupTexStorage.release();
    context.releaseTextureView(texViewStencil);
    context.releaseTextureView(texViewColor);
    context.releaseTexture(texStencil);
    context.releaseTexture(texColor);
}


void WgRenderTarget::renderShape(WGPUCommandEncoder commandEncoder, WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(commandEncoder);
    WGPURenderPassEncoder renderPassEncoder = beginRenderPass(commandEncoder);
    drawShape(renderPassEncoder, renderData);
    drawStroke(renderPassEncoder, renderData);
    endRenderPass(renderPassEncoder);
}


void WgRenderTarget::renderPicture(WGPUCommandEncoder commandEncoder, WgRenderDataPicture* renderData)
{
    assert(renderData);
    assert(commandEncoder);
    WGPURenderPassEncoder renderPassEncoder = beginRenderPass(commandEncoder);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    mPipelines->image.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, renderData->bindGroupPicture);
    renderData->meshData.drawImage(renderPassEncoder);
    endRenderPass(renderPassEncoder);
}


void WgRenderTarget::drawShape(WGPURenderPassEncoder renderPassEncoder, WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(renderPassEncoder);
    // draw shape geometry
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    for (uint32_t i = 0; i < renderData->meshGroupShapes.meshes.count; i++) {
        // draw to stencil (first pass)
        mPipelines->fillShape.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint);
        renderData->meshGroupShapes.meshes[i]->draw(renderPassEncoder);
        // fill shape (second pass)
        WgRenderSettings& settings = renderData->renderSettingsShape;
        if (settings.fillType == WgRenderSettingsType::Solid)
            mPipelines->solid.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupSolid);
        else if (settings.fillType == WgRenderSettingsType::Linear)
            mPipelines->linear.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupLinear);
        else if (settings.fillType == WgRenderSettingsType::Radial)
            mPipelines->radial.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupRadial);
        renderData->meshBBoxShapes.draw(renderPassEncoder);
    }
}


void WgRenderTarget::drawStroke(WGPURenderPassEncoder renderPassEncoder, WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(renderPassEncoder);
    // draw stroke geometry
    if (renderData->meshGroupStrokes.meshes.count > 0) {
        // draw strokes to stencil (first pass)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
        for (uint32_t i = 0; i < renderData->meshGroupStrokes.meshes.count; i++) {
            mPipelines->fillStroke.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint);
            renderData->meshGroupStrokes.meshes[i]->draw(renderPassEncoder);
        }
        // fill shape (second pass)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        WgRenderSettings& settings = renderData->renderSettingsStroke;
        if (settings.fillType == WgRenderSettingsType::Solid)
            mPipelines->solid.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupSolid);
        else if (settings.fillType == WgRenderSettingsType::Linear)
            mPipelines->linear.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupLinear);
        else if (settings.fillType == WgRenderSettingsType::Radial)
            mPipelines->radial.use(renderPassEncoder, mBindGroupCanvas, renderData->bindGroupPaint, settings.bindGroupRadial);
        renderData->meshBBoxStrokes.draw(renderPassEncoder);
    }
}


WGPURenderPassEncoder WgRenderTarget::beginRenderPass(WGPUCommandEncoder commandEncoder)
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
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilClearValue = 0;
    depthStencilAttachment.stencilReadOnly = false;
    // render pass color attachment
    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view = texViewColor;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Clear;
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
    return wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
}


void WgRenderTarget::endRenderPass(WGPURenderPassEncoder renderPassEncoder)
{
    assert(renderPassEncoder);
    wgpuRenderPassEncoderEnd(renderPassEncoder);
    wgpuRenderPassEncoderRelease(renderPassEncoder);
}

//*****************************************************************************
// render storage
//*****************************************************************************

 void WgRenderStorage::initialize(WgContext& context, uint32_t w, uint32_t h)
 {
    release(context);
    // store target storage size
    width = w;
    height = h;
    workgroupsCountX = (width  + WG_COMPUTE_WORKGROUP_SIZE_X - 1) / WG_COMPUTE_WORKGROUP_SIZE_X; // workgroup size x == 8
    workgroupsCountY = (height + WG_COMPUTE_WORKGROUP_SIZE_Y - 1) / WG_COMPUTE_WORKGROUP_SIZE_Y; // workgroup size y == 8
    // create color and stencil textures
    texStorage = context.createTexture2d(
        WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc,
        WGPUTextureFormat_RGBA8Unorm,
        w, h, "The target texture storage color");
    assert(texStorage);
    texViewStorage = context.createTextureView2d(texStorage, "The target texture storage view color");
    assert(texViewStorage);
    // initialize bind group for blitting
    bindGroupTexStorage.initialize(context.device, context.queue, texViewStorage);
    mPipelines = context.pipelines;
 }


void WgRenderStorage::release(WgContext& context)
{
    bindGroupTexStorage.release();
    context.releaseTextureView(texViewStorage);
    context.releaseTexture(texStorage);
    workgroupsCountX = 0;
    workgroupsCountY = 0;
    height = 0;
    width = 0;
}

void WgRenderStorage::clear(WGPUCommandEncoder commandEncoder)
{
    assert(commandEncoder);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    mPipelines->computeClear.use(computePassEncoder, bindGroupTexStorage);
    dispatchWorkgroups(computePassEncoder);
    endRenderPass(computePassEncoder);
}


void WgRenderStorage::blend(WGPUCommandEncoder commandEncoder, WgRenderTarget* targetSrc, WgBindGroupBlendMethod* blendMethod)
{
    assert(commandEncoder);
    assert(targetSrc);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    mPipelines->computeBlend.use(computePassEncoder, targetSrc->bindGroupTexStorage, bindGroupTexStorage, *blendMethod);
    dispatchWorkgroups(computePassEncoder);
    endRenderPass(computePassEncoder);
};

void WgRenderStorage::blend(WGPUCommandEncoder commandEncoder, WgRenderStorage* targetSrc, WgBindGroupBlendMethod* blendMethod)
{
    assert(commandEncoder);
    assert(targetSrc);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    mPipelines->computeBlend.use(computePassEncoder, targetSrc->bindGroupTexStorage, bindGroupTexStorage, *blendMethod);
    dispatchWorkgroups(computePassEncoder);
    endRenderPass(computePassEncoder);
};


void WgRenderStorage::compose(WGPUCommandEncoder commandEncoder, WgRenderStorage* targetMsk, WgBindGroupCompositeMethod* composeMethod, WgBindGroupOpacity* opacity)
{
    assert(commandEncoder);
    assert(targetMsk);
    WGPUComputePassEncoder computePassEncoder = beginComputePass(commandEncoder);
    mPipelines->computeCompose.use(computePassEncoder, bindGroupTexStorage, targetMsk->bindGroupTexStorage, *composeMethod, *opacity);
    dispatchWorkgroups(computePassEncoder);
    endRenderPass(computePassEncoder);
};


void WgRenderStorage::dispatchWorkgroups(WGPUComputePassEncoder computePassEncoder)
{
    assert(computePassEncoder);
    wgpuComputePassEncoderDispatchWorkgroups(computePassEncoder, workgroupsCountX, workgroupsCountY, 1);
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


void WgRenderStorage::endRenderPass(WGPUComputePassEncoder computePassEncoder)
{
    assert(computePassEncoder);
    wgpuComputePassEncoderEnd(computePassEncoder);
}

//*****************************************************************************
// render storage pool
//*****************************************************************************

WgRenderStorage* WgRenderStoragePool::allocate(WgContext& context, uint32_t w, uint32_t h)
{
   WgRenderStorage* renderStorage{};
   if (mPool.count > 0) {
      renderStorage = mPool.last();
      mPool.pop();
   } else {
      renderStorage = new WgRenderStorage;
      renderStorage->initialize(context, w, h);
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
