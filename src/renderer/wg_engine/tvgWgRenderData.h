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
#include "tvgWgShaderTypes.h"

struct WgMeshData {
    Array<Point> vbuffer;
    Array<Point> tbuffer;
    Array<uint32_t> ibuffer;
    size_t voffset{};
    size_t toffset{};
    size_t ioffset{};

    void update(WgContext& context, const WgVertexBuffer& vertexBuffer);
    void update(WgContext& context, const WgIndexedVertexBuffer& vertexBufferInd);
    void bbox(WgContext& context, const Point pmin, const Point pmax);
    void imageBox(WgContext& context, float w, float h);
    void blitBox(WgContext& context);
    void release(WgContext& context) {};
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
    void append(WgContext& context, const WgIndexedVertexBuffer& vertexBufferInd);
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

    void updateFill(WgContext& context, const Fill* fill);
    void updateColor(WgContext& context, const RenderColor& c);
    void release(WgContext& context);
};

struct WgRenderDataPaint
{
    WgShaderTypePaintSettings paintSettings;
    uint32_t bindGroupPaintInd{};
    RenderRegion viewport{};
    BBox aabb{{},{}};
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
    void appendStroke(WgContext& context, const WgIndexedVertexBuffer& vertexBufferInd);
    void updateBBox(Point pmin, Point pmax);
    void updateAABB(const Matrix& tr);
    void updateMeshes(WgContext& context, const RenderShape& rshape, const Matrix& tr, WgGeometryBufferPool* pool);
    void proceedStrokes(WgContext& context, const RenderStroke* rstroke, const WgVertexBuffer& buff, WgGeometryBufferPool* pool);
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

struct WgRenderDataViewport
{
    WGPUBindGroup bindGroupViewport{};
    WGPUBuffer bufferViewport{};

    void update(WgContext& context, const RenderRegion& region);
    void release(WgContext& context);
};

class WgRenderDataViewportPool {
private:
    // pool contains all created but unused render data for viewport
    Array<WgRenderDataViewport*> mPool;
    // list contains all created render data for viewport
    // to ensure that all created instances will be released
    Array<WgRenderDataViewport*> mList;
public:
    WgRenderDataViewport* allocate(WgContext& context);
    void free(WgContext& context, WgRenderDataViewport* renderData);
    void release(WgContext& context);
};

// gaussian blur, drop shadow, fill, tint, tritone
#define WG_GAUSSIAN_MAX_LEVEL 3
struct WgRenderDataEffectParams
{
    WGPUBindGroup bindGroupParams{};
    WGPUBuffer bufferParams{};
    uint32_t extend{};
    uint32_t level{};
    Point offset{};

    void update(WgContext& context, const WgShaderTypeEffectParams& effectParams);
    void update(WgContext& context, const RenderEffectGaussianBlur* gaussian, const Matrix& transform);
    void update(WgContext& context, const RenderEffectDropShadow* dropShadow, const Matrix& transform);
    void update(WgContext& context, const RenderEffectFill* fill);
    void update(WgContext& context, const RenderEffectTint* tint);
    void update(WgContext& context, const RenderEffectTritone* tritone);
    void release(WgContext& context);
};

// effect params pool
class WgRenderDataEffectParamsPool {
private:
    // pool contains all created but unused render data for params
    Array<WgRenderDataEffectParams*> mPool;
    // list contains all created render data for params
    // to ensure that all created instances will be released
    Array<WgRenderDataEffectParams*> mList;
public:
    WgRenderDataEffectParams* allocate(WgContext& context);
    void free(WgContext& context, WgRenderDataEffectParams* renderData);
    void release(WgContext& context);
};

class WgStageBufferGeometry {
private:
    Array<uint8_t> vbuffer;
    Array<uint8_t> ibuffer;
public:
    WGPUBuffer vbuffer_gpu{};
    WGPUBuffer ibuffer_gpu{};

    void append(WgMeshData* meshData);
    void append(WgMeshDataGroup* meshDataGroup);
    void append(WgRenderDataShape* renderDataShape);
    void append(WgRenderDataPicture* renderDataPicture);
    void initialize(WgContext& context){};
    void release(WgContext& context);
    void clear();
    void flush(WgContext& context);
    void bind(WGPURenderPassEncoder renderPass, size_t voffset, size_t toffset);
};

// typed uniform stage buffer with related bind groups handling
template<typename T>
class WgStageBufferUniform {
private:
    Array<T> ubuffer;
    WGPUBuffer ubuffer_gpu{};
    Array<WGPUBindGroup> bbuffer;
public:
    // append uniform data to cpu staged buffer and return related bind group index
    uint32_t append(const T& value) {
        ubuffer.push(value);
        return ubuffer.count - 1;
    }

    void flush(WgContext& context) {
        // flush data to gpu buffer from cpu memory including reserved data to prevent future gpu buffer reallocations
        bool bufferChanged = context.allocateBufferUniform(ubuffer_gpu, (void*)ubuffer.data, ubuffer.reserved*sizeof(T));
        // if gpu buffer handle was changed we must to remove all created binding groups
        if (bufferChanged) releaseBindGroups(context);
        // allocate bind groups for all new data items
        for (uint32_t i = bbuffer.count; i < ubuffer.count; i++)
            bbuffer.push(context.layouts.createBindGroupBuffer1Un(ubuffer_gpu, i*sizeof(T), sizeof(T)));
        assert(bbuffer.count >= ubuffer.count);
    }

    // please, use index that was returned from append method
    WGPUBindGroup operator[](const uint32_t index) const {
        return bbuffer[index];
    }

    void clear() {
        ubuffer.clear();
    }

    void release(WgContext& context) {
        context.releaseBuffer(ubuffer_gpu);
        releaseBindGroups(context);
    }

    void releaseBindGroups(WgContext& context) {
        ARRAY_FOREACH(p, bbuffer)
            context.layouts.releaseBindGroup(*p);
        bbuffer.clear();
    }
};

#endif // _TVG_WG_RENDER_DATA_H_
