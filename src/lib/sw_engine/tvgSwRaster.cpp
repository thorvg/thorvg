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

struct Vertex
{
   Point pt;
   Point uv;
};

struct Polygon
{
   Vertex vertex[3];
};

static inline void _swap(float& a, float& b, float& tmp)
{
    tmp = a;
    a = b;
    b = tmp;
}

static inline uint32_t _interpolate(uint32_t a, uint32_t c0, uint32_t c1)
{
    return (((((((c0 >> 8) & 0xff00ff) - ((c1 >> 8) & 0xff00ff)) * a) + (c1 & 0xff00ff00)) & 0xff00ff00) + ((((((c0 & 0xff00ff) - (c1 & 0xff00ff)) * a) >> 8) + (c1 & 0xff00ff)) & 0xff00ff));
}

static inline uint32_t _multiplyAlpha(uint32_t c, uint32_t a)
{
    return ((c * a + 0xff) >> 8);
}


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

static bool _translucentRectMask(SwSurface* surface, const SwBBox& region, uint32_t color, uint32_t (*blendMethod)(uint32_t))
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

static bool _translucentRleMask(SwSurface* surface, SwRleData* rle, uint32_t color, uint32_t (*blendMethod)(uint32_t))
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

#if defined(THORVG_AVX_VECTOR_SUPPORT)
    return avxRasterTranslucentRle(surface, rle, color);
#elif defined(THORVG_NEON_VECTOR_SUPPORT)
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
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _translucentImageRleMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Transformed Image Rle Alpha Mask / Inverse Alpha Mask Composition");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        if (alpha == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
                auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
                if (rX >= w || rY >= h) continue;
                auto tmp = ALPHA_BLEND(img[rY * image->stride + rX], blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
                auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
                if (rX >= w || rY >= h) continue;
                auto src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterTranslucentImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentImageRleMask(surface, image, opacity, itransform, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentImageRleMask(surface, image, opacity, itransform, surface->blender.ialpha);
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
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto fX = (span->x + x) * itransform->e11 + ey1;
            auto fY = (span->x + x) * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, image->stride, h, fX, fY), alpha);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _translucentUpScaleImageRleMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Image Rle Alpha Mask / Inverse Alpha Mask Composition");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        if (alpha == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto fX = (span->x + x) * itransform->e11 + ey1;
                auto fY = (span->x + x) * itransform->e21 + ey2;
                auto rX = static_cast<uint32_t>(roundf(fX));
                auto rY = static_cast<uint32_t>(roundf(fY));
                if (rX >= w || rY >= h) continue;
                uint32_t src;
                if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
                else src = ALPHA_BLEND(_applyBilinearInterpolation(img, image->stride, h, fX, fY), alpha);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto fX = (span->x + x) * itransform->e11 + ey1;
                auto fY = (span->x + x) * itransform->e21 + ey2;
                auto rX = static_cast<uint32_t>(roundf(fX));
                auto rY = static_cast<uint32_t>(roundf(fY));
                if (rX >= w || rY >= h) continue;
                uint32_t src;
                if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
                else src = ALPHA_BLEND(_applyBilinearInterpolation(img, image->stride, h, fX, fY), alpha);
                auto tmp = ALPHA_BLEND(src, _multiplyAlpha(alpha, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterTranslucentUpScaleImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentUpScaleImageRleMask(surface, image, opacity, itransform, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentUpScaleImageRleMask(surface, image, opacity, itransform, surface->blender.ialpha);
        }
    }
    return _translucentUpScaleImageRle(surface, image, opacity, itransform);
}


static bool _translucentDownScaleImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t halfScale)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, image->stride, h, rX, rY, halfScale), alpha);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}

static bool _translucentDownScaleImageRleMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Image Rle Alpha Mask / Inverse Alpha Mask Composition");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto alpha = _multiplyAlpha(span->coverage, opacity);

        if (alpha == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
                auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
                if (rX >= w || rY >= h) continue;
                uint32_t src;
                if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
                else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, image->stride, h, rX, rY, halfScale), alpha);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
                auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
                if (rX >= w || rY >= h) continue;
                uint32_t src;
                if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
                else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, image->stride, h, rX, rY, halfScale), alpha);
                auto tmp = ALPHA_BLEND(src, _multiplyAlpha(alpha, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterTranslucentDownScaleImageRle(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t halfScale)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _translucentDownScaleImageRleMask(surface, image, opacity, itransform, halfScale, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _translucentDownScaleImageRleMask(surface, image, opacity, itransform, halfScale, surface->blender.ialpha);
        }
    }
    return _translucentDownScaleImageRle(surface, image, opacity, itransform, halfScale);
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
            auto src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
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
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, image->stride, h, fX, fY), span->coverage);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterDownScaleImageRle(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t halfScale)
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
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
            else src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, image->stride, h, rX, rY, halfScale), span->coverage);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


