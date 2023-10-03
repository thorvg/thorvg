/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include "tvgWgpuRenderData.h"

//***********************************************************************
// WgpuGeometryData
//***********************************************************************

// synk
void WgpuGeometryData::draw(WGPURenderPassEncoder renderPassEncoder) {
    wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, 0, mBufferVertex, 0, mVertexCount * sizeof(float) * 3);
    wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, mBufferIndex, WGPUIndexFormat_Uint32, 0, mIndexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, mIndexCount, 1, 0, 0, 0);
}

// update
void WgpuGeometryData::update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount) {
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
void WgpuGeometryData::release() {
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
// WgpuRenderDataShape
//***********************************************************************

// initialize
void WgpuRenderDataShape::initialize(WGPUDevice device) {
}

// release
void WgpuRenderDataShape::release() {
    mBrushColorDataBindGroup.release();
}