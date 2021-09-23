/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
#include <float.h>
#include <math.h>
#include "tvgSwCommon.h"


static bool _inverse(const Matrix* transform, Matrix* invM)
{
    //computes the inverse of a matrix m
    auto det = transform->e11 * (transform->e22 * transform->e33 - transform->e32 * transform->e23) -
               transform->e12 * (transform->e21 * transform->e33 - transform->e23 * transform->e31) +
               transform->e13 * (transform->e21 * transform->e32 - transform->e22 * transform->e31);

    if (fabsf(det) < FLT_EPSILON) return false;

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

    return true;
}


static bool _identity(const Matrix* transform)
{
    if (transform) {
        if (transform->e11 != 1.0f || transform->e12 != 0.0f || transform->e13 != 0.0f ||
            transform->e21 != 0.0f || transform->e22 != 1.0f || transform->e23 != 0.0f ||
            transform->e31 != 0.0f || transform->e32 != 0.0f || transform->e33 != 1.0f) {
            return false;
        }
    }

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


static bool _rasterImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto src = img + span->x + span->y * w;    //TODO: need to use image's stride
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++src) {
            *src = ALPHA_BLEND(*src, span->coverage);
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
    }
    return true;
}


static bool _rasterTranslucentImageRle(SwSurface* surface, const SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto src = img + span->x + span->y * w;    //TODO: need to use image's stride
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++src) {
            *src = ALPHA_BLEND(*src, alpha);
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
    }
    return true;
}


static bool _rasterTranslucentImageRle(SwSurface* surface, const SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const Matrix* invTransform)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * w + rX], alpha);     //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterTranslucentUpScaleImageRle(SwSurface* surface, const SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const Matrix* invTransform)
{
    auto span = rle->spans;
    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto fX = (span->x + x) * invTransform->e11 + ey1;
            auto fY = (span->x + x) * invTransform->e21 + ey2;
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


static bool _rasterTranslucentDownScaleImageRle(SwSurface* surface, const SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const Matrix* invTransform, float scaling)
{
    uint32_t halfScaling = static_cast<uint32_t>(0.5f / scaling);
    if (halfScaling == 0) halfScaling = 1;
    auto span = rle->spans;
    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = ALPHA_MULTIPLY(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScaling || rY < halfScaling || rX >= w - halfScaling || rY >= h - halfScaling) src = ALPHA_BLEND(img[rY * w + rX], alpha);     //TODO: need to use image's stride
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScaling), alpha);     //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, const Matrix* invTransform)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
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
    }
    return true;
}


static bool _rasterUpScaleImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, const Matrix* invTransform)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto fX = (span->x + x) * invTransform->e11 + ey1;
            auto fY = (span->x + x) * invTransform->e21 + ey2;
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


static bool _rasterDownScaleImageRle(SwSurface* surface, SwRleData* rle, uint32_t *img, uint32_t w, uint32_t h, const Matrix* invTransform, float scaling)
{
    uint32_t halfScaling = static_cast<uint32_t>(0.5f / scaling);
    if (halfScaling == 0) halfScaling = 1;
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto ey1 = span->y * invTransform->e12 + invTransform->e13;
        auto ey2 = span->y * invTransform->e22 + invTransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScaling || rY < halfScaling || rX >= w - halfScaling || rY >= h - halfScaling) src = ALPHA_BLEND(img[rY * w + rX], span->coverage);    //TODO: need to use image's stride
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScaling), span->coverage);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _translucentImage(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * w)], opacity);    //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _translucentImageAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    TVGLOG("SW_ENGINE", "Transformed Image Alpha Mask Composition");

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}

static bool _translucentImageInvAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    TVGLOG("SW_ENGINE", "Transformed Image Inverse Alpha Mask Composition");

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, 255 - surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentImageAlphaMask(surface, img, w, h, opacity, region, invTransform);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentImageInvAlphaMask(surface, img, w, h, opacity, region, invTransform);
        }
    }
    return _translucentImage(surface, img, w, h, opacity, region, invTransform);
}


