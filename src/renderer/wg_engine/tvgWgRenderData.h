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

#include "tvgWgPipelineSolid.h"
#include "tvgWgPipelineLinear.h"
#include "tvgWgPipelineRadial.h"
#include "tvgWgGeometry.h"

class WgGeometryData {
public:
    WGPUBuffer mBufferVertex{};
    WGPUBuffer mBufferIndex{};
    size_t mVertexCount{};
    size_t mIndexCount{};
public:
    WgGeometryData() {}
    virtual ~WgGeometryData() { release(); }

    void initialize(WGPUDevice device) {};
    void draw(WGPURenderPassEncoder renderPassEncoder);
    void update(WGPUDevice device, WGPUQueue queue, WgVertexList* vertexList);
    void update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount);
    void release();
};

class WgRenderData {
public:
    virtual void initialize(WGPUDevice device) {};
    virtual void release() = 0;
};

struct WgRenderDataShapeSettings {
    WgPipelineBindGroupSolid mPipelineBindGroupSolid{};
    WgPipelineBindGroupLinear mPipelineBindGroupLinear{};
    WgPipelineBindGroupRadial mPipelineBindGroupRadial{};

    WgPipelineBase* mPipelineBase{}; // external
    WgPipelineBindGroup* mPipelineBindGroup{}; // external

    // update render shape settings defined by flags and fill settings
    void update(WGPUQueue queue, const Fill* fill, const RenderUpdateFlag flags,
                const RenderTransform* transform, const float* viewMatrix, const uint8_t* color,
                WgPipelineLinear& linear, WgPipelineRadial& radial, WgPipelineSolid& solid);
    void release();
};

class WgRenderDataShape: public WgRenderData {
public:
    Array<WgGeometryData*> mGeometryDataShape;
    Array<WgGeometryData*> mGeometryDataStroke;

    WgRenderDataShapeSettings mRenderSettingsShape;
    WgRenderDataShapeSettings mRenderSettingsStroke;
public:
    WgRenderDataShape() {}
    
    void release() override;
    void releaseRenderData();

    void tesselate(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape);
    void stroke(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape);
private:
    void decodePath(const RenderShape& rshape, Array<WgVertexList*>& outlines);
    void strokeSublines(const RenderShape& rshape, Array<WgVertexList*>& outlines, WgVertexList& strokes);
};

#endif //_TVG_WG_RENDER_DATA_H_
