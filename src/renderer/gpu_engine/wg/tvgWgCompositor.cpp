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

#include <cassert>
#include <algorithm>
#include "tvgWgCompositor.h"

static WGPURenderPipeline _gradientPipeline(const WgPipelines& pipelines, WgRenderSettingsType type, bool convex)
{
    switch (type) {
        case WgRenderSettingsType::Linear: return convex ? pipelines.linearConv : pipelines.linear;
        case WgRenderSettingsType::Radial: return convex ? pipelines.radialConv : pipelines.radial;
        case WgRenderSettingsType::Conic: return convex ? pipelines.conicConv : pipelines.conic;
        default: return nullptr;
    }
}


static WGPURenderPipeline _gradientBlendPipeline(WgPipelines& pipelines, WgContext& context, WgRenderSettingsType type, BlendMethod blendMethod)
{
    switch (type) {
        case WgRenderSettingsType::Linear: return pipelines.linearBlend(context, blendMethod);
        case WgRenderSettingsType::Radial: return pipelines.radialBlend(context, blendMethod);
        case WgRenderSettingsType::Conic: return pipelines.conicBlend(context, blendMethod);
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
    if (this->width != width || this->height != height) {
        context.layouts.releaseBindGroup(bindGroupStorageTemp);
        // release intermediate render target
        targetTemp1.release(context);
        targetTemp0.release(context);
        ARRAY_FOREACH(attachment, depthStencilAttachments) {
            context.releaseTextureView(attachment->view);
            context.releaseTexture(attachment->texture);
        }
        depthStencilAttachments.clear();
    }

    this->width = width;
    this->height = height;

    // create render targets handles
    if (width > 0 && height > 0) {
        // update global view matrix handles
        updateViewMat(context, width, height);
        // initialize intermediate render targets
        targetTemp0.initialize(context, width, height);
        targetTemp1.initialize(context, width, height);
        bindGroupStorageTemp = context.layouts.createBindGroupStrorage2RO(targetTemp0.texView, targetTemp1.texView);
    }
}

void WgCompositor::setScissorRegion(const RenderRegion& region)
{
    const auto& bounds = currentTargetState->bounds;
    const RenderRegion rect{
        tvg::clamp(region.xMin, bounds.xMin, bounds.xMax) - bounds.xMin, tvg::clamp(region.yMin, bounds.yMin, bounds.yMax) - bounds.yMin,
        tvg::clamp(region.xMax, bounds.xMin, bounds.xMax) - bounds.xMin, tvg::clamp(region.yMax, bounds.yMin, bounds.yMax) - bounds.yMin};
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, rect.x(), rect.y(), rect.w(), rect.h());
}

WGPUTextureView WgCompositor::getDepthStencilView(WgContext& context, uint32_t width, uint32_t height, uint32_t sampleCount)
{
    ARRAY_FOREACH(attachment, depthStencilAttachments) {
        if (attachment->width == width && attachment->height == height && attachment->sampleCount == sampleCount) return attachment->view;
    }
    auto texture = context.createTexAttachement(width, height, WGPUTextureFormat_Depth24PlusStencil8, sampleCount);
    auto view = context.createTextureView(texture);
    // TODO: Add an eviction policy to release unused cached resources.
    depthStencilAttachments.push({width, height, sampleCount, texture, view});
    return view;
}

void WgCompositor::copyTexture(const WgRenderTarget* dst, const WgRenderTarget* src)
{
    const WGPUTexelCopyTextureInfo texSrc{.texture = src->texture};
    const WGPUTexelCopyTextureInfo texDst{.texture = dst->texture};
    const WGPUExtent3D copySize{.width = src->width, .height = src->height, .depthOrArrayLayers = 1};
    wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &texSrc, &texDst, &copySize);
}

