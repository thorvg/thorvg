/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_SHAPE_IMPL_H_
#define _TVG_SHAPE_IMPL_H_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct ShapeStroke
{
    float width = 0;
    uint8_t color[4] = {0, 0, 0, 0};
    float* dashPattern = nullptr;
    uint32_t dashCnt = 0;
    StrokeCap cap = StrokeCap::Square;
    StrokeJoin join = StrokeJoin::Bevel;

    ~ShapeStroke()
    {
        if (dashPattern) free(dashPattern);
    }
};


struct Shape::Impl
{
    ShapePath *path = nullptr;
    Fill *fill = nullptr;
    ShapeStroke *stroke = nullptr;
    RenderTransform *rTransform = nullptr;
    uint8_t color[4] = {0, 0, 0, 0};    //r, g, b, a
    uint32_t flag = RenderUpdateFlag::None;
    void *edata = nullptr;              //engine data
    Shape *shape = nullptr;


    Impl(Shape* s) : path(new ShapePath), shape(s)
    {
    }

    ~Impl()
    {
        if (path) delete(path);
        if (fill) delete(fill);
        if (stroke) delete(stroke);
        if (rTransform) delete(rTransform);
    }

    bool dispose(RenderMethod& renderer)
    {
        return renderer.dispose(*shape, edata);
    }

    bool render(RenderMethod& renderer)
    {
        return renderer.render(*shape, edata);
    }

    bool update(RenderMethod& renderer, const RenderTransform* pTransform, uint32_t pFlag)
    {
        if (flag & RenderUpdateFlag::Transform) {
            if (!rTransform) return false;
            if (!rTransform->update()) {
                delete(rTransform);
                rTransform = nullptr;
            }
        }

        if (rTransform && pTransform) {
            RenderTransform outTransform(pTransform, rTransform);
            edata = renderer.prepare(*shape, edata, &outTransform, static_cast<RenderUpdateFlag>(pFlag | flag));
        } else {
            auto outTransform = pTransform ? pTransform : rTransform;
            edata = renderer.prepare(*shape, edata, outTransform, static_cast<RenderUpdateFlag>(pFlag | flag));
        }

        flag = RenderUpdateFlag::None;

        if (edata) return true;
        return false;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        if (!path) return false;
        return path->bounds(x, y, w, h);
    }

    bool scale(float factor)
    {
        if (rTransform) {
            if (fabsf(factor - rTransform->scale) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(factor) <= FLT_EPSILON) return true;
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->scale = factor;
        if (!rTransform->overriding) flag |= RenderUpdateFlag::Transform;

        return true;
    }

    bool rotate(float degree)
    {
        if (rTransform) {
            if (fabsf(degree - rTransform->degree) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(degree) <= FLT_EPSILON) return true;
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->degree = degree;
        if (!rTransform->overriding) flag |= RenderUpdateFlag::Transform;

        return true;
    }

    bool translate(float x, float y)
    {
        if (rTransform) {
            if (fabsf(x - rTransform->x) <= FLT_EPSILON && fabsf(y - rTransform->y) <= FLT_EPSILON) return true;
        } else {
            if (fabsf(x) <= FLT_EPSILON && fabsf(y) <= FLT_EPSILON) return true;
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->x = x;
        rTransform->y = y;
        if (!rTransform->overriding) flag |= RenderUpdateFlag::Transform;

        return true;
    }


    bool transform(const Matrix& m)
    {
        if (!rTransform) {
            rTransform = new RenderTransform();
            if (!rTransform) return false;
        }
        rTransform->override(m);
        flag |= RenderUpdateFlag::Transform;

        return true;
    }


    bool strokeWidth(float width)
    {
        //TODO: Size Exception?

        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->width = width;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeCap(StrokeCap cap)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->cap = cap;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeJoin(StrokeJoin join)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->join = join;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!stroke) stroke = new ShapeStroke();
        if (!stroke) return false;

        stroke->color[0] = r;
        stroke->color[1] = g;
        stroke->color[2] = b;
        stroke->color[3] = a;

        flag |= RenderUpdateFlag::Stroke;

        return true;
    }

    bool strokeDash(const float* pattern, uint32_t cnt)
    {
        assert(pattern);

       if (!stroke) stroke = new ShapeStroke();
       if (!stroke) return false;

        if (stroke->dashCnt != cnt) {
            if (stroke->dashPattern) free(stroke->dashPattern);
            stroke->dashPattern = nullptr;
        }

        if (!stroke->dashPattern) stroke->dashPattern = static_cast<float*>(malloc(sizeof(float) * cnt));
        assert(stroke->dashPattern);

        for (uint32_t i = 0; i < cnt; ++i)
            stroke->dashPattern[i] = pattern[i];

        stroke->dashCnt = cnt;
        flag |= RenderUpdateFlag::Stroke;

        return true;
    }
};

#endif //_TVG_SHAPE_IMPL_H_