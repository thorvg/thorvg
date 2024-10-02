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

#include "tvgWgCompositor.h"
#include "tvgWgShaderTypes.h"

void WgCompositor::initialize(WgContext& context, uint32_t width, uint32_t height)
{
    // pipelines (external handle, do not release)
    pipelines = context.pipelines;
    // store render target dimensions
    this->width = width;
    this->height = height;
    // allocate global stencil buffer handles
    texStencil = context.createTexStencil(width, height, WGPUTextureFormat_Stencil8);
    texViewStencil = context.createTextureView(texStencil);
    // allocate global view matrix handles
    WgShaderTypeMat4x4f viewMat(width, height);
    context.allocateBufferUniform(bufferViewMat, &viewMat, sizeof(viewMat));
    bindGroupViewMat = pipelines->layouts.createBindGroupBuffer1Un(bufferViewMat);
    // initialize opacity pool
    for (uint32_t i = 0; i < 256; i++) {
        float opacity = i / 255.0f;
        context.allocateBufferUniform(bufferOpacities[i], &opacity, sizeof(float));
        bindGroupOpacities[i] = pipelines->layouts.createBindGroupBuffer1Un(bufferOpacities[i]);
    }
    // initialize intermediate render storages
    storageClipPath.initialize(context, width, height);
    storageInterm.initialize(context, width, height);
    storageDstCopy.initialize(context, width, height);
    // composition and blend geometries
    WgVertexBufferInd vertexBuffer;
    vertexBuffer.appendBlitBox();
    meshData.update(context, vertexBuffer);
}


void WgCompositor::release(WgContext& context)
{
    // composition and blend geometries
    meshData.release(context);
    // release intermediate render storages
    storageInterm.release(context);
    storageDstCopy.release(context);
    storageClipPath.release(context);
    // release opacity pool
    for (uint32_t i = 0; i < 256; i++) {
        context.pipelines->layouts.releaseBindGroup(bindGroupOpacities[i]);
        context.releaseBuffer(bufferOpacities[i]);
    }
    // release global view matrix handles
    context.pipelines->layouts.releaseBindGroup(bindGroupViewMat);
    context.releaseBuffer(bufferViewMat);
    // release global stencil buffer handles
    context.releaseTextureView(texViewStencil);
    context.releaseTexture(texStencil);
    height = 0;
    width = 0;
    pipelines = nullptr;
}


WgPipelineBlendType WgCompositor::blendMethodToBlendType(BlendMethod blendMethod)
{
    if (blendMethod == BlendMethod::Normal) return WgPipelineBlendType::Normal;
    return WgPipelineBlendType::Custom;
}


RenderRegion WgCompositor::shrinkRenderRegion(RenderRegion& rect)
{
    // cut viewport to screen dimensions
    int32_t xmin = std::max(0, std::min((int32_t)width, rect.x));
    int32_t ymin = std::max(0, std::min((int32_t)height, rect.y));
    int32_t xmax = std::max(0, std::min((int32_t)width, rect.x + rect.w));
    int32_t ymax = std::max(0, std::min((int32_t)height, rect.y + rect.h));
    return { xmin, ymin, xmax - xmin, ymax - ymin };
}


void WgCompositor::beginRenderPass(WGPUCommandEncoder commandEncoder, WgRenderStorage* target, bool clear)
{
    assert(commandEncoder);
    assert(target);
    this->currentTarget = target;
    this->commandEncoder = commandEncoder;
    WGPURenderPassDepthStencilAttachment depthStencilAttachment{ .view = texViewStencil, .stencilLoadOp = WGPULoadOp_Clear, .stencilStoreOp = WGPUStoreOp_Discard };
    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view = target->texView,
    colorAttachment.loadOp = clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
    colorAttachment.storeOp = WGPUStoreOp_Store;
    #ifdef __EMSCRIPTEN__
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    #endif
    WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
    assert(renderPassEncoder);
}


void WgCompositor::endRenderPass()
{
    assert(renderPassEncoder);
    wgpuRenderPassEncoderEnd(renderPassEncoder);
    wgpuRenderPassEncoderRelease(renderPassEncoder);
    this->renderPassEncoder = nullptr;
    this->currentTarget = nullptr;
}