/************************************************************************/
/* Clipped Image (Direct)                                               */
/************************************************************************/


static bool _rasterMaskedDirectRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Image Rle Alpha Mask / Inverse Alpha Mask Composition");

    auto span = image->rle->spans;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->stride + span->x];
        auto img = image->data + (span->y + image->y) * image->stride + (span->x + image->x);
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        if (alpha == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++img) {
                auto tmp = ALPHA_BLEND(*img, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++img) {
                auto tmp = ALPHA_BLEND(*img, _multiplyAlpha(alpha, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterTranslucentDirectRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto img = image->data + (span->y + image->y) * image->stride + (span->x + image->x);
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
            auto src = ALPHA_BLEND(*img, alpha);
            *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
        }
    }
    return true;
}


static bool _rasterOpaqueDirectRleImage(SwSurface* surface, const SwImage* image)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto img = image->data + (span->y + image->y) * image->stride + (span->x + image->x);
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
                *dst = *img;
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
                auto src = ALPHA_BLEND(*img, span->coverage);
                *dst = src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(src));
            }
        }
    }
    return true;
}


static bool _rasterDirectRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, bool translucent)
{
    if (translucent) {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterMaskedDirectRleImage(surface, image, opacity, surface->blender.alpha);
            }
            if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterMaskedDirectRleImage(surface, image, opacity, surface->blender.ialpha);
            }
        }
        return _rasterTranslucentDirectRleImage(surface, image, opacity);
    } else {
        return _rasterOpaqueDirectRleImage(surface, image);
    }
}


/************************************************************************/
/* Transformed Whole Image                                              */
/************************************************************************/

static float dudx, dvdx;
static float dxdya, dxdyb, dudya, dvdya;
static float xa, xb, ua, va;


static void _rasterPolygonImageSegment(SwSurface* surface, const SwImage* image, const SwBBox& region, int ystart, int yend, bool translucent)
{
    float _dudx = dudx, _dvdx = dvdx;
    float _dxdya = dxdya, _dxdyb = dxdyb, _dudya = dudya, _dvdya = dvdya;
    float _xa = xa, _xb = xb, _ua = ua, _va = va;
    auto sbuf = image->data;
    auto dbuf = surface->buffer;
    int sw = static_cast<int>(image->stride);
    int sh = image->h;
    int dw = surface->stride;
    int x1, x2, x, y, uu, vv, ar, ab, iru, irv, px;
    float dx, u, v, iptr;
    uint32_t* buf;

    //Range exception handling
    if (ystart >= region.max.y) return;
    if (ystart < region.min.y) ystart = region.min.y;
    if (yend > region.max.y) yend = region.max.y;

    //Loop through all lines in the segment
    y = ystart;

    while (y < yend) {
        x1 = _xa;
        x2 = _xb;

        //Range exception handling
        if (x1 < region.min.x) x1 = region.min.x;
        if (x2 > region.max.x) x2 = region.max.x;

        if ((x2 - x1) < 1) goto next;
        if ((x1 >= region.max.x) || (x2 <= region.min.x)) goto next;

        //Perform subtexel pre-stepping on UV
        dx = 1 - (_xa - x1);
        u = _ua + dx * _dudx;
        v = _va + dx * _dvdx;

        buf = dbuf + ((y * dw) + x1);

        x = x1;

        //Draw horizontal line
        while (x++ < x2) {
            uu = (int) u;
            vv = (int) v;
            // FIXME: sometimes u and v are < 0 - don'tc crash
            if (uu < 0) uu = 0;
            if (vv < 0) vv = 0;

            //Range exception handling
            //OPTIMIZE ME, handle in advance?
            if (uu >= sw) uu = sw - 1;
            if (vv >= sh) vv = sh - 1;

            ar = (int)(255 * (1 - modff(u, &iptr)));
            ab = (int)(255 * (1 - modff(v, &iptr)));
            iru = uu + 1;
            irv = vv + 1;
            px = *(sbuf + (vv * sw) + uu);

            //horizontal interpolate
            if (iru < sw) {
                //right pixel
                int px2 = *(sbuf + (vv * sw) + iru);
                px = _interpolate(ar, px, px2);
            }
            //vertical interpolate
            if (irv < sh) {
                //bottom pixel
                int px2 = *(sbuf + (irv * sw) + uu);

                //horizontal interpolate
                if (iru < sw) {
                    //bottom right pixel
                    int px3 = *(sbuf + (irv * sw) + iru);
                    px2 = _interpolate(ar, px2, px3);
                }
                px = _interpolate(ab, px, px2);
            }

            *(buf++) = px;
  
            //Step UV horizontally
            u += _dudx;
            v += _dvdx;
        }
next:
        //Step along both edges
        _xa += _dxdya;
        _xb += _dxdyb;
        _ua += _dudya;
        _va += _dvdya;

        y++;
     }
   xa = _xa;
   xb = _xb;
   ua = _ua;
   va = _va;
}


