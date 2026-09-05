
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

#include <algorithm>
#include <cmath>
#include "tvgCommon.h"
#include "tvgWgTessellator.h"
#include "tvgWgRenderData.h"
#include "tvgWgTextureMgr.h"
#include "tvgWgShaderTypes.h"

//***********************************************************************
// WgImageData
//***********************************************************************

void WgImageData::update(WgContext& context, const Fill* fill, FillSpread& currentSpread)
{
    // compute gradient data
    WgShaderTypeGradientData gradientData;
    gradientData.update(fill);
    // allocate new texture handle
    auto bytesPerRow = WG_TEXTURE_GRADIENT_SIZE * sizeof(uint32_t);
    bool texHandleChanged = context.allocateTexture(texture, WG_TEXTURE_GRADIENT_SIZE, 1, WGPUTextureFormat_RGBA8Unorm, gradientData.data, bytesPerRow, bytesPerRow);
    // update texture view if texture handle was changed
    if (texHandleChanged) {
        context.releaseTextureView(textureView);
        textureView = context.createTextureView(texture);
    }
    // get sampler by spread type. Conic gradients always use repeat to filter
    // across the first/last ramp texels at their seam.
    auto spread = fill->type() == Type::ConicGradient ? FillSpread::Repeat : fill->spread();
    if (texHandleChanged || currentSpread != spread) {
        auto sampler = context.samplerLinearClamp;
        if (spread == FillSpread::Reflect) sampler = context.samplerLinearMirror;
        if (spread == FillSpread::Repeat) sampler = context.samplerLinearRepeat;
        // update bind group
        context.layouts.releaseBindGroup(bindGroup);
        bindGroup = context.layouts.createBindGroupTexSampled(sampler, textureView);
        currentSpread = spread;
    }
};


void WgImageData::release(WgContext& context)
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

void WgRenderSettings::update(WgContext& context, const Fill* fill, const Matrix* modelTransform, bool updateColorRamp)
{
    assert(fill);
    settings.gradient.update(fill, modelTransform);
    if (updateColorRamp) gradientData.update(context, fill, spread);
    if (fill->type() == Type::LinearGradient)
        fillType = WgRenderSettingsType::Linear;
    else if (fill->type() == Type::RadialGradient)
        fillType = WgRenderSettingsType::Radial;
    else if (fill->type() == Type::ConicGradient)
        fillType = WgRenderSettingsType::Conic;
};


void WgRenderSettings::release(WgContext& context)
{
    gradientData.release(context);
};

//***********************************************************************
// WgRenderPaint
//***********************************************************************

void WgRenderPaint::release(WgContext& context)
{
    clips.clear();
};

void WgRenderPaint::update(const Array<RenderData>& clips)
{
    this->clips.clear();
    // RenderData == WgRenderPaint*, just copy it.
    this->clips = *((Array<WgRenderPaint*>*)&clips);
}

//***********************************************************************
// WgRenderShape
//***********************************************************************

void WgRenderShape::updateBBox(const BBox& bb)
{
    bbox.min = tvg::min(bbox.min, bb.min);
    bbox.max = tvg::max(bbox.max, bb.max);
}

void WgRenderShape::updateVisibility(const RenderShape& rshape, uint8_t opacity)
{
    shape.setting.valid = rshape.fill || (rshape.color.a * opacity > 0);
    stroke.setting.valid = rshape.stroke && (rshape.stroke->fill || (rshape.stroke->color.a * opacity > 0));
}

void WgRenderShape::updateMeshes(const RenderShape& rshape, RenderUpdateFlag flag, const Matrix& matrix)
{
    releaseMeshes();  //Optimize: bad idea to reset meshes always. it could re-use the meshes if there haven't been any path changes.

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
            gpuOptimize(trimmed, result, matrix);
            optPathThin = result.thin;
            optPathSkipFill = result.skipFill;
        }
        else optPath.clear();
    } else {
        GpuOptimizeResult result{&optPath, localOut};
        gpuOptimize(rshape.path, result, matrix);
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
                shape.bbox.bbox(bbox.min, bbox.max);
                updateBBox(bbox);
            }
        }
    }
    // update strokes shapes
    if (rshape.stroke && (updatePath || (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke)))) {
        auto qualityScale = scaling(matrix);
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
                stroke.bbox.bbox(bbox.min, bbox.max);
                auto strokeBounds = gpuTransformBounds(stroker.bounds(), matrix);
                updateBBox({{(float)strokeBounds.min.x, (float)strokeBounds.min.y}, {(float)strokeBounds.max.x, (float)strokeBounds.max.y}});
            }
        }
    }
    // update shapes bbox (with empty path handling)
    if (!shape.mesh.vbuffer.empty() || !stroke.mesh.vbuffer.empty()) updateAABB();
    else bbox = aabb = {{0, 0}, {0, 0}};
    meshBBox.bbox(bbox.min, bbox.max);
}

