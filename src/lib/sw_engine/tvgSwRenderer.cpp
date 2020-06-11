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
#ifndef _TVG_SW_RENDERER_CPP_
#define _TVG_SW_RENDERER_CPP_

#include "tvgSwCommon.h"
#include "tvgSwRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static RenderInitializer renderInit;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool SwRenderer::clear()
{
    if (!surface.buffer) return false;

    assert(surface.stride > 0 && surface.w > 0 && surface.h > 0);

    //OPTIMIZE ME: SIMD!
    for (uint32_t i = 0; i < surface.h; i++) {
        for (uint32_t j = 0; j < surface.w; j++)
            surface.buffer[surface.stride * i + j] = 0xff000000;  //Solid Black
    }

    return true;
}

bool SwRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h)
{
    if (!buffer || stride == 0 || w == 0 || h == 0) return false;

    surface.buffer = buffer;
    surface.stride = stride;
    surface.w = w;
    surface.h = h;

    return true;
}

bool SwRenderer::render(const Shape& sdata, void *data)
{
    SwShape* shape = static_cast<SwShape*>(data);
    if (!shape) return false;

    uint8_t r, g, b, a;

    if (sdata.fill()) {
        rasterGradientShape(surface, *shape);
    } else {
        sdata.fill(&r, &g, &b, &a);
        if (a > 0) rasterSolidShape(surface, *shape, r, g, b, a);
    }

    sdata.strokeColor(&r, &g, &b, &a);
    if (a > 0) rasterStroke(surface, *shape, r, g, b, a);

    return true;
}


bool SwRenderer::dispose(const Shape& sdata, void *data)
{
    auto shape = static_cast<SwShape*>(data);
    if (!shape) return true;
    shapeFree(shape);
    return true;
}


void* SwRenderer::prepare(const Shape& sdata, void* data, const RenderTransform* transform, RenderUpdateFlag flags)
{
    //prepare shape data
    auto shape = static_cast<SwShape*>(data);
    if (!shape) {
        shape = static_cast<SwShape*>(calloc(1, sizeof(SwShape)));
        assert(shape);
    }

    if (flags == RenderUpdateFlag::None) return shape;

    //TODO: Threading

    SwSize clip = {static_cast<SwCoord>(surface.w), static_cast<SwCoord>(surface.h)};

    //Shape
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) {
        shapeReset(*shape);
        uint8_t alpha = 0;
        sdata.fill(nullptr, nullptr, nullptr, &alpha);
        if (alpha > 0 || sdata.fill()) {
            if (!shapeGenRle(*shape, sdata, clip, transform)) return shape;
        }
    }

    //Fill
    if (flags & (RenderUpdateFlag::Gradient)) {
        auto fill = sdata.fill();
        if (fill) {
            shapeResetFill(*shape, fill);
            if (!shapeGenFillColors(*shape, fill)) return shape;
        } else {
            shapeDelFill(*shape);
        }
    }

    //Stroke
    if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
        if (sdata.strokeWidth() > 0.5) {
            shapeResetStroke(*shape, sdata);
            uint8_t alpha = 0;
            sdata.strokeColor(nullptr, nullptr, nullptr, &alpha);
            if (alpha > 0) {
                if (!shapeGenStrokeRle(*shape, sdata, clip)) return shape;
            }
        } else {
            shapeDelStroke(*shape);
        }
    }

    shapeDelOutline(*shape);

    return shape;
}


int SwRenderer::init()
{
    return RenderInitializer::init(renderInit, new SwRenderer);
}


int SwRenderer::term()
{
    return RenderInitializer::term(renderInit);
}


uint32_t SwRenderer::unref()
{
    return RenderInitializer::unref(renderInit);
}


uint32_t SwRenderer::ref()
{
    return RenderInitializer::ref(renderInit);
}


SwRenderer* SwRenderer::inst()
{
    //We know renderer type, avoid dynamic_cast for performance.
    return static_cast<SwRenderer*>(RenderInitializer::inst(renderInit));
}

#endif /* _TVG_SW_RENDERER_CPP_ */
