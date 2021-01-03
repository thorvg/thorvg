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
#include <math.h>

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


static void _inverse(const Matrix* transform, Matrix* invM)
{
    // computes the inverse of a matrix m
    auto det = transform->e11 * (transform->e22 * transform->e33 - transform->e32 * transform->e23) -
               transform->e12 * (transform->e21 * transform->e33 - transform->e23 * transform->e31) +
               transform->e13 * (transform->e21 * transform->e32 - transform->e22 * transform->e31);

    auto invDet = 1 / det;

    invM->e11 = (transform->e22 * transform->e33 - transform->e32 * transform->e23) * invDet;
    invM->e12 = (transform->e13 * transform->e32 - transform->e12 * transform->e33) * invDet;
    invM->e13 = (transform->e12 * transform->e23 - transform->e13 * transform->e22) * invDet;
    invM->e21 = (transform->e23 * transform->e31 - transform->e21 * transform->e33) * invDet;
    invM->e22 = (transform->e11 * transform->e33 - transform->e13 * transform->e31) * invDet;
    invM->e23 = (transform->e21 * transform->e13 - transform->e11 * transform->e23) * invDet;
    invM->e31 = (transform->e21 * transform->e32 - transform->e31 * transform->e22) * invDet;
    invM->e32 = (transform->e31 * transform->e12 - transform->e11 * transform->e32) * invDet;
    invM->e33 = (transform->e11 * transform->e22 - transform->e21 * transform->e12) * invDet;
}


static bool _identify(const Matrix* transform)
{
    if (!transform ||
        transform->e11 != 1.0f || transform->e12 != 0.0f || transform->e13 != 0.0f ||
        transform->e21 != 0.0f || transform->e22 != 1.0f || transform->e23 != 0.0f ||
        transform->e31 != 0.0f || transform->e32 != 0.0f || transform->e33 != 1.0f)
        return false;

    return true;
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


static bool _translucent(SwSurface* surface, uint8_t a)
{
    if (a < 255) return true;
    if (!surface->compositor || surface->compositor->method == CompositeMethod::None) return false;
    return true;
}

static bool _translucentRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto ialpha = 255 - surface->blender.alpha(color);

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = color + ALPHA_BLEND(dst[x], ialpha);
        }
    }
    return true;
}


static bool _translucentRectAlphaMask(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

#ifdef THORVG_LOG_ENABLED
    printf("SW_ENGINE: Rectangle Alpha Mask Composition\n");
#endif

    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer
    auto tbuffer = static_cast<uint32_t*>(alloca(sizeof(uint32_t) * w));                                //temp buffer for intermediate processing

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        auto cmp = &cbuffer[y * surface->stride];
        auto tmp = tbuffer;
        //Composition
        for (uint32_t x = 0; x < w; ++x) {
            *tmp = ALPHA_BLEND(color, surface->blender.alpha(*cmp));
            dst[x] = *tmp + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(*tmp));
            ++tmp;
            ++cmp;
        }
    }
    return true;
}


static bool _rasterTranslucentRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentRectAlphaMask(surface, region, color);
        }
    }
    return _translucentRect(surface, region, color);
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


static bool _translucentRle(SwSurface* surface, SwRleData* rle, uint32_t color)
{
    auto span = rle->spans;
    uint32_t src;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        auto ialpha = 255 - surface->blender.alpha(src);
        for (uint32_t x = 0; x < span->len; ++x) {
            dst[x] = src + ALPHA_BLEND(dst[x], ialpha);
        }
        ++span;
    }
    return true;
}


static bool _translucentRleAlphaMask(SwSurface* surface, SwRleData* rle, uint32_t color)
{
#ifdef THORVG_LOG_ENABLED
    printf("SW_ENGINE: Rle Alpha Mask Composition\n");
#endif
    auto span = rle->spans;
    uint32_t src;
    auto tbuffer = static_cast<uint32_t*>(alloca(sizeof(uint32_t) * surface->w));  //temp buffer for intermediate processing
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto tmp = tbuffer;
        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        for (uint32_t x = 0; x < span->len; ++x) {
            *tmp = ALPHA_BLEND(src, surface->blender.alpha(*cmp));
            dst[x] = *tmp + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(*tmp));
            ++tmp;
            ++cmp;
        }
        ++span;
    }
    return true;
}


