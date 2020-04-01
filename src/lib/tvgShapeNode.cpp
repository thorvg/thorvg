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
#ifndef _TVG_SHAPE_NODE_CPP_
#define _TVG_SHAPE_NODE_CPP_

#include "tvgCommon.h"
#include "tvgShapePath.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct ShapeFill
{
};


struct ShapeStroke
{
};


struct ShapeTransform
{
    float e[4*4];
};


struct ShapeNode::Impl
{
    ShapeTransform *transform = nullptr;
    ShapeFill *fill = nullptr;
    ShapeStroke *stroke = nullptr;
    ShapePath *path = nullptr;

    uint8_t color[4] = {0, 0, 0, 0};    //r, g, b, a

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
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

ShapeNode :: ShapeNode() : pImpl(make_unique<Impl>())
{

}


ShapeNode :: ~ShapeNode()
{
    cout << "ShapeNode(" << this << ") destroyed!" << endl;
}


unique_ptr<ShapeNode> ShapeNode::gen()
{
    return unique_ptr<ShapeNode>(new ShapeNode);
}


int ShapeNode :: update() noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return 0;
}


int ShapeNode :: clear() noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->path->clear();
}


int ShapeNode ::appendCircle(float cx, float cy, float radius) noexcept
{
    return 0;
}


int ShapeNode :: appendRect(float x, float y, float w, float h, float radius) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    //clamping radius by minimum size
    auto min = (w < h ? w : h) * 0.5f;
    if (radius > min) radius = min;

    //rectangle
    if (radius == 0) {
        impl->path->reserve(5, 4);
        impl->path->moveTo(x, y);
        impl->path->lineTo(x + w, y);
        impl->path->lineTo(x + w, y + h);
        impl->path->lineTo(x, y + h);
        impl->path->close();
    //circle
    } else if (w == h && radius * 2 == w) {
        appendCircle(x + (w * 0.5f), y + (h * 0.5f), radius);
    } else {
        //...
    }

    return 0;
}


int ShapeNode :: fill(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    impl->color[0] = r;
    impl->color[1] = g;
    impl->color[2] = b;
    impl->color[3] = a;

    return 0;
}

#endif //_TVG_SHAPE_NODE_CPP_