/* This mapping algorithm is based on Mikael Kalms's. */
static void _rasterPolygonImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, Polygon& polygon, bool translucent)
{
    float x[3] = {polygon.vertex[0].pt.x, polygon.vertex[1].pt.x, polygon.vertex[2].pt.x};
    float y[3] = {polygon.vertex[0].pt.y, polygon.vertex[1].pt.y, polygon.vertex[2].pt.y};
    float u[3] = {polygon.vertex[0].uv.x, polygon.vertex[1].uv.x, polygon.vertex[2].uv.x};
    float v[3] = {polygon.vertex[0].uv.y, polygon.vertex[1].uv.y, polygon.vertex[2].uv.y};

    float off_y;
    float dxdy[3] = {0, 0, 0};
    float tmp;

    bool upper = false;

    //Sort the vertices in ascending Y order
    if (y[0] > y[1]) {
        _swap(x[0], x[1], tmp);
        _swap(y[0], y[1], tmp);
        _swap(u[0], u[1], tmp);
        _swap(v[0], v[1], tmp);
    }
    if (y[0] > y[2])  {
        _swap(x[0], x[2], tmp);
        _swap(y[0], y[2], tmp);
        _swap(u[0], u[2], tmp);
        _swap(v[0], v[2], tmp);
    }
    if (y[1] > y[2]) {
        _swap(x[1], x[2], tmp);
        _swap(y[1], y[2], tmp);
        _swap(u[1], u[2], tmp);
        _swap(v[1], v[2], tmp);
    }

    //Y indexes
    int yi[3] = {(int)y[0], (int)y[1], (int)y[2]};

    //Skip drawing if it's too thin to cover any pixels at all.
    if ((yi[0] == yi[1] && yi[0] == yi[2]) || ((int) x[0] == (int) x[1] && (int) x[0] == (int) x[2])) return;

    //Calculate horizontal and vertical increments for UV axes (these calcs are certainly not optimal, although they're stable (handles any dy being 0)
    float denom = ((x[2] - x[0]) * (y[1] - y[0]) - (x[1] - x[0]) * (y[2] - y[0]));

    //Skip poly if it's an infinitely thin line
    if (mathZero(denom)) return;

    denom = 1 / denom;   //Reciprocal for speeding up
    dudx = ((u[2] - u[0]) * (y[1] - y[0]) - (u[1] - u[0]) * (y[2] - y[0])) * denom;
    dvdx = ((v[2] - v[0]) * (y[1] - y[0]) - (v[1] - v[0]) * (y[2] - y[0])) * denom;
    auto dudy = ((u[1] - u[0]) * (x[2] - x[0]) - (u[2] - u[0]) * (x[1] - x[0])) * denom;
    auto dvdy = ((v[1] - v[0]) * (x[2] - x[0]) - (v[2] - v[0]) * (x[1] - x[0])) * denom;

    //Calculate X-slopes along the edges
    if (y[1] > y[0]) dxdy[0] = (x[1] - x[0]) / (y[1] - y[0]);
    if (y[2] > y[0]) dxdy[1] = (x[2] - x[0]) / (y[2] - y[0]);
    if (y[2] > y[1]) dxdy[2] = (x[2] - x[1]) / (y[2] - y[1]);

    //Determine which side of the polygon the longer edge is on
    auto side = (dxdy[1] > dxdy[0]) ? true : false;

    if (mathEqual(y[0], y[1])) side = x[0] > x[1];
    if (mathEqual(y[1], y[2])) side = x[2] > x[1];

    //Longer edge is on the left side
    if (!side) {
        //Calculate slopes along left edge
        dxdya = dxdy[1];
        dudya = dxdya * dudx + dudy;
        dvdya = dxdya * dvdx + dvdy;

        //Perform subpixel pre-stepping along left edge
        float dy = 1 - (y[0] - yi[0]);
        xa = x[0] + dy * dxdya;
        ua = u[0] + dy * dudya;
        va = v[0] + dy * dvdya;

        //Draw upper segment if possibly visible
        if (yi[0] < yi[1]) {
            off_y = y[0] < region.min.y ? (region.min.y - y[0]) : 0;
            xa += (off_y * dxdya);
            ua += (off_y * dudya);
            va += (off_y * dvdya);

            // Set right edge X-slope and perform subpixel pre-stepping
            dxdyb = dxdy[0];
            xb = x[0] + dy * dxdyb + (off_y * dxdyb);
            _rasterPolygonImageSegment(surface, image, region, yi[0], yi[1], translucent);
            upper = true;
        }
        //Draw lower segment if possibly visible
        if (yi[1] < yi[2]) {
            off_y = y[1] < region.min.y ? (region.min.y - y[1]) : 0;
            if (!upper) {
                xa += (off_y * dxdya);
                ua += (off_y * dudya);
                va += (off_y * dvdya);
            }
            // Set right edge X-slope and perform subpixel pre-stepping
            dxdyb = dxdy[2];
            xb = x[1] + (1 - (y[1] - yi[1])) * dxdyb + (off_y * dxdyb);
            _rasterPolygonImageSegment(surface, image, region, yi[1], yi[2], translucent);
        }
    //Longer edge is on the right side
    } else {
        //Set right edge X-slope and perform subpixel pre-stepping
        dxdyb = dxdy[1];
        float dy = 1 - (y[0] - yi[0]);
        xb = x[0] + dy * dxdyb;

        //Draw upper segment if possibly visible
        if (yi[0] < yi[1]) {
            off_y = y[0] < region.min.y ? (region.min.y - y[0]) : 0;
            xb += (off_y *dxdyb);

            // Set slopes along left edge and perform subpixel pre-stepping
            dxdya = dxdy[0];
            dudya = dxdya * dudx + dudy;
            dvdya = dxdya * dvdx + dvdy;

            xa = x[0] + dy * dxdya + (off_y * dxdya);
            ua = u[0] + dy * dudya + (off_y * dudya);
            va = v[0] + dy * dvdya + (off_y * dvdya);

            _rasterPolygonImageSegment(surface, image, region, yi[0], yi[1], translucent);

            upper = true;
        }
        //Draw lower segment if possibly visible
        if (yi[1] < yi[2]) {
            off_y = y[1] < region.min.y ? (region.min.y - y[1]) : 0;
            if (!upper) xb += (off_y *dxdyb);

            // Set slopes along left edge and perform subpixel pre-stepping
            dxdya = dxdy[2];
            dudya = dxdya * dudx + dudy;
            dvdya = dxdya * dvdx + dvdy;
            dy = 1 - (y[1] - yi[1]);
            xa = x[1] + dy * dxdya + (off_y * dxdya);
            ua = u[1] + dy * dudya + (off_y * dudya);
            va = v[1] + dy * dvdya + (off_y * dvdya);

            _rasterPolygonImageSegment(surface, image, region, yi[1], yi[2], translucent);
        }
    }
}


