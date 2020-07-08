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

static SwBBox _clipRegion(Surface& surface, SwBBox& in)
{
    auto bbox = in;

    if (bbox.min.x < 0) bbox.min.x = 0;
    if (bbox.min.y < 0) bbox.min.y = 0;
    if (bbox.max.x > surface.w) bbox.max.x = surface.w;
    if (bbox.max.y > surface.h) bbox.max.y = surface.h;

    return bbox;
}

static bool _rasterTranslucentRect(Surface& surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface.buffer + (region.min.y * surface.stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto ialpha = 255 - COLOR_ALPHA(color);

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface.stride];
        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = color + COLOR_ALPHA_BLEND(dst[x], ialpha);
        }
    }
    return true;
}


static bool _rasterSolidRect(Surface& surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface.buffer + (region.min.y * surface.stride);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);

    for (uint32_t y = 0; y < h; ++y) {
        rasterARGB32(buffer + y * surface.stride, color, region.min.x, w);
    }
    return true;
}


static bool _rasterTranslucentRle(Surface& surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    auto span = rle->spans;
    uint32_t src;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface.buffer[span->y * surface.stride + span->x];
        if (span->coverage < 255) src = COLOR_ALPHA_BLEND(color, span->coverage);
        else src = color;
        auto ialpha = 255 - COLOR_ALPHA(src);
        for (uint32_t i = 0; i < span->len; ++i) {
            dst[i] = src + COLOR_ALPHA_BLEND(dst[i], ialpha);
        }
        ++span;
    }
    return true;
}


static bool _rasterSolidRle(Surface& surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i) {
        if (span->coverage == 255) {
            rasterARGB32(surface.buffer + span->y * surface.stride, color, span->x, span->len);
        } else {
            auto dst = &surface.buffer[span->y * surface.stride + span->x];
            auto src = COLOR_ALPHA_BLEND(color, span->coverage);
            auto ialpha = 255 - span->coverage;
            for (uint32_t i = 0; i < span->len; ++i) {
                dst[i] = src + COLOR_ALPHA_BLEND(dst[i], ialpha);
            }
        }
        ++span;
    }
    return true;
}


static bool _rasterLinearGradientRect(Surface& surface, const SwBBox& region, const SwFill* fill)
{
    if (!fill) return false;

    auto buffer = surface.buffer + (region.min.y * surface.stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //Translucent Gradient
    if (fill->translucent) {

        auto tmpBuf = static_cast<uint32_t*>(alloca(surface.w * sizeof(uint32_t)));
        if (!tmpBuf) return false;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface.stride];
            fillFetchLinear(fill, tmpBuf, region.min.y + y, region.min.x, 0, w);
            for (uint32_t x = 0; x < w; ++x) {
                dst[x] = tmpBuf[x] + COLOR_ALPHA_BLEND(dst[x], 255 - COLOR_ALPHA(tmpBuf[x]));
            }
        }
    //Opaque Gradient
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            fillFetchLinear(fill, buffer + y * surface.stride, region.min.y + y, region.min.x, 0, w);
        }
    }
    return true;
}


static bool _rasterRadialGradientRect(Surface& surface, const SwBBox& region, const SwFill* fill)
{
    if (!fill) return false;

    auto buffer = surface.buffer + (region.min.y * surface.stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //Translucent Gradient
    if (fill->translucent) {

        auto tmpBuf = static_cast<uint32_t*>(alloca(surface.w * sizeof(uint32_t)));
        if (!tmpBuf) return false;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface.stride];
            fillFetchRadial(fill, tmpBuf, region.min.y + y, region.min.x, w);
            for (uint32_t x = 0; x < w; ++x) {
                dst[x] = tmpBuf[x] + COLOR_ALPHA_BLEND(dst[x], 255 - COLOR_ALPHA(tmpBuf[x]));
            }
        }
    //Opaque Gradient
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface.stride];
            fillFetchRadial(fill, dst, region.min.y + y, region.min.x, w);
        }
    }
    return true;
}


static bool _rasterLinearGradientRle(Surface& surface, SwRleData* rle, const SwFill* fill)
{
    if (!rle || !fill) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface.w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface.buffer[span->y * surface.stride + span->x];
            fillFetchLinear(fill, buf, span->y, span->x, 0, span->len);
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
            if (span->coverage == 255) {
                fillFetchLinear(fill, surface.buffer + span->y * surface.stride, span->y, span->x, span->x, span->len);
            } else {
                auto dst = &surface.buffer[span->y * surface.stride + span->x];
                fillFetchLinear(fill, buf, span->y, span->x, 0, span->len);
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

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface.buffer[span->y * surface.stride + span->x];
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
            auto dst = &surface.buffer[span->y * surface.stride + span->x];
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
    //Fast Track
    if (shape.rect) {
        auto region = _clipRegion(surface, shape.bbox);
        if (id == FILL_ID_LINEAR) return _rasterLinearGradientRect(surface, region, shape.fill);
        return _rasterRadialGradientRect(surface, region, shape.fill);
    } else {
        if (id == FILL_ID_LINEAR) return _rasterLinearGradientRle(surface, shape.rle, shape.fill);
        return _rasterRadialGradientRle(surface, shape.rle, shape.fill);
    }
    return false;
}


bool rasterSolidShape(Surface& surface, SwShape& shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    //Fast Track
    if (shape.rect) {
        auto region = _clipRegion(surface, shape.bbox);
        if (a == 255) return _rasterSolidRect(surface, region, COLOR_ARGB_JOIN(r, g, b, a));
        return _rasterTranslucentRect(surface, region, COLOR_ARGB_JOIN(r, g, b, a));
    } else{
        if (a == 255) return _rasterSolidRle(surface, shape.rle, COLOR_ARGB_JOIN(r, g, b, a));
        return _rasterTranslucentRle(surface, shape.rle, COLOR_ARGB_JOIN(r, g, b, a));
    }
    return false;
}


bool rasterStroke(Surface& surface, SwShape& shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (a == 255) return _rasterSolidRle(surface, shape.strokeRle, COLOR_ARGB_JOIN(r, g, b, a));
    return _rasterTranslucentRle(surface, shape.strokeRle, COLOR_ARGB_JOIN(r, g, b, a));
}


bool rasterClear(Surface& surface)
{
    if (!surface.buffer || surface.stride <= 0 || surface.w <= 0 || surface.h <= 0) return false;

    if (surface.w == surface.stride) {
        rasterARGB32(surface.buffer, 0x00000000, 0, surface.w * surface.h);
    } else {
        for (uint32_t i = 0; i < surface.h; i++) {
            rasterARGB32(surface.buffer + surface.stride * i, 0x00000000, 0, surface.w);
        }
    }
    return true;
}

#endif /* _TVG_SW_RASTER_CPP_ */