void WgCompositor::beginRenderPassMS(WgContext& context, WGPUCommandEncoder commandEncoder, const WgRenderTargetState& targetState, bool clearBuffer)
{
    auto target = targetState.target;
    if (renderPassEncoder && currentTargetState == &targetState && !clearBuffer) return;
    // we must to end render pass first
    endRenderPass();
    currentTargetState = &targetState;
    // start new render pass
    this->commandEncoder = commandEncoder;
    const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
        .view = getDepthStencilView(context, target->width, target->height, 4),
        .depthLoadOp = WGPULoadOp_Clear,
        .depthStoreOp = WGPUStoreOp_Discard,
        .depthClearValue = 1.0f,
        .stencilLoadOp = WGPULoadOp_Clear,
        .stencilStoreOp = WGPUStoreOp_Discard,
        .stencilClearValue = 0};
    const WGPURenderPassColorAttachment colorAttachment{
        .view = target->texViewMS,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .resolveTarget = target->texView,
        .loadOp = clearBuffer ? WGPULoadOp_Clear : WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {0.0, 0.0, 0.0, 0.0}};
    WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
    assert(renderPassEncoder);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, targetState.transformOffset, 4 * sizeof(float));
}

void WgCompositor::beginRenderPass(WgContext& context, WGPUCommandEncoder encoder, WgRenderTarget* target, bool clearBuffer)
{
    assert(!renderPassEncoder);
    currentTargetState = nullptr;
    const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
        .view = getDepthStencilView(context, target->width, target->height, 1),
        .depthLoadOp = WGPULoadOp_Load,
        .depthStoreOp = WGPUStoreOp_Discard,
        .stencilLoadOp = WGPULoadOp_Load,
        .stencilStoreOp = WGPUStoreOp_Discard};
    const WGPURenderPassColorAttachment colorAttachment{
        .view = target->texView,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .loadOp = clearBuffer ? WGPULoadOp_Clear : WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
    };
    const WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    renderPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    // Native backends require even the unused vertex slot to be bound.
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, 0, 4 * sizeof(float));
}


void WgCompositor::endRenderPass()
{
    if (renderPassEncoder) {
        wgpuRenderPassEncoderEnd(renderPassEncoder);
        wgpuRenderPassEncoderRelease(renderPassEncoder);
        renderPassEncoder = nullptr;
    }
}

void WgCompositor::reset(WgContext& context)
{
    currentTargetState = nullptr;
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

void WgCompositor::requestQuad(WgMesh& mesh, const WgRenderTargetState& src, const WgRenderTargetState& dst, const WgRenderTargetState* mask)
{
    mesh.blitBox();
    if (mask) {
        mesh.tbuffer.reserve(8);
        mesh.tbuffer.count = 8;
    }
    for (uint32_t i = 0; i < 4; ++i) {
        auto uv = mesh.tbuffer[i];
        auto x = src.bounds.xMin + uv.x * src.bounds.w();
        auto y = src.bounds.yMin + uv.y * src.bounds.h();
        mesh.vbuffer[i] = {(x - dst.bounds.xMin) * 2.0f / dst.target->width - 1.0f,
                           1.0f - (y - dst.bounds.yMin) * 2.0f / dst.target->height};
        mesh.tbuffer[i] = {uv.x * src.bounds.w() / src.target->width, uv.y * src.bounds.h() / src.target->height};
        if (mask) mesh.tbuffer[i + 4] = {(x - mask->bounds.xMin) / mask->target->width, (y - mask->bounds.yMin) / mask->target->height};
    }
    stageBufferGeometry.append(&mesh);
}

void WgCompositor::requestScene(WgRenderTargetState& targetState, WgMesh& mesh, WgMesh& effectMesh, const WgRenderTargetState* dst, const WgRenderTargetState* mask, bool effects)
{
    // Keep paint/stroke matrices destination-independent so shared clips remain reusable.
    const float adjust[] = {float(width) / targetState.target->width, float(height) / targetState.target->height,
                            (float(width) - 2.0f * targetState.bounds.xMin) / targetState.target->width - 1.0f,
                            1.0f - (float(height) - 2.0f * targetState.bounds.yMin) / targetState.target->height};
    targetState.transformOffset = stageBufferGeometry.vbuffer.count;
    stageBufferGeometry.vbuffer.grow(sizeof(adjust));
    memcpy(stageBufferGeometry.vbuffer.data + targetState.transformOffset, adjust, sizeof(adjust));
    stageBufferGeometry.vbuffer.count += sizeof(adjust);
    if (dst) requestQuad(mesh, targetState, *dst, mask);
    if (effects) {
        WgRenderTargetState scratch{&targetTemp0, {0, 0, int32_t(width), int32_t(height)}};
        requestQuad(effectMesh, scratch, targetState);
    }
}

void WgCompositor::copyEffectTexture(WgContext& context, WgRenderTarget* dst, const WgRenderTargetState& src, bool clear)
{
    if (clear) {
        beginRenderPass(context, commandEncoder, dst, true);
        endRenderPass();
    }
    const WGPUTexelCopyTextureInfo texSrc{.texture = src.target->texture};
    const WGPUTexelCopyTextureInfo texDst{.texture = dst->texture, .origin = {.x = src.bounds.x(), .y = src.bounds.y()}};
    const WGPUExtent3D size{.width = src.bounds.w(), .height = src.bounds.h(), .depthOrArrayLayers = 1};
    wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &texSrc, &texDst, &size);
}

