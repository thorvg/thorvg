
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

void WgMeshData::draw(WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, bufferIndex, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
}


void WgMeshData::drawImage(WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, bufferPosition, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, bufferTexCoord, 0, vertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, bufferIndex, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, 1, 0, 0, 0);
};


void WgMeshData::update(WgContext& context, WgGeometryData* geometryData){
    assert(geometryData);
    vertexCount = geometryData->positions.count;
    indexCount = geometryData->indexes.count;
    // buffer position data create and write
    if (geometryData->positions.count > 0)
        context.allocateVertexBuffer(bufferPosition, &geometryData->positions[0], vertexCount * sizeof(float) * 2);
    // buffer tex coords data create and write
    if (geometryData->texCoords.count > 0)
        context.allocateVertexBuffer(bufferTexCoord, &geometryData->texCoords[0], vertexCount * sizeof(float) * 2);
    // buffer index data create and write
    if (geometryData->indexes.count > 0)
        context.allocateIndexBuffer(bufferIndex, &geometryData->indexes[0], indexCount * sizeof(uint32_t));
};


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

WgMeshDataPool* WgMeshDataGroup::MeshDataPool = nullptr;

//***********************************************************************
// WgMeshDataGroup
//***********************************************************************

void WgMeshDataGroup::update(WgContext& context, WgGeometryDataGroup* geometryDataGroup)
{
    release(context);
    assert(geometryDataGroup);
    for (uint32_t i = 0; i < geometryDataGroup->geometries.count; i++) {
        if (geometryDataGroup->geometries[i]->positions.count > 2) {
            meshes.push(MeshDataPool->allocate(context));
            meshes.last()->update(context, geometryDataGroup->geometries[i]);
        }
    }
};


void WgMeshDataGroup::release(WgContext& context)
{
    for (uint32_t i = 0; i < meshes.count; i++)
        MeshDataPool->free(context, meshes[i]);
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
    } else if ((flags & (RenderUpdateFlag::Color)) && !fill) {
        WgShaderTypeSolidColor solidColor(color);
        bindGroupSolid.initialize(context.device, context.queue, solidColor);
        fillType = WgRenderSettingsType::Solid;
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

void WgRenderDataShape::updateMeshes(WgContext &context, const RenderShape &rshape)
{
    releaseMeshes(context);
    strokeFirst = false;
    // update shapes geometry
    WgGeometryDataGroup shapes;
    shapes.tesselate(rshape);
    meshGroupShapes.update(context, &shapes);
    // update shapes bbox
    WgPoint pmin{}, pmax{};
    shapes.getBBox(pmin, pmax);
    WgGeometryData box;
    box.appendBox(pmin, pmax);
    meshBBoxShapes.update(context, &box);
    // update strokes geometry
    if (rshape.stroke) {
        strokeFirst = rshape.stroke->strokeFirst;
        WgGeometryDataGroup strokes;
        strokes.stroke(rshape);
        strokes.getBBox(pmin, pmax);
        meshGroupStrokes.update(context, &strokes);
        // update strokes bbox
        WgPoint pmin{}, pmax{};
        strokes.getBBox(pmin, pmax);
        WgGeometryData box;
        box.appendBox(pmin, pmax);
        meshBBoxStrokes.update(context, &box);
    }
}


void WgRenderDataShape::releaseMeshes(WgContext &context)
{
    meshGroupStrokes.release(context);
    meshGroupShapes.release(context);
}


void WgRenderDataShape::release(WgContext& context)
{
    meshBBoxStrokes.release(context);
    meshBBoxShapes.release(context);
    releaseMeshes(context);
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
