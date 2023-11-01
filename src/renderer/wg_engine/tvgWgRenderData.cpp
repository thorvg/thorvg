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

void WgGeometryData::draw(WGPURenderPassEncoder renderPassEncoder) {
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, mBufferVertex, 0, mVertexCount * sizeof(float) * 2);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, mBufferIndex, WGPUIndexFormat_Uint32, 0, mIndexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, mIndexCount, 1, 0, 0, 0);
}

void WgGeometryData::update(WGPUDevice device, WGPUQueue queue, WgVertexList* vertexList) {
    update(device, queue, 
           (float *)vertexList->mVertexList.data,
           vertexList->mVertexList.count,
           vertexList->mIndexList.data,
           vertexList->mIndexList.count);
}

void WgGeometryData::update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount) {
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

void WgGeometryData::release() {
    if (mBufferIndex) { 
        wgpuBufferDestroy(mBufferIndex);
        wgpuBufferRelease(mBufferIndex);
        mBufferIndex = nullptr;
        mVertexCount = 0;
    }
    if (mBufferVertex) {
        wgpuBufferDestroy(mBufferVertex);
        wgpuBufferRelease(mBufferVertex);
        mBufferVertex = nullptr;
        mIndexCount = 0;
    }
}

//***********************************************************************
// WgRenderDataShapeSettings
//***********************************************************************

void WgRenderDataShapeSettings::update(WGPUQueue queue, const Fill* fill, const RenderUpdateFlag flags,
                                       const RenderTransform* transform, const float* viewMatrix, const uint8_t* color,
                                       WgPipelineLinear& linear, WgPipelineRadial& radial, WgPipelineSolid& solid)

{
    // setup fill properties
    if ((flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) && fill) {
        // setup linear fill properties
        if (fill->identifier() == TVG_CLASS_ID_LINEAR) {
            WgPipelineDataLinear brushDataLinear{};
            brushDataLinear.updateMatrix(viewMatrix, transform);
            brushDataLinear.updateGradient((LinearGradient*)fill);
            mPipelineBindGroupLinear.update(queue, brushDataLinear);

            mPipelineBindGroup = &mPipelineBindGroupLinear;
            mPipelineBase = &linear;
        } // setup radial fill properties
        else if (fill->identifier() == TVG_CLASS_ID_RADIAL) {
            WgPipelineDataRadial brushDataRadial{};
            brushDataRadial.updateMatrix(viewMatrix, transform);
            brushDataRadial.updateGradient((RadialGradient*)fill);
            mPipelineBindGroupRadial.update(queue, brushDataRadial);

            mPipelineBindGroup = &mPipelineBindGroupRadial;
            mPipelineBase = &radial;
        }
    } // setup solid fill properties
    else if ((flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Transform)) && !fill) {
        WgPipelineDataSolid pipelineDataSolid{};
        pipelineDataSolid.updateMatrix(viewMatrix, transform);
        pipelineDataSolid.updateColor(color);
        mPipelineBindGroupSolid.update(queue, pipelineDataSolid);

        mPipelineBindGroup = &mPipelineBindGroupSolid;
        mPipelineBase = &solid;
    }
}

void WgRenderDataShapeSettings::release() {
    mPipelineBindGroupSolid.release();
    mPipelineBindGroupLinear.release();
    mPipelineBindGroupRadial.release();
}

//***********************************************************************
// WgRenderDataShape
//***********************************************************************

void WgRenderDataShape::release() {
    releaseRenderData();
    mRenderSettingsShape.release();
    mRenderSettingsStroke.release();
}

void WgRenderDataShape::releaseRenderData() {
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

void WgRenderDataShape::tesselate(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape) {
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
void WgRenderDataShape::stroke(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape) {
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

void WgRenderDataShape::decodePath(const RenderShape& rshape, Array<WgVertexList*>& outlines) {
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

void WgRenderDataShape::strokeSegments(const RenderShape& rshape, Array<WgVertexList*>& outlines, Array<WgVertexList*>& segments) {
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

void WgRenderDataShape::strokeSublines(const RenderShape& rshape, Array<WgVertexList*>& outlines, WgVertexList& strokes) {
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