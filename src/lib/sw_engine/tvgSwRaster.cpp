/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include "tvgMath.h"
#include "tvgRender.h"
#include "tvgSwCommon.h"
#include "tvgSwRasterC.h"
#include "tvgSwRasterAvx.h"
#include "tvgSwRasterNeon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static uint32_t _colorAlpha(uint32_t c)
{
    return (c >> 24);
}


static uint32_t _colorInvAlpha(uint32_t c)
{
    return (~c >> 24);
}


static uint32_t _abgrJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | b << 16 | g << 8 | r);
}


static uint32_t _argbJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | r << 16 | g << 8 | b);
}


static bool _translucent(const SwSurface* surface, uint8_t a)
{
    if (a < 255) return true;
    if (!surface->compositor || surface->compositor->method == CompositeMethod::None) return false;
    return true;
}


static uint32_t _applyBilinearInterpolation(const uint32_t *img, uint32_t w, uint32_t h, float fX, float fY)
{
    auto rX = static_cast<uint32_t>(fX);
    auto rY = static_cast<uint32_t>(fY);

    auto dX = static_cast<uint32_t>((fX - rX) * 255.0);
    auto dY = static_cast<uint32_t>((fY - rY) * 255.0);

    auto c1 = img[rX + (rY * w)];
    auto c2 = img[(rX + 1) + (rY * w)];
    auto c3 = img[(rX + 1) + ((rY + 1) * w)];
    auto c4 = img[rX + ((rY + 1) * w)];

    if (c1 == c2 && c1 == c3 && c1 == c4) return img[rX + (rY * w)];
    return COLOR_INTERPOLATE(COLOR_INTERPOLATE(c1, 255 - dX, c2, dX), 255 - dY, COLOR_INTERPOLATE(c4, 255 - dX, c3, dX), dY);
}

static uint32_t _average2Nx2NPixel(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t rX, uint32_t rY, uint32_t n)
{
    uint32_t c[4] = { 0 };
    auto n2 = n * n;
    auto source = img + rX - n + (rY - n) * w;
    for (auto y = rY - n; y < rY + n; ++y) {
        auto src = source;
        for (auto x = rX - n; x < rX + n; ++x, ++src) {
            c[0] += *src >> 24;
            c[1] += (*src >> 16) & 0xff;
            c[2] += (*src >> 8) & 0xff;
            c[3] += *src & 0xff;
        }
        source += w;
    }
    for (auto i = 0; i < 4; ++i) {
        c[i] = (c[i] >> 2) / n2;
    }
    return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}

/************************************************************************/
/* Rect                                                                 */
/************************************************************************/

static bool _translucentRectMask(SwSurface* surface, const SwBBox& region, uint32_t color, uint32_t (*blendMethod)(uint32_t rgba))
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    TVGLOG("SW_ENGINE", "Rectangle Alpha Mask / Inverse Alpha Mask Composition");

    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        auto cmp = &cbuffer[y * surface->stride];
        for (uint32_t x = 0; x < w; ++x) {
            auto tmp = ALPHA_BLEND(color, blendMethod(*cmp));
            dst[x] = tmp + ALPHA_BLEND(dst[x], surface->blender.ialpha(tmp));
            ++cmp;
        }
    }
    return true;
}

static bool _rasterTranslucentRect(SwSurface* surface, const SwBBox& region, uint32_t color)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentRectMask(surface, region, color, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentRectMask(surface, region, color, surface->blender.ialpha);
        }
    }

#if defined(THORVG_AVX_VECTOR_SUPPORT)
    return avxRasterTranslucentRect(surface, region, color);
#elif defined(THORVG_NEON_VECTOR_SUPPORT)
    return neonRasterTranslucentRect(surface, region, color);
#else
    return cRasterTranslucentRect(surface, region, color);
#endif
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


/************************************************************************/
/* Rle                                                                  */
/************************************************************************/

