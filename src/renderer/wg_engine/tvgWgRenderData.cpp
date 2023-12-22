
/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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
    release(context);
    assert(geometryData);
    // buffer position data create and write
    if(geometryData->positions.count > 0) {
        vertexCount = geometryData->positions.count;
        WGPUBufferDescriptor bufferDesc{};
        bufferDesc.nextInChain = nullptr;
        bufferDesc.label = "Buffer position geometry data";
        bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
        bufferDesc.size = sizeof(float) * vertexCount * 2; // x, y
        bufferDesc.mappedAtCreation = false;
        bufferPosition = wgpuDeviceCreateBuffer(context.device, &bufferDesc);
        assert(bufferPosition);
        wgpuQueueWriteBuffer(context.queue, bufferPosition, 0, &geometryData->positions[0], vertexCount * sizeof(float) * 2);
    }
    // buffer vertex data create and write
    if(geometryData->texCoords.count > 0) {
        WGPUBufferDescriptor bufferDesc{};
        bufferDesc.nextInChain = nullptr;
        bufferDesc.label = "Buffer tex coords geometry data";
        bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
        bufferDesc.size = sizeof(float) * vertexCount * 2; // u, v
        bufferDesc.mappedAtCreation = false;
        bufferTexCoord = wgpuDeviceCreateBuffer(context.device, &bufferDesc);
        assert(bufferPosition);
        wgpuQueueWriteBuffer(context.queue, bufferTexCoord, 0, &geometryData->texCoords[0], vertexCount * sizeof(float) * 2);
    }
    // buffer index data create and write
    if(geometryData->indexes.count > 0) {
        indexCount = geometryData->indexes.count;
        WGPUBufferDescriptor bufferDesc{};
        bufferDesc.nextInChain = nullptr;
        bufferDesc.label = "Buffer index geometry data";
        bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
        bufferDesc.size = sizeof(uint32_t) * indexCount;
        bufferDesc.mappedAtCreation = false;
        bufferIndex = wgpuDeviceCreateBuffer(context.device, &bufferDesc);
        assert(bufferIndex);
        wgpuQueueWriteBuffer(context.queue, bufferIndex, 0, &geometryData->indexes[0], indexCount * sizeof(uint32_t));
    }
};


void WgMeshData::release(WgContext& context)
{
    if (bufferIndex) { 
        wgpuBufferDestroy(bufferIndex);
        wgpuBufferRelease(bufferIndex);
        bufferIndex = nullptr;
        indexCount = 0;
    }
    if (bufferTexCoord) {
        wgpuBufferDestroy(bufferTexCoord);
        wgpuBufferRelease(bufferTexCoord);
        bufferTexCoord = nullptr;
    }
    if (bufferPosition) {
        wgpuBufferDestroy(bufferPosition);
        wgpuBufferRelease(bufferPosition);
        bufferPosition = nullptr;
        bufferPosition = 0;
    }
};

//***********************************************************************
// WgMeshDataGroup
//***********************************************************************

void WgMeshDataGroup::update(WgContext& context, WgGeometryDataGroup* geometryDataGroup)
{
    release(context);
    assert(geometryDataGroup);
    for (uint32_t i = 0; i < geometryDataGroup->geometries.count; i++) {
        if (geometryDataGroup->geometries[i]->positions.count > 2) {
            meshes.push(new WgMeshData());
            meshes.last()->update(context, geometryDataGroup->geometries[i]);
        }
    }
};


void WgMeshDataGroup::release(WgContext& context)
{
    for (uint32_t i = 0; i < meshes.count; i++)
        meshes[i]->release(context);
    meshes.clear();
};

//***********************************************************************
// WgImageData
//***********************************************************************

void WgImageData::update(WgContext& context, Surface* surface)
{
    release(context);
    assert(surface);
    // sampler descriptor
    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.nextInChain = nullptr;
    samplerDesc.label = "The shape sampler";
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Nearest;
    samplerDesc.minFilter = WGPUFilterMode_Nearest;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.compare = WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;
    sampler = wgpuDeviceCreateSampler(context.device, &samplerDesc);
    assert(sampler);
    // texture descriptor
    WGPUTextureDescriptor textureDesc{};
    textureDesc.nextInChain = nullptr;
    textureDesc.label = "The shape texture";
    textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size = { surface->w, surface->h, 1 };
    textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    texture = wgpuDeviceCreateTexture(context.device, &textureDesc);
    assert(texture);
    // texture view descriptor
    WGPUTextureViewDescriptor textureViewDesc{};
    textureViewDesc.nextInChain = nullptr;
    textureViewDesc.label = "The shape texture view";
    textureViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
    textureViewDesc.dimension = WGPUTextureViewDimension_2D;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.aspect = WGPUTextureAspect_All;
    textureView = wgpuTextureCreateView(texture, &textureViewDesc);
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
    if (textureView) {
        wgpuTextureViewRelease(textureView);
        textureView = nullptr;
    }
    if (texture) {
        wgpuTextureDestroy(texture);
        wgpuTextureRelease(texture);
        texture = nullptr;
    } 
    if (sampler) {
        wgpuSamplerRelease(sampler);
        sampler = nullptr;
    }
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
    meshBBoxStrokes.release(context);
    meshBBoxShapes.release(context);
    meshGroupStrokes.release(context);
    meshGroupShapes.release(context);
}


void WgRenderDataShape::release(WgContext& context)
{
    releaseMeshes(context);
    renderSettingsStroke.release(context);
    renderSettingsShape.release(context);
    WgRenderDataPaint::release(context);
};

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
