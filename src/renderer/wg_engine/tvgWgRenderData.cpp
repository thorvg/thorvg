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

// synk
void WgGeometryData::draw(WGPURenderPassEncoder renderPassEncoder) {
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, mBufferVertex, 0, mVertexCount * sizeof(float) * 3);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, mBufferIndex, WGPUIndexFormat_Uint32, 0, mIndexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, mIndexCount, 1, 0, 0, 0);
}

// update
void WgGeometryData::update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount) {
    // release first (Sergii: I don`t like this solution, but I can`t come up how to do it better)
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

// release
void WgGeometryData::release() {
    // buffer index destroy
    if (mBufferIndex) wgpuBufferDestroy(mBufferIndex);
    if (mBufferIndex) wgpuBufferRelease(mBufferIndex);
    mBufferIndex = nullptr;
    // buffer vertex destroy
    if (mBufferVertex) wgpuBufferDestroy(mBufferVertex);
    if (mBufferVertex) wgpuBufferRelease(mBufferVertex);
    mBufferVertex = nullptr;
    // null counts
    mVertexCount = 0;
    mIndexCount = 0;
}

//***********************************************************************
// WgRenderDataShape
//***********************************************************************

// initialize
void WgRenderDataShape::initialize(WGPUDevice device) {
}

// release
void WgRenderDataShape::release() {
    // release render data
    releaseRenderData();
    // release bind group
    mBrushBindGroupSolid.release();
    //mBrushBindGroupLinear.release();
    //mBrushBindGroupRadial.release();
}

// release render data
void WgRenderDataShape::releaseRenderData() {
    // release and delete geometry render data fill
    for (uint32_t i = 0; i < mGeometryDataFill.count; i++) {
        mGeometryDataFill[i]->release();
        delete mGeometryDataFill[i];
    }
    // clear render data fill
    mGeometryDataFill.clear();
}

// tesselate
void WgRenderDataShape::tesselate(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape) {
    // release render data
    releaseRenderData();
    // outlines
    Array<Array<float>*> outlines;
    // iterate commands
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        // get current command
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        // MoveTo command
        if (cmd == PathCommand::MoveTo) {
            // create new outline
            outlines.push(new Array<float>);
            // get last outline
            auto outline = outlines.last();
            // get point coordinates
            float x = rshape.path.pts[pntIndex].x;
            float y = rshape.path.pts[pntIndex].y;
            // push new vertex
            outline->push(x);
            outline->push(y);
            outline->push(0.0f);
            // move to next point
            pntIndex++;
        } else // LineTo command
        if (cmd == PathCommand::LineTo) {
            // get last outline
            auto outline = outlines.last();
            // get point coordinates
            float x = rshape.path.pts[pntIndex].x;
            float y = rshape.path.pts[pntIndex].y;
            // push new vertex
            outline->push(x);
            outline->push(y);
            outline->push(0.0f);
            // move to next point
            pntIndex++;
        } else // Close command
        if (cmd == PathCommand::Close) {
            // get last outline
            auto outline = outlines.last();
            // close path
            if ((outline) && (outline->count > 1)) {
                // get point coordinates
                float x = outline->data[0]; // x
                float y = outline->data[1]; // y
                // push new vertex
                outline->push(x);
                outline->push(y);
                outline->push(0.0f);
            }
        } else
        if (cmd == PathCommand::CubicTo) {
            // get last outline
            auto outline = outlines.last();
            // get cubic base points
            Point p0{};
            p0.x = outline->data[outline->count - 3];
            p0.y = outline->data[outline->count - 2];
            Point p1{};
            p1.x = rshape.path.pts[pntIndex + 0].x;
            p1.y = rshape.path.pts[pntIndex + 0].y;
            Point p2{};
            p2.x = rshape.path.pts[pntIndex + 1].x;
            p2.y = rshape.path.pts[pntIndex + 1].y;
            Point p3{};
            p3.x = rshape.path.pts[pntIndex + 2].x;
            p3.y = rshape.path.pts[pntIndex + 2].y;
            // calculate cubic spline
            const size_t segs = 16;
            for (size_t i = 1; i <= segs; i++) {
                float t = i / (float)segs;
                // get coefficients
                float t0 = 1 * (1.0f - t) * (1.0f - t) * (1.0f - t);
                float t1 = 3 * (1.0f - t) * (1.0f - t) * t;
                float t2 = 3 * (1.0f - t) * t * t;
                float t3 = 1 * t * t * t;
                // compute coordinates
                float x = p0.x * t0 + p1.x * t1 + p2.x * t2 + p3.x * t3;
                float y = p0.y * t0 + p1.y * t1 + p2.y * t2 + p3.y * t3;
                // push new vertex
                outline->push(x);
                outline->push(y);
                outline->push(0.0f);
            }
            // move to next point
            pntIndex += 3;
        } 
    }
    // create render index buffers
    Array<uint32_t> indexBuffer;
    for (size_t i = 0; i < outlines.count; i++) {
        // get current outline
        auto outline = outlines[i];
        // get vertex count
        size_t vertexCount = outline->count / 3;
        assert(vertexCount > 2);
        // index buffer (for triangle fan)
        indexBuffer.clear();
        indexBuffer.reserve((vertexCount - 2) * 3);
        for (size_t j = 0; j < vertexCount - 2; j++) {
            indexBuffer.push(0);
            indexBuffer.push(j + 1);
            indexBuffer.push(j + 2);
        }
        // create new geometry data
        WgGeometryData* geometryData = new WgGeometryData();
        geometryData->initialize(device);
        geometryData->update(device, queue, outline->data, vertexCount, indexBuffer.data, indexBuffer.count);
        // push geometry data
        mGeometryDataFill.push(geometryData);
    }
    // delete outlines
    for (size_t i = 0; i < outlines.count; i++)
        delete outlines[i];
}
