
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

#include <algorithm>
#include "tvgWgRenderData.h"
#include "tvgWgShaderTypes.h"

//***********************************************************************
// WgMeshData
//***********************************************************************

void WgMeshData::draw(WgContext& context, WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, bufferIndex, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
}


void WgMeshData::drawFan(WgContext& context, WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, context.bufferIndexFan, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
}


void WgMeshData::drawImage(WgContext& context, WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, bufferTexCoord, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, bufferIndex, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
};


void WgMeshData::update(WgContext& context, const WgPolyline* polyline)
{
    assert(polyline);
    assert(polyline->pts.count > 2);
    vertexCount = polyline->pts.count;
    indexCount = (polyline->pts.count - 2) * 3;
    context.allocateBufferVertex(bufferPosition, (float *)&polyline->pts[0], vertexCount * sizeof(float) * 2);
    context.allocateBufferIndexFan(vertexCount);
}


void WgMeshData::update(WgContext& context, const WgGeometryData* geometryData)
{
    assert(geometryData);
    vertexCount = geometryData->positions.pts.count;
    indexCount = geometryData->indexes.count;
    // buffer position data create and write
    if (geometryData->positions.pts.count > 0)
        context.allocateBufferVertex(bufferPosition, (float *)&geometryData->positions.pts[0], vertexCount * sizeof(float) * 2);
    // buffer tex coords data create and write
    if (geometryData->texCoords.count > 0)
        context.allocateBufferVertex(bufferTexCoord, (float *)&geometryData->texCoords[0], vertexCount * sizeof(float) * 2);
    // buffer index data create and write
    if (geometryData->indexes.count > 0)
        context.allocateBufferIndex(bufferIndex, &geometryData->indexes[0], indexCount * sizeof(uint32_t));
};


void WgMeshData::update(WgContext& context, const WgPoint pmin, const WgPoint pmax)
{
    vertexCount = 4;
    indexCount = 6;
    const float data[] = {
        pmin.x, pmin.y, pmax.x, pmin.y,
        pmax.x, pmax.y, pmin.x, pmax.y
    };
    context.allocateBufferVertex(bufferPosition, data, sizeof(data));
    context.allocateBufferIndexFan(vertexCount);
}


void WgMeshData::release(WgContext& context)
{
    context.releaseBuffer(bufferIndex);
    context.releaseBuffer(bufferTexCoord);
    context.releaseBuffer(bufferPosition);
};


//***********************************************************************
// WgMeshDataPool
//***********************************************************************

WgMeshData* WgMeshDataPool::allocate(WgContext& context)
{
    WgMeshData* meshData{};
    if (mPool.count > 0) {
        meshData = mPool.last();
        mPool.pop();
    } else {
        meshData = new WgMeshData();
        mList.push(meshData);
    }
    return meshData;
}


void WgMeshDataPool::free(WgContext& context, WgMeshData* meshData)
{
    mPool.push(meshData);
}


void WgMeshDataPool::release(WgContext& context)
{
    for (uint32_t i = 0; i < mList.count; i++) {
        mList[i]->release(context);
        delete mList[i];
    }
    mPool.clear();
    mList.clear();
}

WgMeshDataPool* WgMeshDataGroup::gMeshDataPool = nullptr;

//***********************************************************************
// WgMeshDataGroup
//***********************************************************************

void WgMeshDataGroup::append(WgContext& context, const WgPolyline* polyline)
{
    assert(polyline);
    assert(polyline->pts.count >= 3);
    meshes.push(gMeshDataPool->allocate(context));
    meshes.last()->update(context, polyline);
}


void WgMeshDataGroup::append(WgContext& context, const WgGeometryData* geometryData)
{
    assert(geometryData);
    assert(geometryData->positions.pts.count >= 3);
    meshes.push(gMeshDataPool->allocate(context));
    meshes.last()->update(context, geometryData);
}


