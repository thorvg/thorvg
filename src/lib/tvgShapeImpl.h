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

struct ShapeFill
{
};


struct ShapeStroke
{
    float width = 0;
    uint8_t color[4] = {0, 0, 0, 0};
    size_t* dashPattern = nullptr;
    size_t dashCnt = 0;
    StrokeCap cap = StrokeCap::Square;
    StrokeJoin join = StrokeJoin::Bevel;

    ~ShapeStroke()
    {
        if (dashPattern) free(dashPattern);
    }
};


struct Shape::Impl
{
    ShapeFill *fill = nullptr;
    ShapeStroke *stroke = nullptr;
    ShapePath *path = nullptr;
    RenderTransform *transform = nullptr;
    uint8_t color[4] = {0, 0, 0, 0};    //r, g, b, a
    size_t flag = RenderUpdateFlag::None;
    void *edata = nullptr;              //engine data


    Impl() : path(new ShapePath)
    {
    }

    ~Impl()
    {
        if (path) delete(path);
        if (stroke) delete(stroke);
        if (fill) delete(fill);
        if (transform) delete(transform);
    }

    bool dispose(Shape& shape, RenderMethod& renderer)
    {
        return renderer.dispose(shape, edata);
    }

    bool render(Shape& shape, RenderMethod& renderer)
    {
        return renderer.render(shape, edata);
    }

    bool update(Shape& shape, RenderMethod& renderer, const RenderTransform* pTransform = nullptr, size_t pFlag = 0)
    {
        if (flag & RenderUpdateFlag::Transform) {
            assert(transform);
            if (!transform->update()) {
                delete(transform);
                transform = nullptr;
            }
        }

        if (transform && pTransform) {
            RenderTransform outTransform(pTransform, transform);
            edata = renderer.prepare(shape, edata, &outTransform, static_cast<RenderUpdateFlag>(pFlag | flag));
        } else {
            auto outTransform = pTransform ? pTransform : transform;
            edata = renderer.prepare(shape, edata, outTransform, static_cast<RenderUpdateFlag>(pFlag | flag));
        }

        flag = RenderUpdateFlag::None;

        if (edata) return true;
        return false;
    }

    bool bounds(float* x, float* y, float* w, float* h)
    {
        assert(path);
        return path->bounds(x, y, w, h);
    }

    bool scale(float factor)
    {
        if (transform) {
            if (fabsf(factor - transform->factor) <= FLT_EPSILON) return -1;
        } else {
            if (fabsf(factor) <= FLT_EPSILON) return -1;
            transform = new RenderTransform();
            assert(transform);
        }
        transform->factor = factor;
        flag |= RenderUpdateFlag::Transform;

        return 0;
    }

    bool rotate(float degree)
    {
        if (transform) {
            if (fabsf(degree - transform->degree) <= FLT_EPSILON) return -1;
        } else {
            if (fabsf(degree) <= FLT_EPSILON) return -1;
            transform = new RenderTransform();
            assert(transform);
        }
        transform->degree = degree;
        flag |= RenderUpdateFlag::Transform;

        return 0;
    }

    bool translate(float x, float y)
    {
        if (transform) {
            if (fabsf(x - transform->x) <= FLT_EPSILON && fabsf(y - transform->y) <= FLT_EPSILON) return -1;
        } else {
            if (fabsf(x) <= FLT_EPSILON && fabsf(y) <= FLT_EPSILON) return -1;
            transform = new RenderTransform();
            assert(transform);
        }
        transform->x = x;
        transform->y = y;
        flag |= RenderUpdateFlag::Transform;

        return 0;
    }

    bool strokeWidth(float width)
    {
        //TODO: Size Exception?

        if (!stroke) stroke = new ShapeStroke();
        assert(stroke);

        stroke->width = width;
        flag |= RenderUpdateFlag::Stroke;

        return 0;
    }

    bool strokeCap(StrokeCap cap)
    {
        if (!stroke) stroke = new ShapeStroke();
        assert(stroke);

        stroke->cap = cap;
        flag |= RenderUpdateFlag::Stroke;

        return 0;
    }

    bool strokeJoin(StrokeJoin join)
    {
        if (!stroke) stroke = new ShapeStroke();
        assert(stroke);

        stroke->join = join;
        flag |= RenderUpdateFlag::Stroke;

        return 0;
    }

    bool strokeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!stroke) stroke = new ShapeStroke();
        assert(stroke);

        stroke->color[0] = r;
        stroke->color[1] = g;
        stroke->color[2] = b;
        stroke->color[3] = a;

        flag |= RenderUpdateFlag::Stroke;

        return 0;
    }

    bool strokeDash(const size_t* pattern, size_t cnt)
    {
        assert(pattern);

       if (!stroke) stroke = new ShapeStroke();
        assert(stroke);

        if (stroke->dashCnt != cnt) {
            if (stroke->dashPattern) free(stroke->dashPattern);
            stroke->dashPattern = nullptr;
        }

        if (!stroke->dashPattern) stroke->dashPattern = static_cast<size_t*>(malloc(sizeof(size_t) * cnt));
        assert(stroke->dashPattern);

        memcpy(stroke->dashPattern, pattern, cnt);
        stroke->dashCnt = cnt;

        flag |= RenderUpdateFlag::Stroke;

        return 0;

    }
};

#endif //_TVG_SHAPE_IMPL_H_