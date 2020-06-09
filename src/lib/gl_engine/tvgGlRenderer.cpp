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
#ifndef _TVG_GL_RENDERER_CPP_
#define _TVG_GL_RENDERER_CPP_

#include "tvgCommon.h"
#include "tvgGlRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static RenderInitializer renderInit;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool GlRenderer::clear()
{
    //TODO: (Request) to clear target

    return true;
}

bool GlRenderer::render(const Shape& shape, void *data)
{
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    //TODO:

    return true;
}


bool GlRenderer::dispose(const Shape& shape, void *data)
{
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    //TODO:

    free(sdata);
    return true;
}


void* GlRenderer::prepare(const Shape& shape, void* data, const RenderTransform* transform, RenderUpdateFlag flags)
{
    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = static_cast<GlShape*>(calloc(1, sizeof(GlShape)));
        assert(sdata);
    }

    if (flags & RenderUpdateFlag::Path) {
        //TODO: Updated Vertices
    }

    if (flags & RenderUpdateFlag::Transform) {
        //TODO: Updated Transform
    }

    //TODO:

    return sdata;
}


int GlRenderer::init()
{
    return RenderInitializer::init(renderInit, new GlRenderer);
}


int GlRenderer::term()
{
    return RenderInitializer::term(renderInit);
}


uint32_t GlRenderer::unref()
{
    return RenderInitializer::unref(renderInit);
}


uint32_t GlRenderer::ref()
{
    return RenderInitializer::ref(renderInit);
}


GlRenderer* GlRenderer::inst()
{
    return dynamic_cast<GlRenderer*>(RenderInitializer::inst(renderInit));
}


#endif /* _TVG_GL_RENDERER_CPP_ */
