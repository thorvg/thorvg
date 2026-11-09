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

struct TvgScene;

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

    const T operator()(float p) const
    {
        if (cnt == 0) return value;
        if (p <= frames[0].progress) return frames[0].value;
        if (p >= frames[cnt - 1].progress) return frames[cnt - 1].value;
        auto idx = tvg::bsearch(frames, cnt, sizeof(Keyframe), p);
        auto& a = frames[idx];
        auto& b = frames[idx + 1];
        return tvg::lerp(a.value, b.value, (p - a.progress) / (b.progress - a.progress));
    }
};

using TvgPoint = TvgProperty<Point>;

struct TvgPaint
{
    Paint* obj;

    struct Transform
    {
        float r = 0.0f;             // rotation
        float s[2] = {1.0f, 1.0f};  // scale
        TvgPoint t;                 // translate
        TvgPoint a;                 // anchor
        TvgPoint k;                 // skew
    }* t = nullptr;

    uint8_t o = 255;  // opacity
    bool statical = true;

    TvgPaint(TvgScene* parent, Paint* obj, const char* id);
    virtual ~TvgPaint();

    void build(float progress);

    Transform* transform()
    {
        if (!t) t = new Transform;
        return t;
    }
};

struct TvgScene : TvgPaint
{
    float i[2];  // interval
    float z[2];  // viewport

    TvgScene(TvgScene* parent, const char* id) : TvgPaint(parent, Scene::gen(), id) {}
    ~TvgScene();

    Scene* scene() { return (Scene*)obj; }
    float map(float progress) { return (progress - i[0]) / (i[1] - i[0]); }
};

struct TvgShape : TvgPaint
{
    struct Path
    {
        enum Type
        {
            Custom = 0,
            Rect,
            Circle
        };
        Path(Type type) : type(type) {}
        Type type;
    };

    struct Custom : Path
    {
        Custom() : Path(Type::Custom) {}
        RenderPath path;
    };

    struct Rect : Path
    {
        Rect() : Path(Type::Rect) {}

        TvgPoint p;     // position
        TvgPoint z;     // size
        TvgPoint r;     // corner-radius
        uint8_t d = 1;  // direction
    };

    struct Circle : Path
    {
        Circle() : Path(Type::Circle) {}

        TvgPoint c;     // center
        TvgPoint r;     // radius
        uint8_t d = 1;  // direction
    };

    Array<Path*> list;  // path list
    RGBA f{};           // fill

    TvgShape(TvgScene* parent, const char* id) : TvgPaint(parent, Shape::gen(), id) {}
    ~TvgShape();

    Shape* shape() { return (Shape*)obj; }
    void build(float progress);
};

#endif  // _TVG_TVG_MODEL_H_
