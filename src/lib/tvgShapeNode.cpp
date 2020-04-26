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
constexpr auto PATH_KAPPA = 0.552284f;

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


unique_ptr<ShapeNode> ShapeNode::gen() noexcept
{
    return unique_ptr<ShapeNode>(new ShapeNode);
}


void* ShapeNode::engine() noexcept
{
    auto impl = pImpl.get();
    assert(impl);
    return impl->edata;
}


int ShapeNode::update(RenderMethod* engine) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    impl->edata = engine->prepare(*this, impl->edata, RenderMethod::UpdateFlag::All);
    if (impl->edata) return 0;
    return - 1;
}


int ShapeNode::clear() noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    return impl->path->clear();
}


int ShapeNode::pathCommands(const PathCommand** cmds) const noexcept
{
    auto impl = pImpl.get();
    assert(impl && cmds);

    *cmds = impl->path->cmds;

    return impl->path->cmdCnt;
}


int ShapeNode::pathCoords(const Point** pts) const noexcept
{
    auto impl = pImpl.get();
    assert(impl && pts);

    *pts = impl->path->pts;

    return impl->path->ptsCnt;
}


int ShapeNode::appendCircle(float cx, float cy, float radiusW, float radiusH) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    auto halfKappaW = radiusW * PATH_KAPPA;
    auto halfKappaH = radiusH * PATH_KAPPA;

    impl->path->reserve(6, 13);
    impl->path->moveTo(cx, cy - radiusH);
    impl->path->cubicTo(cx + halfKappaW, cy - radiusH, cx + radiusW, cy - halfKappaH, cx + radiusW, cy);
    impl->path->cubicTo(cx + radiusW, cy + halfKappaH, cx + halfKappaW, cy + radiusH, cx, cy + radiusH);
    impl->path->cubicTo(cx - halfKappaW, cy + radiusH, cx - radiusW, cy + halfKappaH, cx - radiusW, cy);
    impl->path->cubicTo(cx - radiusW, cy - halfKappaH, cx - halfKappaW, cy - radiusH, cx, cy - radiusH);
    impl->path->close();

    return 0;
}


int ShapeNode::appendRect(float x, float y, float w, float h, float cornerRadius) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    //clamping cornerRadius by minimum size
    auto min = (w < h ? w : h) * 0.5f;
    if (cornerRadius > min) cornerRadius = min;

    //rectangle
    if (cornerRadius == 0) {
        impl->path->reserve(5, 4);
        impl->path->moveTo(x, y);
        impl->path->lineTo(x + w, y);
        impl->path->lineTo(x + w, y + h);
        impl->path->lineTo(x, y + h);
        impl->path->close();
    //circle
    } else if (w == h && cornerRadius * 2 == w) {
        return appendCircle(x + (w * 0.5f), y + (h * 0.5f), cornerRadius, cornerRadius);
    } else {
        auto halfKappa = cornerRadius * 0.5;
        impl->path->reserve(10, 17);
        impl->path->moveTo(x + cornerRadius, y);
        impl->path->lineTo(x + w - cornerRadius, y);
        impl->path->cubicTo(x + w - cornerRadius + halfKappa, y, x + w, y + cornerRadius - halfKappa, x + w, y + cornerRadius);
        impl->path->lineTo(x + w, y + h - cornerRadius);
        impl->path->cubicTo(x + w, y + h - cornerRadius + halfKappa, x + w - cornerRadius + halfKappa, y + h, x + w - cornerRadius, y + h);
        impl->path->lineTo(x + cornerRadius, y + h);
        impl->path->cubicTo(x + cornerRadius - halfKappa, y + h, x, y + h - cornerRadius + halfKappa, x, y + h - cornerRadius);
        impl->path->lineTo(x, y + cornerRadius);
        impl->path->cubicTo(x, y + cornerRadius - halfKappa, x + cornerRadius - halfKappa, y, x + cornerRadius, y);
        impl->path->close();
    }

    return 0;
}


int ShapeNode::fill(size_t r, size_t g, size_t b, size_t a) noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    impl->color[0] = r;
    impl->color[1] = g;
    impl->color[2] = b;
    impl->color[3] = a;

    return 0;
}


int ShapeNode::fill(size_t* r, size_t* g, size_t* b, size_t* a) const noexcept
{
    auto impl = pImpl.get();
    assert(impl);

    if (r) *r = impl->color[0];
    if (g) *g = impl->color[1];
    if (b) *b = impl->color[2];
    if (a) *a = impl->color[3];

    return 0;
}

#endif //_TVG_SHAPE_NODE_CPP_
