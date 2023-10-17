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
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, mBufferVertex, 0, mVertexCount * sizeof(float) * 3);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, mBufferIndex, WGPUIndexFormat_Uint32, 0, mIndexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, mIndexCount, 1, 0, 0, 0);
}

void WgGeometryData::update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount) {
    release();

    // buffer vertex data create and write
    WGPUBufferDescriptor bufferVertexDesc{};
    bufferVertexDesc.nextInChain = nullptr;
    bufferVertexDesc.label = "Buffer vertex geometry data";
    bufferVertexDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    bufferVertexDesc.size = sizeof(float) * vertexCount * 3; // x, y, z
    bufferVertexDesc.mappedAtCreation = false;
    mBufferVertex = wgpuDeviceCreateBuffer(device, &bufferVertexDesc);
    assert(mBufferVertex);
    wgpuQueueWriteBuffer(queue, mBufferVertex, 0, vertexData, sizeof(float) * vertexCount * 3);
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
    wgpuQueueWriteBuffer(queue, mBufferIndex, 0, indexData, sizeof(uint32_t) * indexCount);
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
// WgRenderDataShape
//***********************************************************************

void WgRenderDataShape::release() {
    releaseRenderData();
    mPipelineBindGroupSolid.release();
}

void WgRenderDataShape::releaseRenderData() {
    for (uint32_t i = 0; i < mGeometryDataFill.count; i++) {
        mGeometryDataFill[i]->release();
        delete mGeometryDataFill[i];
    }
    mGeometryDataFill.clear();
}

void WgRenderDataShape::tesselate(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape) {
    releaseRenderData();
    
    Array<Array<float>*> outlines;
    
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        if (cmd == PathCommand::MoveTo) {
            outlines.push(new Array<float>);

            auto outline = outlines.last();
            outline->push(rshape.path.pts[pntIndex].x);
            outline->push(rshape.path.pts[pntIndex].y);
            outline->push(0.0f);

            pntIndex++;
        } else if (cmd == PathCommand::LineTo) {
            auto outline = outlines.last();
            outline->push(rshape.path.pts[pntIndex].x);
            outline->push(rshape.path.pts[pntIndex].y);
            outline->push(0.0f);
            
            pntIndex++;
        } else if (cmd == PathCommand::Close) {
            auto outline = outlines.last();
            if ((outline) && (outline->count > 1)) {
                outline->push(outline->data[0]);
                outline->push(outline->data[1]);
                outline->push(0.0f);
            }
        } else if (cmd == PathCommand::CubicTo) {
            auto outline = outlines.last();
            
            Point p0 = { outline->data[outline->count - 3], outline->data[outline->count - 2] };
            Point p1 = { rshape.path.pts[pntIndex + 0].x, rshape.path.pts[pntIndex + 0].y };
            Point p2 = { rshape.path.pts[pntIndex + 1].x, rshape.path.pts[pntIndex + 1].y };
            Point p3 = { rshape.path.pts[pntIndex + 2].x, rshape.path.pts[pntIndex + 2].y };
            
            const size_t segs = 16;
            for (size_t i = 1; i <= segs; i++) {
                float t = i / (float)segs;
                // get cubic spline interpolation coefficients
                float t0 = 1 * (1.0f - t) * (1.0f - t) * (1.0f - t);
                float t1 = 3 * (1.0f - t) * (1.0f - t) * t;
                float t2 = 3 * (1.0f - t) * t * t;
                float t3 = 1 * t * t * t;
                
                outline->push(p0.x * t0 + p1.x * t1 + p2.x * t2 + p3.x * t3);
                outline->push(p0.y * t0 + p1.y * t1 + p2.y * t2 + p3.y * t3);
                outline->push(0.0f);
            }
            
            pntIndex += 3;
        } 
    }

    // create render index buffers to emulate triangle fan polygon
    Array<uint32_t> indexBuffer;
    for (size_t i = 0; i < outlines.count; i++) {
        auto outline = outlines[i];

        size_t vertexCount = outline->count / 3;
        assert(vertexCount > 2);

        indexBuffer.clear();
        indexBuffer.reserve((vertexCount - 2) * 3);
        for (size_t j = 0; j < vertexCount - 2; j++) {
            indexBuffer.push(0);
            indexBuffer.push(j + 1);
            indexBuffer.push(j + 2);
        }
        
        WgGeometryData* geometryData = new WgGeometryData();
        geometryData->initialize(device);
        geometryData->update(device, queue, outline->data, vertexCount, indexBuffer.data, indexBuffer.count);
        
        mGeometryDataFill.push(geometryData);

    }
    for (size_t i = 0; i < outlines.count; i++)
        delete outlines[i];
}