void WgMeshDataGroup::append(WgContext& context, const WgPoint pmin, const WgPoint pmax)
{
    meshes.push(gMeshDataPool->allocate(context));
    meshes.last()->update(context, pmin, pmax);
}


void WgMeshDataGroup::release(WgContext& context)
{
    for (uint32_t i = 0; i < meshes.count; i++)
        gMeshDataPool->free(context, meshes[i]);
    meshes.clear();
};


//***********************************************************************
// WgImageData
//***********************************************************************

void WgImageData::update(WgContext& context, Surface* surface)
{
    release(context);
    assert(surface);
    texture = context.createTexture(surface->w, surface->h, WGPUTextureFormat_RGBA8Unorm);
    assert(texture);
    textureView = context.createTextureView(texture);
    assert(textureView);
    // update texture data
    WGPUImageCopyTexture imageCopyTexture{ .texture = texture };
    WGPUTextureDataLayout textureDataLayout{ .bytesPerRow = 4 * surface->w, .rowsPerImage = surface->h };
    WGPUExtent3D writeSize{ .width = surface->w, .height = surface->h, .depthOrArrayLayers = 1 };
    wgpuQueueWriteTexture(context.queue, &imageCopyTexture, surface->data, 4 * surface->w * surface->h, &textureDataLayout, &writeSize);
    wgpuQueueSubmit(context.queue, 0, nullptr);
};


void WgImageData::release(WgContext& context)
{
    context.releaseTextureView(textureView);
    context.releaseTexture(texture);
};

//***********************************************************************
// WgRenderSettings
//***********************************************************************

void WgRenderSettings::update(WgContext& context, const Fill* fill, const uint8_t* color, const RenderUpdateFlag flags)
{
    // setup fill properties
    if ((flags & (RenderUpdateFlag::Gradient)) && fill) {
        rasterType = WgRenderRasterType::Gradient;
        // get gradient transfrom matrix
        Matrix fillTransform = fill->transform();
        Matrix invFillTransform;
        WgShaderTypeMat4x4f gradientTrans; // identity by default
        if (inverse(&fillTransform, &invFillTransform))
            gradientTrans.update(invFillTransform);
        // get gradient rasterisation settings
        WgShaderTypeGradient gradient;
        if (fill->type() == Type::LinearGradient) {
            gradient.update((LinearGradient*)fill);
            fillType = WgRenderSettingsType::Linear;
        } else if (fill->type() == Type::RadialGradient) {
            gradient.update((RadialGradient*)fill);
            fillType = WgRenderSettingsType::Radial;
        }
        // update gpu assets
        bool bufferGradientSettingsChanged = context.allocateBufferUniform(bufferGroupGradient, &gradient.settings, sizeof(gradient.settings));
        bool bufferGradientTransformChanged = context.allocateBufferUniform(bufferGroupTransfromGrad, &gradientTrans.mat, sizeof(gradientTrans.mat));
        bool textureGradientChanged = context.allocateTexture(texGradient, WG_TEXTURE_GRADIENT_SIZE, 1, WGPUTextureFormat_RGBA8Unorm, gradient.texData);
        if (bufferGradientSettingsChanged || textureGradientChanged || bufferGradientTransformChanged) {
            // update texture view
            context.releaseTextureView(texViewGradient);
            texViewGradient = context.createTextureView(texGradient);
            // get sampler by spread type
            WGPUSampler sampler = context.samplerLinearClamp;
            if (fill->spread() == FillSpread::Reflect) sampler = context.samplerLinearMirror;
            if (fill->spread() == FillSpread::Repeat) sampler = context.samplerLinearRepeat;
            // update bind group
            context.pipelines->layouts.releaseBindGroup(bindGroupGradient);
            bindGroupGradient = context.pipelines->layouts.createBindGroupTexSampledBuff2Un(
                sampler, texViewGradient, bufferGroupGradient, bufferGroupTransfromGrad);
        }
        skip = false;
    } else if ((flags & (RenderUpdateFlag::Color)) && !fill) {
        rasterType = WgRenderRasterType::Solid;
        WgShaderTypeSolidColor solidColor(color);
        if (context.allocateBufferUniform(bufferGroupSolid, &solidColor, sizeof(solidColor))) {
            context.pipelines->layouts.releaseBindGroup(bindGroupSolid);
            bindGroupSolid = context.pipelines->layouts.createBindGroupBuffer1Un(bufferGroupSolid);
        }
        fillType = WgRenderSettingsType::Solid;
        skip = (color[3] == 0);
    }
};


