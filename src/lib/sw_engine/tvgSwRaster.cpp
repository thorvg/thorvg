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


bool rasterShape(Surface& surface, SwShape& sdata, size_t color)
{
    SwRleData* rle = sdata.rle;
    assert(rle);

    auto stride = surface.stride;
    auto span = rle->spans;

    for (size_t i = 0; i < rle->size; ++i) {
        assert(span);
        for (auto j = 0; j < span->len; ++j) {
            surface.buffer[span->y * stride + span->x + j] = color;
        }
        ++span;
    }

    return true;
}

#endif /* _TVG_SW_RASTER_CPP_ */