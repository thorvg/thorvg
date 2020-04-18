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
#ifndef _TVG_GL_ENGINE_CPP_
#define _TVG_GL_ENGINE_CPP_

#include "tvgCommon.h"
#include "tvgGlEngine.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static RasterMethodInit engineInit;

struct GlShape
{
    //TODO:
};

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void* GlEngine::dispose(const ShapeNode& shape, void *data)
{
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) return nullptr;
    free(sdata);
    return nullptr;
}


void* GlEngine::prepare(const ShapeNode& shape, void* data, UpdateFlag flags)
{
    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = static_cast<GlShape*>(calloc(1, sizeof(GlShape)));
        assert(sdata);
    }
    return sdata;
}


int GlEngine::init()
{
    return RasterMethodInit::init(engineInit, new GlEngine);
}


int GlEngine::term()
{
    return RasterMethodInit::term(engineInit);
}


size_t GlEngine::unref()
{
    return RasterMethodInit::unref(engineInit);
}


size_t GlEngine::ref()
{
    return RasterMethodInit::ref(engineInit);
}


GlEngine* GlEngine::inst()
{
    return dynamic_cast<GlEngine*>(RasterMethodInit::inst(engineInit));
}


#endif /* _TVG_GL_ENGINE_CPP_ */
