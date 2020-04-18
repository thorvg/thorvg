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

static RenderMethodInit engineInit;

struct GlShape
{
    //TODO:
};

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void* GlRenderer::dispose(const ShapeNode& shape, void *data)
{
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) return nullptr;
    free(sdata);
    return nullptr;
}


void* GlRenderer::prepare(const ShapeNode& shape, void* data, UpdateFlag flags)
{
    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = static_cast<GlShape*>(calloc(1, sizeof(GlShape)));
        assert(sdata);
    }
    return sdata;
}


int GlRenderer::init()
{
    return RenderMethodInit::init(engineInit, new GlRenderer);
}


int GlRenderer::term()
{
    return RenderMethodInit::term(engineInit);
}


size_t GlRenderer::unref()
{
    return RenderMethodInit::unref(engineInit);
}


size_t GlRenderer::ref()
{
    return RenderMethodInit::ref(engineInit);
}


GlRenderer* GlRenderer::inst()
{
    return dynamic_cast<GlRenderer*>(RenderMethodInit::inst(engineInit));
}


#endif /* _TVG_GL_RENDERER_CPP_ */
