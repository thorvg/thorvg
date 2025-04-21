/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_RENDER_H_
#define _TVG_RENDER_H_

#include <math.h>
#include <cstdarg>
#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgLock.h"

namespace tvg
{

using RenderData = void*;
using pixel_t = uint32_t;

#define DASH_PATTERN_THRESHOLD 0.001f

enum RenderUpdateFlag : uint16_t {None = 0, Path = 1, Color = 2, Gradient = 4, Stroke = 8, Transform = 16, Image = 32, GradientStroke = 64, Blend = 128, Clip = 256, All = 0xffff};
enum CompositionFlag : uint8_t {Invalid = 0, Opacity = 1, Blending = 2, Masking = 4, PostProcessing = 8};  //Composition Purpose

static inline void operator|=(RenderUpdateFlag& a, const RenderUpdateFlag b)
{
    a = RenderUpdateFlag(uint16_t(a) | uint16_t(b));
}

static inline RenderUpdateFlag operator|(const RenderUpdateFlag a, const RenderUpdateFlag b)
{
    return RenderUpdateFlag(uint16_t(a) | uint16_t(b));
}


struct RenderSurface
{
    union {
        pixel_t* data = nullptr;    //system based data pointer
        uint32_t* buf32;            //for explicit 32bits channels
        uint8_t*  buf8;             //for explicit 8bits grayscale
    };
    Key key;                        //a reserved lock for the thread safety
    uint32_t stride = 0;
    uint32_t w = 0, h = 0;
    ColorSpace cs = ColorSpace::Unknown;
    uint8_t channelSize = 0;
    bool premultiplied = false;         //Alpha-premultiplied

    RenderSurface()
    {
    }

    RenderSurface(const RenderSurface* rhs)
    {
        data = rhs->data;
        stride = rhs->stride;
        w = rhs->w;
        h = rhs->h;
        cs = rhs->cs;
        channelSize = rhs->channelSize;
        premultiplied = rhs->premultiplied;
    }
};

struct RenderColor
{
    uint8_t r, g, b, a;
};

struct RenderCompositor
{
    MaskMethod method;
    uint8_t opacity;
};

struct RenderRegion
{
    int32_t x, y, w, h;

    void intersect(const RenderRegion& rhs);
    void add(const RenderRegion& rhs);

    bool operator==(const RenderRegion& rhs) const
    {
        if (x == rhs.x && y == rhs.y && w == rhs.w && h == rhs.h) return true;
        return false;
    }
};

struct RenderPath
{
    Array<PathCommand> cmds;
    Array<Point> pts;

    void clear()
    {
        pts.clear();
        cmds.clear();
    }

    bool bounds(Matrix* m, float* x, float* y, float* w, float* h);
};

struct RenderTrimPath
{
    float begin = 0.0f;
    float end = 1.0f;
    bool simultaneous = true;

    bool valid()
    {
        if (begin != 0.0f || end != 1.0f) return true;
        return false;
    }

    bool trim(const RenderPath& in, RenderPath& out) const;
};

struct RenderStroke
{
    float width = 0.0f;
    RenderColor color{};
    Fill *fill = nullptr;
    struct {
        float* pattern = nullptr;
        uint32_t count = 0;
        float offset = 0.0f;
        float length = 0.0f;
    } dash;
    float miterlimit = 4.0f;
    RenderTrimPath trim;
    StrokeCap cap = StrokeCap::Square;
    StrokeJoin join = StrokeJoin::Bevel;
    bool first = false;

    void operator=(const RenderStroke& rhs)
    {
        width = rhs.width;
        color = rhs.color;

        delete(fill);
        if (rhs.fill) fill = rhs.fill->duplicate();
        else fill = nullptr;

        tvg::free(dash.pattern);
        if (rhs.dash.count > 0) {
            dash.pattern = tvg::malloc<float*>(sizeof(float) * rhs.dash.count);
            memcpy(dash.pattern, rhs.dash.pattern, sizeof(float) * rhs.dash.count);
        } else {
            dash.pattern = nullptr;
        }
        dash.count = rhs.dash.count;
        dash.offset = rhs.dash.offset;
        dash.length = rhs.dash.length;
        miterlimit = rhs.miterlimit;
        cap = rhs.cap;
        join = rhs.join;
        first = rhs.first;
        trim = rhs.trim;
    }

    ~RenderStroke()
    {
        tvg::free(dash.pattern);
        delete(fill);
    }
};

struct RenderShape
{
    RenderPath path;
    Fill *fill = nullptr;
    RenderColor color{};
    RenderStroke *stroke = nullptr;
    FillRule rule = FillRule::NonZero;

