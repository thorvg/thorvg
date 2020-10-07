/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "tvgSwCommon.h"
#include "tvgRender.h"
#include <float.h>

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static uint32_t _colorAlpha(uint32_t c)
{
    return (c >> 24) & 0xff;
}


static uint32_t _abgrJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | b << 16 | g << 8 | r);
}


static uint32_t _argbJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | r << 16 | g << 8 | b);
}


static SwBBox _clipRegion(Surface* surface, SwBBox& in)
{
    auto bbox = in;

    if (bbox.min.x < 0) bbox.min.x = 0;
    if (bbox.min.y < 0) bbox.min.y = 0;
    if (bbox.max.x > static_cast<SwCoord>(surface->w)) bbox.max.x = surface->w;
    if (bbox.max.y > static_cast<SwCoord>(surface->h)) bbox.max.y = surface->h;

    return bbox;
}

static bool _rasterTranslucentRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto ialpha = 255 - surface->comp.alpha(color);

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = color + ALPHA_BLEND(dst[x], ialpha);
        }
    }
    return true;
}


static bool _rasterSolidRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);

    for (uint32_t y = 0; y < h; ++y) {
        rasterRGBA32(buffer + y * surface->stride, color, region.min.x, w);
    }
    return true;
}


static bool _rasterTranslucentRle(SwSurface* surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    auto span = rle->spans;
    uint32_t src;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        auto ialpha = 255 - surface->comp.alpha(src);
        for (uint32_t i = 0; i < span->len; ++i) {
            dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
        }
        ++span;
    }
    return true;
}


static bool _rasterSolidRle(SwSurface* surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i) {
        if (span->coverage == 255) {
            rasterRGBA32(surface->buffer + span->y * surface->stride, color, span->x, span->len);
        } else {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto src = ALPHA_BLEND(color, span->coverage);
            auto ialpha = 255 - span->coverage;
            for (uint32_t i = 0; i < span->len; ++i) {
                dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
            }
        }
        ++span;
    }
    return true;
}


static bool _rasterLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (!fill || fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //Translucent Gradient
    if (fill->translucent) {

        auto tmpBuf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
        if (!tmpBuf) return false;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            fillFetchLinear(fill, tmpBuf, region.min.y + y, region.min.x, 0, w);
            for (uint32_t x = 0; x < w; ++x) {
                dst[x] = tmpBuf[x] + ALPHA_BLEND(dst[x], 255 - surface->comp.alpha(tmpBuf[x]));
            }
        }
    //Opaque Gradient
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            fillFetchLinear(fill, buffer + y * surface->stride, region.min.y + y, region.min.x, 0, w);
        }
    }
    return true;
}


static bool _rasterRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (!fill || fill->radial.a < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    //Translucent Gradient
    if (fill->translucent) {

        auto tmpBuf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
        if (!tmpBuf) return false;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            fillFetchRadial(fill, tmpBuf, region.min.y + y, region.min.x, w);
            for (uint32_t x = 0; x < w; ++x) {
                dst[x] = tmpBuf[x] + ALPHA_BLEND(dst[x], 255 - surface->comp.alpha(tmpBuf[x]));
            }
        }
    //Opaque Gradient
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            fillFetchRadial(fill, dst, region.min.y + y, region.min.x, w);
        }
    }
    return true;
}


static bool _rasterLinearGradientRle(SwSurface* surface, SwRleData* rle, const SwFill* fill)
{
    if (!rle || !fill || fill->linear.len < FLT_EPSILON) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            fillFetchLinear(fill, buf, span->y, span->x, 0, span->len);
            if (span->coverage == 255) {
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = buf[i] + ALPHA_BLEND(dst[i], 255 - surface->comp.alpha(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->comp.alpha(tmp));
                }
            }
            ++span;
        }
    //Opaque Gradient
    } else {
        for (uint32_t i = 0; i < rle->size; ++i) {
            if (span->coverage == 255) {
                fillFetchLinear(fill, surface->buffer + span->y * surface->stride, span->y, span->x, span->x, span->len);
            } else {
                auto dst = &surface->buffer[span->y * surface->stride + span->x];
                fillFetchLinear(fill, buf, span->y, span->x, 0, span->len);
                auto ialpha = 255 - span->coverage;
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = ALPHA_BLEND(buf[i], span->coverage) + ALPHA_BLEND(dst[i], ialpha);
                }
            }
            ++span;
        }
    }
    return true;
}