/*
    2 triangles constructs 1 mesh.
    below figure illustrates vert[4] index info.
    If you need better quality, please divide a mesh by more number of triangles.

    0 -- 1
    |  / |
    | /  |
    3 -- 2 
*/
static bool _rasterMappingTexture(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* transform, bool translucent)
{
   /* Prepare vertices.
      shift XY coordinates to match the sub-pixeling technique. */
    Vertex vertices[4];
    vertices[0] = {{0.0f, 0.0f}, 0.0f, 0.0f};
    vertices[1] = {{float(image->w), 0.0f}, float(image->w), 0.0f};
    vertices[2] = {{float(image->w), float(image->h)}, float(image->w), float(image->h)};
    vertices[3] = {{0.0f, float(image->h)}, 0.0f, float(image->h)};

    for (int i = 0; i < 4; i++) mathMultiply(&vertices[i].pt, transform);

    Polygon polygon;

    //Draw the first polygon
    polygon.vertex[0] = vertices[0];
    polygon.vertex[1] = vertices[1];
    polygon.vertex[2] = vertices[3];

    _rasterPolygonImage(surface, image, opacity, region, polygon, translucent);

    //Draw the second polygon
    polygon.vertex[0] = vertices[1];
    polygon.vertex[1] = vertices[2];
    polygon.vertex[2] = vertices[3];

    _rasterPolygonImage(surface, image, opacity, region, polygon, translucent);
    
    return true;
}


