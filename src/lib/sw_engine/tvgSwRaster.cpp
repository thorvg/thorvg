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


//Bilinear Interpolation
static uint32_t _interpUpScaler(const uint32_t *img, uint32_t w, uint32_t h, float fX, float fY)
{
    auto rX = static_cast<uint32_t>(fX);
    auto rY = static_cast<uint32_t>(fY);

    auto dX = static_cast<uint32_t>((fX - rX) * 255.0f);
    auto dY = static_cast<uint32_t>((fY - rY) * 255.0f);

    auto c1 = img[rX + (rY * w)];
    auto c2 = img[(rX + 1) + (rY * w)];
    auto c3 = img[(rX + 1) + ((rY + 1) * w)];
    auto c4 = img[rX + ((rY + 1) * w)];

    return COLOR_INTERPOLATE(COLOR_INTERPOLATE(c1, 255 - dX, c2, dX), 255 - dY, COLOR_INTERPOLATE(c4, 255 - dX, c3, dX), dY);
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
/* RLE Transformed Translucent Image                                    */
/************************************************************************/

static bool _rasterTransformedMaskedRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
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


static bool _rasterTransformedTranslucentRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
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


static bool _rasterDownScaledMaskedRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
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
                else src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), alpha);
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
                else src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), alpha);
                auto tmp = ALPHA_BLEND(src, _multiplyAlpha(alpha, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterDownScaledTranslucentRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t halfScale)
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


static bool _rasterUpScaledMaskedRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
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
                else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), alpha);
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
                else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), alpha);
                auto tmp = ALPHA_BLEND(src, _multiplyAlpha(alpha, blendMethod(*cmp)));
                *dst = tmp + ALPHA_BLEND(*dst, surface->blender.ialpha(tmp));
            }
        }
    }
    return true;
}


static bool _rasterUpScaledTranslucentRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform)
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


static bool _rasterTransformedTranslucentRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const Matrix* itransform, uint32_t halfScale)
{
    //Transformed
    if (mathEqual(image->scale, 1.0f)) {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterTransformedMaskedRleImage(surface, image, opacity, itransform, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterTransformedMaskedRleImage(surface, image, opacity, itransform, surface->blender.ialpha);
            }
        }
        return _rasterTransformedTranslucentRleImage(surface, image, opacity, itransform);
    //Transformed + Down Scaled
    } else if (image->scale < DOWN_SCALE_TOLERANCE) {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDownScaledMaskedRleImage(surface, image, opacity, itransform, halfScale, surface->blender.alpha);
            } else  if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDownScaledMaskedRleImage(surface, image, opacity, itransform, halfScale, surface->blender.ialpha);
            }
        }
        return _rasterDownScaledTranslucentRleImage(surface, image, opacity, itransform, halfScale);  
    //Transformed + Up Scaled
    } else {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterUpScaledMaskedRleImage(surface, image, opacity, itransform, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterUpScaledMaskedRleImage(surface, image, opacity, itransform, surface->blender.ialpha);
            }
        }
        return _rasterUpScaledTranslucentRleImage(surface, image, opacity, itransform);
    }
}


/************************************************************************/
/* RLE Transformed Solid Image                                         */
/************************************************************************/

static bool _rasterSolidRleImage(SwSurface* surface, const SwImage* image, const Matrix* itransform)
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
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterDownScaledSolidRleImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t halfScale)
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
            else src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), span->coverage);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterUpScaledSolidRleImage(SwSurface* surface, const SwImage* image, const Matrix* itransform)
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
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), span->coverage);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterTransformedSolidRleImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, uint32_t halfScale)
{  
    if (mathEqual(image->scale, 1.0f)) return _rasterSolidRleImage(surface, image, itransform);
    else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterDownScaledSolidRleImage(surface, image, itransform, halfScale);
    else return _rasterUpScaledSolidRleImage(surface, image, itransform);
}


/************************************************************************/
/* RLE Direct (Solid + Translucent) Image                              */
/************************************************************************/

static bool _rasterDirectMaskedRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity, uint32_t (*blendMethod)(uint32_t))
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


static bool __rasterDirectTranslucentRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity)
{
    auto span = image->rle->spans;

    for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto img = image->data + (span->y + image->y) * image->stride + (span->x + image->x);
        auto alpha = _multiplyAlpha(span->coverage, opacity);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
            auto src = ALPHA_BLEND(*img, alpha);
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
    }
    return true;
}


static bool _rasterDirectTranslucentRleImage(SwSurface* surface, const SwImage* image, uint32_t opacity)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterDirectMaskedRleImage(surface, image, opacity, surface->blender.alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterDirectMaskedRleImage(surface, image, opacity, surface->blender.ialpha);
        }
    }
    return __rasterDirectTranslucentRleImage(surface, image, opacity);
}


static bool _rasterDirectSolidRleImage(SwSurface* surface, const SwImage* image)
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
                *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
            }
        }
    }
    return true;
}


/************************************************************************/
/* Whole Transformed Translucent Image                                  */
/************************************************************************/

static bool _rasterTransformedMaskedImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
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
            auto src = ALPHA_BLEND(img[rX + (rY * image->stride)], _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTransformedTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform)
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


static bool _rasterDownScaledMaskedImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
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
                src = ALPHA_BLEND(_interpDownScaler(img, image->stride, h, rX, rY, halfScale), _multiplyAlpha(opacity, blendMethod(*cmp)));
            }
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterDownScaledTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t halfScale)
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


static bool _rasterUpScaledMaskedImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t (*blendMethod)(uint32_t))
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
            else src = ALPHA_BLEND(_interpUpScaler(img, image->stride, h, fX, fY), _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = src + ALPHA_BLEND(*dst, surface->blender.ialpha(src));
        }
        dbuffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterUpScaledTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform)
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


static bool _rasterTransformedTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, const Matrix* itransform, uint32_t halfScale)
{
    //Transformd
    if (mathEqual(image->scale, 1.0f)) {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterTransformedMaskedImage(surface, image, opacity, region, itransform, surface->blender.alpha);
            }
            if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterTransformedMaskedImage(surface, image, opacity, region, itransform, surface->blender.ialpha);
            }
        }
        return _rasterTransformedTranslucentImage(surface, image, opacity, region, itransform);
    //Transformed + DownScaled
    } else if (image->scale < DOWN_SCALE_TOLERANCE) {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDownScaledMaskedImage(surface, image, opacity, region, itransform, halfScale, surface->blender.alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDownScaledMaskedImage(surface, image, opacity, region, itransform, halfScale, surface->blender.ialpha);
            }
        }
        return _rasterDownScaledTranslucentImage(surface, image, opacity, region, itransform, halfScale);
    //Transformed + UpScaled
    } else {
        if (surface->compositor) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterUpScaledMaskedImage(surface, image, opacity, region, itransform, surface->blender.alpha);
            }else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterUpScaledMaskedImage(surface, image, opacity, region, itransform, surface->blender.ialpha);
            }
        }
        return _rasterUpScaledTranslucentImage(surface, image, opacity, region, itransform);
    }
}


/************************************************************************/
/* Whole Transformed Solid Image                                       */
/************************************************************************/

static bool _rasterTransformedSolidImage(SwSurface* surface, const SwImage* image, const SwBBox& region, const Matrix* itransform)
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
            *dst = img[rX + (rY * image->stride)];
        }
    }
    return true;
}


static bool _rasterDownScaledSolidImage(SwSurface* surface, const SwImage* image, const SwBBox& region, const Matrix* itransform, uint32_t halfScale)
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
            *dst = src;
        }
    }
    return true;
}


static bool _rasterUpScaledSolidImage(SwSurface* surface, const SwImage* image, const SwBBox& region, const Matrix* itransform)
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
            *dst = src;
        }
    }
    return true;
}


static bool _rasterTransformedSolidImage(SwSurface* surface, const SwImage* image, const SwBBox& region, const Matrix* itransform, uint32_t halfScale)
{  
    if (mathEqual(image->scale, 1.0f)) return _rasterTransformedSolidImage(surface, image, region, itransform);
    else if (image->scale < DOWN_SCALE_TOLERANCE) return _rasterDownScaledSolidImage(surface, image, region, itransform, halfScale);
    else return _rasterUpScaledSolidImage(surface, image, region, itransform);
}


/************************************************************************/
/* Whole (Solid + Translucent) Direct Image                            */
/************************************************************************/

static bool _rasterDirectMaskedImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region, uint32_t (*blendMethod)(uint32_t))
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


static bool __rasterDirectTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + (region.min.y + image->y) * image->stride + (region.min.x + image->x);

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++src) {
            auto p = ALPHA_BLEND(*src, opacity);
            *dst = p + ALPHA_BLEND(*dst, surface->blender.ialpha(p));
        }
        dbuffer += surface->stride;
        sbuffer += image->stride;
    }
    return true;
}


static bool _rasterDirectTranslucentImage(SwSurface* surface, const SwImage* image, uint32_t opacity, const SwBBox& region)
{
    if (surface->compositor) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterDirectMaskedImage(surface, image, opacity, region, surface->blender.alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterDirectMaskedImage(surface, image, opacity, region, surface->blender.ialpha);
        }
    }
    return __rasterDirectTranslucentImage(surface, image, opacity, region);
}


static bool _rasterDirectSolidImage(SwSurface* surface, const SwImage* image, const SwBBox& region)
{
    auto dbuffer = &surface->buffer[region.min.y * surface->stride + region.min.x];
    auto sbuffer = image->data + (region.min.y + image->y) * image->stride + (region.min.x + image->x);

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto dst = dbuffer;
        auto src = sbuffer;
        for (auto x = region.min.x; x < region.max.x; x++, dst++, src++) {
            *dst = *src;
        }
        dbuffer += surface->stride;
        sbuffer += image->stride;
    }
    return true;
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
    Matrix itransform;
    if (transform && !mathInverse(transform, &itransform)) return false;

    auto halfScale = static_cast<uint32_t>(0.5f / image->scale);
    if (halfScale == 0) halfScale = 1;

    auto translucent = _translucent(surface, opacity);

    //Clipped Image
    if (image->rle) {
        if (image->transformed) {
            if (translucent) return _rasterTransformedTranslucentRleImage(surface, image, opacity, &itransform, halfScale);
            else return _rasterTransformedSolidRleImage(surface, image, &itransform, halfScale);
        } else {
            if (translucent) return _rasterDirectTranslucentRleImage(surface, image, opacity);
            else return _rasterDirectSolidRleImage(surface, image);
        }
    //Whole Image
    } else {
        if (image->transformed) {
            if (translucent) return _rasterTransformedTranslucentImage(surface, image, opacity, bbox, &itransform, halfScale);
            else return _rasterTransformedSolidImage(surface, image, bbox, &itransform, halfScale);
        } else {
            if (translucent) return _rasterDirectTranslucentImage(surface, image, opacity, bbox);
            else return _rasterDirectSolidImage(surface, image, bbox);
        }
    }
}