void WgCompositor::requestShape(WgShape* shape)
{
    stageBufferGeometry.append(shape);

    auto& shapeSettings = shape->shape.setting;
    auto& shapeSolid = shape->shape.solid;
    if (shapeSettings.fillType == WgRenderSettingsType::Solid) shapeSolid.colorIdx = stageBufferSolidColor.append(shapeSolid.packedColor());
    else shapeSettings.bindGroupIdx = stageBufferPaint.append(shapeSettings.settings);

    if (!shape->stroke.mesh.vbuffer.empty()) {
        Matrix viewMatrix{2.0f / width, 0.0f, -1.0f, 0.0f, -2.0f / height, 1.0f, 0.0f, 0.0f, 1.0f};
        WgShaderTypeMat4x4fBlock strokeViewMat{{viewMatrix * shape->transform}, {}};
        // Test the clip depth during the stroke color draw.
        strokeViewMat.matrix.mat[14] = 128.0f / 255.0f;
        shape->strokeViewMatIdx = stageBufferViewMat.append(strokeViewMat);
    }

    if (shape->stroke.setting.valid && !shape->stroke.mesh.vbuffer.empty()) {
        auto& strokeSettings = shape->stroke.setting;
        auto& strokeSolid = shape->stroke.solid;
        if (strokeSettings.fillType == WgRenderSettingsType::Solid) strokeSolid.colorIdx = stageBufferSolidColor.append(strokeSolid.packedColor());
        else strokeSettings.bindGroupIdx = stageBufferPaint.append(strokeSettings.settings);
    }
    ARRAY_FOREACH(p, shape->clips) {
        requestShape((WgShape*)(*p));
    }
}

void WgCompositor::requestImage(WgImage* image)
{
    stageBufferGeometry.append(image);
    image->setting.bindGroupIdx = stageBufferPaint.append(image->setting.settings);
    ARRAY_FOREACH(p, image->clips) {
        requestShape((WgShape*)(*p));
    }
}

void WgCompositor::requestSolidBatch(const Array<WgShape*>& renderShapes, WgSolidBatchRange& range)
{
    stageBufferGeometry.appendSolidBatch(renderShapes, stageBufferSolidColor, range);
    range.viewport = renderShapes[0]->viewport;
}

void WgCompositor::requestImageBatch(const Array<WgImage*>& renderImages, WgImageBatchRange& range)
{
    stageBufferGeometry.appendImageBatch(renderImages, range);
    auto image = renderImages[0];
    image->setting.bindGroupIdx = stageBufferPaint.append(image->setting.settings);
    range.viewport = image->viewport;
}

void WgCompositor::requestStencilBatch(const Array<WgShape*>& renderShapes, WgStencilBatchRange& range)
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
        const auto count = rdata->bboxMesh.vbuffer.count;

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

void WgCompositor::renderShape(WgContext& context, WgShape* rdata, BlendMethod blendMethod)
{
    // apply clip path if necessary
    if (!rdata->clips.empty()) {
        renderClipPath(context, rdata);
        if (rdata->strokeFirst) {
            drawStrokes(context, rdata);
            clipShape(context, rdata);
        } else {
            clipShape(context, rdata);
            drawStrokes(context, rdata);
        }
        clearClipPath(context, rdata);
    // use custom blending
    } else if (blendMethod != BlendMethod::Normal) {
        // TODO: Custom-blended shapes always use a stencil pass, even when convex.
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

    setScissorRegion(range.viewport);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solidBatch);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, range.vertexOffset, vertexSize);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 2, stageBufferSolidColor.vbuffer_gpu, range.colorOffset, colorSize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, range.indexOffset, indexSize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, range.indexCount, 1, 0, 0, 0);
}

