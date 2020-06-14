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

static bool _rasterTranslucentRle(Surface& surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    auto span = rle->spans;
    auto stride = surface.stride;
    uint32_t tmp;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface.buffer[span->y * stride + span->x];
        if (span->coverage < 255) tmp = COLOR_ALPHA_BLEND(color, span->coverage);
        else tmp = color;
        for (uint32_t i = 0; i < span->len; ++i) {
            dst[i] = tmp + COLOR_ALPHA_BLEND(dst[i], 255 - COLOR_ALPHA(tmp));
        }
        ++span;
    }
    return true;
}


static bool _rasterSolidRle(Surface& surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    auto span = rle->spans;
    auto stride = surface.stride;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface.buffer[span->y * stride + span->x];
        if (span->coverage == 255) {
            for (uint32_t i = 0; i < span->len; ++i) {
              dst[i] = color;
            }
        } else {
            for (uint32_t i = 0; i < span->len; ++i) {
                dst[i] = COLOR_ALPHA_BLEND(color, span->coverage) + COLOR_ALPHA_BLEND(dst[i], 255 - span->coverage);
            }
        }
        ++span;
    }
    return true;
}


static bool _rasterLinearGradientRle(Surface& surface, SwRleData* rle, const SwFill* fill)
{
    if (!rle || !fill) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface.w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;
    auto stride = surface.stride;

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface.buffer[span->y * stride + span->x];
            fillFetchLinear(fill, buf, span->y, span->x, span->len);
            if (span->coverage == 255) {
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = buf[i] + COLOR_ALPHA_BLEND(dst[i], 255 - COLOR_ALPHA(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = COLOR_ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + COLOR_ALPHA_BLEND(dst[i], 255 - COLOR_ALPHA(tmp));
                }
            }
            ++span;
        }
    //Opaque Gradient
    } else {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface.buffer[span->y * stride + span->x];
            if (span->coverage == 255) {
                fillFetchLinear(fill, dst, span->y, span->x, span->len);
            } else {
                fillFetchLinear(fill, buf, span->y, span->x, span->len);
                auto ialpha = 255 - span->coverage;
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = COLOR_ALPHA_BLEND(buf[i], span->coverage) + COLOR_ALPHA_BLEND(dst[i], ialpha);
                }
            }
            ++span;
        }
    }
    return true;
}


static bool _rasterRadialGradientRle(Surface& surface, SwRleData* rle, const SwFill* fill)
{
    if (!rle || !fill) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface.w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;
    auto stride = surface.stride;

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface.buffer[span->y * stride + span->x];
            fillFetchRadial(fill, buf, span->y, span->x, span->len);
            if (span->coverage == 255) {
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = buf[i] + COLOR_ALPHA_BLEND(dst[i], 255 - COLOR_ALPHA(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = COLOR_ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + COLOR_ALPHA_BLEND(dst[i], 255 - COLOR_ALPHA(tmp));
                }
            }
            ++span;
        }
    //Opaque Gradient
    } else {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface.buffer[span->y * stride + span->x];
            if (span->coverage == 255) {
                fillFetchRadial(fill, dst, span->y, span->x, span->len);
            } else {
                fillFetchRadial(fill, buf, span->y, span->x, span->len);
                auto ialpha = 255 - span->coverage;
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = COLOR_ALPHA_BLEND(buf[i], span->coverage) + COLOR_ALPHA_BLEND(dst[i], ialpha);
                }
            }
            ++span;
        }
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool rasterGradientShape(Surface& surface, SwShape& shape, unsigned id)
{
    if (id == FILL_ID_LINEAR) return _rasterLinearGradientRle(surface, shape.rle, shape.fill);
    return _rasterRadialGradientRle(surface, shape.rle, shape.fill);
}


bool rasterSolidShape(Surface& surface, SwShape& shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (a == 255) return _rasterSolidRle(surface, shape.rle, COLOR_ARGB_JOIN(r, g, b, a));
    return _rasterTranslucentRle(surface, shape.rle, COLOR_ARGB_JOIN(r, g, b, a));
}


bool rasterStroke(Surface& surface, SwShape& shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (a == 255) return _rasterSolidRle(surface, shape.strokeRle, COLOR_ARGB_JOIN(r, g, b, a));
    return _rasterTranslucentRle(surface, shape.strokeRle, COLOR_ARGB_JOIN(r, g, b, a));
}


#endif /* _TVG_SW_RASTER_CPP_ */