void WgCompositor::renderClipPath(WgContext& context, WgRenderDataPaint* renderData, WgRenderStorage* dst)
{
    assert(renderData);
    if (renderData->clips.count == 0) return;
    // store current render pass
    WgRenderStorage *target = currentTarget;
    endRenderPass();
    // render first clip path
    beginRenderPass(commandEncoder, dst, true);
    drawClipPath(context, (WgRenderDataShape*)(renderData->clips[0]));
    endRenderPass();
    // render amd merge clip paths
    for (uint32_t i = 1 ; i < renderData->clips.count; i++) {
        // render clip path
        beginRenderPass(commandEncoder, &storageInterm, true);
        drawClipPath(context, (WgRenderDataShape*)(renderData->clips[i]));
        endRenderPass();
        // merge masks
        mergeMasks(commandEncoder, &storageInterm, dst);
    }
    // restore current render pass
    beginRenderPass(commandEncoder, target, false);
}


void WgCompositor::renderShape(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod)
{
    assert(renderData);
    assert(renderPassEncoder);
    WgPipelineBlendType blentType = blendMethodToBlendType(blendMethod);
    // apply clip path if neccessary
    if (renderData->clips.count != 0) {
        renderClipPath(context, renderData, &storageClipPath);
        if (renderData->strokeFirst) {
            drawStrokesClipped(context, renderData, &storageClipPath);
            drawShapeClipped(context, renderData, &storageClipPath);
        } else {
            drawShapeClipped(context, renderData, &storageClipPath);
            drawStrokesClipped(context, renderData, &storageClipPath);
        }
    // use custom blending
    } else if (blentType == WgPipelineBlendType::Custom) {
        if (renderData->strokeFirst) {
            blendStrokes(context, renderData, blendMethod);
            blendShape(context, renderData, blendMethod);
        } else {
            blendShape(context, renderData, blendMethod);
            blendStrokes(context, renderData, blendMethod);
        }
    // use direct hardware blending
    } else {
        if (renderData->strokeFirst) {
            drawStrokes(context, renderData, blentType);
            drawShape(context, renderData, blentType);
        } else {
            drawShape(context, renderData, blentType);
            drawStrokes(context, renderData, blentType);
        }
    }
}


void WgCompositor::renderImage(WgContext& context, WgRenderDataPicture* renderData, BlendMethod blendMethod)
{
    assert(renderData);
    assert(renderPassEncoder);
    WgPipelineBlendType blentType = blendMethodToBlendType(blendMethod);
    // apply clip path if neccessary
    if (renderData->clips.count != 0) {
        renderClipPath(context, renderData, &storageClipPath);
        drawImageClipped(context, renderData, &storageClipPath);
    // use custom blending
    } else if (blentType == WgPipelineBlendType::Custom)
        blendImage(context, renderData, blendMethod);
    else // use direct hardware blending
        drawImage(context, renderData, blentType);
};


void WgCompositor::blendShape(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod)
{
    assert(renderData);
    assert(commandEncoder);
    assert(currentTarget);
    // skip shape blending if shapes do not exist
    if (renderData->renderSettingsShape.skip) return;
    if (renderData->meshGroupShapes.meshes.count == 0) return;
    // store current render pass
    WgRenderStorage *target = currentTarget;
    endRenderPass();
    // render into intermediate buffer
    beginRenderPass(commandEncoder, &storageInterm, true);
    drawShape(context, renderData, WgPipelineBlendType::Custom);
    endRenderPass();
    // run blend
    blend(commandEncoder, &storageInterm, target, 255, blendMethod, renderData->renderSettingsShape.rasterType);
    // restore current render pass
    beginRenderPass(commandEncoder, target, false);
}

void WgCompositor::blendStrokes(WgContext& context, WgRenderDataShape* renderData, BlendMethod blendMethod)
{
    assert(renderData);
    assert(commandEncoder);
    assert(currentTarget);
    // skip strokes blending if strokes do not exist
    if (renderData->renderSettingsStroke.skip) return;
    if (renderData->meshGroupStrokes.meshes.count == 0) return;
    // store current render pass
    WgRenderStorage *target = currentTarget;
    endRenderPass();
    // render into intermediate buffer
    beginRenderPass(commandEncoder, &storageInterm, true);
    drawStrokes(context, renderData, WgPipelineBlendType::Custom);
    endRenderPass();
    // run blend
    blend(commandEncoder, &storageInterm, target, 255, blendMethod, renderData->renderSettingsStroke.rasterType);
    // restore current render pass
    beginRenderPass(commandEncoder, target, false);
};


