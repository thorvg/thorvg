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
#ifndef _TVG_GL_RASTER_CPP_
#define _TVG_GL_RASTER_CPP_

#include "tvgCommon.h"
#include "tvgGlRaster.h"


static GlRaster* pInst = nullptr;

struct GlShape
{
    //TODO:
};


void* GlRaster::prepare(const ShapeNode& shape, void* data, UpdateFlag flags)
{
    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = static_cast<GlShape*>(calloc(1, sizeof(GlShape)));
        assert(sdata);
    }

    return sdata;
}


int GlRaster::init()
{
    if (pInst) return -1;
    pInst = new GlRaster();
    assert(pInst);

    return 0;
}


int GlRaster::term()
{
    if (!pInst) return -1;
    cout << "GlRaster(" << pInst << ") destroyed!" << endl;
    delete(pInst);
    pInst = nullptr;
    return 0;
}


GlRaster* GlRaster::inst()
{
    assert(pInst);
    return pInst;
}


#endif /* _TVG_GL_RASTER_CPP_ */
