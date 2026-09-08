
/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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
#include "tvgCommon.h"
#include "tvgMath.h"
#include "tvgWgTessellator.h"
#include "tvgWgTextureMgr.h"
#include "tvgWgShaderTypes.h"
#include "tvgWgRenderData.h"

//***********************************************************************
// WgGradientTexture
//***********************************************************************

void WgGradientTexture::update(WgContext& context, const Fill* fill)
{
    // compute gradient data
    WgShaderTypeGradientData gradientData;
    gradientData.update(fill);
    // allocate new texture handle
    auto bytesPerRow = WG_TEXTURE_GRADIENT_SIZE * sizeof(uint32_t);
    bool texHandleChanged = context.allocateTexture(texture, WG_TEXTURE_GRADIENT_SIZE, 1, WGPUTextureFormat_RGBA8Unorm, gradientData.data, bytesPerRow, bytesPerRow);
    // update texture view of texture handle was changed
    if (texHandleChanged) {
        context.releaseTextureView(textureView);
        textureView = context.createTextureView(texture);
        // get sampler by spread type
        WGPUSampler sampler = context.samplerLinearClamp;
        if (fill->spread() == FillSpread::Reflect) sampler = context.samplerLinearMirror;
        if (fill->spread() == FillSpread::Repeat) sampler = context.samplerLinearRepeat;
        // update bind group
        context.layouts.releaseBindGroup(bindGroup);
        bindGroup = context.layouts.createBindGroupTexSampled(sampler, textureView);
    }
};

void WgGradientTexture::release(WgContext& context)
{
    context.layouts.releaseBindGroup(bindGroup);
    context.releaseTextureView(textureView);
    context.releaseTexture(texture);
};

//***********************************************************************
// WgRenderSettings
//***********************************************************************

uint8_t WgRenderSettings::update(tvg::ColorSpace cs, uint8_t opacity)
{
    auto effectiveOpacity = static_cast<uint8_t>(opacity * opacityMultiplier);
    settings.options.update(cs, effectiveOpacity);
    return effectiveOpacity;
}

void WgRenderSettings::update(WgContext& context, const Fill* fill, const Matrix* transform, bool updateColorRamp)
{
    settings.gradient.update(fill, transform);
    if (updateColorRamp) gradientData.update(context, fill);
    if (fill->type() == Type::LinearGradient)
        fillType = WgRenderSettingsType::Linear;
    else if (fill->type() == Type::RadialGradient)
        fillType = WgRenderSettingsType::Radial;
};


void WgRenderSettings::release(WgContext& context)
{
    gradientData.release(context);
};

//***********************************************************************
// WgPaint
//***********************************************************************

void WgPaint::update(const Array<RenderData>& clips)
{
    this->clips.clear();
    // RenderData == WgPaint*, just copy it.
    this->clips = *((Array<WgPaint*>*)&clips);
}

//***********************************************************************
// WgShape
//***********************************************************************

void WgShape::updateBBox(const BBox& bb)
{
    bbox.min = tvg::min(bbox.min, bb.min);
    bbox.max = tvg::max(bbox.max, bb.max);
}

void WgShape::update(const RenderShape& rshape, const RenderRegion& vport, uint8_t shapeOpacity, uint8_t strokeOpacity, uint8_t opacity)
{
    shape.setting.valid = rshape.fill || (rshape.color.a * opacity > 0);
    stroke.setting.valid = rshape.stroke && (rshape.stroke->fill || (rshape.stroke->color.a * opacity > 0));

    shape.solid.opacity = shapeOpacity;
    stroke.solid.opacity = strokeOpacity;

    fillRule = rshape.rule;
    viewport = vport;
}

