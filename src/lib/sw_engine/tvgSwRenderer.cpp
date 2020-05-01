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
    for (size_t i = 0; i < surface.h; i++) {
        for (size_t j = 0; j < surface.w; j++)
            surface.buffer[surface.stride * i + j] = 0xff000000;  //Solid Black
    }

    return true;
}

bool SwRenderer::target(uint32_t* buffer, size_t stride, size_t w, size_t h)
{
    assert(buffer && stride > 0 && w > 0 && h > 0);

    surface.buffer = buffer;
    surface.stride = stride;
    surface.w = w;
    surface.h = h;

    return true;
}


bool SwRenderer::render(const ShapeNode& shape, void *data)
{
    SwShape* sdata = static_cast<SwShape*>(data);
    if (!sdata) return false;

    //invisible?
    size_t r, g, b, a;
    shape.fill(&r, &g, &b, &a);

    //TODO: Threading
    return rasterShape(surface, *sdata, r, g, b, a);
}


bool SwRenderer::dispose(const ShapeNode& shape, void *data)
{
    SwShape* sdata = static_cast<SwShape*>(data);
    if (!sdata) return false;
    shapeReset(*sdata);
    free(sdata);
    return true;
}

void* SwRenderer::prepare(const ShapeNode& shape, void* data, UpdateFlag flags)
{
    //prepare shape data
    SwShape* sdata = static_cast<SwShape*>(data);
    if (!sdata) {
        sdata = static_cast<SwShape*>(calloc(1, sizeof(SwShape)));
        assert(sdata);
    }

    if (flags == UpdateFlag::None) return nullptr;

    //invisible?
    size_t alpha;
    shape.fill(nullptr, nullptr, nullptr, &alpha);
    if (alpha == 0) return sdata;

    //TODO: Threading
    if (flags & UpdateFlag::Path) {
        shapeReset(*sdata);
        if (!shapeGenOutline(shape, *sdata)) return sdata;
        if (!shapeTransformOutline(shape, *sdata)) return sdata;

        SwSize clip = {static_cast<SwCoord>(surface.w), static_cast<SwCoord>(surface.h)};
        if (!shapeGenRle(shape, *sdata, clip)) return sdata;
    }

    return sdata;
}


int SwRenderer::init()
{
    return RenderInitializer::init(renderInit, new SwRenderer);
}


int SwRenderer::term()
{
    return RenderInitializer::term(renderInit);
}


size_t SwRenderer::unref()
{
    return RenderInitializer::unref(renderInit);
}


size_t SwRenderer::ref()
{
    return RenderInitializer::ref(renderInit);
}


SwRenderer* SwRenderer::inst()
{
    return dynamic_cast<SwRenderer*>(RenderInitializer::inst(renderInit));
}


#endif /* _TVG_SW_RENDERER_CPP_ */