static bool _rasterRadialGradientRle(SwSurface* surface, SwRleData* rle, const SwFill* fill)
{
    if (!rle || !fill || fill->radial.a < FLT_EPSILON) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;

    //Translucent Gradient
    if (fill->translucent) {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            fillFetchRadial(fill, buf, span->y, span->x, span->len);
            if (span->coverage == 255) {
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = buf[i] + ALPHA_BLEND(dst[i], 255 - surface->comp.alpha(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->comp.alpha(tmp));
                }
            }
            ++span;
        }
    //Opaque Gradient
    } else {
        for (uint32_t i = 0; i < rle->size; ++i) {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            if (span->coverage == 255) {
                fillFetchRadial(fill, dst, span->y, span->x, span->len);
            } else {
                fillFetchRadial(fill, buf, span->y, span->x, span->len);
                auto ialpha = 255 - span->coverage;
                for (uint32_t i = 0; i < span->len; ++i) {
                    dst[i] = ALPHA_BLEND(buf[i], span->coverage) + ALPHA_BLEND(dst[i], ialpha);
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

bool rasterCompositor(SwSurface* surface)
{
    if (surface->cs == SwCanvas::ABGR8888) {
        surface->comp.alpha = _colorAlpha;
        surface->comp.join = _abgrJoin;
    } else if (surface->cs == SwCanvas::ARGB8888) {
        surface->comp.alpha = _colorAlpha;
        surface->comp.join = _argbJoin;
    } else {
        //What Color Space ???
        return false;
    }

    return true;
}


bool rasterGradientShape(SwSurface* surface, SwShape* shape, unsigned id)
{
    //Fast Track
    if (shape->rect) {
        auto region = _clipRegion(surface, shape->bbox);
        if (id == FILL_ID_LINEAR) return _rasterLinearGradientRect(surface, region, shape->fill);
        return _rasterRadialGradientRect(surface, region, shape->fill);
    } else {
        if (id == FILL_ID_LINEAR) return _rasterLinearGradientRle(surface, shape->rle, shape->fill);
        return _rasterRadialGradientRle(surface, shape->rle, shape->fill);
    }
    return false;
}


bool rasterSolidShape(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    r = ALPHA_MULTIPLY(r, a);
    g = ALPHA_MULTIPLY(g, a);
    b = ALPHA_MULTIPLY(b, a);

    auto color = surface->comp.join(r, g, b, a);

    //Fast Track
    if (shape->rect) {
        auto region = _clipRegion(surface, shape->bbox);
        if (a == 255) return _rasterSolidRect(surface, region, color);
        return _rasterTranslucentRect(surface, region, color);
    } else{
        if (a == 255) return _rasterSolidRle(surface, shape->rle, color);
        return _rasterTranslucentRle(surface, shape->rle, color);
    }
    return false;
}


bool rasterStroke(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    r = ALPHA_MULTIPLY(r, a);
    g = ALPHA_MULTIPLY(g, a);
    b = ALPHA_MULTIPLY(b, a);

    auto color = surface->comp.join(r, g, b, a);

    if (a == 255) return _rasterSolidRle(surface, shape->strokeRle, color);
    return _rasterTranslucentRle(surface, shape->strokeRle, color);
}


bool rasterClear(SwSurface* surface)
{
    if (!surface || !surface->buffer || surface->stride <= 0 || surface->w <= 0 || surface->h <= 0) return false;

    if (surface->w == surface->stride) {
        rasterRGBA32(surface->buffer, 0x00000000, 0, surface->w * surface->h);
    } else {
        for (uint32_t i = 0; i < surface->h; i++) {
            rasterRGBA32(surface->buffer + surface->stride * i, 0x00000000, 0, surface->w);
        }
    }
    return true;
}