static bool _translucentRleMask(SwSurface* surface, SwRleData* rle, uint32_t color, uint32_t (*blendMethod)(uint32_t rgba))
{
    TVGLOG("SW_ENGINE", "Rle Alpha Mask / Inverse Alpha Mask Composition");

    auto span = rle->spans;
    uint32_t src;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        for (uint32_t x = 0; x < span->len; ++x) {
            auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
            dst[x] = tmp + ALPHA_BLEND(dst[x], surface->blender.ialpha(tmp));
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
            return _translucentRleMask(surface, rle, color, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentRleMask(surface, rle, color, surface->blender.ialpha);
        }
    }

#if defined(THORVG_NEON_VECTOR_SUPPORT)
    return neonRasterTranslucentRle(surface, rle, color);
#else
    return cRasterTranslucentRle(surface, rle, color);
#endif
}


static bool _rasterSolidRle(SwSurface* surface, const SwRleData* rle, uint32_t color)
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


/************************************************************************/
/* Image                                                                */
/************************************************************************/

static bool _translucentImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto src = image->data + span->x + span->y * image->w;    //TODO: need to use image's stride
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++src) {
            *src = ALPHA_BLEND(*src, alpha);
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
    }
    return true;
}


static bool _rasterTranslucentImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentImageRleAlphaMask()");
//          return _translucentImageRleAlphaMask(surface, image, opacity);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentImageRleInvAlphaMask()");
//            return _translucentImageRleInvAlphaMask(surface, image, opacity);
        }
    }
    return _translucentImageRle(surface, image, opacity);
}


static bool _translucentImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * w + rX], alpha);     //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterTranslucentImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentImageRleAlphaMask()");
//          return _translucentImageRleAlphaMask(surface, image, opacity, itransform);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentImageRleInvAlphaMask()");
//          return _translucentImageRleInvAlphaMask(surface, image, opacity, itransform);
        }
    }
    return _translucentImageRle(surface, image, opacity, itransform);
}


static bool _translucentUpScaleImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto fX = (span->x + x) * itransform->e11 + ey1;
            auto fY = (span->x + x) * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * w + rX], alpha);     //TODO: need to use image's stride
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, w, h, fX, fY), alpha);     //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterTranslucentUpScaleImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentUpScaleImageRleAlphaMask()");
//          return _translucentUpScaleImageRleAlphaMask(surface, image, opacity, itransform);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentUpScaleImageRleInvAlphaMask()");
//          return _translucentUpScaleImageRleInvAlphaMask(surface, image, opacity, itransform);
        }
    }
    return _translucentUpScaleImageRle(surface, image, opacity, itransform);
}


static bool _translucentDownScaleImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, float scale)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    auto halfScale = static_cast<uint32_t>(0.5f / scale);
    if (halfScale == 0) halfScale = 1;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * w + rX], alpha);     //TODO: need to use image's stride
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScale), alpha);     //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterTranslucentDownScaleImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, float scale)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentDownScaleImageRleAlphaMask()");
//          return _translucentDownScaleImageRleAlphaMask(surface, image, opacity, itransform, scale);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            TVGERR("SW_ENGINE", "Missing Implementation _translucentDownScaleImageRleInvAlphaMask()");
//          return _translucentDownScaleImageRleInvAlphaMask(surface, image, opacity, itransform, scale);
        }
    }
    return _translucentDownScaleImageRle(surface, image, opacity, itransform, scale);
}


static bool _rasterImageRle(SwSurface* surface, const SwImage* image)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto src = image->data + span->x + span->y * image->w;    //TODO: need to use image's stride
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++src) {
            *src = ALPHA_BLEND(*src, span->coverage);
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
    }
    return true;
}