void WgCompositor::renderImageBatch(WgImage* image, const WgImageBatchRange& range)
{
    const uint64_t vertexSize = static_cast<uint64_t>(range.vertexCount) * sizeof(Point);
    const uint64_t indexSize = static_cast<uint64_t>(range.indexCount) * sizeof(uint32_t);

    setScissorRegion(range.viewport);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[image->setting.bindGroupIdx], 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, image->bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.imageDirect);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, range.vertexOffset, vertexSize);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 2, stageBufferGeometry.vbuffer_gpu, range.texCoordOffset, vertexSize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, range.indexOffset, indexSize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, range.indexCount, 1, 0, 0, 0);
}

void WgCompositor::renderStencilBatch(const Array<WgShape*>& renderShapes, const WgStencilBatchRange& range)
{
    const uint64_t stencilVertexSize = static_cast<uint64_t>(range.stencil.vertexCount) * sizeof(Point);
    const uint64_t stencilIndexSize = static_cast<uint64_t>(range.stencil.indexCount) * sizeof(uint32_t);
    const uint64_t coverVertexSize = static_cast<uint64_t>(range.cover.vertexCount) * sizeof(Point);
    const uint64_t coverIndexSize = static_cast<uint64_t>(range.cover.indexCount) * sizeof(uint32_t);

    setScissorRegion(range.viewport);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, range.fillRule == FillRule::NonZero ? pipelines.nonzero : pipelines.evenodd);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, range.stencil.vertexOffset, stencilVertexSize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, range.stencil.indexOffset, stencilIndexSize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, range.stencil.indexCount, 1, 0, 0, 0);

    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, range.cover.vertexOffset, coverVertexSize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, range.cover.indexOffset, coverIndexSize);

    // Keep the common all-solid case identical to the original two-draw path.
    if (range.solidOnly) {
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solidStencilBatch);
        wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 2, stageBufferSolidColor.vbuffer_gpu, range.colorOffset, static_cast<uint64_t>(range.cover.vertexCount) * sizeof(RenderColor));
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
            const auto indexCount = rdata->bboxMesh.ibuffer.count;
            const uint64_t nextIndex = static_cast<uint64_t>(firstIndex) + indexCount;
            firstIndex = static_cast<uint32_t>(nextIndex);
            if (++p == end || settings.fillType != WgRenderSettingsType::Solid) break;
            rdata = *p;
        } while (rdata->shape.setting.fillType == WgRenderSettingsType::Solid);

        if (settings.fillType == WgRenderSettingsType::Solid) {
            if (!colorsBound) {
                wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 2, stageBufferSolidColor.vbuffer_gpu, range.colorOffset, static_cast<uint64_t>(range.cover.vertexCount) * sizeof(RenderColor));
                colorsBound = true;
            }
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solidStencilBatch);
        } else {
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, false));
        }
        wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, firstIndex - batchFirstIndex, 1, batchFirstIndex, 0, 0);
    }
}

void WgCompositor::renderImage(WgContext& context, WgImage* rdata, BlendMethod blendMethod)
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

void WgCompositor::renderScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose, WgMesh& mesh)
{
    // use custom blending
    if (compose->blend != BlendMethod::Normal)
        blendScene(context, scene, compose, mesh);
    // use direct hardware blending
    else drawScene(context, scene, compose, mesh);
}

void WgCompositor::composeScene(WgContext& context, WgRenderTarget* src, WgRenderTarget* mask, WgCompose* cmp, WgMesh& mesh)
{
    setScissorRegion(cmp->aabb);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, src->bgTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, mask->bgTexture, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.sceneCompose[(uint32_t)cmp->method]);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 3, stageBufferGeometry.vbuffer_gpu, mesh.toffset + 4 * sizeof(Point), 4 * sizeof(Point));
    drawMeshImage(context, &mesh);
}

