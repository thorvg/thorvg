
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

#ifndef _TVG_WG_RENDER_DATA_H_
#define _TVG_WG_RENDER_DATA_H_

#include "tvgWgRenderData.h"

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
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, context.indexBufferFan, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
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
    context.allocateVertexBuffer(bufferPosition, &polyline->pts[0], vertexCount * sizeof(float) * 2);
    context.allocateIndexBufferFan(vertexCount);
}


void WgMeshData::update(WgContext& context, const WgGeometryData* geometryData)
{
    assert(geometryData);
    vertexCount = geometryData->positions.pts.count;
    indexCount = geometryData->indexes.count;
    // buffer position data create and write
    if (geometryData->positions.pts.count > 0)
        context.allocateVertexBuffer(bufferPosition, &geometryData->positions.pts[0], vertexCount * sizeof(float) * 2);
    // buffer tex coords data create and write
    if (geometryData->texCoords.count > 0)
        context.allocateVertexBuffer(bufferTexCoord, &geometryData->texCoords[0], vertexCount * sizeof(float) * 2);
    // buffer index data create and write
    if (geometryData->indexes.count > 0)
        context.allocateIndexBuffer(bufferIndex, &geometryData->indexes[0], indexCount * sizeof(uint32_t));
};


void WgMeshData::update(WgContext& context, const WgPoint pmin, const WgPoint pmax)
{
    vertexCount = 4;
    indexCount = 6;
    const float data[] = {
        pmin.x, pmin.y, pmax.x, pmin.y,
        pmax.x, pmax.y, pmin.x, pmax.y
    };
    context.allocateVertexBuffer(bufferPosition, data, sizeof(data));
    context.allocateIndexBufferFan(vertexCount);
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
    texture = context.createTexture2d(
        WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        WGPUTextureFormat_RGBA8Unorm,
        surface->w, surface->h, "The shape texture");
    assert(texture);
    textureView = context.createTextureView2d(texture, "The shape texture view");
    assert(textureView);
    // update texture data
    WGPUImageCopyTexture imageCopyTexture{};
    imageCopyTexture.nextInChain = nullptr;
    imageCopyTexture.texture = texture;
    imageCopyTexture.mipLevel = 0;
    imageCopyTexture.origin = { 0, 0, 0 };
    imageCopyTexture.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout textureDataLayout{};
    textureDataLayout.nextInChain = nullptr;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = 4 * surface->w;
    textureDataLayout.rowsPerImage = surface->h;
    WGPUExtent3D writeSize{};
    writeSize.width = surface->w;
    writeSize.height = surface->h;
    writeSize.depthOrArrayLayers = 1;
    wgpuQueueWriteTexture(context.queue, &imageCopyTexture, surface->data, 4 * surface->w * surface->h, &textureDataLayout, &writeSize);
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
        // setup linear fill properties
        if (fill->identifier() == TVG_CLASS_ID_LINEAR) {
            WgShaderTypeLinearGradient linearGradient((LinearGradient*)fill);
            bindGroupLinear.initialize(context.device, context.queue, linearGradient);
            fillType = WgRenderSettingsType::Linear;
        } else if (fill->identifier() == TVG_CLASS_ID_RADIAL) {
            WgShaderTypeRadialGradient radialGradient((RadialGradient*)fill);
            bindGroupRadial.initialize(context.device, context.queue, radialGradient);
            fillType = WgRenderSettingsType::Radial;
        }
        skip = false;
    } else if ((flags & (RenderUpdateFlag::Color)) && !fill) {
        WgShaderTypeSolidColor solidColor(color);
        bindGroupSolid.initialize(context.device, context.queue, solidColor);
        fillType = WgRenderSettingsType::Solid;
        skip = (color[3] == 0);
    }
};


void WgRenderSettings::release(WgContext& context)
{
    bindGroupSolid.release();
    bindGroupLinear.release();
    bindGroupRadial.release();
};

//***********************************************************************
// WgRenderDataPaint
//***********************************************************************

void WgRenderDataPaint::release(WgContext& context)
{
    bindGroupPaint.release();
};

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


void WgRenderDataShape::updateMeshes(WgContext &context, const RenderShape &rshape)
{
    releaseMeshes(context);
    strokeFirst = rshape.stroke ? rshape.stroke->strokeFirst : false;

    static WgPolyline polyline;
    polyline.clear();
    // decode path
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        if (cmd == PathCommand::MoveTo) {
            // proceed current polyline
            updateMeshes(context, &polyline, rshape.stroke);
            polyline.clear();
            polyline.appendPoint(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::LineTo) {
            polyline.appendPoint(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::Close) {
            polyline.close();
        } else if (cmd == PathCommand::CubicTo) {
            polyline.appendCubic(
                rshape.path.pts[pntIndex + 0],
                rshape.path.pts[pntIndex + 1],
                rshape.path.pts[pntIndex + 2]);
            pntIndex += 3;
        }
    }
    // proceed last polyline
    updateMeshes(context, &polyline, rshape.stroke);
    meshDataBBox.update(context, pMin, pMax);
}


void WgRenderDataShape::updateMeshes(WgContext& context, const WgPolyline* polyline, const RenderStroke* rstroke)
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
    // generate strokes geometry
    if ((polyline->pts.count >= 1) && rstroke && (rstroke->width > 0.0f)) {
        static WgGeometryData geometryData; geometryData.clear();
        static WgPolyline trimmed;
        // trim -> split -> stroke
        float trimBegin = rstroke->trim.begin < rstroke->trim.end ? rstroke->trim.begin : rstroke->trim.end;
        float trimEnd   = rstroke->trim.begin < rstroke->trim.end ? rstroke->trim.end : rstroke->trim.begin;
        if (trimBegin == trimEnd) return;
        if ((rstroke->dashPattern) && ((trimBegin != 0.0f) || (trimEnd != 1.0f))) {
            polyline->trim(&trimmed, trimBegin, trimEnd);
            geometryData.appendStrokeDashed(&trimmed, rstroke);
        } else // trim -> stroke
        if ((trimBegin != 0.0f) || (trimEnd != 1.0f)) {
            polyline->trim(&trimmed, trimBegin, trimEnd);
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
    meshData.release(context);
    imageData.release(context);
    bindGroupPicture.release();
    WgRenderDataPaint::release(context);
}

#endif //_TVG_WG_RENDER_DATA_H_
