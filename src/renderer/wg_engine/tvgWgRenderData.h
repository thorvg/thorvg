/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

struct WgMeshData {
    WGPUBuffer bufferPosition{};
    WGPUBuffer bufferTexCoord{};
    WGPUBuffer bufferIndex{};
    size_t vertexCount{};
    size_t indexCount{};

    void draw(WgContext& context, WGPURenderPassEncoder renderPassEncoder);
    void drawFan(WgContext& context, WGPURenderPassEncoder renderPassEncoder);
    void drawImage(WgContext& context, WGPURenderPassEncoder renderPassEncoder);

    void update(WgContext& context, const WgVertexBuffer& vertexBuffer);
    void update(WgContext& context, const WgVertexBufferInd& vertexBufferInd);
    void bbox(WgContext& context, const Point pmin, const Point pmax);
    void imageBox(WgContext& context, float w, float h);
    void blitBox(WgContext& context);
    void release(WgContext& context);
};

class WgMeshDataPool {
private:
    Array<WgMeshData*> mPool;
    Array<WgMeshData*> mList;
public:
    static WgMeshDataPool* gMeshDataPool;
    WgMeshData* allocate(WgContext& context);
    void free(WgContext& context, WgMeshData* meshData);
    void release(WgContext& context);
};

struct WgMeshDataGroup {
    Array<WgMeshData*> meshes{};
    
    void append(WgContext& context, const WgVertexBuffer& vertexBuffer);
    void append(WgContext& context, const WgVertexBufferInd& vertexBufferInd);
    void append(WgContext& context, const Point pmin, const Point pmax);
    void release(WgContext& context);
};

struct WgImageData {
    WGPUTexture texture{};
    WGPUTextureView textureView{};

    void update(WgContext& context, const RenderSurface* surface);
    void release(WgContext& context);
};

enum class WgRenderSettingsType { None = 0, Solid = 1, Linear = 2, Radial = 3 };
enum class WgRenderRasterType { Solid = 0, Gradient, Image };

struct WgRenderSettings
{
    WGPUBuffer bufferGroupSolid{};
    WGPUBindGroup bindGroupSolid{};
    WGPUTexture texGradient{};
    WGPUTextureView texViewGradient{};
    WGPUBuffer bufferGroupGradient{};
    WGPUBuffer bufferGroupTransfromGrad{};
    WGPUBindGroup bindGroupGradient{};
    WgRenderSettingsType fillType{};
    WgRenderRasterType rasterType{};
    bool skip{};

    void update(WgContext& context, const Fill* fill, const RenderColor& c, const RenderUpdateFlag flags);
    void release(WgContext& context);
};

struct WgRenderDataPaint
{
    WGPUBuffer bufferModelMat{};
    WGPUBuffer bufferBlendSettings{};
    WGPUBindGroup bindGroupPaint{};
    RenderRegion viewport{};
    RenderRegion aabb{};
    float opacity{};
    Array<WgRenderDataPaint*> clips;

    virtual ~WgRenderDataPaint() {};
    virtual void release(WgContext& context);
    virtual Type type() { return Type::Undefined; };

    void update(WgContext& context, const tvg::Matrix& transform, tvg::ColorSpace cs, uint8_t opacity);
    void updateClips(tvg::Array<tvg::RenderData> &clips);
};

struct WgRenderDataShape: public WgRenderDataPaint
{
    WgRenderSettings renderSettingsShape{};
    WgRenderSettings renderSettingsStroke{};
    WgMeshDataGroup meshGroupShapes{};
    WgMeshDataGroup meshGroupShapesBBox{};
    WgMeshData meshDataBBox{};
    WgMeshDataGroup meshGroupStrokes{};
    WgMeshDataGroup meshGroupStrokesBBox{};
    Point pMin{};
    Point pMax{};
    bool strokeFirst{};
    FillRule fillRule{};

    void appendShape(WgContext& context, const WgVertexBuffer& vertexBuffer);
    void appendStroke(WgContext& context, const WgVertexBufferInd& vertexBufferInd);
    void updateBBox(Point pmin, Point pmax);
    void updateAABB(const Matrix& tr);
    void updateMeshes(WgContext& context, const RenderShape& rshape, const Matrix& tr);
    void proceedStrokes(WgContext& context, const RenderStroke* rstroke, const WgVertexBuffer& buff);
    void releaseMeshes(WgContext& context);
    void release(WgContext& context) override;
    Type type() override { return Type::Shape; };
};

class WgRenderDataShapePool {
private:
    Array<WgRenderDataShape*> mPool;
    Array<WgRenderDataShape*> mList;
public:
    WgRenderDataShape* allocate(WgContext& context);
    void free(WgContext& context, WgRenderDataShape* renderData);
    void release(WgContext& context);
};

struct WgRenderDataPicture: public WgRenderDataPaint
{
    WGPUBindGroup bindGroupPicture{};
    WgImageData imageData{};
    WgMeshData meshData{};

    void updateSurface(WgContext& context, const RenderSurface* surface);
    void release(WgContext& context) override;
    Type type() override { return Type::Picture; };
};

class WgRenderDataPicturePool {
private:
    Array<WgRenderDataPicture*> mPool;
    Array<WgRenderDataPicture*> mList;
public:
    WgRenderDataPicture* allocate(WgContext& context);
    void free(WgContext& context, WgRenderDataPicture* dataPicture);
    void release(WgContext& context);
};

#endif // _TVG_WG_RENDER_DATA_H_
