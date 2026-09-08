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
#include "tvgWgMesh.h"
#include "tvgWgShaderTypes.h"

struct WgTextureMgr;
struct WgStageBufferSolidColor;

struct WgGradientTexture
{
    WGPUTexture texture{};
    WGPUTextureView textureView{};
    WGPUBindGroup bindGroup{};

    void update(WgContext& context, const Fill* fill);
    void release(WgContext& context);
};

enum class WgRenderSettingsType { None = 0, Solid = 1, Linear = 2, Radial = 3 };

static_assert(sizeof(RenderColor) == 4, "Solid color vertex data must remain tightly packed RGBA8");

struct WgSolidFill
{
    uint32_t colorIdx;
    RenderColor color;
    uint8_t opacity;

    RenderColor packedColor() const
    {
        return {color.r, color.g, color.b, MULTIPLY(color.a, opacity)};
    }
};

struct WgRenderSettings
{
    uint32_t bindGroupIdx;
    WgShaderTypePaintSettings settings;
    WgGradientTexture gradientData;
    WgRenderSettingsType fillType{};
    float opacityMultiplier = 1.0f;
    bool valid = false;

    uint8_t update(tvg::ColorSpace cs, uint8_t opacity);
    void update(WgContext& context, const Fill* fill, const Matrix* transform, bool updateColorRamp);
    void release(WgContext& context);
};

struct WgPaint
{
    BBox aabb{};
    RenderRegion viewport;
    Array<WgPaint*> clips;
    Matrix transform;

    virtual ~WgPaint(){};
    virtual void release(WgContext& context) = 0;
    virtual Type type() { return Type::Undefined; };

    void update(const Array<RenderData>& clips);
};

struct WgShape : WgPaint
{
    using WgPaint::update;

    struct
    {
        WgRenderSettings setting;
        WgSolidFill solid;
        WgMesh mesh;
        WgMesh bbox;
        BBox bounds;
    } shape;

    struct
    {
        WgRenderSettings setting;
        WgSolidFill solid;
        WgMesh mesh;
        WgMesh bbox;
        BBox bounds;
    } stroke;

    WgMesh meshBBox;
    FillRule fillRule;
    BBox bbox;
    uint32_t strokeViewMatIdx;
    bool convex;
    bool strokeFirst;

    void updateBBox(const BBox& bb);
    void updateAABB() { aabb = bbox; }
    void update(const RenderShape& rshape, const RenderRegion& vport, uint8_t shapeOpacity, uint8_t strokeOpacity, uint8_t opacity);
    void update(const RenderShape& rshape, const Matrix& transform, RenderUpdateFlag flag);
    void reset();
    void release(WgContext& context) override;
    Type type() override { return Type::Shape; };
};

struct WgImage : WgPaint
{
    using WgPaint::update;

    WgRenderSettings renderSettings;
    WGPUTexture imageTexture{};
    WGPUBindGroup imageBindGroup{};
    const RenderSurface* imageSource = nullptr;
    FilterMethod imageFilter = FilterMethod::Bilinear;
    uint16_t imageStamp = 0;
    WgMesh meshData;

    void update(const RenderSurface* surface, const Matrix& transform);
    void setup(WGPUTexture texture, WGPUBindGroup bindGroup, const RenderSurface* surface, FilterMethod filter, uint16_t stamp);
    void release(WgTextureMgr& textures, WgContext& context);
    void reset();
    void release(WgContext& context) override;
    Type type() override { return Type::Picture; };
};

struct WgGeometryRange
{
    size_t vertexOffset;
    size_t indexOffset;
    uint32_t vertexCount;
    uint32_t indexCount;
};

struct WgSolidBatchRange : WgGeometryRange
{
    size_t colorOffset{};
    RenderRegion viewport;
};

struct WgStencilBatchRange
{
    WgGeometryRange stencil;
    WgGeometryRange cover;
    size_t colorOffset{};
    RenderRegion viewport;
    FillRule fillRule;
    bool solidOnly;
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
    void appendBatch(const Array<WgShape*>& renderShapes, WgGeometryRange& range, bool cover);

    WGPUBuffer vbuffer_gpu{};
    WGPUBuffer ibuffer_gpu{};

    void append(WgMesh* meshData);
    void append(WgShape* renderShape);
    void append(WgImage* renderPicture);
    void appendSolidBatch(const Array<WgShape*>& renderShapes, WgStageBufferSolidColor& colors, WgSolidBatchRange& range);
    void appendStencilBatch(const Array<WgShape*>& renderShapes, WgStencilBatchRange& range);
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

#endif // _TVG_WG_RENDER_DATA_H_
