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

#include "tvgWgRenderData.h"

//***********************************************************************
// WgGeometryData
//***********************************************************************

void WgGeometryData::draw(WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, mBufferVertex, 0, mVertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, mBufferIndex, WGPUIndexFormat_Uint32, 0, mIndexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, mIndexCount, 1, 0, 0, 0);
}


void WgGeometryData::drawImage(WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, mBufferVertex, 0, mVertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 1, mBufferTexCoords, 0, mVertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, mBufferIndex, WGPUIndexFormat_Uint32, 0, mIndexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, mIndexCount, 1, 0, 0, 0);
}


void WgGeometryData::update(WGPUDevice device, WGPUQueue queue, WgVertexList* vertexList)
{
    update(device, queue, 
           (float *)vertexList->mVertexList.data,
           vertexList->mVertexList.count,
           vertexList->mIndexList.data,
           vertexList->mIndexList.count);
}


void WgGeometryData::update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount)
{
    release();

    // buffer vertex data create and write
    WGPUBufferDescriptor bufferVertexDesc{};
    bufferVertexDesc.nextInChain = nullptr;
    bufferVertexDesc.label = "Buffer vertex geometry data";
    bufferVertexDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    bufferVertexDesc.size = sizeof(float) * vertexCount * 2; // x, y
    bufferVertexDesc.mappedAtCreation = false;
    mBufferVertex = wgpuDeviceCreateBuffer(device, &bufferVertexDesc);
    assert(mBufferVertex);
    wgpuQueueWriteBuffer(queue, mBufferVertex, 0, vertexData, vertexCount * sizeof(float) * 2);
    mVertexCount = vertexCount;
    // buffer index data create and write
    WGPUBufferDescriptor bufferIndexDesc{};
    bufferIndexDesc.nextInChain = nullptr;
    bufferIndexDesc.label = "Buffer index geometry data";
    bufferIndexDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    bufferIndexDesc.size = sizeof(uint32_t) * indexCount;
    bufferIndexDesc.mappedAtCreation = false;
    mBufferIndex = wgpuDeviceCreateBuffer(device, &bufferIndexDesc);
    assert(mBufferIndex);
    wgpuQueueWriteBuffer(queue, mBufferIndex, 0, indexData, indexCount * sizeof(uint32_t));
    mIndexCount = indexCount;
}


void WgGeometryData::update(WGPUDevice device, WGPUQueue queue, float* vertexData, float* texCoordsData, size_t vertexCount, uint32_t* indexData, size_t indexCount) {
    update(device, queue, vertexData, vertexCount, indexData, indexCount);
    // buffer tex coords data create and write
    WGPUBufferDescriptor bufferTexCoordsDesc{};
    bufferTexCoordsDesc.nextInChain = nullptr;
    bufferTexCoordsDesc.label = "Buffer tex coords geometry data";
    bufferTexCoordsDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    bufferTexCoordsDesc.size = sizeof(float) * vertexCount * 2; // u, v
    bufferTexCoordsDesc.mappedAtCreation = false;
    mBufferTexCoords = wgpuDeviceCreateBuffer(device, &bufferTexCoordsDesc);
    assert(mBufferTexCoords);
    wgpuQueueWriteBuffer(queue, mBufferTexCoords, 0, texCoordsData, vertexCount * sizeof(float) * 2);
}


void WgGeometryData::release()
{
    if (mBufferIndex) { 
        wgpuBufferDestroy(mBufferIndex);
        wgpuBufferRelease(mBufferIndex);
        mBufferIndex = nullptr;
        mVertexCount = 0;
    }
    if (mBufferTexCoords) {
        wgpuBufferDestroy(mBufferTexCoords);
        wgpuBufferRelease(mBufferTexCoords);
        mBufferTexCoords = nullptr;
    }
    if (mBufferVertex) {
        wgpuBufferDestroy(mBufferVertex);
        wgpuBufferRelease(mBufferVertex);
        mBufferVertex = nullptr;
        mIndexCount = 0;
    }
}