void WgCompositor::blendImage(WgContext& context, WgRenderDataPicture* renderData, BlendMethod blendMethod)
{
    assert(renderData);
    assert(commandEncoder);
    assert(currentTarget);
    // store current render pass
    WgRenderStorage *target = currentTarget;
    endRenderPass();
    // render into intermediate buffer
    beginRenderPass(commandEncoder, &storageInterm, true);
    drawImage(context, renderData, WgPipelineBlendType::Custom);
    endRenderPass();
    // run blend
    blend(commandEncoder, &storageInterm, target, 255, blendMethod, WgRenderRasterType::Image);
    // restore current render pass
    beginRenderPass(commandEncoder, target, false);
};


void WgCompositor::blendScene(WgContext& context, WgRenderStorage* src, WgCompose* cmp)
{
    assert(currentTarget);
    RenderRegion rect = shrinkRenderRegion(cmp->aabb);
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rect.x, rect.y, rect.w, rect.h);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, src->bindGroupTexure, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[cmp->opacity], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->sceneBlend);
    meshData.drawImage(context, renderPassEncoder);
}


void WgCompositor::drawShapeClipped(WgContext& context, WgRenderDataShape* renderData, WgRenderStorage* mask)
{
    assert(mask);
    assert(renderData);
    assert(commandEncoder);
    assert(currentTarget);
    // skip shape composing if shape do not exist
    if (renderData->renderSettingsShape.skip) return;
    if (renderData->meshGroupShapes.meshes.count == 0) return;
    // store current render pass
    WgRenderStorage *target = currentTarget;
    endRenderPass();
    // render into intermediate buffer
    beginRenderPass(commandEncoder, &storageInterm, true);
    drawShape(context, renderData, WgPipelineBlendType::Custom);
    endRenderPass();
    // restore current render pass
    beginRenderPass(commandEncoder, target, false);
    RenderRegion rect { 0, 0, (int32_t)width, (int32_t)height };
    clipRegion(context, &storageInterm, mask, rect);
}


void WgCompositor::drawStrokesClipped(WgContext& context, WgRenderDataShape* renderData, WgRenderStorage* mask)
{
    assert(mask);
    assert(renderData);
    assert(commandEncoder);
    assert(currentTarget);
    // skip shape composing if strokes do not exist
    if (renderData->renderSettingsStroke.skip) return;
    if (renderData->meshGroupStrokes.meshes.count == 0) return;
    // store current render pass
    WgRenderStorage *target = currentTarget;
    endRenderPass();
    // render into intermediate buffer
    beginRenderPass(commandEncoder, &storageInterm, true);
    drawStrokes(context, renderData, WgPipelineBlendType::Custom);
    endRenderPass();
    // restore current render pass
    beginRenderPass(commandEncoder, target, false);
    RenderRegion rect { 0, 0, (int32_t)width, (int32_t)height };
    clipRegion(context, &storageInterm, mask, rect);
}


void WgCompositor::drawImageClipped(WgContext& context, WgRenderDataPicture* renderData, WgRenderStorage* mask)
{
    assert(mask);
    assert(renderData);
    assert(commandEncoder);
    assert(currentTarget);
    // store current render pass
    WgRenderStorage *target = currentTarget;
    endRenderPass();
    // render into intermediate buffer
    beginRenderPass(commandEncoder, &storageInterm, true);
    drawImage(context, renderData, WgPipelineBlendType::Custom);
    endRenderPass();
    // restore current render pass
    beginRenderPass(commandEncoder, target, false);
    RenderRegion rect { 0, 0, (int32_t)width, (int32_t)height };
    clipRegion(context, &storageInterm, mask, rect);
}


void WgCompositor::clipRegion(WgContext& context, WgRenderStorage* src, WgRenderStorage* mask, RenderRegion& rect)
{
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rect.x, rect.y, rect.w, rect.h);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, storageInterm.bindGroupTexure, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, mask->bindGroupTexure, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->sceneClip);
    meshData.drawImage(context, renderPassEncoder);
}


void WgCompositor::composeScene(WgContext& context, WgRenderStorage* src, WgRenderStorage* mask, WgCompose* cmp)
{
    assert(cmp);
    assert(src);
    assert(mask);
    assert(renderPassEncoder);
    RenderRegion rect = shrinkRenderRegion(cmp->aabb);
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rect.x, rect.y, rect.w, rect.h);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, src->bindGroupTexure, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, mask->bindGroupTexure, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->sceneComp[(uint32_t)cmp->method]);
    meshData.drawImage(context, renderPassEncoder);
}