void WgRenderSettings::release(WgContext& context)
{
    context.pipelines->layouts.releaseBindGroup(bindGroupSolid);
    context.pipelines->layouts.releaseBindGroup(bindGroupGradient);
    context.releaseBuffer(bufferGroupSolid);
    context.releaseBuffer(bufferGroupGradient);
    context.releaseBuffer(bufferGroupTransfromGrad);
    context.releaseTexture(texGradient);
    context.releaseTextureView(texViewGradient);
};

//***********************************************************************
// WgRenderDataPaint
//***********************************************************************

void WgRenderDataPaint::release(WgContext& context)
{
    context.pipelines->layouts.releaseBindGroup(bindGroupPaint);
    context.releaseBuffer(bufferModelMat);
    context.releaseBuffer(bufferBlendSettings);
    clips.clear();
};


void WgRenderDataPaint::update(WgContext& context, const tvg::Matrix& transform, tvg::ColorSpace cs, uint8_t opacity)
{
    WgShaderTypeMat4x4f modelMat(transform);
    WgShaderTypeBlendSettings blendSettings(cs, opacity);
    bool bufferModelMatChanged = context.allocateBufferUniform(bufferModelMat, &modelMat, sizeof(modelMat));
    bool bufferBlendSettingsChanged = context.allocateBufferUniform(bufferBlendSettings, &blendSettings, sizeof(blendSettings));
    if (bufferModelMatChanged || bufferBlendSettingsChanged) {
        context.pipelines->layouts.releaseBindGroup(bindGroupPaint);
        bindGroupPaint = context.pipelines->layouts.createBindGroupBuffer2Un(bufferModelMat, bufferBlendSettings);
    }
}


void WgRenderDataPaint::updateClips(tvg::Array<tvg::RenderData> &clips) {
    this->clips.clear();
    for (uint32_t i = 0; i < clips.count; i++)
        if (clips[i])
            this->clips.push((WgRenderDataPaint*)clips[i]);
}

//***********************************************************************
// WgRenderDataShape
//***********************************************************************

void WgRenderDataShape::updateBBox(WgPoint pmin, WgPoint pmax)
{
    pMin.x = std::min(pMin.x, pmin.x);
    pMin.y = std::min(pMin.y, pmin.y);
    pMax.x = std::max(pMax.x, pmax.x);
    pMax.y = std::max(pMax.y, pmax.y);
}


void WgRenderDataShape::updateAABB(const Matrix& rt) {
    WgPoint p0 = WgPoint(pMin.x, pMin.y).trans(rt);
    WgPoint p1 = WgPoint(pMax.x, pMin.y).trans(rt);
    WgPoint p2 = WgPoint(pMin.x, pMax.y).trans(rt);
    WgPoint p3 = WgPoint(pMax.x, pMax.y).trans(rt);
    aabb.x = std::min({p0.x, p1.x, p2.x, p3.x});
    aabb.y = std::min({p0.y, p1.y, p2.y, p3.y});
    aabb.w = std::max({p0.x, p1.x, p2.x, p3.x}) - aabb.x;
    aabb.h = std::max({p0.y, p1.y, p2.y, p3.y}) - aabb.y;
}


