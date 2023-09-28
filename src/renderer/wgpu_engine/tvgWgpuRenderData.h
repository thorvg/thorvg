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

#include "tvgWgpuBrushColor.h"

#ifndef _TVG_WGPU_RENDER_DATA_H_
#define _TVG_WGPU_RENDER_DATA_H_

// geometry data
class WgpuGeometryData {
public:
    WGPUBuffer mBufferVertex{};
    WGPUBuffer mBufferIndex{};
    size_t     mVertexCount{};
    size_t     mIndexCount{};
public:
    // constructor and destructor
    WgpuGeometryData() {}
    virtual ~WgpuGeometryData() { release(); }

    // update
    void update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount);
    // release
    void release();
};

// render data
class WgpuRenderData {
public:
    // initialize and release
    virtual void initialize(WGPUDevice device) = 0;
    virtual void release() = 0;

    // sync (draw)
    virtual void sync(WGPUCommandBuffer commandBuffer) = 0;
};

// render data shape
class WgpuRenderDataShape: public WgpuRenderData {
public:
    // render shape (external)
    const RenderShape* mRenderShape{};
    // geometry data for fill and stroke
    WgpuGeometryData mGeometryDataFill;
    WgpuGeometryData mGeometryDataStroke;
    // brush color data
    WgpuBrushColorData          mBrushColorData{};
    WgpuBrushColorDataBindGroup mBrushColorDataBindGroup{};
public:
    // constructor
    WgpuRenderDataShape(const RenderShape* renderShape): mRenderShape(renderShape) {}

    // initialize and release
    void initialize(WGPUDevice device) override;
    void release() override;

    // sync (draw)
    void sync(WGPUCommandBuffer commandBuffer) override {}
};

#endif // _TVG_WGPU_RENDER_DATA_H_