static bool _rasterTranslucentRle(SwSurface* surface, SwRleData* rle, uint32_t color)
{
    if (!rle) return false;

    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentRleAlphaMask(surface, rle, color);
        }
    }
    return _translucentRle(surface, rle, color);
}


static bool _rasterTranslucentImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const Matrix* invTransform)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
            auto src = ALPHA_BLEND(img[rY * w + rX], alpha);     //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        ++span;
    }
    return true;
}


static bool _rasterImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, const Matrix* invTransform)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * w + rX], span->coverage);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        ++span;
    }
    return true;
}


static bool _rasterTranslucentImage(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * w)], opacity);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterImage(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, const SwBBox& region)
{
    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto src = img + region.min.x + (y * w);    //TODO: need to use image's stride
        for (auto x = region.min.x; x < region.max.x; x++, dst++, src++) {
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
    }
    return true;
}


static bool _rasterImage(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, const SwBBox& region, const Matrix* invTransform)
{
    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = img[rX + (rY * w)];    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
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
                dst[x] = tmpBuf[x] + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(tmpBuf[x]));
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
                dst[x] = tmpBuf[x] + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(tmpBuf[x]));
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
                    dst[i] = buf[i] + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(tmp));
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
                    dst[i] = buf[i] + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(buf[i]));
                }
            } else {
                for (uint32_t i = 0; i < span->len; ++i) {
                    auto tmp = ALPHA_BLEND(buf[i], span->coverage);
                    dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(tmp));
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
        surface->blender.alpha = _colorAlpha;
        surface->blender.join = _abgrJoin;
    } else if (surface->cs == SwCanvas::ARGB8888) {
        surface->blender.alpha = _colorAlpha;
        surface->blender.join = _argbJoin;
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

    auto color = surface->blender.join(r, g, b, a);
    auto translucent = _translucent(surface, a);

    //Fast Track
    if (shape->rect) {
        auto region = _clipRegion(surface, shape->bbox);
        if (translucent) return _rasterTranslucentRect(surface, region, color);
        return _rasterSolidRect(surface, region, color);
    }
    if (translucent) {
        return _rasterTranslucentRle(surface, shape->rle, color);
    }
    return _rasterSolidRle(surface, shape->rle, color);
}


bool rasterStroke(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    r = ALPHA_MULTIPLY(r, a);
    g = ALPHA_MULTIPLY(g, a);
    b = ALPHA_MULTIPLY(b, a);

    auto color = surface->blender.join(r, g, b, a);
    auto translucent = _translucent(surface, a);

    if (translucent) return _rasterTranslucentRle(surface, shape->strokeRle, color);
    return _rasterSolidRle(surface, shape->strokeRle, color);
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


bool rasterImage(SwSurface* surface, SwImage* image, const Matrix* transform, SwBBox& bbox, uint8_t opacity)
{
    Matrix invTransform;

    if (transform) _inverse(transform, &invTransform);
    else invTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    if (image->rle) {
        if (opacity < 255) return _rasterTranslucentImageRle(surface, image->rle, image->data, image->w, image->h, opacity, &invTransform);
        return _rasterImageRle(surface, image->rle, image->data, image->w, image->h, &invTransform);
    }
    else {
        // Fast track
        if (_identify(transform)) {
            return _rasterImage(surface, image->data, image->w, image->h, bbox);
        }
        else {
            if (opacity < 255) return _rasterTranslucentImage(surface, image->data, image->w, image->h, opacity, bbox, &invTransform);
            return _rasterImage(surface, image->data, image->w, image->h, bbox, &invTransform);
        }
    }
}