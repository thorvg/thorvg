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
#ifndef _TVG_SW_ENGINE_CPP_
#define _TVG_SW_ENGINE_CPP_

#include "tvgSwCommon.h"
#include "tvgSwEngine.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static RasterMethodInit engineInit;


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void* SwEngine::dispose(const ShapeNode& shape, void *data)
{
    SwShape* sdata = static_cast<SwShape*>(data);
    if (!sdata) return nullptr;
    shapeReset(*sdata);
    free(sdata);
    return nullptr;
}

void* SwEngine::prepare(const ShapeNode& shape, void* data, UpdateFlag flags)
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

    if (flags & UpdateFlag::Path) {
        shapeReset(*sdata);
        if (!shapeGenOutline(shape, *sdata)) return sdata;
        //TODO: From below sequence starts threading?
        if (!shapeTransformOutline(shape, *sdata)) return sdata;
        if (!shapeGenRle(shape, *sdata)) return sdata;
    }

    return sdata;
}


int SwEngine::init()
{
    return RasterMethodInit::init(engineInit, new SwEngine);
}


int SwEngine::term()
{
    return RasterMethodInit::term(engineInit);
}


size_t SwEngine::unref()
{
    return RasterMethodInit::unref(engineInit);
}


size_t SwEngine::ref()
{
    return RasterMethodInit::ref(engineInit);
}


SwEngine* SwEngine::inst()
{
    return dynamic_cast<SwEngine*>(RasterMethodInit::inst(engineInit));
}


#endif /* _TVG_SW_ENGINE_CPP_ */
