/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

struct WgTextureMgr;
struct WgStageBufferSolidColor;

struct WgImageData
{
    WGPUTexture texture{};
    WGPUTextureView textureView{};
    WGPUBindGroup bindGroup{};

    void update(WgContext& context, const Fill* fill, FillSpread& currentSpread);
    void release(WgContext& context);
};

enum class WgRenderSettingsType { None = 0, Solid = 1, Linear = 2, Radial = 3, Conic = 4 };

static_assert(sizeof(RenderColor) == 4, "Solid color vertex data must remain tightly packed RGBA8");

struct WgSolidData
{
    uint32_t colorIdx{};
    RenderColor color{};
    uint8_t opacity = 255;

    RenderColor packedColor() const
    {
        return {color.r, color.g, color.b, MULTIPLY(color.a, opacity)};
    }
};

struct WgRenderSettings
{
    uint32_t bindGroupIdx{};
    WgShaderTypePaintSettings settings;
    WgImageData gradientData;
    WgRenderSettingsType fillType{};
    float opacityMultiplier = 1.0f;
    bool valid = false;
    FillSpread spread{};  // cached here to use existing tail padding

    uint8_t update(tvg::ColorSpace cs, uint8_t opacity);
    void update(WgContext& context, const Fill* fill, const Matrix* modelTransform, bool updateColorRamp);
    void release(WgContext& context);
};

struct WgRenderPaint
{
    BBox aabb{{},{}};
    RenderRegion viewport{};
    Array<WgRenderPaint*> clips;
    Matrix transform;

    virtual ~WgRenderPaint(){};
    virtual void release(WgContext& context);
    virtual Type type() { return Type::Undefined; };

    void update(const Array<RenderData>& clips);
};

struct WgRenderShape : WgRenderPaint
{
    struct
    {
        WgRenderSettings setting;
        WgSolidData solid;
        WgMeshData mesh;
        WgMeshData bbox;
    } shape;

    struct
    {
        WgRenderSettings setting;
        WgSolidData solid;
        WgMeshData mesh;
        WgMeshData bbox;
    } stroke;

    WgMeshData meshBBox;
    FillRule fillRule;
    BBox bbox;
    uint32_t strokeViewMatIdx;
    bool convex;
    bool strokeFirst;

    void updateBBox(const BBox& bb);
    void updateAABB() { aabb = bbox; }
    void updateVisibility(const RenderShape& rshape, uint8_t opacity);
    void updateMeshes(const RenderShape& rshape, RenderUpdateFlag flag, const Matrix& matrix);
    void releaseMeshes();
    void release(WgContext& context) override;
    Type type() override { return Type::Shape; };
};

struct WgRenderShapePool
{
    Array<WgRenderShape*> mPool;
    Array<WgRenderShape*> mList;

    WgRenderShape* allocate(WgContext& context);
    void free(WgContext& context, WgRenderShape* rdata);
    void release(WgContext& context);
};

struct WgRenderPicture : WgRenderPaint
{
    using WgRenderPaint::update;

    WgRenderSettings renderSettings{};
    WGPUTexture imageTexture{};
    WGPUBindGroup imageBindGroup{};
    const RenderSurface* imageSource = nullptr;
    FilterMethod imageFilter = FilterMethod::Bilinear;
    uint16_t imageStamp = 0;
    WgMeshData meshData{};

    void update(const RenderSurface* surface, const Matrix& transform);
    void setImage(WGPUTexture texture, WGPUBindGroup bindGroup, const RenderSurface* surface, FilterMethod filter, uint16_t stamp);
    void releaseTexture(WgTextureMgr& textures, WgContext& context);
    void clearImage();
    void release(WgContext& context) override;
    Type type() override { return Type::Picture; };
};

struct WgRenderPicturePool
{
    Array<WgRenderPicture*> mPool;
    Array<WgRenderPicture*> mList;

    WgRenderPicture* allocate(WgContext& context);
    void free(WgContext& context, WgRenderPicture* dataPicture);
    void release(WgContext& context);
};

struct WgGeometryRange
{
    size_t vertexOffset{};
    size_t indexOffset{};
    uint32_t vertexCount{};
    uint32_t indexCount{};
};

struct WgSolidBatchRange : WgGeometryRange
{
    size_t colorOffset{};
    RenderRegion viewport{};
};

