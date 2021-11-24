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
constexpr auto DOWN_SCALE_TOLERANCE = 0.5f;


static inline uint32_t _multiplyAlpha(uint32_t c, uint32_t a)
{
    return ((c * a + 0xff) >> 8);
}


static inline uint32_t _colorAlpha(uint32_t c)
{
    return (c >> 24);
}


static inline uint32_t _colorInvAlpha(uint32_t c)
{
    return (~c >> 24);
}


static inline uint32_t _abgrJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | b << 16 | g << 8 | r);
}


static inline uint32_t _argbJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | r << 16 | g << 8 | b);
}


static bool _translucent(const SwSurface* surface, uint8_t a)
{
    if (a < 255) return true;
    if (!surface->compositor || surface->compositor->method == CompositeMethod::None) return false;
    return true;
}


static inline bool _compositing(const SwSurface* surface)
{
    if (!surface->compositor || surface->compositor->method == CompositeMethod::None) return false;
    return true;
}


static inline uint32_t _halfScale(float scale)
{
    auto halfScale = static_cast<uint32_t>(0.5f / scale);
    if (halfScale == 0) halfScale = 1;
    return halfScale;
}


//Bilinear Interpolation
static uint32_t _interpUpScaler(const uint32_t *img, uint32_t w, uint32_t h, float sx, float sy)
{
    auto rx = static_cast<uint32_t>(sx);
    auto ry = static_cast<uint32_t>(sy);

    auto dx = static_cast<uint32_t>((sx - rx) * 255.0f);
    auto dy = static_cast<uint32_t>((sy - ry) * 255.0f);

    auto c1 = img[rx + (ry * w)];
    auto c2 = img[(rx + 1) + (ry * w)];
    auto c3 = img[(rx + 1) + ((ry + 1) * w)];
    auto c4 = img[rx + ((ry + 1) * w)];

    return COLOR_INTERPOLATE(COLOR_INTERPOLATE(c1, 255 - dx, c2, dx), 255 - dy, COLOR_INTERPOLATE(c4, 255 - dx, c3, dx), dy);
}


//2n x 2n Mean Kernel
static uint32_t _interpDownScaler(const uint32_t *img, uint32_t w, uint32_t h, uint32_t rX, uint32_t rY, uint32_t n)
{
    uint32_t c[4] = { 0 };
    auto n2 = n * n;
    auto src = img + rX - n + (rY - n) * w;
    for (auto y = rY - n; y < rY + n; ++y) {
        auto p = src;
        for (auto x = rX - n; x < rX + n; ++x, ++p) {
            c[0] += *p >> 24;
            c[1] += (*p >> 16) & 0xff;
            c[2] += (*p >> 8) & 0xff;
            c[3] += *p & 0xff;
        }
        src += w;
    }
    for (auto i = 0; i < 4; ++i) {
        c[i] = (c[i] >> 2) / n2;
    }
    return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}


/************************************************************************/
/* Rect                                                                 */
/************************************************************************/