static bool _rasterImageRle(SwSurface* surface, const SwImage* image, const Matrix* itransform)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * w + rX], span->coverage);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterUpScaleImageRle(SwSurface* surface, const SwImage* image, const Matrix* itransform)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto fX = (span->x + x) * itransform->e11 + ey1;
            auto fY = (span->x + x) * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * w + rX], span->coverage);    //TODO: need to use image's stride
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, w, h, fX, fY), span->coverage);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterDownScaleImageRle(SwSurface* surface, const SwImage* image, const Matrix* itransform, float scale)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    auto halfScale = static_cast<uint32_t>(0.5f / scale);
    if (halfScale == 0) halfScale = 1;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * w + rX], span->coverage);    //TODO: need to use image's stride
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScale), span->coverage);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _translucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform)
{
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto ey1 = y * itransform->e12 + itransform->e13;
        auto ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * w)], opacity);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _translucentImageMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t rgba))
{
    TVGLOG("SW_ENGINE", "Transformed Image AlphaMask / Inverse Alpha Mask Composition");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * itransform->e12 + itransform->e13;
        float ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, blendMethod(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentImageMask(surface, image, opacity, region, itransform, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentImageMask(surface, image, opacity, region, itransform, surface->blender.ialpha);
        }
    }
    return _translucentImage(surface, image, opacity, region, itransform);
}


static bool _translucentUpScaleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform)
{
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto ey1 = y * itransform->e12 + itransform->e13;
        auto ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto fX = x * itransform->e11 + ey1;
            auto fY = x * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rX + (rY * w)], opacity);    //TODO: need to use image's stride
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, w, h, fX, fY), opacity);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _translucentUpScaleImageMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t rgba))
{
    TVGLOG("SW_ENGINE", "Transformed Image Alpha Mask / Inverse Alpha Mask Composition");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * itransform->e12 + itransform->e13;
        float ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto fX = x * itransform->e11 + ey1;
            auto fY = x * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, blendMethod(*cmp)));  //TODO: need to use image's stride
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, w, h, fX, fY), ALPHA_MULTIPLY(opacity, blendMethod(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentUpScaleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentUpScaleImageMask(surface, image, opacity, region, itransform, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentUpScaleImageMask(surface, image, opacity, region, itransform, surface->blender.ialpha);
        }
    }
    return _translucentUpScaleImage(surface, image, opacity, region, itransform);
}


static bool _translucentDownScaleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, float scale)
{
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    auto halfScale = static_cast<uint32_t>(0.5f / scale);
    if (halfScale == 0) halfScale = 1;

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto ey1 = y * itransform->e12 + itransform->e13;
        auto ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rX + (rY * w)], opacity);
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScale), opacity);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _translucentDownScaleImageMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, float scale, uint32_t (*blendMethod)(uint32_t rgba))
{
    TVGLOG("SW_ENGINE", "Transformed Image Alpha Mask / Inverse Alpha Mask Composition");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    auto halfScale = static_cast<uint32_t>(0.5f / scale);
    if (halfScale == 0) halfScale = 1;

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * itransform->e12 + itransform->e13;
        float ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, blendMethod(*cmp)));  //TODO: need to use image's stride
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScale), ALPHA_MULTIPLY(opacity, blendMethod(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentDownScaleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, float scale)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentDownScaleImageMask(surface, image, opacity, region, itransform, scale, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentDownScaleImageMask(surface, image, opacity, region, itransform, scale, surface->blender.ialpha);
        }
    }
    return _translucentDownScaleImage(surface, image, opacity, region, itransform, scale);
}


static bool _translucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + region.min.x + region.min.y * image->w;    //TODO: need to use image's stride

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++src) {
            auto p = ALPHA_BLEND(*src, opacity);
            *dst = p + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(p));
        }
        dbuffer += surface->stride;
        sbuffer += image->w;    //TODO: need to use image's stride
    }
    return true;
}


static bool _translucentImageMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, uint32_t (*blendMethod)(uint32_t rgba))
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

    TVGLOG("SW_ENGINE", "Image Alpha Mask / Inverse Alpha Mask Composition");

    auto sbuffer = image->data + (region.min.y * image->w) + region.min.x;
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, ALPHA_MULTIPLY(opacity, blendMethod(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
        sbuffer += image->w;   //TODO: need to use image's stride
    }
    return true;
}


