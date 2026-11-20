/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_TVG_MODEL_H_
#define _TVG_TVG_MODEL_H_

#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgColor.h"
#include "tvgMath.h"
#include "tvgTvgUtil.h"
#include "tvgRender.h"

template<typename T>
struct TvgProperty
{
    struct Keyframe
    {
        float progress;
        T value;
    };

    union {
        Keyframe* frames = nullptr;
        T value;
    };

    uint32_t cnt = 0;

    TvgProperty(const T& value = T{}) : value(value) {}

    ~TvgProperty()
    {
        if (cnt > 0) tvg::free(frames);
    }

    const T operator()(float progress) const
    {
        if (cnt == 0) return value;
        if (progress <= frames[0].progress) return frames[0].value;
        if (progress >= frames[cnt - 1].progress) return frames[cnt - 1].value;
        auto idx = tvg::bsearch(frames, cnt, sizeof(Keyframe), progress);
        auto& a = frames[idx];
        auto& b = frames[idx + 1];
        return tvg::lerp(a.value, b.value, pos(progress, a.progress, b.progress));
    }
};

struct TvgPoints : TvgProperty<Point*>
{
    void operator()(float progress, Array<Point>& out, uint32_t ptsCnt) const;
};

struct TvgColorStop : TvgProperty<ColorStops>
{
    void operator()(float progress, Fill* out) const;
};

using TvgFloat = TvgProperty<float>;
using TvgFloat3 = TvgProperty<Float3>;
using TvgFloat4 = TvgProperty<Float4>;
using TvgPoint = TvgProperty<Point>;
using TvgSolid = TvgProperty<RGBA>;
using TvgOpacity = TvgProperty<uint8_t>;

struct TvgTransform
{
    void dispatch(float progress, Matrix& out) const;

    TvgFloat r;         // rotation
    TvgFloat s = 1.0f;  // scale
    TvgPoint t;         // translate
    TvgPoint a;         // anchor
    TvgPoint k;         // skew
};

struct TvgPaint
{
    Paint* obj;

    TvgTransform* t = nullptr;
    TvgOpacity o = {255};  // opacity
    bool statical = true;

    TvgPaint(TvgPaint* parent, Paint* obj, const char* id);
    virtual ~TvgPaint();
    virtual bool build(float progress, bool force = false);
};

struct TvgScene : TvgPaint
{
    float i[2];  // interval
    float z[2];  // viewport

    TvgScene(TvgScene* parent, const char* id) : TvgPaint(parent, Scene::gen(), id) {}
    ~TvgScene();
    Scene* scene() { return (Scene*)obj; }
    bool build(float progress, bool force = true) override;
    float map(float progress) { return (progress - i[0]) / (i[1] - i[0]); }
};

struct TvgGradient
{
    TvgGradient(Fill* fill) : fill(fill) {}
    virtual ~TvgGradient();
    virtual Fill* dispatch(float progress) = 0;

    Fill* fill = nullptr;
    TvgColorStop c;
    TvgTransform* t = nullptr;
};

struct TvgLinear : TvgGradient
{
    TvgLinear() : TvgGradient(LinearGradient::gen()) {}
    Fill* dispatch(float progress) override;

    TvgFloat4 p;  // x1, y1, x2, y2
};

struct TvgRadial : TvgGradient
{
    TvgRadial() : TvgGradient(RadialGradient::gen()) {}
    Fill* dispatch(float progress) override;

    TvgFloat3 p;  // cx, cy, r
    TvgFloat3 f;  // fx, fy, fr
};

struct TvgConic : TvgGradient
{
    TvgConic() : TvgGradient(ConicGradient::gen()) {}
    Fill* dispatch(float progress) override;

    TvgPoint p;   // cx, cy
    TvgFloat d;   // degree
};

struct TvgShape : TvgPaint
{
    struct Path
    {
        enum Type {Custom = 0, Rect, Circle};
        Path(Type type) : type(type) {}
        virtual ~Path() = default;
        virtual void dispatch(float progress, Shape* out) = 0;

        Type type;
    };

    struct Rect : Path
    {
        Rect() : Path(Type::Rect) {}
        void dispatch(float progress, Shape* out) override;

        TvgPoint p;  // position
        TvgPoint z;  // size
        TvgPoint r;  // corner-radius
        bool d = 1;  // direction
    };

    struct Circle : Path
    {
        Circle() : Path(Type::Circle) {}
        void dispatch(float progress, Shape* out) override;

        TvgPoint c;  // center
        TvgPoint r;  // radius
        bool d = 1;  // direction
    };

    struct Custom : Path
    {
        Custom() : Path(Type::Custom) {}
        void dispatch(float progress, Shape* out) override;

        struct Pts {
            ~Pts();
            TvgPoints pts;
            uint16_t cnt = 0;
        } pts;  // path command list

        struct Cmd {
            ~Cmd() { tvg::free(cmds); }
            PathCommand* cmds = nullptr;
            uint16_t cnt = 0;
        } cmd;  // path point list (fixed)
    };

    struct Stroke {
        TvgFloat w;  // width
        TvgSolid f;  // solid fill
    }* s = nullptr;

    struct Trim {
        TvgFloat b = 0.0f;  // segment begin
        TvgFloat e = 1.0f;  // segment end
        bool m = true;      // simultaneous(true) / indivisual
    }* t = nullptr;

    Array<Path*> list;         // path list
    TvgSolid f;                // solid fill
    TvgGradient* g = nullptr;  // gradient fill

    TvgShape(TvgScene* parent, const char* id) : TvgPaint(parent, Shape::gen(), id) {}
    ~TvgShape();
    Shape* shape() { return (Shape*)obj; }
    Stroke* stroke();
    Trim* trim();
    bool build(float progress, bool force = false) override;
};

#endif  // _TVG_TVG_MODEL_H_