void WgShape::update(const RenderShape& rshape, const Matrix& transform, RenderUpdateFlag flag)
{
    reset();  // Optimize: bad idea to reset meshes always. it could re-use the meshes if there haven't been any path changes.

    convex = false;
    strokeFirst = rshape.strokeFirst();
    shape.setting.opacityMultiplier = 1.0f;
    stroke.setting.opacityMultiplier = 1.0f;

    // optimize path
    auto& optPath = RenderPath::scratch();
    RenderPath optStrokePath;
    bool optPathThin = false;
    bool optPathSkipFill = false;
    auto strokeWidth = rshape.strokeWidth();
    auto localOut = (std::isfinite(strokeWidth) && !tvg::zero(strokeWidth)) ? &optStrokePath : nullptr;
    if (rshape.trimpath()) {
        auto& trimmed = RenderPath::scratch();
        if (rshape.stroke->trim.trim(rshape.path, trimmed)) {
            GpuOptimizeResult result{&optPath, localOut};
            gpuOptimize(trimmed, result, transform);
            optPathThin = result.thin;
            optPathSkipFill = result.skipFill;
        }
        else optPath.clear();
    } else {
        GpuOptimizeResult result{&optPath, localOut};
        gpuOptimize(rshape.path, result, transform);
        optPathThin = result.thin;
        optPathSkipFill = result.skipFill;
    }

    auto updatePath = flag & (RenderUpdateFlag::Transform | RenderUpdateFlag::Path);

    // update fill shapes
    if (updatePath || (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient))) {
        if (optPathSkipFill) {
            // Too-thin fills are suppressed instead of going through thin fallback.
            shape.mesh.clear();
        } else {
            BBox bbox;
            // Drawable thin fills are tessellated as a minimal-width stroke.
            if (optPathThin && tvg::zero(rshape.strokeWidth())) {
                WgStroker stroker(&shape.mesh, MIN_WG_STROKE_WIDTH, StrokeCap::Butt, StrokeJoin::Bevel);
                stroker.run(optPath);
                bbox = stroker.getBBox();
                shape.setting.opacityMultiplier = MIN_WG_STROKE_ALPHA;
            } else {
                WgBWTessellator bwTess{&shape.mesh};
                bwTess.tessellate(optPath);
                convex = bwTess.convex;
                bbox = bwTess.getBBox();
            }
            if (shape.mesh.ibuffer.empty()) {
                shape.mesh.clear();
            } else {
                shape.bounds = bbox;
                shape.bbox.bbox(bbox.min, bbox.max);
                updateBBox(bbox);
            }
        }
    }
    // update strokes shapes
    if (rshape.stroke && (updatePath || (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke)))) {
        auto qualityScale = scaling(transform);
        auto strokeWidthWorld = strokeWidth * qualityScale;
        if (!std::isfinite(strokeWidthWorld)) strokeWidthWorld = strokeWidth;
        if (!std::isfinite(strokeWidthWorld)) strokeWidthWorld = 0.0f;
        if (!std::isfinite(qualityScale) || tvg::zero(qualityScale)) qualityScale = 1.0f;

        //run stroking only if it's valid
        if (!tvg::zero(strokeWidthWorld)) {
            WgStroker stroker(&stroke.mesh, strokeWidth, rshape.strokeCap(), rshape.strokeJoin(), rshape.strokeMiterlimit(), qualityScale);
            auto& dashed = RenderPath::scratch();
            if (gpuStrokeDash(rshape, dashed, nullptr)) stroker.run(dashed);
            else stroker.run(optStrokePath);
            stroke.setting.opacityMultiplier = 1.0f;
            if (stroke.mesh.ibuffer.empty()) {
                stroke.mesh.clear();
            } else {
                auto bbox = stroker.getBBox();
                stroke.bounds = bbox;
                stroke.bbox.bbox(bbox.min, bbox.max);
                auto strokeBounds = gpuTransformBounds(stroker.bounds(), transform);
                updateBBox({{(float)strokeBounds.min.x, (float)strokeBounds.min.y}, {(float)strokeBounds.max.x, (float)strokeBounds.max.y}});
            }
        }
    }
    // update shapes bbox (with empty path handling)
    if (!shape.mesh.vbuffer.empty() || !stroke.mesh.vbuffer.empty()) updateAABB();
    else bbox = aabb = {{0, 0}, {0, 0}};
    meshBBox.bbox(bbox.min, bbox.max);
}