static bool _rasterTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentImageMask(surface, image, opacity, region, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentImageMask(surface, image, opacity, region, surface->blender.ialpha);
        }
    }
    return _translucentImage(surface, image, opacity, region);
}


static bool _rasterImage(SwSurface* surface, const SwImage* image, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + region.min.x + region.min.y * image->w;   //TODO: need to use image's stride

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; x++, dst++, src++) {
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
        dbuffer += surface->stride;
        sbuffer += image->w;    //TODO: need to use image's stride
    }
    return true;
}


static bool _rasterImage(SwSurface* surface, const SwImage* image, const SwBBox& region, const Matrix* itransform)
{
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * itransform->e12 + itransform->e13;
        auto ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = img[rX + (rY * w)];    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterUpScaleImage(SwSurface* surface, const SwImage* image, const SwBBox& region, const Matrix* itransform)
{
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * itransform->e12 + itransform->e13;
        auto ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto fX = x * itransform->e11 + ey1;
            auto fY = x * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = img[rX + (rY * w)];
            else src = _applyBilinearInterpolation(img, w, h, fX, fY);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterDownScaleImage(SwSurface* surface, const SwImage* image, const SwBBox& region, const Matrix* itransform, float scale)
{
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    auto halfScale = static_cast<uint32_t>(0.5f / scale);
    if (halfScale == 0) halfScale = 1;

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * itransform->e12 + itransform->e13;
        auto ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = img[rX + (rY * w)];
            else src = _average2Nx2NPixel(surface, img, w, h, rX, rY, halfScale);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


/************************************************************************/
/* Gradient                                                             */
/************************************************************************/

static bool _translucentLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    auto sbuffer = static_cast<uint32_t*>(alloca(w * sizeof(uint32_t)));
    if (!sbuffer) return false;

    auto dst = buffer;
    for (uint32_t y = 0; y < h; ++y) {
        fillFetchLinear(fill, sbuffer, region.min.y + y, region.min.x, w);
        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = sbuffer[x] + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(sbuffer[x]));
        }
        dst += surface->stride;
    }
    return true;
}


static bool _translucentLinearGradientRectMask(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t rgba))
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;

    auto sbuffer = static_cast<uint32_t*>(alloca(w * sizeof(uint32_t)));
    if (!sbuffer) return false;

    for (uint32_t y = 0; y < h; ++y) {
        fillFetchLinear(fill, sbuffer, region.min.y + y, region.min.x, w);
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w; ++x, ++dst, ++cmp, ++src) {
            auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentLinearGradientRectMask(surface, region, fill, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentLinearGradientRectMask(surface, region, fill, surface->blender.ialpha);
        }
    }
    return _translucentLinearGradientRect(surface, region, fill);
}


static bool _rasterOpaqueLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    for (uint32_t y = 0; y < h; ++y) {
        fillFetchLinear(fill, buffer + y * surface->stride, region.min.y + y, region.min.x, w);
    }
    return true;
}


static bool _translucentRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    auto sbuffer = static_cast<uint32_t*>(alloca(w * sizeof(uint32_t)));
    if (!sbuffer) return false;

    auto dst = buffer;
    for (uint32_t y = 0; y < h; ++y) {
        fillFetchRadial(fill, sbuffer, region.min.y + y, region.min.x, w);
        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = sbuffer[x] + ALPHA_BLEND(dst[x], 255 - surface->blender.alpha(sbuffer[x]));
        }
        dst += surface->stride;
    }
    return true;
}


static bool _translucentRadialGradientRectMask(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t rgba))
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;

    auto sbuffer = static_cast<uint32_t*>(alloca(w * sizeof(uint32_t)));
    if (!sbuffer) return false;

    for (uint32_t y = 0; y < h; ++y) {
        fillFetchRadial(fill, sbuffer, region.min.y + y, region.min.x, w);
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w; ++x, ++dst, ++cmp, ++src) {
             auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
             *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentRadialGradientRectMask(surface, region, fill, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentRadialGradientRectMask(surface, region, fill, surface->blender.ialpha);
        }
    }
    return _translucentRadialGradientRect(surface, region, fill);
}