static bool _rasterTranslucentMaskedRect(SwSurface* surface, const SwBBox& region, uint32_t color, uint32_t (*blendMethod)(uint32_t))
{
    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    TVGLOG("SW_ENGINE", "Translucent Masked Rect");

    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x;   //compositor buffer

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
            return _rasterTranslucentMaskedRect(surface, region, color, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTranslucentMaskedRect(surface, region, color, surface->blender.ialpha);
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

static bool _rasterTranslucentMaskedRle(SwSurface* surface, SwRleData* rle, uint32_t color, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Translucent Masked Rle");

    auto span = rle->spans;
    uint32_t src;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
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
            return _rasterTranslucentMaskedRle(surface, rle, color, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTranslucentMaskedRle(surface, rle, color, surface->blender.ialpha);
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
/* RLE Transformed RGBA Image                                           */
/************************************************************************/

static bool _rasterTransformedMaskedTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Transformed Masked Translucent Rle Image");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
            auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
    }
    return true;
}


static bool _rasterTransformedMaskedRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Transformed Masked Rle Image");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
         if (span->coverage == 255) {
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
                auto src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterTransformedTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t opacity)
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
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterDownScaledMaskedTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t opacity, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Down Scaled Masked Translucent Rle Image");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
            else src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), alpha);
            auto tmp = ALPHA_BLEND(src, _multiplyAlpha(alpha, blendMethod(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
    }
    return true;
}


static bool _rasterDownScaledMaskedRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Down Scaled Masked Rle Image");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
                auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
                if (rX >= w || rY >= h) continue;
                uint32_t src;
                if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = img[rY * image->stride + rX];
                else src = _interpDownScaler(img, image->stride, h, rX, rY, halfScale);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
                auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
                if (rX >= w || rY >= h) continue;
                uint32_t src;
                if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
                else src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), span->coverage);
                auto tmp = ALPHA_BLEND(src, _multiplyAlpha(span->coverage, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterDownScaledTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t opacity, uint32_t halfScale)
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
            else src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), alpha);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterUpScaledMaskedTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Up Scaled Masked Translucent Rle Image");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
            auto fX = (span->x + x) * itransform->e11 + ey1;
            auto fY = (span->x + x) * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * image->stride + rX], alpha);
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), alpha);
            auto tmp = ALPHA_BLEND(src, _multiplyAlpha(alpha, blendMethod(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
    }
    return true;
}


static bool _rasterUpScaledMaskedRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Up Scaled Masked Rle Image");

    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
                auto fX = (span->x + x) * itransform->e11 + ey1;
                auto fY = (span->x + x) * itransform->e21 + ey2;
                auto rX = static_cast<uint32_t>(roundf(fX));
                auto rY = static_cast<uint32_t>(roundf(fY));
                if (rX >= w || rY >= h) continue;
                uint32_t src;
                if (rX == w - 1 || rY == h - 1) src = img[rY * image->stride + rX];
                else src = _interpUpScaler(img, image->stride, h, fX, fY);
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
                if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
                else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), span->coverage);
                auto tmp = ALPHA_BLEND(src, _multiplyAlpha(span->coverage, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterUpScaledTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t opacity)
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
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), alpha);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterTransformedRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];

        for (uint32_t x = 0; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf((span->x + x) * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf((span->x + x) * itransform->e21 + ey2));
            if (rX >= image->w || rY >= image->h) continue;
            auto src = ALPHA_BLEND(image->data[rY * image->stride + rX], span->coverage);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterDownScaledRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t halfScale)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = span->x; x < span->len; ++x, ++dst) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;

            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
            else src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), span->coverage);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterUpScaledRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform)
{
    auto span = image->rle->spans;
    auto img = image->data;
    auto w = image->w;
    auto h = image->h;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto ey1 = span->y * itransform->e12 + itransform->e13;
        auto ey2 = span->y * itransform->e22 + itransform->e23;
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        for (uint32_t x = span->x; x < span->len; ++x, ++dst) {
            auto fX = x * itransform->e11 + ey1;
            auto fY = x * itransform->e21 + ey2;
            auto rX = static_cast<uint32_t>(roundf(fX));
            auto rY = static_cast<uint32_t>(roundf(fY));
            if (rX >= w || rY >= h) continue;
            uint32_t src;
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rY * image->stride + rX], span->coverage);
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), span->coverage);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _transformedRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, uint32_t opacity)
{
    Matrix itransform;
    if (transform && !mathInverse(transform, &itransform)) return false;

    auto halfScale = _halfScale(image->scale);

    if (_compositing(surface)) {
        if (opacity == 255) {
            //Transformed
            if (mathEqual(image->scale, 1.0f)) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterTransformedMaskedRleRGBAImage(surface, image, &itransform, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterTransformedMaskedRleRGBAImage(surface, image, &itransform, surface->blender.ialpha);
                }                
            //Transformed + Down Scaled
            } else if (image->scale < DOWN_SCALE_TOLERANCE) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterDownScaledMaskedRleRGBAImage(surface, image, &itransform, halfScale, surface->blender.alpha);
                } else  if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterDownScaledMaskedRleRGBAImage(surface, image, &itransform, halfScale, surface->blender.ialpha);
                }
                
            //Transformed + Up Scaled
            } else {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterUpScaledMaskedRleRGBAImage(surface, image, &itransform, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterUpScaledMaskedRleRGBAImage(surface, image, &itransform, surface->blender.ialpha);
                }                
            }
        } else {
            //Transformed
            if (mathEqual(image->scale, 1.0f)) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterTransformedMaskedTranslucentRleRGBAImage(surface, image, &itransform, opacity, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterTransformedMaskedTranslucentRleRGBAImage(surface, image, &itransform, opacity, surface->blender.ialpha);
                }                
            //Transformed + Down Scaled
            } else if (image->scale < DOWN_SCALE_TOLERANCE) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterDownScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, opacity, halfScale, surface->blender.alpha);
                } else  if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterDownScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, opacity, halfScale, surface->blender.ialpha);
                }
                
            //Transformed + Up Scaled
            } else {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterUpScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, opacity, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterUpScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, opacity, surface->blender.ialpha);
                }                
            }
        }
    } else {
        if (opacity == 255) {
            if (mathEqual(image->scale, 1.0f)) return _rasterTransformedRleRGBAImage(surface, image, &itransform);
            else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterDownScaledRleRGBAImage(surface, image, &itransform, halfScale);
            else return _rasterUpScaledRleRGBAImage(surface, image, &itransform);
        } else {
            if (mathEqual(image->scale, 1.0f)) return _rasterTransformedTranslucentRleRGBAImage(surface, image, &itransform, opacity);
            else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterDownScaledTranslucentRleRGBAImage(surface, image, &itransform, opacity, halfScale);
            else return _rasterUpScaledTranslucentRleRGBAImage(surface, image, &itransform, opacity);
        }
    }
    return false;
}

