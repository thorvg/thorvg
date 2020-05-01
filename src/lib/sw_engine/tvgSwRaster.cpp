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


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static inline size_t COLOR_ALPHA_BLEND(size_t color, size_t alpha)
{
  return (((((color >> 8) & 0x00ff00ff) * alpha) & 0xff00ff00) +
          ((((color & 0x00ff00ff) * alpha) >> 8) & 0x00ff00ff));
}


static inline size_t COLOR_ARGB_JOIN(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | r << 16 | g << 8 | b);
}


static void
_drawTranslucentSpan(uint32_t* dst, size_t len, size_t color, size_t alpha)
{
  //OPTIMIZE ME: SIMD
  auto ialpha = 255 - alpha;
  for (size_t i = 0; i < len; ++i) {
    dst[i] = color + COLOR_ALPHA_BLEND(dst[i], ialpha);
  }
}


static void
_drawSolidSpan(uint32_t* dst, size_t len, size_t color)
{
  //OPTIMIZE ME: SIMD
  for (size_t i = 0; i < len; ++i) {
    dst[i] = color;
  }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool rasterShape(Surface& surface, SwShape& sdata, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SwRleData* rle = sdata.rle;
    if (!rle) return false;

    auto span = rle->spans;
    auto stride = surface.stride;
    auto color = COLOR_ARGB_JOIN(r, g, b, a);

    for (size_t i = 0; i < rle->size; ++i) {
        assert(span);

        auto dst = &surface.buffer[span->y * stride + span->x];
        assert(dst);

        if (a == 255) _drawSolidSpan(dst, span->len, color);
        else _drawTranslucentSpan(dst, span->len, color, a);

        ++span;
    }

    return true;
}

#endif /* _TVG_SW_RASTER_CPP_ */