    ~RenderShape()
    {
        delete(fill);
        delete(stroke);
    }

    void fillColor(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const
    {
        if (r) *r = color.r;
        if (g) *g = color.g;
        if (b) *b = color.b;
        if (a) *a = color.a;
    }

    bool trimpath() const
    {
        if (!stroke) return false;
        return stroke->trim.valid();
    }

    bool strokeFirst() const
    {
        return (stroke && stroke->first) ? true : false;
    }

    float strokeWidth() const
    {
        if (!stroke) return 0;
        return stroke->width;
    }

    bool strokeFill(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) const
    {
        if (!stroke) return false;

        if (r) *r = stroke->color.r;
        if (g) *g = stroke->color.g;
        if (b) *b = stroke->color.b;
        if (a) *a = stroke->color.a;

        return true;
    }

    const Fill* strokeFill() const
    {
        if (!stroke) return nullptr;
        return stroke->fill;
    }

    uint32_t strokeDash(const float** dashPattern, float* offset) const
    {
        if (!stroke) return 0;
        if (dashPattern) *dashPattern = stroke->dash.pattern;
        if (offset) *offset = stroke->dash.offset;
        return stroke->dash.count;
    }

    StrokeCap strokeCap() const
    {
        if (!stroke) return StrokeCap::Square;
        return stroke->cap;
    }

    StrokeJoin strokeJoin() const
    {
        if (!stroke) return StrokeJoin::Bevel;
        return stroke->join;
    }

    float strokeMiterlimit() const
    {
        if (!stroke) return 4.0f;
        return stroke->miterlimit;;
    }
};

struct RenderEffect
{
    RenderData rd = nullptr;
    RenderRegion extend = {0, 0, 0, 0};
    SceneEffect type;
    bool valid = false;

    virtual ~RenderEffect() {}
};

struct RenderEffectGaussianBlur : RenderEffect
{
    float sigma;
    uint8_t direction; //0: both, 1: horizontal, 2: vertical
    uint8_t border;    //0: duplicate, 1: wrap
    uint8_t quality;   //0 ~ 100  (optional)

    static RenderEffectGaussianBlur* gen(va_list& args)
    {
        auto inst = new RenderEffectGaussianBlur;
        inst->sigma = std::max((float) va_arg(args, double), 0.0f);
        inst->direction = std::min(va_arg(args, int), 2);
        inst->border = std::min(va_arg(args, int), 1);
        inst->quality = std::min(va_arg(args, int), 100);
        inst->type = SceneEffect::GaussianBlur;
        return inst;
    }
};

struct RenderEffectDropShadow : RenderEffect
{
    uint8_t color[4];  //rgba
    float angle;
    float distance;
    float sigma;
    uint8_t quality;   //0 ~ 100  (optional)

    static RenderEffectDropShadow* gen(va_list& args)
    {
        auto inst = new RenderEffectDropShadow;
        inst->color[0] = va_arg(args, int);
        inst->color[1] = va_arg(args, int);
        inst->color[2] = va_arg(args, int);
        inst->color[3] = va_arg(args, int);
        inst->angle = (float) va_arg(args, double);
        inst->distance = (float) va_arg(args, double);
        inst->sigma = std::max((float) va_arg(args, double), 0.0f);
        inst->quality = std::min(va_arg(args, int), 100);
        inst->type = SceneEffect::DropShadow;
        return inst;
    }
};

struct RenderEffectFill : RenderEffect
{
    uint8_t color[4];  //rgba

    static RenderEffectFill* gen(va_list& args)
    {
        auto inst = new RenderEffectFill;
        inst->color[0] = va_arg(args, int);
        inst->color[1] = va_arg(args, int);
        inst->color[2] = va_arg(args, int);
        inst->color[3] = va_arg(args, int);
        inst->type = SceneEffect::Fill;
        return inst;
    }
};

struct RenderEffectTint : RenderEffect
{
    uint8_t black[3];  //rgb
    uint8_t white[3];  //rgb
    uint8_t intensity; //0 - 255

    static RenderEffectTint* gen(va_list& args)
    {
        auto inst = new RenderEffectTint;
        inst->black[0] = va_arg(args, int);
        inst->black[1] = va_arg(args, int);
        inst->black[2] = va_arg(args, int);
        inst->white[0] = va_arg(args, int);
        inst->white[1] = va_arg(args, int);
        inst->white[2] = va_arg(args, int);
        inst->intensity = (uint8_t)(va_arg(args, double) * 2.55);
        inst->type = SceneEffect::Tint;
        return inst;
    }
};

struct RenderEffectTritone : RenderEffect
{
    uint8_t shadow[3];       //rgb
    uint8_t midtone[3];      //rgb
    uint8_t highlight[3];    //rgb