void WgCompositor::blit(WgContext& context, WGPUCommandEncoder encoder, WgRenderTarget* src, WGPUTextureView dstView, bool premultiplied, bool clearBuffer)
{
    const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
        .view = getDepthStencilView(context, src->width, src->height, 1),
        .depthLoadOp = WGPULoadOp_Load,
        .depthStoreOp = WGPUStoreOp_Discard,
        .stencilLoadOp = WGPULoadOp_Load,
        .stencilStoreOp = WGPUStoreOp_Discard};
    const WGPURenderPassColorAttachment colorAttachment{
        .view = dstView,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .loadOp = clearBuffer ? WGPULoadOp_Clear : WGPULoadOp_Load,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {0.0, 0.0, 0.0, 0.0}};
    const WGPURenderPassDescriptor renderPassDesc{ .colorAttachmentCount = 1, .colorAttachments = &colorAttachment, .depthStencilAttachment = &depthStencilAttachment };
    renderPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, stageBufferGeometry.vbuffer_gpu, 0, 4 * sizeof(float));
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, src->bgTexture, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, premultiplied ? pipelines.blit : pipelines.blitUnpremultiplied);
    drawMeshImage(context, &meshDataBlit);
    wgpuRenderPassEncoderEnd(renderPassEncoder);
    wgpuRenderPassEncoderRelease(renderPassEncoder);
    renderPassEncoder = nullptr;
}


void WgCompositor::drawMesh(WgContext& context, WgMesh* meshData)
{
    uint64_t icount = meshData->ibuffer.count;
    uint64_t vsize = meshData->vbuffer.count * sizeof(Point);
    uint64_t isize = icount * sizeof(uint32_t);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, meshData->voffset, vsize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, meshData->ioffset, isize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, icount, 1, 0, 0, 0);
};


void WgCompositor::drawMeshSolid(WgContext& context, WgMesh* meshData, uint32_t solidColorInd)
{
    const uint64_t icount = meshData->ibuffer.count;
    const uint64_t vsize = meshData->vbuffer.count * sizeof(Point);
    const uint64_t isize = icount * sizeof(uint32_t);
    const uint64_t csize = sizeof(RenderColor);
    const uint64_t coffset = solidColorInd * csize;
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, meshData->voffset, vsize);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 2, stageBufferSolidColor.vbuffer_gpu, coffset, csize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, meshData->ioffset, isize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, icount, 1, 0, 0, 0);
}


void WgCompositor::drawMeshImage(WgContext& context, WgMesh* meshData)
{
    uint64_t icount = meshData->ibuffer.count;
    uint64_t vsize = meshData->vbuffer.count * sizeof(Point);
    uint64_t isize = icount * sizeof(uint32_t);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, stageBufferGeometry.vbuffer_gpu, meshData->voffset, vsize);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 2, stageBufferGeometry.vbuffer_gpu, meshData->toffset, vsize);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, stageBufferGeometry.ibuffer_gpu, WGPUIndexFormat_Uint32, meshData->ioffset, isize);
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, icount, 1, 0, 0, 0);
};

void WgCompositor::drawShape(WgContext& context, WgShape* rdata)
{
    if (!rdata->shape.setting.valid || rdata->shape.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    auto& settings = rdata->shape.setting;
    auto convex = rdata->convex;
    auto mesh = rdata->convex ? &rdata->shape.mesh : &rdata->bboxMesh;

    setScissorRegion(rdata->viewport);

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
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, convex ? pipelines.solidConv : pipelines.solid);
        drawMeshSolid(context, mesh, rdata->shape.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, convex));
        drawMesh(context, mesh);
    }
}

void WgCompositor::blendShape(WgContext& context, WgShape* rdata, BlendMethod blendMethod)
{
    if (!rdata->shape.setting.valid || rdata->shape.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->shape.setting;
    // copy current render target data to dst target
    endRenderPass();
    copyTexture(&targetTemp0, currentTargetState->target);
    beginRenderPassMS(context, commandEncoder, *currentTargetState, false);
    // render shape with blend settings
    setScissorRegion(rdata->viewport);
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
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solidBlend(context, blendMethod));
        drawMeshSolid(context, &rdata->bboxMesh, rdata->shape.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 3, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientBlendPipeline(pipelines, context, settings.fillType, blendMethod));
        drawMesh(context, &rdata->bboxMesh);
    }
}