void WgRenderShape::releaseMeshes()
{
    stroke.mesh.clear();
    stroke.bbox.clear();
    shape.mesh.clear();
    shape.bbox.clear();
    meshBBox.clear();
    bbox.min = {FLT_MAX, FLT_MAX};
    bbox.max = {0.0f, 0.0f};
    aabb = {{0, 0}, {0, 0}};
    clips.clear();
}

void WgRenderShape::release(WgContext& context)
{
    releaseMeshes();
    stroke.setting.release(context);
    shape.setting.release(context);
    WgRenderPaint::release(context);
};

//***********************************************************************
// WgRenderShapePool
//***********************************************************************

WgRenderShape* WgRenderShapePool::allocate(WgContext& context)
{
    WgRenderShape* rdata{};
    if (mPool.count > 0) {
        rdata = mPool.pick();
    } else {
        rdata = new WgRenderShape();
        mList.push(rdata);
    }
    return rdata;
}

void WgRenderShapePool::free(WgContext& context, WgRenderShape* rdata)
{
    rdata->releaseMeshes();
    rdata->clips.clear();
    mPool.push(rdata);
}

void WgRenderShapePool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
}

//***********************************************************************
// WgRenderPicture
//***********************************************************************

void WgRenderPicture::update(const RenderSurface* surface, const Matrix& transform)
{
    meshData.imageBox(surface->w, surface->h, transform);
}

void WgRenderPicture::setImage(WGPUTexture texture, WGPUBindGroup bindGroup, const RenderSurface* surface, FilterMethod filter, uint16_t stamp)
{
    imageTexture = texture;
    imageBindGroup = bindGroup;
    imageSource = texture ? surface : nullptr;
    imageFilter = filter;
    imageStamp = texture ? stamp : 0;
}

void WgRenderPicture::releaseTexture(WgTextureMgr& textures, WgContext& context)
{
    if (imageTexture && imageStamp == textures.stamp) textures.release(context, imageSource, imageFilter, imageTexture);
    clearImage();
}

void WgRenderPicture::clearImage()
{
    imageTexture = nullptr;
    imageBindGroup = nullptr;
    imageSource = nullptr;
    imageFilter = FilterMethod::Bilinear;
    imageStamp = 0;
}

void WgRenderPicture::release(WgContext& context)
{
    renderSettings.release(context);
    clearImage();
    WgRenderPaint::release(context);
}

//***********************************************************************
// WgRenderPicturePool
//***********************************************************************

WgRenderPicture* WgRenderPicturePool::allocate(WgContext& context)
{
    WgRenderPicture* rdata{};
    if (mPool.count > 0) {
        rdata = mPool.pick();
    } else {
        rdata = new WgRenderPicture();
        mList.push(rdata);
    }
    return rdata;
}

void WgRenderPicturePool::free(WgContext& context, WgRenderPicture* rdata)
{
    rdata->clips.clear();
    mPool.push(rdata);
}