void WgCompositor::drawClipPath(WgContext& context, WgRenderDataShape* renderData)
{
    assert(renderData);
    assert(renderPassEncoder);
    assert(renderData->meshGroupShapes.meshes.count == renderData->meshGroupShapesBBox.meshes.count);
    if ((renderData->viewport.w <= 0) || (renderData->viewport.h <= 0)) return;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, renderData->viewport.x, renderData->viewport.y, renderData->viewport.w, renderData->viewport.h);
    // setup stencil rules
    WGPURenderPipeline stencilPipeline = (renderData->fillRule == FillRule::Winding) ? pipelines->winding : pipelines->evenodd;
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, renderData->bindGroupPaint, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, stencilPipeline);
    // draw to stencil (first pass)
    for (uint32_t i = 0; i < renderData->meshGroupShapes.meshes.count; i++)
        renderData->meshGroupShapes.meshes[i]->drawFan(context, renderPassEncoder);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, renderData->bindGroupPaint, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->clipPath);
    // draw to color (second pass)
    renderData->meshDataBBox.drawFan(context, renderPassEncoder);
}


void WgCompositor::drawShape(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType)
{
    assert(renderData);
    assert(renderPassEncoder);
    assert(renderData->meshGroupShapes.meshes.count == renderData->meshGroupShapesBBox.meshes.count);
    uint32_t blendTypeInd = (uint32_t)blendType;
    if (renderData->renderSettingsShape.skip) return;
    if ((renderData->viewport.w <= 0) || (renderData->viewport.h <= 0)) return;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, renderData->viewport.x, renderData->viewport.y, renderData->viewport.w, renderData->viewport.h);
    // setup stencil rules
    WGPURenderPipeline stencilPipeline = (renderData->fillRule == FillRule::Winding) ? pipelines->winding : pipelines->evenodd;
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, renderData->bindGroupPaint, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, stencilPipeline);
    // draw to stencil (first pass)
    for (uint32_t i = 0; i < renderData->meshGroupShapes.meshes.count; i++)
        renderData->meshGroupShapes.meshes[i]->drawFan(context, renderPassEncoder);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, renderData->bindGroupPaint, 0, nullptr);
    WgRenderSettings& settings = renderData->renderSettingsShape;
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.bindGroupSolid, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->solid[blendTypeInd]);
    } else if (settings.fillType == WgRenderSettingsType::Linear) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.bindGroupGradient, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->linear[blendTypeInd]);
    } else if (settings.fillType == WgRenderSettingsType::Radial) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.bindGroupGradient, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->radial[blendTypeInd]);
    }
    // draw to color (second pass)
    renderData->meshDataBBox.drawFan(context, renderPassEncoder);
}


void WgCompositor::drawStrokes(WgContext& context, WgRenderDataShape* renderData, WgPipelineBlendType blendType)
{
    assert(renderData);
    assert(renderPassEncoder);
    assert(renderData->meshGroupStrokes.meshes.count == renderData->meshGroupStrokesBBox.meshes.count);
    uint32_t blendTypeInd = (uint32_t)blendType;
    if (renderData->renderSettingsStroke.skip) return;
    if ((renderData->viewport.w <= 0) || (renderData->viewport.h <= 0)) return;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, renderData->viewport.x, renderData->viewport.y, renderData->viewport.w, renderData->viewport.h);
    // draw strokes to stencil (first pass)
    for (uint32_t i = 0; i < renderData->meshGroupStrokes.meshes.count; i++) {
        // setup stencil rules
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, renderData->bindGroupPaint, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->direct);
        // draw to stencil (first pass)
        renderData->meshGroupStrokes.meshes[i]->draw(context, renderPassEncoder);
        // setup fill rules
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, renderData->bindGroupPaint, 0, nullptr);
        WgRenderSettings& settings = renderData->renderSettingsStroke;
        if (settings.fillType == WgRenderSettingsType::Solid) {
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.bindGroupSolid, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->solid[blendTypeInd]);
        } else if (settings.fillType == WgRenderSettingsType::Linear) {
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.bindGroupGradient, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->linear[blendTypeInd]);
        } else if (settings.fillType == WgRenderSettingsType::Radial) {
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.bindGroupGradient, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->radial[blendTypeInd]);
        }
        // draw to color (second pass)
        renderData->meshGroupStrokesBBox.meshes[i]->drawFan(context, renderPassEncoder);
    }
}