    static RenderEffectTritone* gen(va_list& args)
    {
        auto inst = new RenderEffectTritone;
        inst->shadow[0] = va_arg(args, int);
        inst->shadow[1] = va_arg(args, int);
        inst->shadow[2] = va_arg(args, int);
        inst->midtone[0] = va_arg(args, int);
        inst->midtone[1] = va_arg(args, int);
        inst->midtone[2] = va_arg(args, int);
        inst->highlight[0] = va_arg(args, int);
        inst->highlight[1] = va_arg(args, int);
        inst->highlight[2] = va_arg(args, int);
        inst->type = SceneEffect::Tritone;
        return inst;
    }
};

class RenderMethod
{
private:
    uint32_t refCnt = 0;        //reference count
    Key key;

public:
    uint32_t ref();
    uint32_t unref();

    virtual ~RenderMethod() {}
    virtual bool preUpdate() = 0;
    virtual RenderData prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) = 0;
    virtual RenderData prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) = 0;
    virtual bool postUpdate() = 0;
    virtual bool preRender() = 0;
    virtual bool renderShape(RenderData data) = 0;
    virtual bool renderImage(RenderData data) = 0;
    virtual bool postRender() = 0;
    virtual void dispose(RenderData data) = 0;
    virtual RenderRegion region(RenderData data) = 0;
    virtual RenderRegion viewport() = 0;
    virtual bool viewport(const RenderRegion& vp) = 0;
    virtual bool blend(BlendMethod method) = 0;
    virtual ColorSpace colorSpace() = 0;
    virtual const RenderSurface* mainSurface() = 0;

    virtual bool clear() = 0;
    virtual bool sync() = 0;

    virtual RenderCompositor* target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags) = 0;
    virtual bool beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity) = 0;
    virtual bool endComposite(RenderCompositor* cmp) = 0;

    virtual void prepare(RenderEffect* effect, const Matrix& transform) = 0;
    virtual bool region(RenderEffect* effect) = 0;
    virtual bool render(RenderCompositor* cmp, const RenderEffect* effect, bool direct) = 0;
    virtual void dispose(RenderEffect* effect) = 0;
};

static inline bool MASK_REGION_MERGING(MaskMethod method)
{
    switch(method) {
        case MaskMethod::Alpha:
        case MaskMethod::InvAlpha:
        case MaskMethod::Luma:
        case MaskMethod::InvLuma:
        case MaskMethod::Subtract:
        case MaskMethod::Intersect:
            return false;
        //these might expand the rendering region
        case MaskMethod::Add:
        case MaskMethod::Difference:
        case MaskMethod::Lighten:
        case MaskMethod::Darken:
            return true;
        default:
            TVGERR("RENDERER", "Unsupported Masking Method! = %d", (int)method);
            return false;
    }
}

static inline uint8_t CHANNEL_SIZE(ColorSpace cs)
{
    switch(cs) {
        case ColorSpace::ABGR8888:
        case ColorSpace::ABGR8888S:
        case ColorSpace::ARGB8888:
        case ColorSpace::ARGB8888S:
            return sizeof(uint32_t);
        case ColorSpace::Grayscale8:
            return sizeof(uint8_t);
        case ColorSpace::Unknown:
        default:
            TVGERR("RENDERER", "Unsupported Channel Size! = %d", (int)cs);
            return 0;
    }
}

static inline ColorSpace MASK_TO_COLORSPACE(RenderMethod* renderer, MaskMethod method)
{
    switch(method) {
        case MaskMethod::Alpha:
        case MaskMethod::InvAlpha:
        case MaskMethod::Add:
        case MaskMethod::Difference:
        case MaskMethod::Subtract:
        case MaskMethod::Intersect:
        case MaskMethod::Lighten:
        case MaskMethod::Darken:
            return ColorSpace::Grayscale8;
        //TODO: Optimize Luma/InvLuma colorspace to Grayscale8
        case MaskMethod::Luma:
        case MaskMethod::InvLuma:
            return renderer->colorSpace();
        default:
            TVGERR("RENDERER", "Unsupported Masking Size! = %d", (int)method);
            return ColorSpace::Unknown;
    }
}

static inline uint8_t MULTIPLY(uint8_t c, uint8_t a)
{
    return (((c) * (a) + 0xff) >> 8);
}

}

#endif //_TVG_RENDER_H_