/************************************************************************/
/* RLE Scaled RGBA Image                                                */
/************************************************************************/

static bool _rasterScaledMaskedTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Scaled Masked Translucent Rle Image");

    auto span = image->rle->spans;

    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst, ++cmp) {
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), alpha);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst, ++cmp) {
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), alpha);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterScaledMaskedRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Scaled Masked Rle Image");

    auto span = image->rle->spans;

    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto tmp = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), blendMethod(*cmp));
                    *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
                }
            } else {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), span->coverage);
                    auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                    *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
                }
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto tmp = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), blendMethod(*cmp));
                    *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
                }
            } else {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), span->coverage);
                    auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                    *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
                }
            }
        }
    }
    return true;
}


static bool _rasterScaledTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale)
{
    auto span = image->rle->spans;

    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst) {
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), alpha);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst) {
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), alpha);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    }
    return true;
}


static bool _rasterScaledRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale)
{
    auto span = image->rle->spans;

    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = _interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale);
                    *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
                }
            } else {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), span->coverage);
                    *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
                }
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = static_cast<uint32_t>(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                    *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
                }
            } else {
                for (uint32_t x = span->x; x < ((uint32_t)span->x) + span->len; ++x, ++dst) {
                    auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), span->coverage);
                    *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
                }
            }
        }
    }
    return true;
}


static bool _scaledRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, const SwBBox& region, uint32_t opacity)
{
    Matrix itransform;
    if (transform && !mathInverse(transform, &itransform)) return false;

    auto halfScale = _halfScale(image->scale);

    if (_compositing(surface)) {
        if (opacity == 255) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedRleRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedRleRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.ialpha);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.ialpha);
            }
        }
    } else {
        if (opacity == 255) return _rasterScaledRleRGBAImage(surface, image, &itransform, region, opacity, halfScale);
        else return _rasterScaledTranslucentRleRGBAImage(surface, image, &itransform, region, opacity, halfScale);
    }
    return false;
}


/************************************************************************/
/* RLE Direct RGBA Image                                                */
/************************************************************************/

static bool _rasterDirectMaskedTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Direct Masked Rle Image");

    auto span = image->rle->spans;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        auto img = image->data + (span->y + image->oy) * image->stride + (span->x + image->ox);
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


static bool _rasterDirectMaskedRleRGBAImage(SwSurface* surface, const SwImage* image, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Direct Masked Rle Image");

    auto span = image->rle->spans;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        auto img = image->data + (span->y + image->oy) * image->stride + (span->x + image->ox);
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++img) {
                auto tmp = ALPHA_BLEND(*img, blendMethod(*cmp));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++img) {
                auto tmp = ALPHA_BLEND(*img, _multiplyAlpha(span->coverage, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterDirectTranslucentRleRGBAImage(SwSurface* surface, const SwImage* image, uint32_t opacity)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto img = image->data + (span->y + image->oy) * image->stride + (span->x + image->ox);
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
            auto src = ALPHA_BLEND(*img, alpha);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterDirectRleRGBAImage(SwSurface* surface, const SwImage* image)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto img = image->data + (span->y + image->oy) * image->stride + (span->x + image->ox);
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
                *dst = *img + ALPHA_BLEND(*dst, surface->blender.ialpha(*img));
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
                auto src = ALPHA_BLEND(*img, span->coverage);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    }
    return true;
}


static bool _directRleRGBAImage(SwSurface* surface, const SwImage* image, uint32_t opacity)
{
    if (_compositing(surface)) {
        if (opacity == 255) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDirectMaskedRleRGBAImage(surface, image, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedRleRGBAImage(surface, image, surface->blender.ialpha);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDirectMaskedTranslucentRleRGBAImage(surface, image, opacity, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedTranslucentRleRGBAImage(surface, image, opacity, surface->blender.ialpha);
            }
        }
    } else {
        if (opacity == 255) return _rasterDirectRleRGBAImage(surface, image);
        else return _rasterDirectTranslucentRleRGBAImage(surface, image, opacity);
    }
    return false;
}


/************************************************************************/
/* Transformed RGBA Image                                               */
/************************************************************************/

static bool _rasterTransformedMaskedTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Transformed Masked Image");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->compositor->image.stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * itransform->e12 + itransform->e13;
        float ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * image->stride)], _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
    }
    return true;
}