static bool _translucentUpScaleImage(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto fX = x * invTransform->e11 + ey1;
            auto fY = x * invTransform->e21 + ey2;
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


static bool _translucentUpScaleImageAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    TVGLOG("SW_ENGINE", "Transformed Image Alpha Mask Composition");

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto fX = x * invTransform->e11 + ey1;
            auto fY = x * invTransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, w, h, fX, fY), ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _translucentUpScaleImageInvAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    TVGLOG("SW_ENGINE", "Transformed Image Inverse Alpha Mask Composition");

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto fX = x * invTransform->e11 + ey1;
            auto fY = x * invTransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, 255 - surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, w, h, fX, fY), ALPHA_MULTIPLY(opacity, 255 - surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentUpScaleImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentUpScaleImageAlphaMask(surface, img, w, h, opacity, region, invTransform);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentUpScaleImageInvAlphaMask(surface, img, w, h, opacity, region, invTransform);
        }
    }
    return _translucentUpScaleImage(surface, img, w, h, opacity, region, invTransform);
}


static bool _translucentDownScaleImage(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform, float scaling)
{
    uint32_t halfScaling = static_cast<uint32_t>(0.5f / scaling);
    if (halfScaling == 0) halfScaling = 1;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScaling || rY < halfScaling || rX >= w - halfScaling || rY >= h - halfScaling) src = ALPHA_BLEND(img[rX + (rY * w)], opacity);
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScaling), opacity);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _translucentDownScaleImageAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform, float scaling)
{
    TVGLOG("SW_ENGINE", "Transformed Image Alpha Mask Composition");
    uint32_t halfScaling = static_cast<uint32_t>(0.5f / scaling);
    if (halfScaling == 0) halfScaling = 1;

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScaling || rY < halfScaling || rX >= w - halfScaling || rY >= h - halfScaling) src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScaling), ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _translucentDownScaleImageInvAlphaMask(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform, float scaling)
{
    TVGLOG("SW_ENGINE", "Transformed Image Inverse Alpha Mask Composition");
    uint32_t halfScaling = static_cast<uint32_t>(0.5f / scaling);
    if (halfScaling == 0) halfScaling = 1;

    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * invTransform->e12 + invTransform->e13;
        float ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScaling || rY < halfScaling || rX >= w - halfScaling || rY >= h - halfScaling) src = ALPHA_BLEND(img[rX + (rY * w)], ALPHA_MULTIPLY(opacity, 255 - surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, w, h, rX, rY, halfScaling), ALPHA_MULTIPLY(opacity, 255 - surface->blender.alpha(*cmp)));  //TODO: need to use image's stride
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentDownScaleImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region, const Matrix* invTransform, float scaling)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentDownScaleImageAlphaMask(surface, img, w, h, opacity, region, invTransform, scaling);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentDownScaleImageInvAlphaMask(surface, img, w, h, opacity, region, invTransform, scaling);
        }
    }
    return _translucentDownScaleImage(surface, img, w, h, opacity, region, invTransform, scaling);
}


static bool _translucentImage(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = img + region.min.x + region.min.y * w;    //TODO: need to use image's stride

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++src) {
            auto p = ALPHA_BLEND(*src, opacity);
            *dst = p + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(p));
        }
        dbuffer += surface->stride;
        sbuffer += w;    //TODO: need to use image's stride
    }
    return true;
}


static bool _translucentImageAlphaMask(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

    TVGLOG("SW_ENGINE", "Image Alpha Mask Composition");

    auto sbuffer = img + (region.min.y * w) + region.min.x;
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, ALPHA_MULTIPLY(opacity, surface->blender.alpha(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
        sbuffer += w;   //TODO: need to use image's stride
    }
    return true;
}


static bool _translucentImageInvAlphaMask(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

    TVGLOG("SW_ENGINE", "Image Inverse Alpha Mask Composition");

    auto sbuffer = img + (region.min.y * w) + region.min.x;
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, ALPHA_MULTIPLY(opacity, 255 - surface->blender.alpha(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
        sbuffer += w;   //TODO: need to use image's stride
    }
    return true;
}


static bool _rasterTranslucentImage(SwSurface* surface, uint32_t *img, uint32_t w, uint32_t h, uint32_t opacity, const SwBBox& region)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentImageAlphaMask(surface, img, w, h, opacity, region);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentImageInvAlphaMask(surface, img, w, h, opacity, region);
        }
    }
    return _translucentImage(surface, img, w, h, opacity, region);
}


static bool _rasterImage(SwSurface* surface, uint32_t *img, uint32_t w, TVG_UNUSED uint32_t h, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = img + region.min.x + region.min.y * w;   //TODO: need to use image's stride

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; x++, dst++, src++) {
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
        dbuffer += surface->stride;
        sbuffer += w;    //TODO: need to use image's stride
    }
    return true;
}