void WgCompositor::clipShape(WgContext& context, WgShape* rdata)
{
    if (!rdata->shape.setting.valid || rdata->shape.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->shape.setting;
    setScissorRegion(rdata->viewport);
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
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.mergeDepthStencil);
    drawMesh(context, &rdata->bboxMesh);
    // setup fill rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    if (settings.fillType == WgRenderSettingsType::Solid) {
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solid);
        drawMeshSolid(context, &rdata->bboxMesh, rdata->shape.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientPipeline(pipelines, settings.fillType, false));
        drawMesh(context, &rdata->bboxMesh);
    }
}

void WgCompositor::drawStrokes(WgContext& context, WgShape* rdata)
{
    if (!rdata->stroke.setting.valid || rdata->stroke.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    auto& settings = rdata->stroke.setting;
    auto solid = settings.fillType == WgRenderSettingsType::Solid;
    auto direct = rdata->strokeOverlapFree || (rdata->stroke.solid.opacity == 255 && (solid ? rdata->stroke.solid.color.a == 255 : settings.opaque));
    auto clipped = !rdata->clips.empty();
    auto strokeView = stageBufferViewMat[rdata->strokeViewMatIdx];
    setScissorRegion(rdata->viewport);

    auto mesh = &rdata->stroke.mesh;
    WGPURenderPipeline pipeline;
    if (direct) {
        // Draw eligible stroke geometry with stencil tests and writes disabled.
        pipeline = clipped ? pipelines.clippedStroke(context, settings.fillType) : (solid ? pipelines.solidConv : _gradientPipeline(pipelines, settings.fillType, true));
    } else {
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
        drawMesh(context, mesh);
        if (clipped) {
            wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.mergeDepthStencil);
            drawMesh(context, &rdata->bboxMesh);
        }
        mesh = &rdata->stroke.bboxMesh;
        pipeline = solid ? pipelines.solid : _gradientPipeline(pipelines, settings.fillType, false);
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    }
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, strokeView, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipeline);
    if (solid) {
        drawMeshSolid(context, mesh, rdata->stroke.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        drawMesh(context, mesh);
    }
}

void WgCompositor::blendStrokes(WgContext& context, WgShape* rdata, BlendMethod blendMethod)
{
    if (!rdata->stroke.setting.valid || rdata->stroke.mesh.vbuffer.empty() || rdata->viewport.invalid()) return;
    WgRenderSettings& settings = rdata->stroke.setting;
    auto strokeView = stageBufferViewMat[rdata->strokeViewMatIdx];
    // copy current render target data to dst target
    endRenderPass();
    copyTexture(&targetTemp0, currentTargetState->target);
    beginRenderPassMS(context, commandEncoder, *currentTargetState, false);
    setScissorRegion(rdata->viewport);
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
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.solidBlend(context, blendMethod));
        drawMeshSolid(context, &rdata->stroke.bboxMesh, rdata->stroke.solid.colorIdx);
    } else {
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, settings.gradientData.bindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 3, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, _gradientBlendPipeline(pipelines, context, settings.fillType, blendMethod));
        drawMesh(context, &rdata->stroke.bboxMesh);
    }
};

void WgCompositor::drawImage(WgContext& context, WgImage* rdata)
{
    if (rdata->viewport.invalid() || !rdata->bindGroup) return;
    WgRenderSettings& settings = rdata->setting;
    setScissorRegion(rdata->viewport);
    // draw image
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, rdata->bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.imageDirect);
    drawMeshImage(context, &rdata->mesh);
}

void WgCompositor::blendImage(WgContext& context, WgImage* rdata, BlendMethod blendMethod)
{
    if (rdata->viewport.invalid() || !rdata->bindGroup) return;
    WgRenderSettings& settings = rdata->setting;
    // copy current render target data to dst target
    endRenderPass();
    copyTexture(&targetTemp0, currentTargetState->target);
    beginRenderPassMS(context, commandEncoder, *currentTargetState, false);
    setScissorRegion(rdata->viewport);
    // setup stencil rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    drawMeshImage(context, &rdata->mesh);
    // blend image
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, rdata->bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 3, targetTemp0.bgTexture, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.imageBlend(context, blendMethod));
    drawMeshImage(context, &rdata->mesh);
};