void WgCompositor::drawImage(WgContext& context, WgRenderDataPicture* renderData, WgPipelineBlendType blendType)
{
    assert(renderData);
    assert(renderPassEncoder);
    uint32_t blendTypeInd = (uint32_t)blendType;
    if ((renderData->viewport.w <= 0) || (renderData->viewport.h <= 0)) return;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, renderData->viewport.x, renderData->viewport.y, renderData->viewport.w, renderData->viewport.h);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, renderData->bindGroupPaint, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, renderData->bindGroupPicture, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines->image[blendTypeInd]);
    renderData->meshData.drawImage(context, renderPassEncoder);
}


void WgCompositor::mergeMasks(WGPUCommandEncoder encoder, WgRenderStorage* mask0, WgRenderStorage* mask1)
{
    assert(mask0);
    assert(mask1);
    assert(!renderPassEncoder);
    // copy dst storage to temporary read only storage
    const WGPUImageCopyTexture texSrc { .texture = mask1->texture };
    const WGPUImageCopyTexture texDst { .texture = storageDstCopy.texture };
    const WGPUExtent3D copySize { .width = width, .height = height, .depthOrArrayLayers = 1 };
    wgpuCommandEncoderCopyTextureToTexture(encoder, &texSrc, &texDst, &copySize);
    // execute compose shader
    const WGPUComputePassDescriptor computePassDescriptor{};
    WGPUComputePassEncoder computePassEncoder = wgpuCommandEncoderBeginComputePass(encoder, &computePassDescriptor);
    wgpuComputePassEncoderSetBindGroup(computePassEncoder, 0, mask0->bindGroupRead, 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePassEncoder, 1, storageDstCopy.bindGroupRead, 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePassEncoder, 2, mask1->bindGroupWrite, 0, nullptr);
    wgpuComputePassEncoderSetPipeline(computePassEncoder, pipelines->mergeMasks);
    wgpuComputePassEncoderDispatchWorkgroups(computePassEncoder, (width + 7) / 8, (height + 7) / 8, 1);
    wgpuComputePassEncoderEnd(computePassEncoder);
}


void WgCompositor::blend(WGPUCommandEncoder encoder, WgRenderStorage* src, WgRenderStorage* dst, uint8_t opacity, BlendMethod blendMethod, WgRenderRasterType rasterType)
{
    assert(src);
    assert(dst);
    assert(!renderPassEncoder);
    WGPUComputePipeline pipeline = pipelines->blendImage[(size_t)blendMethod];
    if (rasterType == WgRenderRasterType::Solid) pipeline = pipelines->blendSolid[(size_t)blendMethod];
    if (rasterType == WgRenderRasterType::Gradient) pipeline = pipelines->blendGradient[(size_t)blendMethod];
    // copy texture to texture
    const WGPUImageCopyTexture texSrc { .texture = dst->texture };
    const WGPUImageCopyTexture texDst { .texture = storageDstCopy.texture };
    const WGPUExtent3D copySize { .width = width, .height = height, .depthOrArrayLayers = 1 };
    wgpuCommandEncoderCopyTextureToTexture(encoder, &texSrc, &texDst, &copySize);
    // execute compose shader
    const WGPUComputePassDescriptor computePassDescriptor{};
    WGPUComputePassEncoder computePassEncoder = wgpuCommandEncoderBeginComputePass(encoder, &computePassDescriptor);
    wgpuComputePassEncoderSetBindGroup(computePassEncoder, 0, src->bindGroupRead, 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePassEncoder, 1, storageDstCopy.bindGroupRead, 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePassEncoder, 2, dst->bindGroupWrite, 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePassEncoder, 3, bindGroupOpacities[opacity], 0, nullptr);
    wgpuComputePassEncoderSetPipeline(computePassEncoder, pipeline);
    wgpuComputePassEncoderDispatchWorkgroups(computePassEncoder, (width + 7) / 8, (height + 7) / 8, 1);
    wgpuComputePassEncoderEnd(computePassEncoder);
}


void WgCompositor::blit(WgContext& context, WGPUCommandEncoder encoder, WgRenderStorage* src, WGPUTextureView dstView) {
    WGPURenderPassDepthStencilAttachment depthStencilAttachment{ .view = texViewStencil, .stencilLoadOp = WGPULoadOp_Load, .stencilStoreOp = WGPUStoreOp_Discard };
    WGPURenderPassColorAttachment colorAttachment { .view = dstView, .loadOp = WGPULoadOp_Load, .storeOp = WGPUStoreOp_Store };
    #ifdef __EMSCRIPTEN__
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    #endif
    WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, src->bindGroupTexure, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPass, pipelines->blit);
    meshData.drawImage(context, renderPass);
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);
}