static bool _rasterTransformedMaskedRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Transformed Masked Image");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->compositor->image.stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto cmp = cbuffer;
        float ey1 = y * itransform->e12 + itransform->e13;
        float ey2 = y * itransform->e22 + itransform->e23;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto rX = static_cast<uint32_t>(roundf(x * itransform->e11 + ey1));
            auto rY = static_cast<uint32_t>(roundf(x * itransform->e21 + ey2));
            if (rX >= w || rY >= h) continue;
            auto src = ALPHA_BLEND(img[rX + (rY * image->stride)], blendMethod(*cmp));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
    }
    return true;
}


static bool _rasterTransformedTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity)
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

            auto src = ALPHA_BLEND(img[rX + (rY * image->stride)], opacity);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _rasterDownScaledMaskedTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Down Scaled Masked Image");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->compositor->image.stride + region.min.x];

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
                src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), _multiplyAlpha(opacity, blendMethod(*cmp)));
            }
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
    }
    return true;
}


static bool _rasterDownScaledMaskedRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Down Scaled Masked Image");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->compositor->image.stride + region.min.x];

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
                src = ALPHA_BLEND(img[rX + (rY * image->stride)], blendMethod(*cmp));
            } else {
                src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), blendMethod(*cmp));
            }
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
    }
    return true;
}


static bool _rasterDownScaledTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale)
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
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = ALPHA_BLEND(img[rX + (rY * w)], opacity);
            else src = ALPHA_BLEND(_interpDownScaler(img, w, h, rX, rY, halfScale), opacity);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _rasterUpScaledMaskedTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Up Scaled Masked Image");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->compositor->image.stride + region.min.x];

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
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
    }
    return true;
}


static bool _rasterUpScaledMaskedRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Up Scaled Masked Image");

    auto img = image->data;
    auto w = image->w;
    auto h = image->h;
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto cbuffer = &surface->compositor->image.data[region.min.y * surface->compositor->image.stride + region.min.x];

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
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rX + (rY * image->stride)], blendMethod(*cmp));
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), blendMethod(*cmp));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
    }
    return true;
}


static bool _rasterUpScaledTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity)
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
            if (rX == w - 1 || rY == h - 1) src = ALPHA_BLEND(img[rX + (rY * image->stride)], opacity);
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), opacity);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTransformedRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region)
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
            auto src = img[rX + (rY * image->stride)];
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterDownScaledRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t halfScale)
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
            uint32_t src;
            if (rX < halfScale || rY < halfScale || rX >= w - halfScale || rY >= h - halfScale) src = img[rX + (rY * w)];
            else src = _interpDownScaler(img, w, h, rX, rY, halfScale);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterUpScaledRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region)
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
            else src = _interpUpScaler(img, w, h, fX, fY);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _transformedRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, const SwBBox& region, uint32_t opacity)
{
    Matrix itransform;
    if (transform && !mathInverse(transform, &itransform)) return false;

    auto halfScale = _halfScale(image->scale);

    if (_compositing(surface)) {
        if (opacity == 255) {
            //Transformd
            if (mathEqual(image->scale, 1.0f)) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterTransformedMaskedRGBAImage(surface, image, &itransform, region, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterTransformedMaskedRGBAImage(surface, image, &itransform, region, surface->blender.ialpha);
                }
            //Transformed + DownScaled
            } else if (image->scale < DOWN_SCALE_TOLERANCE) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterDownScaledMaskedRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterDownScaledMaskedRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.ialpha);
                }
            //Transformed + UpScaled
            } else {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterUpScaledMaskedRGBAImage(surface, image, &itransform, region, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterUpScaledMaskedRGBAImage(surface, image, &itransform, region, surface->blender.ialpha);
                }
            }
        } else {
            //Transformd
            if (mathEqual(image->scale, 1.0f)) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterTransformedMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterTransformedMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, surface->blender.ialpha);
                }
            //Transformed + DownScaled
            } else if (image->scale < DOWN_SCALE_TOLERANCE) {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterDownScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterDownScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.ialpha);
                }
            //Transformed + UpScaled
            } else {
                if (surface->compositor->method == CompositeMethod::AlphaMask) {
                    return _rasterUpScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, surface->blender.alpha);
                } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                    return _rasterUpScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity,  surface->blender.ialpha);
                }
            }
        }
    } else {
        if (opacity == 255) {
            if (mathEqual(image->scale, 1.0f)) return _rasterTransformedRGBAImage(surface, image, &itransform, region);
            else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterDownScaledRGBAImage(surface, image, &itransform, region, halfScale);
            else return _rasterUpScaledRGBAImage(surface, image, &itransform, region);
        } else {
            if (mathEqual(image->scale, 1.0f)) return _rasterTransformedTranslucentRGBAImage(surface, image, &itransform, region,  opacity);
            else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterDownScaledTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale);
            else return _rasterUpScaledTranslucentRGBAImage(surface, image, &itransform, region, opacity);
        }
    }
    return false;
}