static bool _rasterTransformedImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* transform, bool translucent)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
         return 1; //   return _rasterMaskedDirectImage(surface, image, opacity, region, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
         return 1; //   return _rasterMaskedDirectImage(surface, image, opacity, region, surface->blender.ialpha);
        }
    }
    return _rasterMappingTexture(surface, image, opacity, region, transform, translucent);
}


static bool _translucentUpScaleImageMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
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
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rX + (rY * image->stride)], _multiplyAlpha(opacity, blendMethod(*cmp)));
            else src = ALPHA_BLEND(_applyBilinearInterpolation(img, image->stride, h, fX, fY), _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _translucentDownScaleImageMask(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
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
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) {
                src = ALPHA_BLEND(img[rX + (rY * image->stride)], _multiplyAlpha(opacity, blendMethod(*cmp)));
            } else {
                src = ALPHA_BLEND(_average2Nx2NPixel(surface, img, image->stride, h, rX, rY, halfScale), _multiplyAlpha(opacity, blendMethod(*cmp)));
            }
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}




/************************************************************************/
/* Whole Image (Direct)                                                 */
/************************************************************************/

static bool _rasterMaskedDirectImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, uint32_t (*blendMethod)(uint32_t))
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

    TVGLOG("SW_ENGINE", "Image Alpha Mask / Inverse Alpha Mask Composition");

    auto sbuffer = image->data + (region.min.y + image->y) * image->stride + (region.min.x + image->x);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
        sbuffer += image->stride;
    }
    return true;
}


static bool _rasterTranslucentDirectImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + (region.min.y + image->y) * image->stride + (region.min.x + image->x);

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++src) {
            auto p = ALPHA_BLEND(*src, opacity);
            *dst = p + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(p));
        }
        dbuffer += surface->stride;
        sbuffer += image->stride;
    }
    return true;
}


static bool _rasterOpaqueDirectImage(SwSurface* surface, const SwImage* image, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + (region.min.y + image->y) * image->stride + (region.min.x + image->x);

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; x++, dst++, src++) {
            *dst = *src + ALPHA_BLEND(*dst, 255 - surface->blender.alpha(*src));
        }
        dbuffer += surface->stride;
        sbuffer += image->stride;
    }
    return true;
}


static bool _rasterDirectImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, bool translucent)
{
    if (translucent) {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterMaskedDirectImage(surface, image, opacity, region, surface->blender.alpha);
            }
            if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterMaskedDirectImage(surface, image, opacity, region, surface->blender.ialpha);
            }
        }
        return _rasterTranslucentDirectImage(surface, image, opacity, region);
    } else {
        return _rasterOpaqueDirectImage(surface, image, region);
    }
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


static bool _translucentLinearGradientRectMask(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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


static bool _translucentRadialGradientRectMask(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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


static bool _translucentLinearGradientRleMask(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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


static bool _translucentRadialGradientRleMask(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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
        r = _multiplyAlpha(r, a);
        g = _multiplyAlpha(g, a);
        b = _multiplyAlpha(b, a);
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
        r = _multiplyAlpha(r, a);
        g = _multiplyAlpha(g, a);
        b = _multiplyAlpha(b, a);
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
    static constexpr float DOWN_SCALE_TOLERANCE = 0.5f;

    uint32_t halfScale = static_cast<uint32_t>(0.5f / image->scale);
    if (halfScale == 0) halfScale = 1;

    auto translucent = _translucent(surface, opacity);

    //Clipped Image
    if (image->rle) {
        if (image->transformed) {
            Matrix itransform;
            if (!mathInverse(transform, &itransform)) return false;
            if (translucent) {
                if (mathEqual(image->scale, 1.0f)) return _rasterTranslucentImageRle(surface, image, opacity, &itransform);
                else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterTranslucentDownScaleImageRle(surface, image, opacity, &itransform, halfScale);
                else return _rasterTranslucentUpScaleImageRle(surface, image, opacity, &itransform);
            } else {
                if (mathEqual(image->scale, 1.0f)) return _rasterImageRle(surface, image, &itransform);
                else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterDownScaleImageRle(surface, image, &itransform, halfScale);
                else return _rasterUpScaleImageRle(surface, image, &itransform);
            }
        } else return _rasterDirectRleImage(surface, image, opacity, translucent);
    //Whole Image
    } else {
        if (image->transformed) return _rasterTransformedImage(surface, image, opacity, bbox, transform, translucent);
        else return _rasterDirectImage(surface, image, opacity, bbox, translucent);
    }
}