void WgShape::reset()
{
    clips.clear();
    stroke.mesh.clear();
    stroke.bbox.clear();
    stroke.bounds.init();
    shape.mesh.clear();
    shape.bbox.clear();
    shape.bounds.init();
    meshBBox.clear();
    bbox.init();
    aabb.init();
}

void WgShape::release(WgContext& context)
{
    reset();
    stroke.setting.release(context);
    shape.setting.release(context);
};

//***********************************************************************
// WgImage
//***********************************************************************

void WgImage::update(const RenderSurface* surface, const Matrix& transform)
{
    meshData.imageBox(surface->w, surface->h, transform);
}

void WgImage::setup(WGPUTexture texture, WGPUBindGroup bindGroup, const RenderSurface* surface, FilterMethod filter, uint16_t stamp)
{
    imageTexture = texture;
    imageBindGroup = bindGroup;
    imageSource = texture ? surface : nullptr;
    imageFilter = filter;
    imageStamp = texture ? stamp : 0;
}

void WgImage::release(WgTextureMgr& textures, WgContext& context)
{
    if (imageTexture && imageStamp == textures.stamp) textures.release(context, imageSource, imageFilter, imageTexture);
    reset();
}

void WgImage::reset()
{
    imageTexture = nullptr;
    imageBindGroup = nullptr;
    imageSource = nullptr;
    imageFilter = FilterMethod::Bilinear;
    imageStamp = 0;
}

void WgImage::release(WgContext& context)
{
    clips.clear();
    renderSettings.release(context);
    reset();
}

//***********************************************************************
// WgRenderEffectParams
//***********************************************************************

void WgRenderEffectParams::update(WgContext& context, WgShaderTypeEffectParams& effectParams)
{
    if (context.allocateBufferUniform(bufferParams, &effectParams.params, sizeof(effectParams.params))) {
        context.layouts.releaseBindGroup(bindGroupParams);
        bindGroupParams = context.layouts.createBindGroupBuffer1Un(bufferParams);
    }
}

void WgRenderEffectParams::update(WgContext& context, RenderEffectGaussianBlur* gaussian, const Matrix& transform)
{
    assert(gaussian);
    WgShaderTypeEffectParams effectParams;
    if (!effectParams.update(gaussian, transform)) return;
    update(context, effectParams);
    extend = effectParams.extend;
}

void WgRenderEffectParams::update(WgContext& context, RenderEffectDropShadow* dropShadow, const Matrix& transform)
{
    assert(dropShadow);
    WgShaderTypeEffectParams effectParams;
    if (!effectParams.update(dropShadow, transform)) return;
    update(context, effectParams);
    extend = effectParams.extend;
    offset = effectParams.offset;
}

void WgRenderEffectParams::update(WgContext& context, RenderEffectFill* fill)
{
    assert(fill);
    WgShaderTypeEffectParams effectParams;
    if (!effectParams.update(fill)) return;
    update(context, effectParams);
}

void WgRenderEffectParams::update(WgContext& context, RenderEffectTint* tint)
{
    assert(tint);
    WgShaderTypeEffectParams effectParams;
    if (!effectParams.update(tint)) return;
    update(context, effectParams);
}

void WgRenderEffectParams::update(WgContext& context, RenderEffectTritone* tritone)
{
    assert(tritone);
    WgShaderTypeEffectParams effectParams;
    if (!effectParams.update(tritone)) return;
    update(context, effectParams);
}

void WgRenderEffectParams::release(WgContext& context)
{
    context.releaseBuffer(bufferParams);
    context.layouts.releaseBindGroup(bindGroupParams);
}

//***********************************************************************
// WgRenderDataColorsPool
//***********************************************************************