static bool _rasterOpaqueRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        fillFetchRadial(fill, dst, region.min.y + y, region.min.x, w);
    }
    return true;
}


static bool _translucentLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        fillFetchLinear(fill, buffer, span->y, span->x, span->len);
        if (span->coverage == 255) {
            for (uint32_t i = 0; i < span->len; ++i) {
                dst[i] = buffer[i] + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(buffer[i]));
            }
        } else {
            for (uint32_t i = 0; i < span->len; ++i) {
                auto tmp = ALPHA_BLEND(buffer[i], span->coverage);
                dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(tmp));
            }
        }
    }
    return true;
}


static bool _translucentLinearGradientRleMask(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t rgba))
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto cbuffer = surface->compositor->image.data;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        fillFetchLinear(fill, buffer, span->y, span->x, span->len);
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto src = buffer;
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++src) {
                auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            auto ialpha = 255 - span->coverage;
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++src) {
                auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
                tmp = ALPHA_BLEND(tmp, span->coverage) + ALPHA_BLEND(*dst, ialpha);
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterTranslucentLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (!rle) return false;

    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentLinearGradientRleMask(surface, rle, fill, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentLinearGradientRleMask(surface, rle, fill, surface->blender.ialpha);
        }
    }
    return _translucentLinearGradientRle(surface, rle, fill);
}


static bool _rasterOpaqueLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        if (span->coverage == 255) {
            fillFetchLinear(fill, surface->buffer + span->y * surface->stride + span->x, span->y, span->x, span->len);
        } else {
            fillFetchLinear(fill, buf, span->y, span->x, span->len);
            auto ialpha = 255 - span->coverage;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            for (uint32_t i = 0; i < span->len; ++i) {
                dst[i] = ALPHA_BLEND(buf[i], span->coverage) + ALPHA_BLEND(dst[i], ialpha);
            }
        }
    }
    return true;
}


static bool _translucentRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        fillFetchRadial(fill, buffer, span->y, span->x, span->len);
        if (span->coverage == 255) {
            for (uint32_t i = 0; i < span->len; ++i) {
                dst[i] = buffer[i] + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(buffer[i]));
            }
        } else {
           for (uint32_t i = 0; i < span->len; ++i) {
                auto tmp = ALPHA_BLEND(buffer[i], span->coverage);
                dst[i] = tmp + ALPHA_BLEND(dst[i], 255 - surface->blender.alpha(tmp));
            }
        }
    }
    return true;
}


static bool _translucentRadialGradientRleMask(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t rgba))
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto cbuffer = surface->compositor->image.data;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        fillFetchRadial(fill, buffer, span->y, span->x, span->len);
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto src = buffer;
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++src) {
                auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            auto ialpha = 255 - span->coverage;
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++src) {
                auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
                tmp = ALPHA_BLEND(tmp, span->coverage) + ALPHA_BLEND(*dst, ialpha);
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterTranslucentRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (!rle) return false;

    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentRadialGradientRleMask(surface, rle, fill, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentRadialGradientRleMask(surface, rle, fill, surface->blender.ialpha);
        }
    }
    return _translucentRadialGradientRle(surface, rle, fill);
}


static bool _rasterOpaqueRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto buf = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buf) return false;

    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
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
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void rasterRGBA32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
#if defined(THORVG_AVX_VECTOR_SUPPORT)
    avxRasterRGBA32(dst, val, offset, len);
#elif defined(THORVG_NEON_VECTOR_SUPPORT)
    neonRasterRGBA32(dst, val, offset, len);
#else
    cRasterRGBA32(dst, val, offset, len);
#endif
}