static bool _rasterImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, const SwBBox& region, const Matrix* invTransform)
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


static bool _rasterUpScaleImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, const SwBBox& region, const Matrix* invTransform)
{
    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto fX = x * invTransform->e11 + ey1;
            auto fY = x * invTransform->e21 + ey2;
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


static bool _rasterDownScaleImage(SwSurface* surface, const uint32_t *img, uint32_t w, uint32_t h, const SwBBox& region, const Matrix* invTransform, float scaling)
{
    uint32_t halfScaling = static_cast<uint32_t>(0.5f / scaling);

    if (halfScaling == 0) halfScaling = 1;
    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = &surface->buffer[y * surface->stride + region.min.x];
        auto ey1 = y * invTransform->e12 + invTransform->e13;
        auto ey2 = y * invTransform->e22 + invTransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * invTransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * invTransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScaling || rY < halfScaling || rX >= w - halfScaling || rY >= h - halfScaling) src = img[rX + (rY * w)];
            else src = _average2Nx2NPixel(surface, img, w, h, rX, rY, halfScaling);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


bool rasterImage(SwSurface* surface, SwImage* image, const Matrix* transform, const SwBBox& bbox, uint32_t opacity)
{
    Matrix invTransform;
    auto scaling = 1.0f;

    if (transform) {
        if (!_inverse(transform, &invTransform)) return false;
        scaling = sqrt((transform->e11 * transform->e11) + (transform->e21 * transform->e21));
        auto scalingY = sqrt((transform->e22 * transform->e22) + (transform->e12 * transform->e12));
        //TODO:If the x and y axis scaling is different, a separate algorithm for each axis should be applied.
        if (scaling != scalingY) scaling = 1.0f;
    }
    else invTransform = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    auto translucent = TRANSLUCENT(surface, opacity);
    const float downScalingFactor = 0.5f;

    if (image->rle) {
        //Fast track
        if (_identity(transform)) {
            //OPTIMIZE ME: Support non transformed image. Only shifted image can use these routines.
            if (translucent) return _rasterTranslucentImageRle(surface, image->rle, image->data, image->w, image->h, opacity);
            return _rasterImageRle(surface, image->rle, image->data, image->w, image->h);
        } else {
            if (translucent) {
                if (fabsf(scaling - 1.0f) <= FLT_EPSILON) return _rasterTranslucentImageRle(surface, image->rle, image->data, image->w, image->h, opacity, &invTransform);
                else if (scaling < downScalingFactor) return _rasterTranslucentDownScaleImageRle(surface, image->rle, image->data, image->w, image->h, opacity, &invTransform, scaling);
                else return _rasterTranslucentUpScaleImageRle(surface, image->rle, image->data, image->w, image->h, opacity, &invTransform);
            }
            if (fabsf(scaling - 1.0f) <= FLT_EPSILON) return _rasterImageRle(surface, image->rle, image->data, image->w, image->h, &invTransform);
            else if (scaling < downScalingFactor) return _rasterDownScaleImageRle(surface, image->rle, image->data, image->w, image->h, &invTransform, scaling);
            else return _rasterUpScaleImageRle(surface, image->rle, image->data, image->w, image->h, &invTransform);
        }
    }
    else {
        //Fast track
        if (_identity(transform)) {
            //OPTIMIZE ME: Support non transformed image. Only shifted image can use these routines.
            if (translucent) return _rasterTranslucentImage(surface, image->data, image->w, image->h, opacity, bbox);
            return _rasterImage(surface, image->data, image->w, image->h, bbox);
        } else {
            if (translucent) {
                if (fabsf(scaling - 1.0f) <= FLT_EPSILON) return _rasterTranslucentImage(surface, image->data, image->w, image->h, opacity, bbox, &invTransform);
                else if (scaling < downScalingFactor) return _rasterTranslucentDownScaleImage(surface, image->data, image->w, image->h, opacity, bbox, &invTransform, scaling);
                else return _rasterTranslucentUpScaleImage(surface, image->data, image->w, image->h, opacity, bbox, &invTransform);
            }
            if (fabsf(scaling - 1.0f) <= FLT_EPSILON) return _rasterImage(surface, image->data, image->w, image->h, bbox, &invTransform);
            else if (scaling  < downScalingFactor) return _rasterDownScaleImage(surface, image->data, image->w, image->h, bbox, &invTransform, scaling);
            else return _rasterUpScaleImage(surface, image->data, image->w, image->h, bbox, &invTransform);
        }
    }
}