WgRenderEffectParams* WgRenderEffectParamsPool::allocate(WgContext& context)
{
    WgRenderEffectParams* rdata{};
    if (mPool.count > 0) {
        rdata = mPool.pick();
    } else {
        rdata = new WgRenderEffectParams();
        mList.push(rdata);
    }
    return rdata;
}

void WgRenderEffectParamsPool::free(WgContext& context, WgRenderEffectParams* rdata)
{
    if (rdata) mPool.push(rdata);
}

void WgRenderEffectParamsPool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
}

//***********************************************************************
// WgStageBufferGeometry
//***********************************************************************

void WgStageBufferUniformBase::flush(WgContext& context, WGPUBuffer& buffer, const void* data, uint64_t reserved, uint32_t count, uint64_t stride)
{
    // Reserve GPU storage for growth, but upload only initialized elements.
    if (context.allocateBufferUniform(buffer, data, reserved, uint64_t(count) * stride)) releaseBindGroups(context);

    for (uint32_t i = bbuffer.count; i < count; ++i)
        bbuffer.push(context.layouts.createBindGroupBuffer1Un(buffer, i * stride, stride));
}

void WgStageBufferUniformBase::releaseBindGroups(WgContext& context)
{
    ARRAY_FOREACH(p, bbuffer)
       context.layouts.releaseBindGroup(*p);
    bbuffer.clear();
}

void WgStageBufferGeometry::append(WgMesh* meshData)
{
    assert(meshData);
    uint32_t vsize = meshData->vbuffer.count * sizeof(meshData->vbuffer[0]);
    uint32_t tsize = meshData->tbuffer.count * sizeof(meshData->tbuffer[0]);
    uint32_t isize = meshData->ibuffer.count * sizeof(meshData->ibuffer[0]);
    // append vertex data
    if (vbuffer.reserved < vbuffer.count + vsize)
        vbuffer.grow(std::max(vsize, vbuffer.reserved));
    if (meshData->vbuffer.count > 0) {
        meshData->voffset = vbuffer.count;
        memcpy(vbuffer.data + vbuffer.count, meshData->vbuffer.data, vsize);
        vbuffer.count += vsize;
    }
    // append tex coords data
    if (vbuffer.reserved < vbuffer.count + tsize)
        vbuffer.grow(std::max(tsize, vbuffer.reserved));
    if (meshData->tbuffer.count > 0) {
        meshData->toffset = vbuffer.count;
        memcpy(vbuffer.data + vbuffer.count, meshData->tbuffer.data, tsize);
        vbuffer.count += tsize;
    }
    // append index data
    if (ibuffer.reserved < ibuffer.count + isize)
        ibuffer.grow(std::max(isize, ibuffer.reserved));
    if (meshData->ibuffer.count > 0) {
        meshData->ioffset = ibuffer.count;
        memcpy(ibuffer.data + ibuffer.count, meshData->ibuffer.data, isize);
        ibuffer.count += isize;
    }
}

void WgStageBufferGeometry::append(WgShape* renderShape)
{
    append(&renderShape->shape.mesh);
    append(&renderShape->shape.bbox);
    append(&renderShape->stroke.mesh);
    append(&renderShape->stroke.bbox);
    append(&renderShape->meshBBox);
}

void WgStageBufferGeometry::append(WgImage* renderPicture)
{
    append(&renderPicture->meshData);
}

void WgStageBufferGeometry::appendSolidBatch(const Array<WgShape*>& renderShapes, WgStageBufferSolidColor& colors, WgSolidBatchRange& range)
{
    appendBatch(renderShapes, range, false);
    if (colors.vbuffer.reserved < colors.vbuffer.count + range.vertexCount)
        colors.vbuffer.grow(std::max(range.vertexCount, colors.vbuffer.reserved));
    range.colorOffset = colors.vbuffer.count * sizeof(RenderColor);

    ARRAY_FOREACH(shape, renderShapes) {
        const auto& mesh = (*shape)->shape.mesh;
        colors.appendRepeated((*shape)->shape.solid.packedColor(), mesh.vbuffer.count);
    }
}

