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

#include "tvgWgPipelines.h"
#include "tvgWgGeometry.h"

struct WgGeometryData
{
    WGPUBuffer mBufferVertex{};
    WGPUBuffer mBufferTexCoords{};
    WGPUBuffer mBufferIndex{};
    size_t mVertexCount{};
    size_t mIndexCount{};

    WgGeometryData() {}
    virtual ~WgGeometryData() { release(); }

    void initialize(WGPUDevice device) {};
    void draw(WGPURenderPassEncoder renderPassEncoder);
    void drawImage(WGPURenderPassEncoder renderPassEncoder);
    void update(WGPUDevice device, WGPUQueue queue, WgVertexList* vertexList);
    void update(WGPUDevice device, WGPUQueue queue, float* vertexData, size_t vertexCount, uint32_t* indexData, size_t indexCount);
    void update(WGPUDevice device, WGPUQueue queue, float* vertexData, float* texCoordsData, size_t vertexCount, uint32_t* indexData, size_t indexCount);
    void release();
};

struct WgImageData
{
    WGPUSampler mSampler{};
    WGPUTexture mTexture{};
    WGPUTextureView mTextureView{};

    WgImageData() {}
    virtual ~WgImageData() { release(); }

    void initialize(WGPUDevice device) {};
    void update(WGPUDevice device, WGPUQueue queue, Surface* surface);
    void release();
};

class WgRenderData
{
public:
    virtual void initialize(WGPUDevice device) {};
    virtual void release() = 0;
};

enum class WgRenderDataShapeFillType { None = 0, Solid = 1, Linear = 2, Radial = 3 };

struct WgRenderDataShapeSettings
{
    WgBindGroupSolidColor mBindGroupSolid{};
    WgBindGroupLinearGradient mBindGroupLinear{};
    WgBindGroupRadialGradient mBindGroupRadial{};
    WgRenderDataShapeFillType mFillType{}; // Default: None

    // update render shape settings defined by flags and fill settings
    void update(WGPUDevice device, WGPUQueue queue,
                const Fill* fill, const uint8_t* color, const RenderUpdateFlag flags);
    void release();
};

class WgRenderDataShape: public WgRenderData
{
public:
    // geometry data for shapes, strokes and image
    Array<WgGeometryData*> mGeometryDataShape;
    Array<WgGeometryData*> mGeometryDataStroke;
    Array<WgGeometryData*> mGeometryDataImage;
    WgImageData mImageData;

    // shader settings
    WgBindGroupPaint mBindGroupPaint;
    WgRenderDataShapeSettings mRenderSettingsShape;
    WgRenderDataShapeSettings mRenderSettingsStroke;
    WgBindGroupPicture mBindGroupPicture;
public:
    WgRenderDataShape() {}
    
    void release() override;
    void releaseRenderData();

    void tesselate(WGPUDevice device, WGPUQueue queue, Surface* surface, const RenderMesh* mesh);
    void tesselate(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape);
    void stroke(WGPUDevice device, WGPUQueue queue, const RenderShape& rshape);
private:
    void decodePath(const RenderShape& rshape, Array<WgVertexList*>& outlines);
    void strokeSegments(const RenderShape& rshape, Array<WgVertexList*>& outlines, Array<WgVertexList*>& segments);
    void strokeSublines(const RenderShape& rshape, Array<WgVertexList*>& outlines, WgVertexList& strokes);
};

#endif //_TVG_WG_RENDER_DATA_H_