bool rasterCompositor(SwSurface* surface)
{
    if (surface->cs == SwCanvas::ABGR8888 || surface->cs == SwCanvas::ABGR8888_STRAIGHT) {
        surface->blender.join = _abgrJoin;
    } else if (surface->cs == SwCanvas::ARGB8888 || surface->cs == SwCanvas::ARGB8888_STRAIGHT) {
        surface->blender.join = _argbJoin;
    } else {
        //What Color Space ???
        return false;
    }
    surface->blender.alpha = _colorAlpha;
    surface->blender.ialpha = _colorInvAlpha;

    return true;
}


bool rasterGradientShape(SwSurface* surface, SwShape* shape, unsigned id)
{
    if (!shape->fill) return false;

    auto translucent = shape->fill->translucent || (surface->compositor && surface->compositor->method != CompositeMethod::None);

    //Fast Track
    if (shape->fastTrack) {
        if (id == TVG_CLASS_ID_LINEAR) {
            if (translucent) return _rasterTranslucentLinearGradientRect(surface, shape->bbox, shape->fill);
            return _rasterOpaqueLinearGradientRect(surface, shape->bbox, shape->fill);
        } else {
            if (translucent) return _rasterTranslucentRadialGradientRect(surface, shape->bbox, shape->fill);
            return _rasterOpaqueRadialGradientRect(surface, shape->bbox, shape->fill);
        }
    } else {
        if (!shape->rle) return false;
        if (id == TVG_CLASS_ID_LINEAR) {
            if (translucent) return _rasterTranslucentLinearGradientRle(surface, shape->rle, shape->fill);
            return _rasterOpaqueLinearGradientRle(surface, shape->rle, shape->fill);
        } else {
            if (translucent) return _rasterTranslucentRadialGradientRle(surface, shape->rle, shape->fill);
            return _rasterOpaqueRadialGradientRle(surface, shape->rle, shape->fill);
        }
    }
    return false;
}


bool rasterSolidShape(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (a < 255) {
        r = ALPHA_MULTIPLY(r, a);
        g = ALPHA_MULTIPLY(g, a);
        b = ALPHA_MULTIPLY(b, a);
    }

    auto color = surface->blender.join(r, g, b, a);
    auto translucent = _translucent(surface, a);

    //Fast Track
    if (shape->fastTrack) {
        if (translucent) return _rasterTranslucentRect(surface, shape->bbox, color);
        return _rasterSolidRect(surface, shape->bbox, color);
    }
    if (translucent) {
        return _rasterTranslucentRle(surface, shape->rle, color);
    }
    return _rasterSolidRle(surface, shape->rle, color);
}


bool rasterStroke(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (a < 255) {
        r = ALPHA_MULTIPLY(r, a);
        g = ALPHA_MULTIPLY(g, a);
        b = ALPHA_MULTIPLY(b, a);
    }

    auto color = surface->blender.join(r, g, b, a);
    auto translucent = _translucent(surface, a);

    if (translucent) return _rasterTranslucentRle(surface, shape->strokeRle, color);
    return _rasterSolidRle(surface, shape->strokeRle, color);
}