struct WgStencilBatchRange
{
    WgGeometryRange stencil{};
    WgGeometryRange cover{};
    size_t colorOffset{};
    RenderRegion viewport{};
    FillRule fillRule = FillRule::NonZero;
    bool solidOnly{};
};

// gaussian blur, drop shadow, fill, tint, tritone
#define WG_GAUSSIAN_MAX_LEVEL 3
struct WgRenderEffectParams
{
    WGPUBindGroup bindGroupParams{};
    WGPUBuffer bufferParams{};
    uint32_t extend{};
    Point offset{};

    void update(WgContext& context, WgShaderTypeEffectParams& effectParams);
    void update(WgContext& context, RenderEffectGaussianBlur* gaussian, const Matrix& transform);
    void update(WgContext& context, RenderEffectDropShadow* dropShadow, const Matrix& transform);
    void update(WgContext& context, RenderEffectFill* fill);
    void update(WgContext& context, RenderEffectTint* tint);
    void update(WgContext& context, RenderEffectTritone* tritone);
    void release(WgContext& context);
};

struct WgRenderEffectParamsPool
{
    // pool contains all created but unused render data for params
    Array<WgRenderEffectParams*> mPool;
    // list contains all created render data for params
    // to ensure that all created instances will be released
    Array<WgRenderEffectParams*> mList;

    WgRenderEffectParams* allocate(WgContext& context);
    void free(WgContext& context, WgRenderEffectParams* rdata);
    void release(WgContext& context);
};

struct WgStageBufferGeometry
{
    Array<uint8_t> vbuffer;
    Array<uint8_t> ibuffer;
    void appendBatch(const Array<WgRenderShape*>& renderShapes, WgGeometryRange& range, bool cover);

    WGPUBuffer vbuffer_gpu{};
    WGPUBuffer ibuffer_gpu{};

    void append(WgMeshData* meshData);
    void append(WgRenderShape* renderShape);
    void append(WgRenderPicture* renderPicture);
    void appendSolidBatch(const Array<WgRenderShape*>& renderShapes, WgStageBufferSolidColor& colors, WgSolidBatchRange& range);
    void appendStencilBatch(const Array<WgRenderShape*>& renderShapes, WgStencilBatchRange& range);
    void initialize(WgContext& context){};
    void release(WgContext& context);
    void clear();
    void flush(WgContext& context);
};

struct WgStageBufferSolidColor
{
    Array<RenderColor> vbuffer;
    WGPUBuffer vbuffer_gpu{};

    uint32_t append(const RenderColor& value)
    {
        vbuffer.push(value);
        return vbuffer.count - 1;
    }

    void appendRepeated(const RenderColor& value, uint32_t count);
    void release(WgContext& context);
    void clear();
    void flush(WgContext& context);
};

struct WgStageBufferUniformBase
{
    Array<WGPUBindGroup> bbuffer;

    void flush(WgContext& context, WGPUBuffer& buffer, const void* data, uint64_t reserved, uint32_t count, uint64_t stride);
    void releaseBindGroups(WgContext& context);
};

// typed uniform stage buffer with related bind groups handling
template<typename T>
struct WgStageBufferUniform : WgStageBufferUniformBase
{
    Array<T> ubuffer;
    WGPUBuffer ubuffer_gpu{};

    uint32_t append(const T& value)
    {
        ubuffer.push(value);
        return ubuffer.count - 1;
    }

    void flush(WgContext& context)
    {
        WgStageBufferUniformBase::flush(context, ubuffer_gpu, ubuffer.data, ubuffer.reserved * sizeof(T), ubuffer.count, sizeof(T));
    }

    WGPUBindGroup operator[](const uint32_t index) const
    {
        return bbuffer[index];
    }

    void clear()
    {
        ubuffer.clear();
    }

    void release(WgContext& context)
    {
        context.releaseBuffer(ubuffer_gpu);
        releaseBindGroups(context);
    }
};

struct WgIntersector
{
    bool isPointInTriangle(const Point& p, const Point& a, const Point& b, const Point& c);
    bool isPointInTris(const Point& p, const WgMeshData& mesh);
    bool isPointInMesh(const Point& p, const WgMeshData& mesh);
    bool intersectClips(const Point& pt, const Array<WgRenderPaint*>& clips);
    bool intersectShape(const RenderRegion region, const WgRenderShape* shape);
    bool intersectImage(const RenderRegion region, const WgRenderPicture* image);
};

#endif // _TVG_WG_RENDER_DATA_H_
