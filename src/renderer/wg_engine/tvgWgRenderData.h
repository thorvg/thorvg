/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

    void update(WgContext& context, const WgPolyline* polyline);
    void update(WgContext& context, const WgGeometryData* geometryData);
    void update(WgContext& context, const WgPoint pmin, const WgPoint pmax);
    void release(WgContext& context);
};

class WgMeshDataPool {
private:
    Array<WgMeshData*> mPool;
    Array<WgMeshData*> mList;
public:
    WgMeshData* allocate(WgContext& context);
    void free(WgContext& context, WgMeshData* meshData);
    void release(WgContext& context);
};

struct WgMeshDataGroup {
    static WgMeshDataPool* gMeshDataPool;

    Array<WgMeshData*> meshes{};
    
    void append(WgContext& context, const WgPolyline* polyline);
    void append(WgContext& context, const WgGeometryData* geometryData);
    void append(WgContext& context, const WgPoint pmin, const WgPoint pmax);
    void release(WgContext& context);
};

struct WgImageData {
    WGPUTexture texture{};
    WGPUTextureView textureView{};

    void update(WgContext& context, Surface* surface);
    void release(WgContext& context);
};

enum class WgRenderSettingsType { None = 0, Solid = 1, Linear = 2, Radial = 3 };

struct WgRenderSettings
{
    WgBindGroupSolidColor bindGroupSolid{};
    WgBindGroupLinearGradient bindGroupLinear{};
    WgBindGroupRadialGradient bindGroupRadial{};
    WgRenderSettingsType fillType{};
    bool skip{};

    void update(WgContext& context, const Fill* fill, const uint8_t* color, const RenderUpdateFlag flags);
    void release(WgContext& context);
};

struct WgRenderDataPaint
{
    WgBindGroupPaint bindGroupPaint{};
    float opacity{};

    virtual ~WgRenderDataPaint() {};
    virtual void release(WgContext& context);
    virtual uint32_t identifier() { return TVG_CLASS_ID_UNDEFINED; };
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
    WgPoint pMin{};
    WgPoint pMax{};
    bool strokeFirst{};
    FillRule fillRule{};

    void updateBBox(WgPoint pmin, WgPoint pmax);
    void updateMeshes(WgContext& context, const RenderShape& rshape, const Matrix* rt);
    void updateMeshes(WgContext& context, const WgPolyline* polyline, const RenderStroke* rstroke);
    void releaseMeshes(WgContext& context);
    void release(WgContext& context) override;
    uint32_t identifier() override { return TVG_CLASS_ID_SHAPE; };
};

class WgRenderDataShapePool {
private:
    Array<WgRenderDataShape*> mPool;
    Array<WgRenderDataShape*> mList;
public:
    WgRenderDataShape* allocate(WgContext& context);
    void free(WgContext& context, WgRenderDataShape* dataShape);
    void release(WgContext& context);
};

struct WgRenderDataPicture: public WgRenderDataPaint
{
    WgBindGroupPicture bindGroupPicture{};
    WgImageData imageData{};
    WgMeshData meshData{};

    void update(WgContext& context);
    void release(WgContext& context) override;
    uint32_t identifier() override { return TVG_CLASS_ID_PICTURE; };
};
