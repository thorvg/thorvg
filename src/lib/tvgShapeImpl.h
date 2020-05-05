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
};


struct Shape::Impl
{
    ShapeFill *fill = nullptr;
    ShapeStroke *stroke = nullptr;
    ShapePath *path = nullptr;
    uint8_t color[4] = {0, 0, 0, 0};    //r, g, b, a
    float scale = 1;
    float rotate = 0;
    void *edata = nullptr;              //engine data
    size_t flag = RenderUpdateFlag::None;

    Impl() : path(new ShapePath)
    {
    }

    ~Impl()
    {
        if (path) delete(path);
        if (stroke) delete(stroke);
        if (fill) delete(fill);
    }

    bool dispose(Shape& shape, RenderMethod& renderer)
    {
        return renderer.dispose(shape, edata);
    }

    bool render(Shape& shape, RenderMethod& renderer)
    {
        return renderer.render(shape, edata);
    }

    bool update(Shape& shape, RenderMethod& renderer)
    {
        edata = renderer.prepare(shape, edata, static_cast<RenderUpdateFlag>(flag));
        flag = RenderUpdateFlag::None;
        if (edata) return true;
        return false;
    }

    bool bounds(float& x, float& y, float& w, float& h)
    {
        assert(path);
        return path->bounds(x, y, w, h);
    }
};

#endif //_TVG_SHAPE_IMPL_H_