/************************************************************************/
/*Scaled RGBA Image                                                     */
/************************************************************************/


static bool _rasterScaledMaskedTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Scaled Masked Image");

    //Top, Bottom Lines
    SwCoord ys[2] = {region.min.y, region.max.y - 1};

    for (auto i = 0; i < 2; ++i) {
        auto y = ys[i];
        auto dst = surface->buffer + (y * surface->stride + region.min.x);
        auto cmp = surface->compositor->image.data + (y * surface->compositor->image.stride + region.min.x);
        auto img = image->data + static_cast<uint32_t>(y * itransform->e22 + itransform->e23) * image->stride;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto alpha = _multiplyAlpha(opacity, blendMethod(*cmp));
            auto src = ALPHA_BLEND(img[static_cast<uint32_t>(x * itransform->e11 + itransform->e13)], alpha);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Left, Right Lines
    SwCoord xs[2] = {region.min.x, region.max.x - 1};

    for (auto i = 0; i < 2; ++i) {
        auto x = xs[i];
        auto dst = surface->buffer + ((region.min.y + 1) * surface->stride + x);
        auto cmp = surface->compositor->image.data + ((region.min.y + 1) * surface->compositor->image.stride + x);
        auto img = image->data + static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y, dst += surface->stride, cmp += surface->stride) {
            auto alpha = _multiplyAlpha(opacity, blendMethod(*cmp));
            auto src = ALPHA_BLEND(img[static_cast<uint32_t>(y * itransform->e22 + itransform->e23) * image->stride], alpha);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        auto dbuffer = surface->buffer + ((region.min.y + 1) * surface->stride + (region.min.x + 1));
        auto cbuffer = surface->compositor->image.data + ((region.min.y + 1) * surface->compositor->image.stride + (region.min.x + 1));
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y) {
            auto dst = dbuffer;
            auto cmp = cbuffer;
            auto sy = static_cast<uint32_t>(y * itransform->e22 + itransform->e23);
            for (auto x = region.min.x + 1; x < region.max.x - 1; ++x, ++dst, ++cmp) {
                auto alpha = _multiplyAlpha(opacity, blendMethod(*cmp));
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), alpha);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
            dbuffer += surface->stride;
            cbuffer += surface->compositor->image.stride;
        }
    //Center (Up-Scaled)
    } else {
        auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);
        auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride + region.min.x);
        for (auto y = region.min.y; y < region.max.y - 1; ++y) {
            auto dst = dbuffer;
            auto cmp = cbuffer;
            auto sy = y * itransform->e22 + itransform->e23;
            for (auto x = region.min.x; x < region.max.x - 1; ++x, ++dst, ++cmp) {
                auto alpha = _multiplyAlpha(opacity, blendMethod(*cmp));
                auto sx = x * itransform->e11 + itransform->e13;
                auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), alpha);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
            dbuffer += surface->stride;
            cbuffer += surface->compositor->image.stride;
        }
    }
    return true;
}


static bool _rasterScaledMaskedRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Scaled Masked Image");

    //Top, Bottom Lines
    SwCoord ys[2] = {region.min.y, region.max.y - 1};

    for (auto i = 0; i < 2; ++i) {
        auto y = ys[i];
        auto dst = surface->buffer + (y * surface->stride + region.min.x);
        auto cmp = surface->compositor->image.data + (y * surface->compositor->image.stride + region.min.x);
        auto img = image->data + static_cast<uint32_t>(y * itransform->e22 + itransform->e23) * image->stride;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
            auto src = ALPHA_BLEND(img[static_cast<uint32_t>(x * itransform->e11 + itransform->e13)], blendMethod(*cmp));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Left, Right Lines
    SwCoord xs[2] = {region.min.x, region.max.x - 1};

    for (auto i = 0; i < 2; ++i) {
        auto x = xs[i];
        auto dst = surface->buffer + ((region.min.y + 1) * surface->stride + x);
        auto cmp = surface->compositor->image.data + ((region.min.y + 1) * surface->compositor->image.stride + x);
        auto img = image->data + static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y, dst += surface->stride, cmp += surface->stride) {
            auto src = ALPHA_BLEND(img[static_cast<uint32_t>(y * itransform->e22 + itransform->e23) * image->stride], blendMethod(*cmp));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        auto dbuffer = surface->buffer + ((region.min.y + 1) * surface->stride + (region.min.x + 1));
        auto cbuffer = surface->compositor->image.data + ((region.min.y + 1) * surface->compositor->image.stride + (region.min.x + 1));
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y) {
            auto dst = dbuffer;
            auto cmp = cbuffer;
            auto sy = static_cast<uint32_t>(y * itransform->e22 + itransform->e23);
            for (auto x = region.min.x + 1; x < region.max.x - 1; ++x, ++dst, ++cmp) {
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), blendMethod(*cmp));
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
            dbuffer += surface->stride;
            cbuffer += surface->compositor->image.stride;
        }
    //Center (Up-Scaled)
    } else {
        auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);
        auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride + region.min.x);
        for (auto y = region.min.y; y < region.max.y - 1; ++y) {
            auto dst = dbuffer;
            auto cmp = cbuffer;
            auto sy = y * itransform->e22 + itransform->e23;
            for (auto x = region.min.x; x < region.max.x - 1; ++x, ++dst, ++cmp) {
                auto sx = x * itransform->e11 + itransform->e13;
                auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), blendMethod(*cmp));
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
            dbuffer += surface->stride;
            cbuffer += surface->compositor->image.stride;
        }
    }
    return true;
}


