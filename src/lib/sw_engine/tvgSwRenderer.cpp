/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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

SwRenderer::~SwRenderer()
{
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


bool SwRenderer::preRender()
{
    return rasterClear(surface);
}


bool SwRenderer::render(const Shape& sdata, TVG_UNUSED void *data)
{
    auto shape = static_cast<SwShape*>(data);
    if (!shape) return false;

    uint8_t r, g, b, a;

    if (auto fill = sdata.fill()) {
        rasterGradientShape(surface, shape, fill->id());
    } else{
        sdata.fill(&r, &g, &b, &a);
        if (a > 0) rasterSolidShape(surface, shape, r, g, b, a);
    }
    sdata.strokeColor(&r, &g, &b, &a);
    if (a > 0) rasterStroke(surface, shape, r, g, b, a);

    return true;
}


bool SwRenderer::dispose(TVG_UNUSED const Shape& sdata, void *data)
{
    auto shape = static_cast<SwShape*>(data);
    if (!shape) return false;
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

    SwSize clip = {static_cast<SwCoord>(surface.w), static_cast<SwCoord>(surface.h)};

    //Valid Stroking?
    uint8_t strokeAlpha = 0;
    auto strokeWidth = sdata.strokeWidth();
    if (strokeWidth > FLT_EPSILON) {
         sdata.strokeColor(nullptr, nullptr, nullptr, &strokeAlpha);
    }

    const Matrix* matrix = (transform ? &transform->m : nullptr);

    //Shape
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) {
        shapeReset(shape);
        uint8_t alpha = 0;
        sdata.fill(nullptr, nullptr, nullptr, &alpha);
        bool renderShape = (alpha > 0 || sdata.fill());
        if (renderShape || strokeAlpha) {
            if (!shapePrepare(shape, &sdata, clip, matrix)) return shape;
            if (renderShape) {
                auto antiAlias = (strokeAlpha > 0 && strokeWidth >= 2) ? false : true;
                if (!shapeGenRle(shape, &sdata, clip, antiAlias)) return shape;
            }
        }
        //Fill
        if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
            auto fill = sdata.fill();
            if (fill) {
                auto ctable = (flags & RenderUpdateFlag::Gradient) ? true : false;
                if (ctable) shapeResetFill(shape);
                if (!shapeGenFillColors(shape, fill, matrix, ctable)) return shape;
            } else {
                shapeDelFill(shape);
            }
        }
        //Stroke
        if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
            if (strokeAlpha > 0) {
                shapeResetStroke(shape, &sdata, matrix);
                if (!shapeGenStrokeRle(shape, &sdata, matrix, clip)) return shape;
            } else {
                shapeDelStroke(shape);
            }
        }
        shapeDelOutline(shape);
    }

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