//***********************************************************************
// WgImageData
//***********************************************************************

void WgImageData::update(WGPUDevice device, WGPUQueue queue, Surface* surface)
{
    release();
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
    mSampler = wgpuDeviceCreateSampler(device, &samplerDesc);
    assert(mSampler);
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
    mTexture = wgpuDeviceCreateTexture(device, &textureDesc);
    assert(mTexture);
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
    mTextureView = wgpuTextureCreateView(mTexture, &textureViewDesc);
    assert(mTextureView);
    // update texture data
    WGPUImageCopyTexture imageCopyTexture{};
    imageCopyTexture.nextInChain = nullptr;
    imageCopyTexture.texture = mTexture;
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
    wgpuQueueWriteTexture(queue, &imageCopyTexture, surface->data, 4 * surface->w * surface->h, &textureDataLayout, &writeSize);
}


void WgImageData::release()
{
    if (mTexture) {
        wgpuTextureDestroy(mTexture);
        wgpuTextureRelease(mTexture);
        mTexture = nullptr;
    } 
    if (mTextureView) {
        wgpuTextureViewRelease(mTextureView);
        mTextureView = nullptr;
    }
    if (mSampler) {
        wgpuSamplerRelease(mSampler);
        mSampler = nullptr;
    }
}

//***********************************************************************
// WgRenderDataShapeSettings
//***********************************************************************

void WgRenderDataShapeSettings::update(WGPUDevice device, WGPUQueue queue,
                                       const Fill* fill, const uint8_t* color,
                                       const RenderUpdateFlag flags)
{
    // setup fill properties
    if ((flags & (RenderUpdateFlag::Gradient)) && fill) {
        // setup linear fill properties
        if (fill->identifier() == TVG_CLASS_ID_LINEAR) {
            WgShaderTypeLinearGradient linearGradient((LinearGradient*)fill);
            mBindGroupLinear.initialize(device, queue, linearGradient);
            mFillType = WgRenderDataShapeFillType::Linear;
        } else if (fill->identifier() == TVG_CLASS_ID_RADIAL) {
            WgShaderTypeRadialGradient radialGradient((RadialGradient*)fill);
            mBindGroupRadial.initialize(device, queue, radialGradient);
            mFillType = WgRenderDataShapeFillType::Radial;
        }
    } else if ((flags & (RenderUpdateFlag::Color)) && !fill) {
        WgShaderTypeSolidColor solidColor(color);
        mBindGroupSolid.initialize(device, queue, solidColor);
        mFillType = WgRenderDataShapeFillType::Solid;
    }
}

void WgRenderDataShapeSettings::release()
{
    mBindGroupSolid.release();
    mBindGroupLinear.release();
    mBindGroupRadial.release();
}

//***********************************************************************
// WgRenderDataShape
//***********************************************************************

void WgRenderDataShape::release()
{
    releaseRenderData();
    mImageData.release();
    mBindGroupPaint.release();
    mRenderSettingsShape.release();
    mRenderSettingsStroke.release();
    mBindGroupPicture.release();
}


void WgRenderDataShape::releaseRenderData()
{
    for (uint32_t i = 0; i < mGeometryDataImage.count; i++) {
        mGeometryDataImage[i]->release();
        delete mGeometryDataImage[i];
    }
    for (uint32_t i = 0; i < mGeometryDataStroke.count; i++) {
        mGeometryDataStroke[i]->release();
        delete mGeometryDataStroke[i];
    }
    mGeometryDataStroke.clear();
    for (uint32_t i = 0; i < mGeometryDataShape.count; i++) {
        mGeometryDataShape[i]->release();
        delete mGeometryDataShape[i];
    }
    mGeometryDataShape.clear();
}