static bool _rasterScaledTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale)
{
    //Top, Bottom Lines
    SwCoord ys[2] = {region.min.y, region.max.y - 1};

    for (auto i = 0; i < 2; ++i) {
        auto y = ys[i];
        auto dst = surface->buffer + (y * surface->stride + region.min.x);
        auto img = image->data + static_cast<uint32_t>(y * itransform->e22 + itransform->e23) * image->stride;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto src = ALPHA_BLEND(img[static_cast<uint32_t>(x * itransform->e11 + itransform->e13)], opacity);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Left, Right Lines
    SwCoord xs[2] = {region.min.x, region.max.x - 1};

    for (auto i = 0; i < 2; ++i) {
        auto x = xs[i];
        auto dst = surface->buffer + ((region.min.y + 1) * surface->stride + x);
        auto img = image->data + static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y, dst += surface->stride) {
            auto src = ALPHA_BLEND(img[static_cast<uint32_t>(y * itransform->e22 + itransform->e23) * image->stride], opacity);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        auto dbuffer = surface->buffer + ((region.min.y + 1) * surface->stride + (region.min.x + 1));
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y, dbuffer += surface->stride) {
            auto sy = static_cast<uint32_t>(y * itransform->e22 + itransform->e23);
            auto dst = dbuffer;
            for (auto x = region.min.x + 1; x < region.max.x - 1; ++x, ++dst) {
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale), opacity);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    //Center (Up-Scaled)
    } else {
        auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);
        for (auto y = region.min.y; y < region.max.y - 1; ++y, dbuffer += surface->stride) {
            auto sy = y * itransform->e22 + itransform->e23;
            auto dst = dbuffer;
            for (auto x = region.min.x; x < region.max.x - 1; ++x, ++dst) {
                auto sx = x * itransform->e11 + itransform->e13;
                auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), opacity);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    }
    return true;
}


static bool _rasterScaledRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t halfScale)
{
    //Top, Bottom Lines
    SwCoord ys[2] = {region.min.y, region.max.y - 1};

    for (auto i = 0; i < 2; ++i) {
        auto y = ys[i];
        auto dst = surface->buffer + (y * surface->stride + region.min.x);
        auto img = image->data + static_cast<uint32_t>((y * itransform->e22 + itransform->e23)) * image->stride;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
            auto src = img[static_cast<uint32_t>(x * itransform->e11 + itransform->e13)];
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Left, Right Lines
    SwCoord xs[2] = {region.min.x, region.max.x - 1};

    for (auto i = 0; i < 2; ++i) {
        auto x = xs[i];
        auto dst = surface->buffer + ((region.min.y + 1) * surface->stride + x);
        auto img = image->data + static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y, dst += surface->stride) {
            auto src = img[static_cast<uint32_t>(y * itransform->e22 + itransform->e23) * image->stride];
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    //Center (Down-Scaled)
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        auto dbuffer = surface->buffer + ((region.min.y + 1) * surface->stride + (region.min.x + 1));
        for (auto y = region.min.y + 1; y < region.max.y - 1; ++y, dbuffer += surface->stride) {
            auto sy = static_cast<uint32_t>(y * itransform->e22 + itransform->e23);
            auto dst = dbuffer;
            for (auto x = region.min.x + 1; x < region.max.x - 1; ++x, ++dst) {
                auto sx = static_cast<uint32_t>(x * itransform->e11 + itransform->e13);
                auto src = _interpDownScaler(image->data, image->w, image->h, sx, sy, halfScale);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    //Center (Up-Scaled)
    } else {
        auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);
        for (auto y = region.min.y; y < region.max.y - 1; ++y, dbuffer += surface->stride) {
            auto sy = y * itransform->e22 + itransform->e23;
            auto dst = dbuffer;
            for (auto x = region.min.x; x < region.max.x - 1; ++x, ++dst) {
                auto sx = x * itransform->e11 + itransform->e13;
                auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    }
    return true;
}


static bool _scaledRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, const SwBBox& region, uint32_t opacity)
{
    Matrix itransform;
    if (transform && !mathInverse(transform, &itransform)) return false;

    auto halfScale = _halfScale(image->scale);

    if (_compositing(surface)) {
        if (opacity == 255) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.ialpha);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.ialpha);
            }
        }
    } else {
        if (opacity == 255) return _rasterScaledRGBAImage(surface, image, &itransform, region, halfScale);
        else return _rasterScaledTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale);
    }
    return false;
}


