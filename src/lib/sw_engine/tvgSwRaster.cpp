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
#ifndef _TVG_SW_RASTER_CPP_
#define _TVG_SW_RASTER_CPP_

#include "tvgSwCommon.h"
#include "tvgSwRaster.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static SwRaster* pInst = nullptr;


void* SwRaster::prepare(ShapeNode *shape, void* data)
{
    SwShape *sdata = static_cast<SwShape*>(data);
    if (!sdata) {
        sdata = static_cast<SwShape*>(calloc(1, sizeof(SwShape)));
        assert(sdata);
    }

    bool closed = shapeGenOutline(shape, sdata);

    return sdata;
}


int SwRaster::init()
{
    if (pInst) return -1;
    pInst = new SwRaster();
    assert(pInst);

    return 0;
}


int SwRaster::term()
{
    if (!pInst) return -1;
    cout << "SwRaster(" << pInst << ") destroyed!" << endl;
    delete(pInst);
    pInst = nullptr;
    return 0;
}


SwRaster* SwRaster::inst()
{
    assert(pInst);
    return pInst;
}


#endif /* _TVG_SW_RASTER_CPP_ */