void WgCompositor::clipImage(WgContext& context, WgImage* rdata)
{
    if (rdata->viewport.invalid() || !rdata->bindGroup) return;
    WgRenderSettings& settings = rdata->setting;
    setScissorRegion(rdata->viewport);
    // setup stencil rules
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.direct);
    drawMeshImage(context, &rdata->mesh);
    // merge depth and stencil buffer
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.mergeDepthStencil);
    drawMeshImage(context, &rdata->mesh);
    // draw image
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, stageBufferPaint[settings.bindGroupIdx], 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, rdata->bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.image);
    drawMeshImage(context, &rdata->mesh);
}

void WgCompositor::drawScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose, WgMesh& mesh)
{
    // draw scene
    setScissorRegion(compose->aabb);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, scene->bgTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[compose->opacity], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.scene);
    drawMeshImage(context, &mesh);
}

void WgCompositor::blendScene(WgContext& context, WgRenderTarget* scene, WgCompose* compose, WgMesh& mesh)
{
    // copy current render target data to dst target
    endRenderPass();
    copyTexture(&targetTemp0, currentTargetState->target);
    beginRenderPassMS(context, commandEncoder, *currentTargetState, false);
    // blend scene
    setScissorRegion(compose->aabb);
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, scene->bgTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp0.bgTexture, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, bindGroupOpacities[compose->opacity], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.sceneBlend(context, compose->blend));
    drawMeshImage(context, &mesh);
}

void WgCompositor::markupClipPath(WgContext& context, WgShape* rdata)
{
    setScissorRegion(rdata->viewport);
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

void WgCompositor::renderClipPath(WgContext& context, WgPaint* paint)
{
    auto rdata0 = (WgShape*)paint->clips[0];
    // markup stencil
    markupClipPath(context, rdata0);
    // copy stencil to depth
    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copyStencilToDepth);
    drawMesh(context, &rdata0->bboxMesh);
    // merge clip paths with AND logic
    for (auto p = paint->clips.begin() + 1; p < paint->clips.end(); ++p) {
        auto rdata = (WgShape*)(*p);
        // markup stencil
        markupClipPath(context, rdata);
        // copy stencil to depth (clear stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[190], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copyStencilToDepthInterm);
        drawMesh(context, &rdata->bboxMesh);
        // copy depth to stencil
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 1);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[190], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copyDepthToStencil);
        drawMesh(context, &rdata->bboxMesh);
        // clear depth current (keep stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[255], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.clearDepth);
        drawMesh(context, &rdata->bboxMesh);
        // clear depth original (keep stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[255], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.clearDepth);
        drawMesh(context, &rdata0->bboxMesh);
        // copy stencil to depth (clear stencil)
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[128], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.copyStencilToDepth);
        drawMesh(context, &rdata->bboxMesh);
    }
}

void WgCompositor::clearClipPath(WgContext& context, WgPaint* paint)
{
    wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, currentTargetState->bounds.w(), currentTargetState->bounds.h());
    // get render data
    ARRAY_FOREACH(p, paint->clips) {
        WgShape* rdata = (WgShape*)(*p);
        // set transformations
        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, bindGroupViewMat, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroupOpacities[255], 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.clearDepth);
        drawMesh(context, &rdata->bboxMesh);
    }
}

bool WgCompositor::motionBlur(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectMotionBlur* motion, WgMesh& mesh)
{
    auto effect = (WgRenderEffect*)motion->rd;
    auto aabb = targetState.bounds;
    auto dst = targetState.target;

    // The final clipped region is only known at composition time.
    // Clamp bilinear sampling to this region within the full-sized texture.
    float region[4] = {float(aabb.x()), float(aabb.y()), float(aabb.w()), float(aabb.h())};
    wgpuQueueWriteBuffer(context.queue, effect->bufferParams, 4 * sizeof(float), region, sizeof(region));

    copyEffectTexture(context, &targetTemp0, targetState);
    beginRenderPass(context, commandEncoder, dst);
    {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.getBindGroupTextureLinear(context), 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectMotionBlur);
        drawMeshImage(context, &mesh);
    }
    endRenderPass();

    return true;
}