/************************************************************************/
/* Direct RGBA Image                                                    */
/************************************************************************/

static bool _rasterDirectMaskedRGBAImage(SwSurface* surface, const SwImage* image, const SwBBox& region, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Direct Masked Image");

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

    auto sbuffer = image->data + (region.min.y + image->oy) * image->stride + (region.min.x + image->ox);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
        sbuffer += image->stride;
    }
    return true;
}


static bool _rasterDirectMaskedTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const SwBBox& region, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Direct Masked Translucent Image");

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h2 = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w2 = static_cast<uint32_t>(region.max.x - region.min.x);

    auto sbuffer = image->data + (region.min.y + image->oy) * image->stride + (region.min.x + image->ox);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x;   //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
        buffer += surface->stride;
        cbuffer += surface->compositor->image.stride;
        sbuffer += image->stride;
    }
    return true;
}


static bool _rasterDirectTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const SwBBox& region, uint32_t opacity)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + (region.min.y + image->oy) * image->stride + (region.min.x + image->ox);

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++src) {
            auto tmp = ALPHA_BLEND(*src, opacity);
            *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
        }
        dbuffer += surface->stride;
        sbuffer += image->stride;
    }
    return true;
}


static bool _rasterDirectRGBAImage(SwSurface* surface, const SwImage* image, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + (region.min.y + image->oy) * image->stride + (region.min.x + image->ox);

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; x++, dst++, src++) {
            *dst = *src + ALPHA_BLEND(*dst, surface->blender.ialpha(*src));
        }
        dbuffer += surface->stride;
        sbuffer += image->stride;
    }
    return true;
}


//Blenders for the following scenarios: [Composition / Non-Composition] * [Opaque / Translucent]
static bool _directRGBAImage(SwSurface* surface, const SwImage* image, const SwBBox& region, uint32_t opacity)
{
    if (_compositing(surface)) {
        if (opacity == 255) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDirectMaskedRGBAImage(surface, image, region, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedRGBAImage(surface, image, region, surface->blender.ialpha);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDirectMaskedTranslucentRGBAImage(surface, image, region, opacity, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedTranslucentRGBAImage(surface, image, region, opacity, surface->blender.ialpha);
            }
        }
    } else {
        if (opacity == 255) return _rasterDirectRGBAImage(surface, image, region);
        else return _rasterDirectTranslucentRGBAImage(surface, image, region, opacity);
    }
    return false;
}


//Blenders for the following scenarios: [RLE / Whole] * [Direct / Scaled / Transformed]
static bool _rasterRGBAImage(SwSurface* surface, SwImage* image, const Matrix* transform, const SwBBox& region, uint32_t opacity)
{
    //RLE Image
    if (image->rle) {
        if (image->direct) return _directRleRGBAImage(surface, image, opacity);
        else if (image->scaled) return _scaledRleRGBAImage(surface, image, transform, region, opacity);
        //OPTIMIZE_ME: Replace with the TexMap Rasterizer
        else return _transformedRleRGBAImage(surface, image, transform, opacity);
    //Whole Image
    } else {
        if (image->direct) return _directRGBAImage(surface, image, region, opacity);
        else if (image->scaled) return _scaledRGBAImage(surface, image, transform, region, opacity);
        //OPTIMIZE_ME: Replace with the TexMap Rasterizer
        else return _transformedRGBAImage(surface, image, transform, region, opacity);
    }
}


/************************************************************************/
/* Rect Linear Gradient                                                 */
/************************************************************************/

static bool _rasterTranslucentLinearGradientMaskedRect(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x;

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


static bool __rasterTranslucentLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
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
            dst[x] = sbuffer[x] + ALPHA_BLEND(dst[x], surface->blender.ialpha(sbuffer[x]));
        }
        dst += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterTranslucentLinearGradientMaskedRect(surface, region, fill, surface->blender.alpha);
        }
        if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTranslucentLinearGradientMaskedRect(surface, region, fill, surface->blender.ialpha);
        }
    }
    return __rasterTranslucentLinearGradientRect(surface, region, fill);
}


