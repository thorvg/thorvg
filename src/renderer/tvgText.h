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

#ifndef _TVG_TEXT_H
#define _TVG_TEXT_H

#include "tvgStr.h"
#include "tvgMath.h"
#include "tvgShape.h"
#include "tvgFill.h"
#include "tvgLoader.h"

#define TEXT(A) static_cast<TextImpl*>(A)
#define CONST_TEXT(A) static_cast<const TextImpl*>(A)

struct TextBox {
    Point size;
};


struct TextImpl : Text
{
    Paint::Impl impl;
    Shape* shape;   //text shape
    FontLoader* loader = nullptr;
    FontMetrics metrics;
    char* utf8 = nullptr;
    float fontSize;
    float outlineWidth = 0.0f;
    float italicShear = 0.0f;
    Point align{};

    TextBox* box = nullptr;

    TextImpl() : impl(Paint::Impl(this)), shape(Shape::gen())
    {
        PAINT(shape)->parent = this;
    }

    ~TextImpl()
    {
        tvg::free(utf8);
        tvg::free(box);
        LoaderMgr::retrieve(loader);
        delete(shape);
    }

    Result text(const char* utf8)
    {
        tvg::free(this->utf8);
        if (utf8) this->utf8 = tvg::duplicate(utf8);
        else this->utf8 = nullptr;

        impl.mark(RenderUpdateFlag::Path);

        return Result::Success;
    }

    Result font(const char* name)
    {
        auto loader = name ? LoaderMgr::font(name) : LoaderMgr::anyfont();
        if (!loader) return Result::InsufficientCondition;

        //Same resource has been loaded.
        if (this->loader == loader) {
            this->loader->sharing--;  //make it sure the reference counting.
            return Result::Success;
        } else if (this->loader) {
            LoaderMgr::retrieve(this->loader);
        }
        this->loader = static_cast<FontLoader*>(loader);

        impl.mark(RenderUpdateFlag::Path);

        return Result::Success;
    }

    RenderRegion bounds()
    {
        return SHAPE(shape)->bounds();
    }

    bool render(RenderMethod* renderer)
    {
        if (!loader) return true;
        renderer->blend(impl.blendMethod);
        return PAINT(shape)->render(renderer);
    }

    float load()
    {
        if (!loader) return 0.0f;

        //reload
        if (impl.marked(RenderUpdateFlag::Path)) loader->read(shape, utf8, metrics);

        return loader->transform(shape, metrics, fontSize, italicShear);
    }

    bool skip(RenderUpdateFlag flag)
    {
        if (flag == RenderUpdateFlag::None) return true;
        return false;
    }

    void arrange(Matrix& m, float scale)
    {
        //alignment
        m.e13 -= (metrics.width / scale) * align.x;
        m.e23 -= ((metrics.ascent - metrics.descent) / scale) * align.y;

        //layouting
        if (box) {
            m.e13 += box->size.x * align.x;
            m.e23 += box->size.y * align.y;
        }
    }

    void layout(float w, float h)
    {
        if (!box) box = tvg::calloc<TextBox*>(1, sizeof(TextBox));
        box->size = {w, h};
        impl.mark(RenderUpdateFlag::Transform);
    }

    bool update(RenderMethod* renderer, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flag, TVG_UNUSED bool clipper)
    {
        auto scale = 1.0f / load();
        if (tvg::zero(scale)) return false;

        arrange(const_cast<Matrix&>(shape->transform()), scale);

        //transform the gradient coordinates based on the final scaled font.
        auto fill = SHAPE(shape)->rs.fill;
        if (fill && SHAPE(shape)->impl.marked(RenderUpdateFlag::Gradient)) {
            if (fill->type() == Type::LinearGradient) {
                LINEAR(fill)->p1 *= scale;
                LINEAR(fill)->p2 *= scale;
            } else {
                RADIAL(fill)->center *= scale;
                RADIAL(fill)->r *= scale;
                RADIAL(fill)->focal *= scale;
                RADIAL(fill)->fr *= scale;
            }
        }

        if (outlineWidth > 0.0f && impl.marked(RenderUpdateFlag::Stroke)) shape->strokeWidth(outlineWidth * scale);

        PAINT(shape)->update(renderer, transform, clips, opacity, flag, false);
        return true;
    }

    bool intersects(const RenderRegion& region)
    {
        if (load() == 0.0f) return false;
        return SHAPE(shape)->intersects(region);
    }

    bool bounds(Point* pt4, const Matrix& m, bool obb)
    {
        auto scale = 1.0f / load();
        if (tvg::zero(scale)) return false;
        arrange(const_cast<Matrix&>(m), scale);
        return PAINT(shape)->bounds(pt4, &const_cast<Matrix&>(m), obb);
    }

    Paint* duplicate(Paint* ret)
    {
        if (ret) TVGERR("RENDERER", "TODO: duplicate()");

        load();

        auto text = Text::gen();
        auto dup = TEXT(text);

        SHAPE(shape)->duplicate(dup->shape);

        if (loader) {
            dup->loader = loader;
            ++dup->loader->sharing;
        }

        dup->metrics = metrics;
        dup->utf8 = tvg::duplicate(utf8);
        dup->fontSize = fontSize;
        dup->italicShear = italicShear;
        dup->outlineWidth = outlineWidth;
        dup->align = align;

        if (box) dup->layout(box->size.x, box->size.y);

        return text;
    }

    Iterator* iterator()
    {
        return nullptr;
    }
};

#endif //_TVG_TEXT_H