void WgRenderDataShape::tesselate(WGPUDevice device, WGPUQueue queue, Surface* surface, const RenderMesh* mesh)
{
    // create image geometry data
    Array<WgPoint> vertexList;
    Array<WgPoint> texCoordsList;
    Array<uint32_t> indexList;

    if (mesh && mesh->triangleCnt) {
        vertexList.reserve(mesh->triangleCnt * 3);
        texCoordsList.reserve(mesh->triangleCnt * 3);
        indexList.reserve(mesh->triangleCnt * 3);
        for (uint32_t i = 0; i < mesh->triangleCnt; i++) {
            vertexList.push(mesh->triangles[i].vertex[0].pt);
            vertexList.push(mesh->triangles[i].vertex[1].pt);
            vertexList.push(mesh->triangles[i].vertex[2].pt);
            texCoordsList.push(mesh->triangles[i].vertex[0].uv);
            texCoordsList.push(mesh->triangles[i].vertex[1].uv);
            texCoordsList.push(mesh->triangles[i].vertex[2].uv);
            indexList.push(i*3 + 0);
            indexList.push(i*3 + 1);
            indexList.push(i*3 + 2);
        }
    } else {
        vertexList.push({ 0.0f, 0.0f });
        vertexList.push({ (float)surface->w, 0.0f });
        vertexList.push({ (float)surface->w, (float)surface->h });
        vertexList.push({ 0.0f, (float)surface->h });
        texCoordsList.push({0.0f, 0.0f});
        texCoordsList.push({1.0f, 0.0f});
        texCoordsList.push({1.0f, 1.0f});
        texCoordsList.push({0.0f, 1.0f});
        indexList.push(0);
        indexList.push(1);
        indexList.push(2);
        indexList.push(0);
        indexList.push(2);
        indexList.push(3);
    }

    // create geometry data for image
    WgGeometryData* geometryData = new WgGeometryData();
    geometryData->update(device, queue,
        (float *)vertexList.data, (float *)texCoordsList.data, vertexList.count,
        indexList.data, indexList.count);
    mGeometryDataImage.push(geometryData);

    // update image data
    mImageData.update(device, queue, surface);
}


void WgRenderDataShape::tesselate(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape)
{
    Array<WgVertexList*> outlines{};
    decodePath(rshape, outlines);

    // create geometry data for fill for each outline
    for (uint32_t i = 0; i < outlines.count; i++) {
        auto outline = outlines[i];
        // append shape if it can create at least one triangle
        if (outline->mVertexList.count > 2) {
            outline->computeTriFansIndexes();

            // create geometry data for fill using triangle fan indexes
            WgGeometryData* geometryData = new WgGeometryData();
            geometryData->update(device, queue, outline);
            mGeometryDataShape.push(geometryData);
        }
    }

    for (uint32_t i = 0; i < outlines.count; i++)
        delete outlines[i];
}


// TODO: separate to entity
void WgRenderDataShape::stroke(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape)
{
    if (!rshape.stroke) return;

    // TODO: chnage to shared_ptrs
    Array<WgVertexList*> outlines{};
    decodePath(rshape, outlines);

    WgVertexList strokes;
    if (rshape.stroke->dashPattern) {
        Array<WgVertexList*> segments;
        strokeSegments(rshape, outlines, segments);
        strokeSublines(rshape, segments, strokes);
        for (uint32_t i = 0; i < segments.count; i++)
            delete segments[i];
    } else 
        strokeSublines(rshape, outlines, strokes);

    // append shape if it can create at least one triangle
    // TODO: create single geometry data for strokes pere shape
    if (strokes.mIndexList.count > 2) {
        WgGeometryData* geometryData = new WgGeometryData();
        geometryData->initialize(device);
        geometryData->update(device, queue, &strokes);
        mGeometryDataStroke.push(geometryData);
    }

    for (uint32_t i = 0; i < outlines.count; i++)
        delete outlines[i];
}