void WgRenderPicturePool::release(WgContext& context)
{
    ARRAY_FOREACH(p, mList) {
        (*p)->release(context);
        delete(*p);
    }
    mPool.clear();
    mList.clear();
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

void WgStageBufferGeometry::append(WgMeshData* meshData)
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

void WgStageBufferGeometry::append(WgRenderShape* renderShape)
{
    append(&renderShape->shape.mesh);
    append(&renderShape->shape.bbox);
    append(&renderShape->stroke.mesh);
    append(&renderShape->stroke.bbox);
    append(&renderShape->meshBBox);
}

void WgStageBufferGeometry::append(WgRenderPicture* renderPicture)
{
    append(&renderPicture->meshData);
}

void WgStageBufferGeometry::appendSolidBatch(const Array<WgRenderShape*>& renderShapes, WgStageBufferSolidColor& colors, WgSolidBatchRange& range)
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

void WgStageBufferGeometry::appendBatch(const Array<WgRenderShape*>& renderShapes, WgGeometryRange& range, bool cover)
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

void WgStageBufferGeometry::appendStencilBatch(const Array<WgRenderShape*>& renderShapes, WgStencilBatchRange& range)
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

//***********************************************************************
// WgIntersector
//***********************************************************************

bool WgIntersector::isPointInTriangle(const Point& p, const Point& a, const Point& b, const Point& c)
{
    auto d1 = tvg::cross(p - a, p - b);
    auto d2 = tvg::cross(p - b, p - c);
    auto d3 = tvg::cross(p - c, p - a);
    auto has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    auto has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}


// triangle list
bool WgIntersector::isPointInTris(const Point& p, const WgMeshData& mesh)
{
    for (uint32_t i = 0; i < mesh.ibuffer.count; i += 3) {
        auto p0 = mesh.vbuffer[mesh.ibuffer[i+0]];
        auto p1 = mesh.vbuffer[mesh.ibuffer[i+1]];
        auto p2 = mesh.vbuffer[mesh.ibuffer[i+2]];
        if (isPointInTriangle(p, p0, p1, p2)) return true;
    }
    return false;
}


// even-odd triangle list
bool WgIntersector::isPointInMesh(const Point& p, const WgMeshData& mesh)
{
    uint32_t crossings = 0;
    for (uint32_t i = 0; i < mesh.ibuffer.count; i += 3) {
        Point triangle[3] = {
            mesh.vbuffer[mesh.ibuffer[i+0]],
            mesh.vbuffer[mesh.ibuffer[i+1]],
            mesh.vbuffer[mesh.ibuffer[i+2]]
        };
        for (uint32_t j = 0; j < 3; j++) {
            auto p1 = triangle[j];
            auto p2 = triangle[(j + 1) % 3];
            if (p1.y == p2.y) continue;
            if (p1.y > p2.y) std::swap(p1, p2);
            if ((p.y > p1.y) && (p.y <= p2.y)) {
                auto intersectionX = (p2.x - p1.x) * (p.y - p1.y) / (p2.y - p1.y) + p1.x;
                if (intersectionX > p.x) crossings++;
            }
        }
    }
    return (crossings % 2) == 1;
}

bool WgIntersector::intersectClips(const Point& pt, const Array<WgRenderPaint*>& clips)
{
    for (uint32_t i = 0; i < clips.count; i++) {
        auto clip = (WgRenderShape*)clips[i];
        if (!isPointInMesh(pt, clip->shape.mesh)) return false;
    }
    return true;
}

bool WgIntersector::intersectShape(const RenderRegion region, const WgRenderShape* shape)
{
    if (!shape || ((shape->shape.mesh.ibuffer.count == 0) && (shape->stroke.mesh.ibuffer.count == 0))) return false;
    Matrix inverseModel;
    auto testStroke = shape->stroke.setting.valid && inverse(&shape->transform, &inverseModel);
    auto sizeX = region.sw();
    auto sizeY = region.sh();
    for (int32_t y = 0; y <= sizeY; y++) {
        for (int32_t x = 0; x <= sizeX; x++) {
            Point pt{(float)x + region.min.x, (float)y + region.min.y};
            if (y % 2 == 1) pt.y = (float) sizeY - y - sizeY % 2 + region.min.y;
            if (intersectClips(pt, shape->clips)) {
                if (shape->shape.setting.valid && isPointInMesh(pt, shape->shape.mesh)) return true;
                if (testStroke && isPointInTris(pt * inverseModel, shape->stroke.mesh)) return true;
            }
        }
    }
    return false;
}

bool WgIntersector::intersectImage(const RenderRegion region, const WgRenderPicture* image)
{
    if (!image) return false;
    auto sizeX = region.sw();
    auto sizeY = region.sh();
    for (int32_t y = 0; y <= sizeY; y++) {
        for (int32_t x = 0; x <= sizeX; x++) {
            Point pt{(float)x + region.min.x, (float)y + region.min.y};
            if (y % 2 == 1) pt.y = (float) sizeY - y - sizeY % 2 + region.min.y;
            if (intersectClips(pt, image->clips)) {
                if (isPointInTris(pt, image->meshData)) return true;
            }
        }
    }
    return false;
}