static bool _rasterSolidLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
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


/************************************************************************/
/* Rle Linear Gradient                                                  */
/************************************************************************/


static bool _rasterTranslucentLinearGradientMaskedRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto cbuffer = surface->compositor->image.data;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        fillFetchLinear(fill, buffer, span->y, span->x, span->len);
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
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


static bool __rasterTranslucentLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
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
                dst[i] = buffer[i] + ALPHA_BLEND(dst[i], surface->blender.ialpha(buffer[i]));
            }
        } else {
            for (uint32_t i = 0; i < span->len; ++i) {
                auto tmp = ALPHA_BLEND(buffer[i], span->coverage);
                dst[i] = tmp + ALPHA_BLEND(dst[i], surface->blender.ialpha(tmp));
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
            return _rasterTranslucentLinearGradientMaskedRle(surface, rle, fill, surface->blender.alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTranslucentLinearGradientMaskedRle(surface, rle, fill, surface->blender.ialpha);
        }
    }
    return __rasterTranslucentLinearGradientRle(surface, rle, fill);
}


static bool _rasterSolidLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
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


/************************************************************************/
/* Rect Radial Gradient                                                 */
/************************************************************************/

static bool _rasterTranslucentRadialGradientMaskedRect(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x;

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


static bool __rasterTranslucentRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
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
            dst[x] = sbuffer[x] + ALPHA_BLEND(dst[x], surface->blender.ialpha(sbuffer[x]));
        }
        dst += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterTranslucentRadialGradientMaskedRect(surface, region, fill, surface->blender.alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTranslucentRadialGradientMaskedRect(surface, region, fill, surface->blender.ialpha);
        }
    }
    return __rasterTranslucentRadialGradientRect(surface, region, fill);
}


static bool _rasterSolidRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
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


/************************************************************************/
/* RLE Radial Gradient                                                  */
/************************************************************************/


static bool _rasterTranslucentRadialGradientMaskedRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto cbuffer = surface->compositor->image.data;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        fillFetchRadial(fill, buffer, span->y, span->x, span->len);
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
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


static bool __rasterTranslucentRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
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
                dst[i] = buffer[i] + ALPHA_BLEND(dst[i], surface->blender.ialpha(buffer[i]));
            }
        } else {
           for (uint32_t i = 0; i < span->len; ++i) {
                auto tmp = ALPHA_BLEND(buffer[i], span->coverage);
                dst[i] = tmp + ALPHA_BLEND(dst[i], surface->blender.ialpha(tmp));
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
            return _rasterTranslucentRadialGradientMaskedRle(surface, rle, fill, surface->blender.alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTranslucentRadialGradientMaskedRle(surface, rle, fill, surface->blender.ialpha);
        }
    }
    return __rasterTranslucentRadialGradientRle(surface, rle, fill);
}


static bool _rasterSolidRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
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
            return _rasterSolidLinearGradientRect(surface, shape->bbox, shape->fill);
        } else {
            if (translucent) return _rasterTranslucentRadialGradientRect(surface, shape->bbox, shape->fill);
            return _rasterSolidRadialGradientRect(surface, shape->bbox, shape->fill);
        }
    } else {
        if (!shape->rle) return false;
        if (id == TVG_CLASS_ID_LINEAR) {
            if (translucent) return _rasterTranslucentLinearGradientRle(surface, shape->rle, shape->fill);
            return _rasterSolidLinearGradientRle(surface, shape->rle, shape->fill);
        } else {
            if (translucent) return _rasterTranslucentRadialGradientRle(surface, shape->rle, shape->fill);
            return _rasterSolidRadialGradientRle(surface, shape->rle, shape->fill);
        }
    }
    return false;
}


bool rasterShape(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
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
        return _rasterSolidLinearGradientRle(surface, shape->strokeRle, shape->stroke->fill);
    } else {
        if (translucent) return _rasterTranslucentRadialGradientRle(surface, shape->strokeRle, shape->stroke->fill);
        return _rasterSolidRadialGradientRle(surface, shape->strokeRle, shape->stroke->fill);
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
    //Verify Boundary
    if (bbox.max.x < 0 || bbox.max.y < 0 || bbox.min.x >= surface->w || bbox.min.y >= surface->h) return false;

    //TOOD: switch (image->format)
    //TODO: case: _rasterRGBImage()
    //TODO: case: _rasterGrayscaleImage()
    //TODO: case: _rasterAlphaImage()

    return _rasterRGBAImage(surface, image, transform, bbox, opacity);
}