void WgRenderDataShape::updateMeshes(WgContext &context, const RenderShape &rshape, const Matrix& rt)
{
    releaseMeshes(context);
    strokeFirst = rshape.stroke ? rshape.stroke->strokeFirst : false;

    Array<WgPolyline*> polylines{};
    // decode path
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        if (cmd == PathCommand::MoveTo) {
            // proceed current polyline
            polylines.push(new WgPolyline);
            polylines.last()->appendPoint(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::LineTo) {
            polylines.last()->appendPoint(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::Close) {
            polylines.last()->close();
        } else if (cmd == PathCommand::CubicTo) {
            assert(polylines.last()->pts.count > 0);
            WgPoint pt0 = polylines.last()->pts.last().trans(rt);
            WgPoint pt1 = WgPoint(rshape.path.pts[pntIndex + 0]).trans(rt);
            WgPoint pt2 = WgPoint(rshape.path.pts[pntIndex + 1]).trans(rt);
            WgPoint pt3 = WgPoint(rshape.path.pts[pntIndex + 2]).trans(rt);
            uint32_t nsegs = (uint32_t)(pt0.dist(pt1) + pt1.dist(pt2) + pt2.dist(pt3));
            polylines.last()->appendCubic(
                rshape.path.pts[pntIndex + 0],
                rshape.path.pts[pntIndex + 1],
                rshape.path.pts[pntIndex + 2],
                nsegs / 2);
            pntIndex += 3;
        }
    }
    // proceed shapes
    float totalLen{};
    for (uint32_t i = 0; i < polylines.count; i++) {
        totalLen += polylines[i]->len;
        updateShapes(context, polylines[i]);
    }
    // proceed strokes
    if (rshape.stroke) {
        float trimBegin{};
        float trimEnd{};
        if (!rshape.stroke->strokeTrim(trimBegin, trimEnd)) { trimBegin = 0.0f; trimEnd = 1.0f; }
        if (rshape.stroke->trim.simultaneous) {
            for (uint32_t i = 0; i < polylines.count; i++)
                updateStrokes(context, polylines[i], rshape.stroke, trimBegin, trimEnd);
        } else {
            if (trimBegin <= trimEnd) {
                updateStrokesList(context, polylines, rshape.stroke, totalLen, trimBegin, trimEnd);
            } else {
                updateStrokesList(context, polylines, rshape.stroke, totalLen, 0.0f, trimEnd);
                updateStrokesList(context, polylines, rshape.stroke, totalLen, trimBegin, 1.0f);
            }
        }
    }
    // delete polylines
    for (uint32_t i = 0; i < polylines.count; i++)
        delete polylines[i];
    // update shapes bbox
    updateAABB(rt);
    meshDataBBox.update(context, pMin, pMax);
}


void WgRenderDataShape::updateShapes(WgContext& context, const WgPolyline* polyline)
{
    assert(polyline);
    // generate fill geometry
    if (polyline->pts.count >= 3) {
        WgPoint pmin{}, pmax{};
        polyline->getBBox(pmin, pmax);
        meshGroupShapes.append(context, polyline);
        meshGroupShapesBBox.append(context, pmin, pmax);
        updateBBox(pmin, pmax);
    }
}

void WgRenderDataShape::updateStrokesList(WgContext& context, Array<WgPolyline*> polylines, const RenderStroke* rstroke, float totalLen, float trimBegin, float trimEnd)
{
    float tp1 = totalLen * trimBegin; // trim point begin
    float tp2 = totalLen * trimEnd; // trim point end
    float pc = 0; // point current
    for (uint32_t i = 0; i < polylines.count; i++) {
        float pl = polylines[i]->len; // current polyline length
        float trimBegin = ((pc <= tp1) && (pc + pl > tp1)) ? (tp1 - pc) / pl : 0.0f;
        float trimEnd   = ((pc <= tp2) && (pc + pl > tp2)) ? (tp2 - pc) / pl : 1.0f;
        if ((pc + pl >= tp1) && (pc <= tp2))
            updateStrokes(context, polylines[i], rstroke, trimBegin, trimEnd);
        pc += pl;
        // break if reached the tail
        if (pc > tp2) break;
    }
}


void WgRenderDataShape::updateStrokes(WgContext& context, const WgPolyline* polyline, const RenderStroke* rstroke, float trimBegin, float trimEnd)
{
    assert(polyline);
    // generate strokes geometry
    if ((polyline->pts.count >= 1) && rstroke && (rstroke->width > 0.0f)) {
        static WgGeometryData geometryData; geometryData.clear();
        static WgPolyline trimmed;
        // trim -> split -> stroke
        if (trimBegin == trimEnd) return;
        if ((rstroke->dashPattern) && ((trimBegin != 0.0f) || (trimEnd != 1.0f))) {
            trimmed.clear();
            if (trimBegin < trimEnd)
                polyline->trim(&trimmed, trimBegin, trimEnd);
            else {
                polyline->trim(&trimmed, trimBegin, 1.0f);
                polyline->trim(&trimmed, 0.0f, trimEnd);
            }
            geometryData.appendStrokeDashed(&trimmed, rstroke);
        } else // trim -> stroke
        if ((trimBegin != 0.0f) || (trimEnd != 1.0f)) {
            trimmed.clear();
            if (trimBegin < trimEnd)
                polyline->trim(&trimmed, trimBegin, trimEnd);
            else {
                polyline->trim(&trimmed, trimBegin, 1.0f);
                polyline->trim(&trimmed, 0.0f, trimEnd);
            }
            geometryData.appendStroke(&trimmed, rstroke);
        } else // split -> stroke
        if (rstroke->dashPattern) {
            geometryData.appendStrokeDashed(polyline, rstroke);
        } else { // stroke
            geometryData.appendStroke(polyline, rstroke);
        }
        // append render meshes and bboxes
        if(geometryData.positions.pts.count >= 3) {
            WgPoint pmin{}, pmax{};
            geometryData.positions.getBBox(pmin, pmax);
            meshGroupStrokes.append(context, &geometryData);
            meshGroupStrokesBBox.append(context, pmin, pmax);
            updateBBox(pmin, pmax);
        }
    }
}


void WgRenderDataShape::releaseMeshes(WgContext &context)
{
    meshGroupStrokesBBox.release(context);
    meshGroupStrokes.release(context);
    meshGroupShapesBBox.release(context);
    meshGroupShapes.release(context);
    pMin = {FLT_MAX, FLT_MAX};
    pMax = {0.0f, 0.0f};
    clips.clear();
}


void WgRenderDataShape::release(WgContext& context)
{
    releaseMeshes(context);
    meshDataBBox.release(context);
    renderSettingsStroke.release(context);
    renderSettingsShape.release(context);
    WgRenderDataPaint::release(context);
};

//***********************************************************************
// WgRenderDataShapePool
//***********************************************************************

WgRenderDataShape* WgRenderDataShapePool::allocate(WgContext& context)
{
    WgRenderDataShape* dataShape{};
    if (mPool.count > 0) {
        dataShape = mPool.last();
        mPool.pop();
    } else {
        dataShape = new WgRenderDataShape();
        mList.push(dataShape);
    }
    return dataShape;
}


void WgRenderDataShapePool::free(WgContext& context, WgRenderDataShape* dataShape)
{
    dataShape->meshGroupShapes.release(context);
    dataShape->meshGroupShapesBBox.release(context);
    dataShape->meshGroupStrokes.release(context);
    dataShape->meshGroupStrokesBBox.release(context);
    dataShape->clips.clear();
    mPool.push(dataShape);
}


void WgRenderDataShapePool::release(WgContext& context)
{
    for (uint32_t i = 0; i < mList.count; i++) {
        mList[i]->release(context);
        delete mList[i];
    }
    mPool.clear();
    mList.clear();
}

//***********************************************************************
// WgRenderDataPicture
//***********************************************************************

void WgRenderDataPicture::release(WgContext& context)
{
    imageData.release(context);
    context.pipelines->layouts.releaseBindGroup(bindGroupPicture);
    WgRenderDataPaint::release(context);
}