bool rasterGradientStroke(SwSurface* surface, SwShape* shape, unsigned id)
{
    if (!shape->stroke || !shape->stroke->fill || !shape->strokeRle) return false;

    auto translucent = shape->stroke->fill->translucent || (surface->compositor && surface->compositor->method != CompositeMethod::None);

    if (id == TVG_CLASS_ID_LINEAR) {
        if (translucent) return _rasterTranslucentLinearGradientRle(surface, shape->strokeRle, shape->stroke->fill);
        return _rasterOpaqueLinearGradientRle(surface, shape->strokeRle, shape->stroke->fill);
    } else {
        if (translucent) return _rasterTranslucentRadialGradientRle(surface, shape->strokeRle, shape->stroke->fill);
        return _rasterOpaqueRadialGradientRle(surface, shape->strokeRle, shape->stroke->fill);
    }

    return false;
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


void rasterUnpremultiply(SwSurface* surface)
{
    //TODO: Create simd avx and neon version
    for (uint32_t y = 0; y < surface->h; y++) {
        auto buffer = surface->buffer + surface->stride * y;
        for (uint32_t x = 0; x < surface->w; ++x) {
            uint8_t a = buffer[x] >> 24;
            if (a == 255) {
                continue;
            } else if (a == 0) {
                buffer[x] = 0x00ffffff;
            } else {
                uint16_t r = ((buffer[x] >> 8) & 0xff00) / a;
                uint16_t g = ((buffer[x]) & 0xff00) / a;
                uint16_t b = ((buffer[x] << 8) & 0xff00) / a;
                if (r > 0xff) r = 0xff;
                if (g > 0xff) g = 0xff;
                if (b > 0xff) b = 0xff;
                buffer[x] = (a << 24) | (r << 16) | (g << 8) | (b);
            }
        }
    }
}


bool rasterImage(SwSurface* surface, SwImage* image, const Matrix* transform, const SwBBox& bbox, uint32_t opacity)
{
    Matrix itransform;
    auto scale = 1.0f;
    bool transformed = false;

    //Figure out the scale factor by transform matrix
    if (transform) {
        if (!mathInverse(transform, &itransform)) return false;
        scale = sqrtf((transform->e11 * transform->e11) + (transform->e21 * transform->e21));
        auto scaleY = sqrtf((transform->e22 * transform->e22) + (transform->e12 * transform->e12));
        //TODO:If the x and y axis scale is different, a separate interpolation algorithm for each axis should be applied.
        if (fabsf(scale - scaleY) > FLT_EPSILON) scale = 1.0f;
        if (!mathIdentity(transform)) transformed = true;
    //TODO: Figure out the scale factor by the image size & drawing region
    } else {
        mathIdentity(&itransform);
    }

    auto translucent = _translucent(surface, opacity);
    auto downScaleTolerance = 0.5f;

    //Clipped Image
    if (image->rle) {
        if (transformed) {
            if (translucent) {
                if (fabsf(scale - 1.0f) <= FLT_EPSILON) return _rasterTranslucentImageRle(surface, image, opacity, &itransform);
                else if (scale < downScaleTolerance) return _rasterTranslucentDownScaleImageRle(surface, image, opacity, &itransform, scale);
                else return _rasterTranslucentUpScaleImageRle(surface, image, opacity, &itransform);
            } else {
                if (fabsf(scale - 1.0f) <= FLT_EPSILON) return _rasterImageRle(surface, image, &itransform);
                else if (scale < downScaleTolerance) return _rasterDownScaleImageRle(surface, image, &itransform, scale);
                else return _rasterUpScaleImageRle(surface, image, &itransform);
            }
        //Fast track
        //OPTIMIZE ME: Support non transformed image. Only shifted image can use these routines.
        } else {
            if (translucent) return _rasterTranslucentImageRle(surface, image, opacity);
            return _rasterImageRle(surface, image);
        }
    //Whole Image
    } else {
        if (transformed) {
            if (translucent) {
                if (fabsf(scale - 1.0f) <= FLT_EPSILON) return _rasterTranslucentImage(surface, image, opacity, bbox, &itransform);
                else if (scale < downScaleTolerance) return _rasterTranslucentDownScaleImage(surface, image, opacity, bbox, &itransform, scale);
                else return _rasterTranslucentUpScaleImage(surface, image, opacity, bbox, &itransform);
            } else {
                if (fabsf(scale - 1.0f) <= FLT_EPSILON) return _rasterImage(surface, image, bbox, &itransform);
                else if (scale  < downScaleTolerance) return _rasterDownScaleImage(surface, image, bbox, &itransform, scale);
                else return _rasterUpScaleImage(surface, image, bbox, &itransform);
            }
        //Fast track
        //OPTIMIZE ME: Support non transformed image. Only shifted image can use these routines.
        } else {
            if (translucent) return _rasterTranslucentImage(surface, image, opacity, bbox);
            return _rasterImage(surface, image, bbox);
        }
    }
}