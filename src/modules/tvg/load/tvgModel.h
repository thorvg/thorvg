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

#ifndef _TVG_MODEL_H_
#define _TVG_MODEL_H_

#include "tvgCommon.h"
#include "tvgArray.h"
#include "tvgColor.h"
#include "tvgMath.h"
#include "tvgInterpolator.h"
#include "tvgRender.h"
#include "tvgUtil.h"

/************************************************************************/
/* TvgProperty                                                          */
/************************************************************************/

template<typename T>
struct TvgProperty
{
    struct Keyframe
    {
        float progress;
        T value;
        Interpolator* i = nullptr;

        float map(const Keyframe& next, float t) const
        {
            t = pos(t, progress, next.progress);
            return i ? i->map(t) : t;
        }
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
        return tvg::lerp(a.value, b.value, a.map(b, progress));
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

/************************************************************************/
/* TvgPaint                                                             */
/************************************************************************/

struct TvgPaint
{
    Paint* obj;

    TvgTransform* t = nullptr;
    TvgOpacity o = {255};  // opacity
    bool statical = true;

    TvgPaint(TvgPaint* parent, Paint* obj, const char* id);
    virtual ~TvgPaint();
    void prepare(TvgPaint* parent);
    void build(float progress);
};

/************************************************************************/
/* TvgPath                                                              */
/************************************************************************/

struct TvgPath
{
    virtual ~TvgPath() = default;
    virtual void dispatch(float progress, Shape* out) = 0;

    Type type;
};

struct TvgRect : TvgPath
{
    void dispatch(float progress, Shape* out) override;

    TvgPoint p;  // position
    TvgPoint z;  // size
    TvgPoint r;  // corner-radius
    bool d = 1;  // direction
};

struct TvgCircle : TvgPath
{
    void dispatch(float progress, Shape* out) override;

    TvgPoint c;  // center
    TvgPoint r;  // radius
    bool d = 1;  // direction
};

struct TvgCmdPath : TvgPath
{
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

/************************************************************************/
/* TvgScene                                                             */
/************************************************************************/

struct TvgScene : TvgPaint
{
    struct Effect
    {
        virtual ~Effect() = default;
        virtual void dispatch(float progress, Scene* out) = 0;
    };

    struct Blur : Effect
    {
        void dispatch(float progress, Scene* out) override;
        TvgFloat s;  // sigma
    };

    struct Shadow : Effect
    {
        void dispatch(float progress, Scene* out) override;
        TvgSolid c;  // color
        TvgFloat a;  // angle
        TvgFloat d;  // distance
        TvgFloat s;  // sigma
    };

    struct Motion : Effect
    {
        void dispatch(float progress, Scene* out) override;
        TvgFloat d;  // distance
        TvgFloat a;  // angle
    };

    struct Fill : Effect
    {
        void dispatch(float progress, Scene* out) override;
        TvgSolid c;  // color
    };

    struct Tint : Effect
    {
        void dispatch(float progress, Scene* out) override;
        TvgSolid b;  // black
        TvgSolid w;  // white
        TvgFloat i;  // intensity (0–100)
    };

    struct Tritone : Effect
    {
        void dispatch(float progress, Scene* out) override;
        TvgSolid s;    // shadow
        TvgSolid m;    // midtone
        TvgSolid h;    // highlight
        TvgOpacity b;  // blend
    };

    float i[2];  // interval

    struct Clip {
        Array<TvgPath*> list;  // path list
        Shape* obj = nullptr;
        Clip() { obj = Shape::gen(); }
        ~Clip() { for (auto p : list) delete (p); }
    }* clip = nullptr;

    struct Effects {
        Array<Effect*> list;
        ~Effects() { for (auto e : list) delete (e); }
    }* effect = nullptr;

    bool interval = false;

    TvgScene(TvgScene* parent, const char* id) : TvgPaint(parent, Scene::gen(), id) {}
    ~TvgScene();
    Scene* scene() { return (Scene*)obj; }
    float map(float progress) { return pos(progress, i[0], i[1]); }
    void prepare(TvgPaint* parent, const float z[]);
    void build(float progress);
    float buildCore(float progress, Scene* obj);
};

/************************************************************************/
/* TvgGradient                                                          */
/************************************************************************/

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

    TvgPoint p;  // cx, cy
    TvgFloat d;  // degree
};

/************************************************************************/
/* TvgShape                                                             */
/************************************************************************/

struct TvgShape : TvgPaint
{
    struct Stroke {
        ~Stroke() { delete (g); }
        TvgFloat w;                // width
        TvgSolid f;                // solid fill
        TvgGradient* g = nullptr;  // gradient fill
    }* s = nullptr;

    struct Trim {
        TvgFloat b = 0.0f;  // segment begin
        TvgFloat e = 1.0f;  // segment end
        bool m = true;      // simultaneous(true) / indivisual
    }* t = nullptr;

    Array<TvgPath*> list;      // path list
    TvgSolid f;                // solid fill
    TvgGradient* g = nullptr;  // gradient fill

    TvgShape(TvgScene* parent, const char* id) : TvgPaint(parent, Shape::gen(), id) {}
    ~TvgShape();
    Shape* shape() { return (Shape*)obj; }
    Stroke* stroke();
    Trim* trim();
    void prepare(TvgPaint* parent);
    void build(float progress);
    void buildCore(float progress);
};

#endif  // _TVG_MODEL_H_