bool WgCompositor::gaussianBlur(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectGaussianBlur* blur, WgMesh& mesh)
{
    auto effect = (WgRenderEffect*)blur->rd;
    auto aabb = targetState.bounds;
    auto dst = targetState.target;

    copyEffectTexture(context, &targetTemp0, targetState, true);
    if (blur->direction == 0) {  // both
        beginRenderPass(context, commandEncoder, &targetTemp1, true);
        {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectGaussianHorz);
            drawMeshImage(context, &meshDataBlit);
        }
        endRenderPass();
        beginRenderPass(context, commandEncoder, dst);
        {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp1.bgTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectGaussianVert);
            drawMeshImage(context, &mesh);
        }
        endRenderPass();
    } else if (blur->direction == 1) {  // horizontal
        beginRenderPass(context, commandEncoder, dst);
        {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectGaussianHorz);
            drawMeshImage(context, &mesh);
        }
        endRenderPass();
    } else if (blur->direction == 2) {  // vertical
        beginRenderPass(context, commandEncoder, dst);
        {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectGaussianVert);
            drawMeshImage(context, &mesh);
        }
        endRenderPass();
    }

    return true;
}

bool WgCompositor::dropShadow(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectDropShadow* shadow, WgMesh& mesh)
{
    auto effect = (WgRenderEffect*)shadow->rd;
    auto aabb = targetState.bounds;
    auto dst = targetState.target;

    copyEffectTexture(context, &targetTemp1, targetState, true);
    if (!tvg::zero(shadow->sigma)) {
        // horizontal
        beginRenderPass(context, commandEncoder, &targetTemp0, true);
        {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp1.bgTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectGaussianHorz);
            drawMeshImage(context, &meshDataBlit);
        }
        endRenderPass();
        // vertical
        beginRenderPass(context, commandEncoder, &targetTemp1);
        {
            wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, aabb.x(), aabb.y(), aabb.w(), aabb.h());
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectGaussianVert);
            drawMeshImage(context, &meshDataBlit);
        }
        endRenderPass();
    }
    // drop shadow
    copyEffectTexture(context, &targetTemp0, targetState);
    beginRenderPass(context, commandEncoder, dst);
    {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, targetTemp1.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, effect->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectDropShadow);
        drawMeshImage(context, &mesh);
    }
    endRenderPass();
    return true;
}

bool WgCompositor::fillEffect(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectFill* fill, WgMesh& mesh)
{
    auto effect = (WgRenderEffect*)fill->rd;
    auto aabb = targetState.bounds;
    auto dst = targetState.target;

    copyEffectTexture(context, &targetTemp0, targetState);
    beginRenderPass(context, commandEncoder, dst);
    {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectFill);
        drawMeshImage(context, &mesh);
    }
    endRenderPass();

    return true;
}

bool WgCompositor::tintEffect(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectTint* tint, WgMesh& mesh)
{
    auto effect = (WgRenderEffect*)tint->rd;
    auto aabb = targetState.bounds;
    auto dst = targetState.target;

    copyEffectTexture(context, &targetTemp0, targetState);
    beginRenderPass(context, commandEncoder, dst);
    {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectTint);
        drawMeshImage(context, &mesh);
    }
    endRenderPass();

    return true;
}

bool WgCompositor::tritoneEffect(WgContext& context, const WgRenderTargetState& targetState, const RenderEffectTritone* tritone, WgMesh& mesh)
{
    auto effect = (WgRenderEffect*)tritone->rd;
    auto aabb = targetState.bounds;
    auto dst = targetState.target;

    copyEffectTexture(context, &targetTemp0, targetState);
    beginRenderPass(context, commandEncoder, dst);
    {
        wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, 0, 0, aabb.w(), aabb.h());
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, targetTemp0.bgTexture, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, effect->bindGroupParams, 0, nullptr);
        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipelines.effectTritone);
        drawMeshImage(context, &mesh);
    }
    endRenderPass();

    return true;
}