void WgRenderDataShape::decodePath(const RenderShape& rshape, Array<WgVertexList*>& outlines)
{
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        if (cmd == PathCommand::MoveTo) {
            outlines.push(new WgVertexList);
            auto outline = outlines.last();
            outline->mVertexList.push(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::LineTo) {
            auto outline = outlines.last();
            if (outline)
                outline->mVertexList.push(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::Close) {
            auto outline = outlines.last();
            if ((outline) && (outline->mVertexList.count > 0))
                outline->mVertexList.push(outline->mVertexList[0]);
        } else if (cmd == PathCommand::CubicTo) {
            auto outline = outlines.last();
            if ((outline) && (outline->mVertexList.count > 0))
                outline->appendCubic(
                    rshape.path.pts[pntIndex + 0],
                    rshape.path.pts[pntIndex + 1],
                    rshape.path.pts[pntIndex + 2]
                );
            pntIndex += 3;
        } 
    }
}


void WgRenderDataShape::strokeSegments(const RenderShape& rshape, Array<WgVertexList*>& outlines, Array<WgVertexList*>& segments)
{
    for (uint32_t i = 0; i < outlines.count; i++) {
        auto& vlist = outlines[i]->mVertexList;
        
        // append single point segment
        if (vlist.count == 1) {
            auto segment = new WgVertexList();
            segment->mVertexList.push(vlist.last());
            segments.push(segment);
        }

        if (vlist.count >= 2) {
            uint32_t icurr = 1;
            uint32_t ipatt = 0;
            WgPoint vcurr = vlist[0];
            while (icurr < vlist.count) {
                if (ipatt % 2 == 0) {
                    segments.push(new WgVertexList());
                    segments.last()->mVertexList.push(vcurr);
                }
                float lcurr = rshape.stroke->dashPattern[ipatt];
                while ((icurr < vlist.count) && (vlist[icurr].dist(vcurr) < lcurr)) {
                    lcurr -= vlist[icurr].dist(vcurr);
                    vcurr = vlist[icurr];
                    icurr++;
                    if (ipatt % 2 == 0) segments.last()->mVertexList.push(vcurr);
                }
                if (icurr < vlist.count) {
                    vcurr = vcurr + (vlist[icurr] - vlist[icurr-1]).normal() * lcurr;
                    if (ipatt % 2 == 0) segments.last()->mVertexList.push(vcurr);
                }
                ipatt = (ipatt + 1) % rshape.stroke->dashCnt;
            }
        }
    }
}


void WgRenderDataShape::strokeSublines(const RenderShape& rshape, Array<WgVertexList*>& outlines, WgVertexList& strokes)
{
    float wdt = rshape.stroke->width / 2;
    for (uint32_t i = 0; i < outlines.count; i++) {
        auto outline = outlines[i];

        // single point sub-path
        if (outline->mVertexList.count == 1) {
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes.appendCircle(outline->mVertexList[0], wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                // for zero length sub-paths no stroke is rendered
            } else if (rshape.stroke->cap == StrokeCap::Square) {
                strokes.appendRect(
                    outline->mVertexList[0] + WgPoint(+wdt, +wdt),
                    outline->mVertexList[0] + WgPoint(+wdt, -wdt),
                    outline->mVertexList[0] + WgPoint(-wdt, +wdt),
                    outline->mVertexList[0] + WgPoint(-wdt, -wdt)
                );
            }
        }

        // single line sub-path
        if (outline->mVertexList.count == 2) {
            WgPoint v0 = outline->mVertexList[0];
            WgPoint v1 = outline->mVertexList[1];
            WgPoint dir0 = (v1 - v0).normal();
            WgPoint nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes.appendRect(
                    v0 - nrm0 * wdt, v0 + nrm0 * wdt, 
                    v1 - nrm0 * wdt, v1 + nrm0 * wdt);
                strokes.appendCircle(outline->mVertexList[0], wdt);
                strokes.appendCircle(outline->mVertexList[1], wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                strokes.appendRect(
                    v0 - nrm0 * wdt, v0 + nrm0 * wdt,
                    v1 - nrm0 * wdt, v1 + nrm0 * wdt
                );
            } else if (rshape.stroke->cap == StrokeCap::Square) {
                strokes.appendRect(
                    v0 - nrm0 * wdt - dir0 * wdt, v0 + nrm0 * wdt - dir0 * wdt,
                    v1 - nrm0 * wdt + dir0 * wdt, v1 + nrm0 * wdt + dir0 * wdt
                );
            }
        }

        // multi-lined sub-path
        if (outline->mVertexList.count > 2) {
            // append first cap
            WgPoint v0 = outline->mVertexList[0];
            WgPoint v1 = outline->mVertexList[1];
            WgPoint dir0 = (v1 - v0).normal();
            WgPoint nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes.appendCircle(v0, wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (rshape.stroke->cap == StrokeCap::Square) {
                strokes.appendRect(
                    v0 - nrm0 * wdt - dir0 * wdt,
                    v0 + nrm0 * wdt - dir0 * wdt,
                    v0 - nrm0 * wdt,
                    v0 + nrm0 * wdt
                );
            }

            // append last cap
            v0 = outline->mVertexList[outline->mVertexList.count - 2];
            v1 = outline->mVertexList[outline->mVertexList.count - 1];
            dir0 = (v1 - v0).normal();
            nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes.appendCircle(v1, wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (rshape.stroke->cap == StrokeCap::Square) {
                strokes.appendRect(
                    v1 - nrm0 * wdt,
                    v1 + nrm0 * wdt,
                    v1 - nrm0 * wdt + dir0 * wdt,
                    v1 + nrm0 * wdt + dir0 * wdt
                );
            }

            // append sub-lines
            for (uint32_t j = 0; j < outline->mVertexList.count - 1; j++) {
                WgPoint v0 = outline->mVertexList[j + 0];
                WgPoint v1 = outline->mVertexList[j + 1];
                WgPoint dir = (v1 - v0).normal();
                WgPoint nrm { -dir.y, +dir.x };
                strokes.appendRect(
                    v0 - nrm * wdt,
                    v0 + nrm * wdt,
                    v1 - nrm * wdt,
                    v1 + nrm * wdt
                );
            }

            // append joints (TODO: separate by joint types)
            for (uint32_t j = 1; j < outline->mVertexList.count - 1; j++) {
                WgPoint v0 = outline->mVertexList[j - 1];
                WgPoint v1 = outline->mVertexList[j + 0];
                WgPoint v2 = outline->mVertexList[j + 1];
                WgPoint dir0 = (v1 - v0).normal();
                WgPoint dir1 = (v1 - v2).normal();
                WgPoint nrm0 { -dir0.y, +dir0.x };
                WgPoint nrm1 { +dir1.y, -dir1.x };
                if (rshape.stroke->join == StrokeJoin::Round) {
                    strokes.appendCircle(v1, wdt);
                } else if (rshape.stroke->join == StrokeJoin::Bevel) {
                    strokes.appendRect(
                        v1 - nrm0 * wdt, v1 - nrm1 * wdt,
                        v1 + nrm1 * wdt, v1 + nrm0 * wdt
                    );
                } else if (rshape.stroke->join == StrokeJoin::Miter) {
                    WgPoint nrm = (dir0 + dir1).normal();
                    float cosine = nrm.dot(nrm0);
                    if ((cosine != 0.0f) && (abs(wdt / cosine) <= rshape.stroke->miterlimit * 2)) {
                        strokes.appendRect(v1 + nrm * (wdt / cosine), v1 + nrm0 * wdt, v1 + nrm1 * wdt, v1);
                        strokes.appendRect(v1 - nrm * (wdt / cosine), v1 - nrm0 * wdt, v1 - nrm1 * wdt, v1);
                    } else {
                        strokes.appendRect(
                            v1 - nrm0 * wdt, v1 - nrm1 * wdt,
                            v1 + nrm1 * wdt, v1 + nrm0 * wdt);
                    }
                }
            }
        }
    }
}
