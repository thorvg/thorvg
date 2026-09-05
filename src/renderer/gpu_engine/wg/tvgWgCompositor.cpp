/*
 * Copyright (c) 2024 - 2026 ThorVG project. All rights reserved.

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
#include <iostream>

static WGPURenderPipeline _gradientPipeline(const WgPipelines& pipelines, WgRenderSettingsType type, bool convex)
{
    switch (type) {
        case WgRenderSettingsType::Linear: return convex ? pipelines.linear_conv : pipelines.linear;
        case WgRenderSettingsType::Radial: return convex ? pipelines.radial_conv : pipelines.radial;
        case WgRenderSettingsType::Conic: return convex ? pipelines.conic_conv : pipelines.conic;
        default: return nullptr;
    }
}


static WGPURenderPipeline _gradientBlendPipeline(const WgPipelines& pipelines, WgRenderSettingsType type, uint32_t blendMethod)
{
    switch (type) {
        case WgRenderSettingsType::Linear: return pipelines.linear_blend[blendMethod];
        case WgRenderSettingsType::Radial: return pipelines.radial_blend[blendMethod];
        case WgRenderSettingsType::Conic: return pipelines.conic_blend[blendMethod];
        default: return nullptr;
    }
}


void WgCompositor::updateViewMat(WgContext& context, uint32_t width, uint32_t height)
{
    if (bindGroupViewMat && viewMatWidth == width && viewMatHeight == height) return;

    WgShaderTypeMat4x4f viewMat(width, height);
    bool bufferChanged = context.allocateBufferUniform(bufferViewMat, &viewMat, sizeof(viewMat));

    if (bufferChanged || !bindGroupViewMat) {
        context.layouts.releaseBindGroup(bindGroupViewMat);
        bindGroupViewMat = context.layouts.createBindGroupBuffer1Un(bufferViewMat);
    }

    viewMatWidth = width;
    viewMatHeight = height;
}


void WgCompositor::initialize(WgContext& context, uint32_t width, uint32_t height)
{
    // pipelines (external handle, do not release)
    pipelines.initialize(context);
    stageBufferGeometry.initialize(context);
    // initialize opacity pool
    initPools(context);
    // allocate global view matrix handles
    updateViewMat(context, width, height);
    // create render targets handles
    resize(context, width, height);
    // composition and blend geometries
    meshDataBlit.blitBox();
    // force stage buffers initialization
    flush(context);
}


void WgCompositor::initPools(WgContext& context)
{
    for (uint32_t i = 0; i < 256; i++) {
        float opacity = i / 255.0f;
        context.allocateBufferUniform(bufferOpacities[i], &opacity, sizeof(float));
        bindGroupOpacities[i] = context.layouts.createBindGroupBuffer1Un(bufferOpacities[i]);
    }
}


void WgCompositor::release(WgContext& context)
{
    // release render targets habdles
    resize(context, 0, 0);
    // release opacity pool
    releasePools(context);
    // release global view matrix handles
    context.layouts.releaseBindGroup(bindGroupViewMat);
    context.releaseBuffer(bufferViewMat);
    viewMatWidth = 0;
    viewMatHeight = 0;
    // release stage buffer
    stageBufferSolidColor.release(context);
    stageBufferPaint.release(context);
    stageBufferViewMat.release(context);
    stageBufferGeometry.release(context);
    // release pipelines
    pipelines.release(context);
}


void WgCompositor::releasePools(WgContext& context)
{
    // release opacity pool
    for (uint32_t i = 0; i < 256; i++) {
        context.layouts.releaseBindGroup(bindGroupOpacities[i]);
        context.releaseBuffer(bufferOpacities[i]);
    }
}


void WgCompositor::resize(WgContext& context, uint32_t width, uint32_t height) {
    // release existig handles
    if ((this->width != width) || (this->height != height)) {
        context.layouts.releaseBindGroup(bindGroupStorageTemp);
        // release intermediate render target
        targetTemp1.release(context);
        targetTemp0.release(context);
        // release global stencil buffer handles
        context.releaseTextureView(texViewDepthStencilMS);
        context.releaseTexture(texDepthStencilMS);
        context.releaseTextureView(texViewDepthStencil);
        context.releaseTexture(texDepthStencil);
        // store render target dimensions
        this->height = height;
        this->width = width;
    }

    // create render targets handles
    if ((width != 0) && (height != 0)) {
        // store render target dimensions
        this->width = width;
        this->height = height;
        // update global view matrix handles
        updateViewMat(context, width, height);
        // allocate global stencil buffer handles
        texDepthStencil = context.createTexAttachement(width, height, WGPUTextureFormat_Depth24PlusStencil8, 1);
        texViewDepthStencil = context.createTextureView(texDepthStencil);
        texDepthStencilMS = context.createTexAttachement(width, height, WGPUTextureFormat_Depth24PlusStencil8, 4);
        texViewDepthStencilMS = context.createTextureView(texDepthStencilMS);
        // initialize intermediate render targets
        targetTemp0.initialize(context, width, height);
        targetTemp1.initialize(context, width, height);
        bindGroupStorageTemp = context.layouts.createBindGroupStrorage2RO(targetTemp0.texView, targetTemp1.texView);
    }
}


RenderRegion WgCompositor::shrinkRenderRegion(const RenderRegion& rect)
{
    return {
        {std::max(0, std::min((int32_t)width, rect.min.x)), std::max(0, std::min((int32_t)height, rect.min.y))},
        {std::max(0, std::min((int32_t)width, rect.max.x)), std::max(0, std::min((int32_t)height, rect.max.y))}
    };
}


void WgCompositor::copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src)
{
    const RenderRegion region = {{0, 0}, {(int32_t)src->width, (int32_t)src->height}};
    copyTexture(dst, src, region);
}


void WgCompositor::copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src, const RenderRegion& region)
{
    const WGPUTexelCopyTextureInfo texSrc { .texture = src->texture, .origin = { .x = (uint32_t)region.min.x, .y = (uint32_t)region.min.y } };
    const WGPUTexelCopyTextureInfo texDst { .texture = dst->texture, .origin = { .x = (uint32_t)region.min.x, .y = (uint32_t)region.min.y } };
    const WGPUExtent3D copySize { .width = region.w(), .height = region.h(), .depthOrArrayLayers = 1 };
    wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &texSrc, &texDst, &copySize);
}


void WgCompositor::beginRenderPassMS(WGPUCommandEncoder commandEncoder, WgRenderTarget* target, bool clear, WGPUColor clearColor)
{
    // do not start same render bass
    if (target == currentTarget) return;
    // we must to end render pass first
    endRenderPass();
    this->currentTarget = target;
    // start new render pass
    this->commandEncoder = commandEncoder;
    const WGPURenderPassDepthStencilAttachment depthStencilAttachment{ 
        .view = texViewDepthStencilMS,
        .depthLoadOp = WGPULoadOp_Clear,
        .depthStoreOp = WGPUStoreOp_Discard,
        .depthClearValue = 1.0f,
        .stencilLoadOp = WGPULoadOp_Clear,
        .stencilStoreOp = WGPUStoreOp_Discard,
        .stencilClearValue = 0
    };
    const WGPURenderPassColorAttachment colorAttachment{
        .view = target->texViewMS,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .resolveTarget = target->texView,
        .loadOp = clear ? WGPULoadOp_Clear : WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = clearColor
    };
    WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
    assert(renderPassEncoder);
}


void WgCompositor::beginRenderPass(WGPUCommandEncoder encoder, WgRenderTarget* target)
{
    assert(!renderPassEncoder);
    currentTarget = target;
    const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
        .view = texViewDepthStencil,
        .depthLoadOp = WGPULoadOp_Load,
        .depthStoreOp = WGPUStoreOp_Discard,
        .stencilLoadOp = WGPULoadOp_Load,
        .stencilStoreOp = WGPUStoreOp_Discard
    };
    const WGPURenderPassColorAttachment colorAttachment {
        .view = target->texView,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .loadOp = WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
    };
    const WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    renderPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
}


void WgCompositor::endRenderPass()
{
    if (currentTarget) {
        assert(renderPassEncoder);
        wgpuRenderPassEncoderEnd(renderPassEncoder);
        wgpuRenderPassEncoderRelease(renderPassEncoder);
        renderPassEncoder = nullptr;
        currentTarget = nullptr;
    }
}

void WgCompositor::reset(WgContext& context)
{
    stageBufferGeometry.clear();
    stageBufferSolidColor.clear();
    stageBufferPaint.clear();
    stageBufferViewMat.clear();
}


void WgCompositor::flush(WgContext& context)
{
    stageBufferGeometry.append(&meshDataBlit);
    stageBufferGeometry.flush(context);
    stageBufferSolidColor.flush(context);
    stageBufferPaint.flush(context);
    stageBufferViewMat.flush(context);
    context.submit();
}

void WgCompositor::requestShape(WgRenderShape* rdata)
{
    stageBufferGeometry.append(rdata);

    auto& shapeSettings = rdata->shape.setting;
    auto& shapeSolid = rdata->shape.solid;
    if (shapeSettings.fillType == WgRenderSettingsType::Solid) shapeSolid.colorIdx = stageBufferSolidColor.append(shapeSolid.packedColor());
    else shapeSettings.bindGroupIdx = stageBufferPaint.append(shapeSettings.settings);

    if (!rdata->stroke.mesh.vbuffer.empty()) {
        Matrix viewMatrix{2.0f / width, 0.0f, -1.0f, 0.0f, -2.0f / height, 1.0f, 0.0f, 0.0f, 1.0f};
        WgShaderTypeMat4x4fBlock strokeViewMat{{viewMatrix * rdata->transform}, {}};
        rdata->strokeViewMatIdx = stageBufferViewMat.append(strokeViewMat);
    }

    if (rdata->stroke.setting.valid && !rdata->stroke.mesh.vbuffer.empty()) {
        auto& strokeSettings = rdata->stroke.setting;
        auto& strokeSolid = rdata->stroke.solid;
        if (strokeSettings.fillType == WgRenderSettingsType::Solid) strokeSolid.colorIdx = stageBufferSolidColor.append(strokeSolid.packedColor());
        else strokeSettings.bindGroupIdx = stageBufferPaint.append(strokeSettings.settings);
    }
    ARRAY_FOREACH(p, rdata->clips)
        requestShape((WgRenderShape*)(*p));
}

void WgCompositor::requestImage(WgRenderPicture* rdata)
{
    stageBufferGeometry.append(rdata);
    rdata->renderSettings.bindGroupIdx = stageBufferPaint.append(rdata->renderSettings.settings);
    ARRAY_FOREACH(p, rdata->clips)
        requestShape((WgRenderShape*)(*p));
}

void WgCompositor::requestSolidBatch(const Array<WgRenderShape*>& renderShapes, WgSolidBatchRange& range)
{
    stageBufferGeometry.appendSolidBatch(renderShapes, stageBufferSolidColor, range);
    range.viewport = renderShapes[0]->viewport;
}

void WgCompositor::requestStencilBatch(const Array<WgRenderShape*>& renderShapes, WgStencilBatchRange& range)
{
    stageBufferGeometry.appendStencilBatch(renderShapes, range);
    range.viewport = renderShapes[0]->viewport;
    range.fillRule = renderShapes[0]->fillRule;
    range.solidOnly = true;

    uint32_t pendingColorCount = 0;
    auto colorsStarted = false;

    ARRAY_FOREACH(p, renderShapes) {
        auto rdata = *p;
        auto& settings = rdata->shape.setting;
        const auto count = rdata->meshBBox.vbuffer.count;

        if (settings.fillType == WgRenderSettingsType::Solid) {
            if (!colorsStarted) {
                range.colorOffset = static_cast<size_t>(stageBufferSolidColor.vbuffer.count) * sizeof(RenderColor);
                if (pendingColorCount) stageBufferSolidColor.appendRepeated({}, pendingColorCount);
                colorsStarted = true;
            }
            stageBufferSolidColor.appendRepeated(rdata->shape.solid.packedColor(), count);
        } else {
            range.solidOnly = false;
            settings.bindGroupIdx = stageBufferPaint.append(settings.settings);
            if (colorsStarted) stageBufferSolidColor.appendRepeated({}, count);
            else pendingColorCount += count;
        }
    }
}

void WgCompositor::renderShape(WgContext& context, WgRenderShape* rdata, BlendMethod blendMethod)
{
    // apply clip path if necessary
    if (!rdata->clips.empty()) {
        renderClipPath(context, rdata);
        if (rdata->strokeFirst) {
            clipStrokes(context, rdata);
            clipShape(context, rdata);
        } else {
            clipShape(context, rdata);
            clipStrokes(context, rdata);
        }
        clearClipPath(context, rdata);
    // use custom blending
    } else if (blendMethod != BlendMethod::Normal) {
        if (rdata->strokeFirst) {
            blendStrokes(context, rdata, blendMethod);
            blendShape(context, rdata, blendMethod);
        } else {
            blendShape(context, rdata, blendMethod);
            blendStrokes(context, rdata, blendMethod);
        }
    // use direct hardware blending
    } else {
        if (rdata->strokeFirst) {
            drawStrokes(context, rdata);
            drawShape(context, rdata);
        } else {
            drawShape(context, rdata);
            drawStrokes(context, rdata);
        }
    }
}

void WgCompositor::renderSolidBatch(const WgSolidBatchRange& range)
{
    const uint64_t vertexSize = static_cast<uint64_t>(range.vertexCount) * sizeof(Point);
    const uint64_t colorSize = static_cast<uint64_t>(range.vertexCount) * sizeof(RenderColor);
    const uint64_t indexSize = static_cast<uint64_t>(range.indexCount) * sizeof(uint32_t);

    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, range.viewport.x(), range.viewport.y(), range.viewport.w(), range.viewport.h());
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid_batch);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, range.vertexOffset, vertexSize);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferSolidColor.vbuffer_gpu, range.colorOffset, colorSize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, range.indexOffset, indexSize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, range.indexCount, 1, 0, 0, 0);
}

void WgCompositor::renderStencilBatch(const Array<WgRenderShape*>& renderShapes, const WgStencilBatchRange& range)
{
    const uint64_t stencilVertexSize = static_cast<uint64_t>(range.stencil.vertexCount) * sizeof(Point);
    const uint64_t stencilIndexSize = static_cast<uint64_t>(range.stencil.indexCount) * sizeof(uint32_t);
    const uint64_t coverVertexSize = static_cast<uint64_t>(range.cover.vertexCount) * sizeof(Point);
    const uint64_t coverIndexSize = static_cast<uint64_t>(range.cover.indexCount) * sizeof(uint32_t);

    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, range.viewport.x(), range.viewport.y(), range.viewport.w(), range.viewport.h());
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, range.fillRule == FillRule::NonZero ? pipelines.nonzero : pipelines.evenodd);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, range.stencil.vertexOffset, stencilVertexSize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, range.stencil.indexOffset, stencilIndexSize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, range.stencil.indexCount, 1, 0, 0, 0);

    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, range.cover.vertexOffset, coverVertexSize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, range.cover.indexOffset, coverIndexSize);

    // Keep the common all-solid case identical to the original two-draw path.
    if (range.solidOnly) {
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid_stencil_batch);
        wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferSolidColor.vbuffer_gpu, range.colorOffset, static_cast<uint64_t>(range.cover.vertexCount) * sizeof(RenderColor));
        wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, range.cover.indexCount, 1, 0, 0, 0);
        return;
    }

    uint32_t firstIndex = 0;
    bool colorsBound = false;

    auto p = renderShapes.begin();
    const auto end = renderShapes.end();
    while (p < end) {
        auto rdata = *p;
        auto& settings = rdata->shape.setting;
        const auto batchFirstIndex = firstIndex;

        do {
            const auto indexCount = rdata->meshBBox.ibuffer.count;
            const uint64_t nextIndex = static_cast<uint64_t>(firstIndex) + indexCount;
            firstIndex = static_cast<uint32_t>(nextIndex);
            if (++p == end || settings.fillType != WgRenderSettingsType::Solid) break;
            rdata = *p;
        } while (rdata->shape.setting.fillType == WgRenderSettingsType::Solid);

        if (settings.fillType == WgRenderSettingsType::Solid) {
            if (!colorsBound) {
                wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferSolidColor.vbuffer_gpu, range.colorOffset, static_cast<uint64_t>(range.cover.vertexCount) * sizeof(RenderColor));
                colorsBound = true;
            }
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid_stencil_batch);
        } else {
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, false));
        }
        wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, firstIndex - batchFirstIndex, 1, batchFirstIndex, 0, 0);
    }
}

void WgCompositor::renderImage(WgContext& context, WgRenderPicture* rdata, BlendMethod blendMethod)
{
    // apply clip path if necessary
    if (rdata->clips.count != 0) {
        renderClipPath(context, rdata);
        clipImage(context, rdata);
        clearClipPath(context, rdata);
    // use custom blending
    } else if (blendMethod != BlendMethod::Normal)
        blendImage(context, rdata, blendMethod);
    // use direct hardware blending
    else drawImage(context, rdata);
}


void WgCompositor::renderScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose)
{
    // use custom blending
    if (compose->blend != BlendMethod::Normal)
        blendScene(context, scene, compose);
    // use direct hardware blending
    else drawScene(context, scene, compose);
}


void WgCompositor::composeScene(WgContext& context, WgRenderTarget* src, WgRenderTarget* mask, WgCompose* cmp)
{
    RenderRegion rect = shrinkRenderRegion(cmp->aabb);
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rect.x(), rect.y(), rect.w(), rect.h());
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, src->bindGroupTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, mask->bindGroupTexture, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.scene_compose[(uint32_t)cmp->method]);
    drawMeshImage(context, &meshDataBlit);
}

void WgCompositor::blit(WgContext& context, WGPUCommandEncoder encoder, WgRenderTarget* src, WGPUTextureView dstView, bool premultiplied)
{
    const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
        .view = texViewDepthStencil,
        .depthLoadOp = WGPULoadOp_Load,
        .depthStoreOp = WGPUStoreOp_Discard,
        .stencilLoadOp = WGPULoadOp_Load,
        .stencilStoreOp = WGPUStoreOp_Discard
    };
    const WGPURenderPassColorAttachment colorAttachment { 
        .view = dstView,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .loadOp = WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
    };
    const WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    renderPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, src->bindGroupTexture, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, premultiplied ? pipelines.blit : pipelines.blit_unpremultiplied);
    drawMeshImage(context, &meshDataBlit);
    wgpuRenderPassEncoderEnd(renderPassEncoder);
    wgpuRenderPassEncoderRelease(renderPassEncoder);
    renderPassEncoder = nullptr;
}


void WgCompositor::drawMesh(WgContext& context, WgMeshData* meshData)
{
    uint64_t icount = meshData->ibuffer.count;
    uint64_t vsize = meshData->vbuffer.count * sizeof(Point);
    uint64_t isize = icount * sizeof(uint32_t);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, meshData->voffset, vsize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, meshData->ioffset, isize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, icount, 1, 0, 0, 0);
};


void WgCompositor::drawMeshSolid(WgContext& context, WgMeshData* meshData, uint32_t solidColorInd)
{
    const uint64_t icount = meshData->ibuffer.count;
    const uint64_t vsize = meshData->vbuffer.count * sizeof(Point);
    const uint64_t isize = icount * sizeof(uint32_t);
    const uint64_t csize = sizeof(RenderColor);
    const uint64_t coffset = solidColorInd * csize;
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, meshData->voffset, vsize);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferSolidColor.vbuffer_gpu, coffset, csize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, meshData->ioffset, isize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, icount, 1, 0, 0, 0);
}


void WgCompositor::drawMeshImage(WgContext& context, WgMeshData* meshData)
{
    uint64_t icount = meshData->ibuffer.count;
    uint64_t vsize = meshData->vbuffer.count * sizeof(Point);
    uint64_t isize = icount * sizeof(uint32_t);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, meshData->voffset, vsize);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, meshData->toffset, vsize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, meshData->ioffset, isize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, icount, 1, 0, 0, 0);
};

void WgCompositor::drawShape(WgContext& context, WgRenderShape* rdata)
{
    if (!rdata->shape.setting.valid || rdata->shape.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    auto& settings = rdata->shape.setting;
    auto convex = rdata->convex;
    auto mesh = rdata->convex ? &rdata->shape.mesh : &rdata->meshBBox;

    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());

    // setup stencil rules
    if (!convex) {
        WGPURenderPipeline stencilPipeline = (rdata->fillRule == FillRule::NonZero) ? pipelines.nonzero : pipelines.evenodd;
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, stencilPipeline);
        // draw to stencil (first pass)
        drawMesh(context, &rdata->shape.mesh);
    }

    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);

    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, convex ? pipelines.solid_conv : pipelines.solid);
        drawMeshSolid(context, mesh, rdata->shape.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, convex));
        drawMesh(context, mesh);
    }
}

void WgCompositor::blendShape(WgContext& context, WgRenderShape* rdata, BlendMethod blendMethod)
{
    if (!rdata->shape.setting.valid || rdata->shape.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->shape.setting;
    // copy current render target data to dst target
    WgRenderTarget *target = currentTarget;
    endRenderPass();
    copyTexture(&targetTemp0, target);
    beginRenderPassMS(commandEncoder, target, false);
    // render shape with blend settings
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // setup stencil rules
    WGPURenderPipeline stencilPipeline = (rdata->fillRule == FillRule::NonZero) ? pipelines.nonzero : pipelines.evenodd;
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, stencilPipeline);
    // draw to stencil (first pass)
    drawMesh(context, &rdata->shape.mesh);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    uint32_t blendMethodInd = (uint32_t)blendMethod;
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid_blend[blendMethodInd]);
        drawMeshSolid(context, &rdata->meshBBox, rdata->shape.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 3, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientBlendPipeline(pipelines, settings.fillType, blendMethodInd));
        drawMesh(context, &rdata->meshBBox);
    }
}

void WgCompositor::clipShape(WgContext& context, WgRenderShape* rdata)
{
    if (!rdata->shape.setting.valid || rdata->shape.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->shape.setting;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // setup stencil rules
    WGPURenderPipeline stencilPipeline = (rdata->fillRule == FillRule::NonZero) ? pipelines.nonzero : pipelines.evenodd;
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, stencilPipeline);
    // draw to stencil (first pass)
    drawMesh(context, &rdata->shape.mesh);
    // merge depth and stencil buffer
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.merge_depth_stencil);
    drawMesh(context, &rdata->meshBBox);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid);
        drawMeshSolid(context, &rdata->meshBBox, rdata->shape.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, false));
        drawMesh(context, &rdata->meshBBox);
    }
}

void WgCompositor::drawStrokes(WgContext& context, WgRenderShape* rdata)
{
    if (!rdata->stroke.setting.valid || rdata->stroke.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->stroke.setting;
    auto strokeView = stageBufferViewMat[rdata->strokeViewMatIdx];
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // draw strokes to stencil (first pass)
    // setup stencil rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    // draw to stencil (first pass)
    drawMesh(context, &rdata->stroke.mesh);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid);
        drawMeshSolid(context, &rdata->stroke.bbox, rdata->stroke.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, false));
        drawMesh(context, &rdata->stroke.bbox);
    }
}

void WgCompositor::blendStrokes(WgContext& context, WgRenderShape* rdata, BlendMethod blendMethod)
{
    if (!rdata->stroke.setting.valid || rdata->stroke.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->stroke.setting;
    auto strokeView = stageBufferViewMat[rdata->strokeViewMatIdx];
    // copy current render target data to dst target
    WgRenderTarget *target = currentTarget;
    endRenderPass();
    copyTexture(&targetTemp0, target);
    beginRenderPassMS(commandEncoder, target, false);
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // draw strokes to stencil (first pass)
    // setup stencil rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    // draw to stencil (first pass)
    drawMesh(context, &rdata->stroke.mesh);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
    uint32_t blendMethodInd = (uint32_t)blendMethod;
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid_blend[blendMethodInd]);
        drawMeshSolid(context, &rdata->stroke.bbox, rdata->stroke.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 3, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientBlendPipeline(pipelines, settings.fillType, blendMethodInd));
        drawMesh(context, &rdata->stroke.bbox);
    }
};

void WgCompositor::clipStrokes(WgContext& context, WgRenderShape* rdata)
{
    if (!rdata->stroke.setting.valid || rdata->stroke.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->stroke.setting;
    auto strokeView = stageBufferViewMat[rdata->strokeViewMatIdx];
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // draw strokes to stencil (first pass)
    // setup stencil rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    // draw to stencil (first pass)
    drawMesh(context, &rdata->stroke.mesh);
    // merge depth and stencil buffer
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.merge_depth_stencil);
    drawMesh(context, &rdata->meshBBox);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid);
        drawMeshSolid(context, &rdata->stroke.bbox, rdata->stroke.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, false));
        drawMesh(context, &rdata->stroke.bbox);
    }
}

void WgCompositor::drawImage(WgContext& context, WgRenderPicture* rdata)
{
    if (rdata->viewport.invalid() || !rdata->imageBindGroup) return;
    WgRenderSettings& settings = rdata->renderSettings;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // draw stencil
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    drawMeshImage(context, &rdata->meshData);
    // draw image
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, rdata->imageBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.image);
    drawMeshImage(context, &rdata->meshData);
}

void WgCompositor::blendImage(WgContext& context, WgRenderPicture* rdata, BlendMethod blendMethod)
{
    if (rdata->viewport.invalid() || !rdata->imageBindGroup) return;
    WgRenderSettings& settings = rdata->renderSettings;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // copy current render target data to dst target
    WgRenderTarget *target = currentTarget;
    endRenderPass();
    copyTexture(&targetTemp0, target);
    beginRenderPassMS(commandEncoder, target, false);
    // setup stencil rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    drawMeshImage(context, &rdata->meshData);
    // blend image
    uint32_t blendMethodInd = (uint32_t)blendMethod;
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, rdata->imageBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 3, targetTemp0.bindGroupTexture, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.image_blend[blendMethodInd]);
    drawMeshImage(context, &rdata->meshData);
};

void WgCompositor::clipImage(WgContext& context, WgRenderPicture* rdata)
{
    if (rdata->viewport.invalid() || !rdata->imageBindGroup) return;
    WgRenderSettings& settings = rdata->renderSettings;
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // setup stencil rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    drawMeshImage(context, &rdata->meshData);
    // merge depth and stencil buffer
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.merge_depth_stencil);
    drawMeshImage(context, &rdata->meshData);
    // draw image
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, rdata->imageBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.image);
    drawMeshImage(context, &rdata->meshData);
}


void WgCompositor::drawScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose)
{
    // draw scene
    RenderRegion rect = shrinkRenderRegion(compose->aabb);
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rect.x(), rect.y(), rect.w(), rect.h());
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, scene->bindGroupTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[compose->opacity], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.scene);
    drawMeshImage(context, &meshDataBlit);
}


void WgCompositor::blendScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose)
{
    // copy current render target data to dst target
    WgRenderTarget *target = currentTarget;
    endRenderPass();
    copyTexture(&targetTemp0, target);
    beginRenderPassMS(commandEncoder, target, false);
    // blend scene
    uint32_t blendMethodInd = (uint32_t)compose->blend;
    RenderRegion rect = shrinkRenderRegion(compose->aabb);
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rect.x(), rect.y(), rect.w(), rect.h());
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, scene->bindGroupTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp0.bindGroupTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, bindGroupOpacities[compose->opacity], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.scene_blend[blendMethodInd]);
    drawMeshImage(context, &meshDataBlit);
}

void WgCompositor::markupClipPath(WgContext& context, WgRenderShape* rdata)
{
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rdata->viewport.x(), rdata->viewport.y(), rdata->viewport.w(), rdata->viewport.h());
    // markup stencil
    if (rdata->stroke.mesh.vbuffer.count > 0) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, stageBufferViewMat[rdata->strokeViewMatIdx], 0, nullptr);
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
        drawMesh(context, &rdata->stroke.mesh);
    } else if (rdata->shape.mesh.vbuffer.count > 0) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        WGPURenderPipeline stencilPipeline = (rdata->fillRule == FillRule::NonZero) ? pipelines.nonzero : pipelines.evenodd;
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, stencilPipeline);
        drawMesh(context, &rdata->shape.mesh);
    }
}

void WgCompositor::renderClipPath(WgContext& context, WgRenderPaint* paint)
{
    // reset scissor recr to full screen
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, width, height);
    auto rdata0 = (WgRenderShape*)paint->clips[0];
    // markup stencil
    markupClipPath(context, rdata0);
    // copy stencil to depth
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copy_stencil_to_depth);
    drawMesh(context, &rdata0->meshBBox);
    // merge clip paths with AND logic
    for (auto p = paint->clips.begin() + 1; p < paint->clips.end(); ++p) {
        auto rdata = (WgRenderShape*)(*p);
        // markup stencil
        markupClipPath(context, rdata);
        // copy stencil to depth (clear stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[190], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copy_stencil_to_depth_interm);
        drawMesh(context, &rdata->meshBBox);
        // copy depth to stencil
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 1);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[190], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copy_depth_to_stencil);
        drawMesh(context, &rdata->meshBBox);
        // clear depth current (keep stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[255], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.clear_depth);
        drawMesh(context, &rdata->meshBBox);
        // clear depth original (keep stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[255], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.clear_depth);
        drawMesh(context, &rdata0->meshBBox);
        // copy stencil to depth (clear stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copy_stencil_to_depth);
        drawMesh(context, &rdata->meshBBox);
    }
}

void WgCompositor::clearClipPath(WgContext& context, WgRenderPaint* paint)
{
    // reset scissor recr to full screen
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, width, height);
    // get render data
    ARRAY_FOREACH(p, paint->clips) {
        WgRenderShape* rdata = (WgRenderShape*)(*p);
        // set transformations
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[255], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.clear_depth);
        drawMesh(context, &rdata->meshBBox);
    }
}


bool WgCompositor::gaussianBlur(WgContext& context, WgRenderTarget* dst, const RenderEffectGaussianBlur* params, const WgCompose* compose)
{
    auto effectParams = (WgRenderEffectParams*)params->rd;
    auto aabb = shrinkRenderRegion(compose->aabb);

    copyTexture(&targetTemp0, dst);
    if (params->direction == 0) { // both
        beginRenderPass(commandEncoder, &targetTemp0); {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, dst->bindGroupTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.gaussian_horz);
            drawMeshImage(context, &meshDataBlit);
        } endRenderPass();
        beginRenderPass(commandEncoder, dst); {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.gaussian_vert);
            drawMeshImage(context, &meshDataBlit);
        } endRenderPass();
    } else if (params->direction == 1) { // horizontal
        beginRenderPass(commandEncoder, dst); {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.gaussian_horz);
            drawMeshImage(context, &meshDataBlit);
        } endRenderPass();
    } else if (params->direction == 2) { // vertical
        beginRenderPass(commandEncoder, dst); {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.gaussian_vert);
            drawMeshImage(context, &meshDataBlit);
        } endRenderPass();
    }

    return true;
}


bool WgCompositor::dropShadow(WgContext& context, WgRenderTarget* dst, const RenderEffectDropShadow* params, const WgCompose* compose)
{
    auto effectParams = (WgRenderEffectParams*)params->rd;
    auto aabb = shrinkRenderRegion(compose->aabb);

    copyTexture(&targetTemp0, dst);
    copyTexture(&targetTemp1, dst);
    if (!tvg::zero(params->sigma)) {
        // horizontal
        beginRenderPass(commandEncoder, &targetTemp0); {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, dst->bindGroupTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.gaussian_horz);
            drawMeshImage(context, &meshDataBlit);
        } endRenderPass();
        // vertical
        beginRenderPass(commandEncoder, &targetTemp1); {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.gaussian_vert);
            drawMeshImage(context, &meshDataBlit);
        } endRenderPass();
    }
    // drop shadow
    copyTexture(&targetTemp0, dst, aabb);
    beginRenderPass(commandEncoder, dst); {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp1.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, effectParams->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.dropshadow);
        drawMeshImage(context, &meshDataBlit);
    } endRenderPass();
    return true;
}


bool WgCompositor::fillEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectFill* params, const WgCompose* compose)
{
    auto effectParams = (WgRenderEffectParams*)params->rd;
    auto aabb = shrinkRenderRegion(compose->aabb);

    copyTexture(&targetTemp0, dst, aabb);
    beginRenderPass(commandEncoder, dst); {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.fill_effect);
        drawMeshImage(context, &meshDataBlit);
    } endRenderPass();

    return true;
}


bool WgCompositor::tintEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectTint* params, const WgCompose* compose)
{
    auto effectParams = (WgRenderEffectParams*)params->rd;
    auto aabb = shrinkRenderRegion(compose->aabb);

    copyTexture(&targetTemp0, dst, aabb);
    beginRenderPass(commandEncoder, dst); {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.tint_effect);
        drawMeshImage(context, &meshDataBlit);
    } endRenderPass();

    return true;
}

bool WgCompositor::tritoneEffect(WgContext& context, WgRenderTarget* dst, const RenderEffectTritone* params, const WgCompose* compose)
{
    auto effectParams = (WgRenderEffectParams*)params->rd;
    auto aabb = shrinkRenderRegion(compose->aabb);

    copyTexture(&targetTemp0, dst, aabb);
    beginRenderPass(commandEncoder, dst); {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bindGroupTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effectParams->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.tritone_effect);
        drawMeshImage(context, &meshDataBlit);
    } endRenderPass();

    return true;
}