void WgStageBufferGeometry::appendBatch(const Array<WgShape*>& renderShapes, WgGeometryRange& range, bool cover)
{
    assert(renderShapes.count > 1);

    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    ARRAY_FOREACH(p, renderShapes) {
        const auto& mesh = cover ? (*p)->meshBBox : (*p)->shape.mesh;
        vertexCount += mesh.vbuffer.count;
        indexCount += mesh.ibuffer.count;
    }

    const uint32_t vertexBytes = vertexCount * sizeof(Point);
    const uint32_t indexBytes = indexCount * sizeof(uint32_t);

    if (vbuffer.reserved < vbuffer.count + vertexBytes)
        vbuffer.grow(std::max(vertexBytes, vbuffer.reserved));
    if (ibuffer.reserved < ibuffer.count + indexBytes)
        ibuffer.grow(std::max(indexBytes, ibuffer.reserved));

    range.vertexOffset = vbuffer.count;
    range.indexOffset = ibuffer.count;
    range.vertexCount = vertexCount;
    range.indexCount = indexCount;

    uint32_t baseVertex = 0;
    auto vertexDst = vbuffer.data + vbuffer.count;
    auto indexDst = ibuffer.data + ibuffer.count;
    ARRAY_FOREACH(p, renderShapes) {
        const auto& mesh = cover ? (*p)->meshBBox : (*p)->shape.mesh;
        const uint32_t meshVertexBytes = mesh.vbuffer.count * sizeof(Point);
        memcpy(vertexDst, mesh.vbuffer.data, meshVertexBytes);
        vertexDst += meshVertexBytes;

        for (uint32_t i = 0; i < mesh.ibuffer.count; ++i) {
            const auto index = mesh.ibuffer[i] + baseVertex;
            memcpy(indexDst, &index, sizeof(index));
            indexDst += sizeof(index);
        }

        baseVertex += mesh.vbuffer.count;
    }
    vbuffer.count += vertexBytes;
    ibuffer.count += indexBytes;
}

void WgStageBufferGeometry::appendStencilBatch(const Array<WgShape*>& renderShapes, WgStencilBatchRange& range)
{
    appendBatch(renderShapes, range.stencil, false);
    appendBatch(renderShapes, range.cover, true);
}

void WgStageBufferGeometry::release(WgContext& context)
{
    context.releaseBuffer(vbuffer_gpu);
    context.releaseBuffer(ibuffer_gpu);
}


void WgStageBufferGeometry::clear()
{
    vbuffer.clear();
    ibuffer.clear();
}


void WgStageBufferGeometry::flush(WgContext& context) 
{
    context.allocateBufferVertex(vbuffer_gpu, vbuffer.data, vbuffer.count);
    context.allocateBufferIndex(ibuffer_gpu, (uint32_t *)ibuffer.data, ibuffer.count);
}

//***********************************************************************
// WgStageBufferSolidColor
//***********************************************************************

void WgStageBufferSolidColor::release(WgContext& context)
{
    context.releaseBuffer(vbuffer_gpu);
}

void WgStageBufferSolidColor::clear()
{
    vbuffer.clear();
}

void WgStageBufferSolidColor::appendRepeated(const RenderColor& value, uint32_t count)
{
    if (vbuffer.reserved < vbuffer.count + count) {
        vbuffer.grow(std::max(count, vbuffer.reserved));
    }
    auto dst = vbuffer.data + vbuffer.count;
    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = value;
    }
    vbuffer.count += count;
}

void WgStageBufferSolidColor::flush(WgContext& context)
{
    if (vbuffer.count > 0) context.allocateBufferVertex(vbuffer_gpu, vbuffer.data, vbuffer.count * sizeof(RenderColor));
}
