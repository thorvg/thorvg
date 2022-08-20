/*
 * Copyright (c) 2020 - 2022 Samsung Electronics Co., Ltd. All rights reserved.

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

#ifdef _WIN32
    #include <malloc.h>
#elif defined(__linux__)
    #include <alloca.h>
#else
    #include <stdlib.h>
#endif

#include "tvgMath.h"
#include "tvgRender.h"
#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
constexpr auto DOWN_SCALE_TOLERANCE = 0.5f;


static inline uint32_t _multiplyAlpha(uint32_t c, uint32_t a)
{
    return ((c * a + 0xff) >> 8);
}


static inline uint32_t _alpha(uint32_t c)
{
    return (c >> 24);
}


static inline uint32_t _ialpha(uint32_t c)
{
    return (~c >> 24);
}


static inline uint32_t _abgrLumaValue(uint32_t c)
{
    return ((((c&0xff)*54) + (((c>>8)&0xff)*183) + (((c>>16)&0xff)*19))) >> 8; //0.2125*R + 0.7154*G + 0.0721*B
}


static inline uint32_t _argbLumaValue(uint32_t c)
{
    return ((((c&0xff)*19) + (((c>>8)&0xff)*183) + (((c>>16)&0xff)*54))) >> 8; //0.0721*B + 0.7154*G + 0.2125*R
}


static inline uint32_t _abgrJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | b << 16 | g << 8 | r);
}


static inline uint32_t _argbJoin(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (a << 24 | r << 16 | g << 8 | b);
}


#include "tvgSwRasterTexmap.h"
#include "tvgSwRasterC.h"
#include "tvgSwRasterAvx.h"
#include "tvgSwRasterNeon.h"


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
    auto rx = (uint32_t)(sx);
    auto ry = (uint32_t)(sy);
    auto rx2 = rx + 1;
    if (rx2 >= w) rx2 = w - 1;
    auto ry2 = ry + 1;
    if (ry2 >= h) ry2 = h - 1;

    auto dx = static_cast<uint32_t>((sx - rx) * 255.0f);
    auto dy = static_cast<uint32_t>((sy - ry) * 255.0f);

    auto c1 = img[rx + ry * w];
    auto c2 = img[rx2 + ry * w];
    auto c3 = img[rx2 + ry2 * w];
    auto c4 = img[rx + ry2 * w];

    return INTERPOLATE(dy, INTERPOLATE(dx, c3, c4), INTERPOLATE(dx, c2, c1));
}


//2n x 2n Mean Kernel
static uint32_t _interpDownScaler(const uint32_t *img, uint32_t stride, uint32_t w, uint32_t h, uint32_t rx, uint32_t ry, uint32_t n)
{
    uint32_t c[4] = {0, 0, 0, 0};
    auto n2 = n * n;
    auto src = img + rx - n + (ry - n) * stride;

    for (auto y = ry - n; y < ry + n; ++y) {
        if (y >= h) continue;
        auto p = src;
        for (auto x = rx - n; x < rx + n; ++x, ++p) {
            if (x >= w) continue;
            c[0] += *p >> 24;
            c[1] += (*p >> 16) & 0xff;
            c[2] += (*p >> 8) & 0xff;
            c[3] += *p & 0xff;
        }
        src += stride;
    }
    for (auto i = 0; i < 4; ++i) {
        c[i] = (c[i] >> 2) / n2;
    }
    return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}


/************************************************************************/
/* Rect                                                                 */
/************************************************************************/

static bool _rasterMaskedRect(SwSurface* surface, const SwBBox& region, uint32_t color, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Masked Rect");

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);

    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x; //compositor buffer

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = &buffer[y * surface->stride];
        auto cmp = &cbuffer[y * surface->stride];
        for (uint32_t x = 0; x < w; ++x, ++dst, ++cmp) {
            auto tmp = ALPHA_BLEND(color, blendMethod(*cmp));
            *dst = surface->blender.blend(color, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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


static bool _rasterRect(SwSurface* surface, const SwBBox& region, uint32_t color, uint8_t opacity)
{
    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterMaskedRect(surface, region, color, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterMaskedRect(surface, region, color, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterMaskedRect(surface, region, color, surface->blender.lumaValue);
        }
    } else {
        if (opacity == 255) {
            return _rasterSolidRect(surface, region, color);
        } else {
#if defined(THORVG_AVX_VECTOR_SUPPORT)
            return avxRasterTranslucentRect(surface, region, color);
#elif defined(THORVG_NEON_VECTOR_SUPPORT)
            return neonRasterTranslucentRect(surface, region, color);
#else
            return cRasterTranslucentRect(surface, region, color);
#endif
        }
    }
    return false;
}


/************************************************************************/
/* Rle                                                                  */
/************************************************************************/

static bool _rasterMaskedRle(SwSurface* surface, SwRleData* rle, uint32_t color, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Masked Rle");

    auto span = rle->spans;
    uint32_t src;
    auto cbuffer = surface->compositor->image.data;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        auto cmp = &cbuffer[span->y * surface->compositor->image.stride + span->x];
        if (span->coverage == 255) src = color;
        else src = ALPHA_BLEND(color, span->coverage);
        for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp) {
            auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
            *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
        }
    }
    return true;
}


static bool _rasterSolidRle(SwSurface* surface, const SwRleData* rle, uint32_t color)
{
    auto span = rle->spans;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        if (span->coverage == 255) {
            rasterRGBA32(surface->buffer + span->y * surface->stride, color, span->x, span->len);
        } else {
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto src = ALPHA_BLEND(color, span->coverage);
            auto ialpha = 255 - span->coverage;
            for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                // TODO: (@Projectitis) Not sure about the src_blended argument here. @hermet or @mmaciola to check
                *dst = surface->blender.blend(color, *dst, span->coverage, ialpha, src);
            }
        }
    }
    return true;
}


static bool _rasterRle(SwSurface* surface, SwRleData* rle, uint32_t color, uint8_t opacity)
{
    if (!rle) return false;

    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterMaskedRle(surface, rle, color, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterMaskedRle(surface, rle, color, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterMaskedRle(surface, rle, color, surface->blender.lumaValue);
        }
    } else {
        if (opacity == 255) {
            return _rasterSolidRle(surface, rle, color);
        } else {
#if defined(THORVG_AVX_VECTOR_SUPPORT)
            return avxRasterTranslucentRle(surface, rle, color);
#elif defined(THORVG_NEON_VECTOR_SUPPORT)
            return neonRasterTranslucentRle(surface, rle, color);
#else
            return cRasterTranslucentRle(surface, rle, color);
#endif
        }
    }
    return false;
}


/************************************************************************/
/* RLE Transformed RGBA Image                                           */
/************************************************************************/

static bool _transformedRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, uint32_t opacity)
{
    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterTexmapPolygon(surface, image, transform, nullptr, opacity, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTexmapPolygon(surface, image, transform, nullptr, opacity, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterTexmapPolygon(surface, image, transform, nullptr, opacity, surface->blender.lumaValue);
        }
    } else {
        return _rasterTexmapPolygon(surface, image, transform, nullptr, opacity, nullptr);
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
            auto sy = (uint32_t)(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst, ++cmp) {
                auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale), alpha);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = span->y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst, ++cmp) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), alpha);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
            auto sy = (uint32_t)(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto tmp = ALPHA_BLEND(_interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale), blendMethod(*cmp));
                    // TODO: (@Projectitis) Not sure about the src argument here. @hermet or @mmaciola to check
                    *dst = surface->blender.blend(tmp, *dst, _alpha(tmp), _ialpha(tmp), tmp);
                }
            } else {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = ALPHA_BLEND(_interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale), span->coverage);
                    auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                    *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
                }
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = span->y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto cmp = &surface->compositor->image.data[span->y * surface->compositor->image.stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = x * itransform->e11 + itransform->e13;
                    if ((uint32_t)sx >= image->w) continue;
                    auto tmp = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), blendMethod(*cmp));
                    *dst = surface->blender.blend(tmp, *dst, _alpha(tmp), _ialpha(tmp), tmp);
                }
            } else {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst, ++cmp) {
                    auto sx = x * itransform->e11 + itransform->e13;
                    if ((uint32_t)sx >= image->w) continue;
                    auto src = ALPHA_BLEND(_interpUpScaler(image->data, image->w, image->h, sx, sy), span->coverage);
                    auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                    *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
            auto sy = (uint32_t)(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst) {
                auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = _interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale);
                auto tmp = ALPHA_BLEND(src, alpha);
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = span->y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            auto alpha = _multiplyAlpha(span->coverage, opacity);
            for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                auto tmp = ALPHA_BLEND(src, alpha);
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
            auto sy = (uint32_t)(span->y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst) {
                    auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = _interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale);
                    *dst = surface->blender.blend(src, *dst, _alpha(src), _ialpha(src), src);
                }
            } else {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst) {
                    auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                    if (sx >= image->w) continue;
                    auto src = _interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale);
                    auto tmp = ALPHA_BLEND(src, span->coverage);
                    *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
                }
            }
        }
    //Center (Up-Scaled)
    } else {
        for (uint32_t i = 0; i < image->rle->size; ++i, ++span) {
            auto sy = span->y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            if (span->coverage == 255) {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst) {
                    auto sx = x * itransform->e11 + itransform->e13;
                    if ((uint32_t)sx >= image->w) continue;
                    auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                    *dst = surface->blender.blend(src, *dst, _alpha(src), _ialpha(src), src);
                }
            } else {
                for (uint32_t x = static_cast<uint32_t>(span->x); x < static_cast<uint32_t>(span->x) + span->len; ++x, ++dst) {
                    auto sx = x * itransform->e11 + itransform->e13;
                    if ((uint32_t)sx >= image->w) continue;
                    auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                    auto tmp = ALPHA_BLEND(src, span->coverage);
                    *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
                }
            }
        }
    }
    return true;
}


static bool _scaledRleRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, const SwBBox& region, uint32_t opacity)
{
    Matrix itransform;

    if (transform) {
        if (!mathInverse(transform, &itransform)) return false;
    } else mathIdentity(&itransform);

    auto halfScale = _halfScale(image->scale);

    if (_compositing(surface)) {
        if (opacity == 255) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedRleRGBAImage(surface, image, &itransform, region, halfScale, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedRleRGBAImage(surface, image, &itransform, region, halfScale, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterScaledMaskedRleRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.lumaValue);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, region, opacity, halfScale, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, region, opacity, halfScale, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterScaledMaskedTranslucentRleRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.lumaValue);
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
                *dst = surface->blender.blend(*img, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++img) {
                auto tmp = ALPHA_BLEND(*img, _multiplyAlpha(alpha, blendMethod(*cmp)));
                *dst = surface->blender.blend(*img, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
                *dst = surface->blender.blend(*img, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++img) {
                auto tmp = ALPHA_BLEND(*img, _multiplyAlpha(span->coverage, blendMethod(*cmp)));
                *dst = surface->blender.blend(*img, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
            *dst = surface->blender.blend(*img, *dst, _alpha(src), _ialpha(src), src);
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
                *dst = surface->blender.blend(*img, *dst, _alpha(*img), _ialpha(*img), *img);
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++img) {
                auto src = ALPHA_BLEND(*img, span->coverage);
                *dst = surface->blender.blend(*img, *dst, _alpha(src), _ialpha(src), src);
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
                return _rasterDirectMaskedRleRGBAImage(surface, image, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedRleRGBAImage(surface, image, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterDirectMaskedRleRGBAImage(surface, image, surface->blender.lumaValue);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDirectMaskedTranslucentRleRGBAImage(surface, image, opacity, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedTranslucentRleRGBAImage(surface, image, opacity, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterDirectMaskedTranslucentRleRGBAImage(surface, image, opacity, surface->blender.lumaValue);
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

static bool _transformedRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, const SwBBox& region, uint32_t opacity)
{
    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterTexmapPolygon(surface, image, transform, &region, opacity, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTexmapPolygon(surface, image, transform, &region, opacity, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterTexmapPolygon(surface, image, transform, &region, opacity, surface->blender.lumaValue);
        }
    } else {
        return _rasterTexmapPolygon(surface, image, transform, &region, opacity, nullptr);
    }
    return false;
}

static bool _transformedRGBAImageMesh(SwSurface* surface, const SwImage* image, const Polygon* triangles, const uint32_t count, const Matrix* transform, const SwBBox* region, uint32_t opacity)
{
    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterTexmapPolygonMesh(surface, image, triangles, count, transform, region, opacity, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterTexmapPolygonMesh(surface, image, triangles, count, transform, region, opacity, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterTexmapPolygonMesh(surface, image, triangles, count, transform, region, opacity, surface->blender.lumaValue);
        }
    } else {
        return _rasterTexmapPolygonMesh(surface, image, triangles, count, transform, region, opacity, nullptr);
    }
    return false;
}


/************************************************************************/
/*Scaled RGBA Image                                                     */
/************************************************************************/


static bool _rasterScaledMaskedTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale, uint32_t (*blendMethod)(uint32_t))
{
    TVGLOG("SW_ENGINE", "Scaled Masked Image");

    auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride + region.min.x);

    // Down-Scaled
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (auto y = region.min.y; y < region.max.y; ++y) {
            auto sy = (uint32_t)(y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = dbuffer;
            auto cmp = cbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
                auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto alpha = _multiplyAlpha(opacity, blendMethod(*cmp));
                auto src = _interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale);
                auto tmp = ALPHA_BLEND(src, alpha);
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
            dbuffer += surface->stride;
            cbuffer += surface->compositor->image.stride;
        }
    // Up-Scaled
    } else {
        for (auto y = region.min.y; y < region.max.y; ++y) {
            auto sy = y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto dst = dbuffer;
            auto cmp = cbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto alpha = _multiplyAlpha(opacity, blendMethod(*cmp));
                auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                auto tmp = ALPHA_BLEND(src, alpha);
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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

    auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride + region.min.x);

    // Down-Scaled
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (auto y = region.min.y; y < region.max.y; ++y) {
            auto sy = (uint32_t)(y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = dbuffer;
            auto cmp = cbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
                auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = _interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
            dbuffer += surface->stride;
            cbuffer += surface->compositor->image.stride;
        }
    // Up-Scaled
    } else {
        for (auto y = region.min.y; y < region.max.y; ++y) {
            auto sy = y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto dst = dbuffer;
            auto cmp = cbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst, ++cmp) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                auto tmp = ALPHA_BLEND(src, blendMethod(*cmp));
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
            dbuffer += surface->stride;
            cbuffer += surface->compositor->image.stride;
        }
    }
    return true;
}


static bool _rasterScaledTranslucentRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t opacity, uint32_t halfScale)
{
    auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);

    // Down-Scaled
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (auto y = region.min.y; y < region.max.y; ++y, dbuffer += surface->stride) {
            auto sy = (uint32_t)(y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = dbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
                auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = _interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale);
                auto tmp = ALPHA_BLEND(src, opacity);
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    // Up-Scaled
    } else {
        for (auto y = region.min.y; y < region.max.y; ++y, dbuffer += surface->stride) {
            auto sy = fabsf(y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = dbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                auto tmp = ALPHA_BLEND(src, opacity);
                *dst = surface->blender.blend(src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    }
    return true;
}


static bool _rasterScaledRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* itransform, const SwBBox& region, uint32_t halfScale)
{
    auto dbuffer = surface->buffer + (region.min.y * surface->stride + region.min.x);

    // Down-Scaled
    if (image->scale < DOWN_SCALE_TOLERANCE) {
        for (auto y = region.min.y; y < region.max.y; ++y, dbuffer += surface->stride) {
            auto sy = (uint32_t)(y * itransform->e22 + itransform->e23);
            if (sy >= image->h) continue;
            auto dst = dbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
                auto sx = (uint32_t)(x * itransform->e11 + itransform->e13);
                if (sx >= image->w) continue;
                auto src = _interpDownScaler(image->data, image->stride, image->w, image->h, sx, sy, halfScale);
                *dst = surface->blender.blend(src, *dst, _alpha(src), _ialpha(src), src);
            }
        }
    // Up-Scaled
    } else {
        for (auto y = region.min.y; y < region.max.y; ++y, dbuffer += surface->stride) {
            auto sy = y * itransform->e22 + itransform->e23;
            if ((uint32_t)sy >= image->h) continue;
            auto dst = dbuffer;
            for (auto x = region.min.x; x < region.max.x; ++x, ++dst) {
                auto sx = x * itransform->e11 + itransform->e13;
                if ((uint32_t)sx >= image->w) continue;
                auto src = _interpUpScaler(image->data, image->w, image->h, sx, sy);
                *dst = surface->blender.blend(src, *dst, _alpha(src), _ialpha(src), src);
            }
        }
    }
    return true;
}


static bool _scaledRGBAImage(SwSurface* surface, const SwImage* image, const Matrix* transform, const SwBBox& region, uint32_t opacity)
{
    Matrix itransform;

    if (transform) {
        if (!mathInverse(transform, &itransform)) return false;
    } else mathIdentity(&itransform);

    auto halfScale = _halfScale(image->scale);

    if (_compositing(surface)) {
        if (opacity == 255) {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedRGBAImage(surface, image, &itransform, region, halfScale, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedRGBAImage(surface, image, &itransform, region, halfScale, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterScaledMaskedRGBAImage(surface, image, &itransform, region, halfScale, surface->blender.lumaValue);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterScaledMaskedTranslucentRGBAImage(surface, image, &itransform, region, opacity, halfScale, surface->blender.lumaValue);
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
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x; //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
            *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
    auto cbuffer = surface->compositor->image.data + (region.min.y * surface->compositor->image.stride) + region.min.x; //compositor buffer

    for (uint32_t y = 0; y < h2; ++y) {
        auto dst = buffer;
        auto cmp = cbuffer;
        auto src = sbuffer;
        for (uint32_t x = 0; x < w2; ++x, ++dst, ++src, ++cmp) {
            auto tmp = ALPHA_BLEND(*src, _multiplyAlpha(opacity, blendMethod(*cmp)));
            *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
            *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
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
            *dst = surface->blender.blend(*src, *dst, _alpha(*src), _ialpha(*src), *src);
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
                return _rasterDirectMaskedRGBAImage(surface, image, region, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedRGBAImage(surface, image, region, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterDirectMaskedRGBAImage(surface, image, region, surface->blender.lumaValue);
            }
        } else {
            if (surface->compositor->method == CompositeMethod::AlphaMask) {
                return _rasterDirectMaskedTranslucentRGBAImage(surface, image, region, opacity, _alpha);
            } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
                return _rasterDirectMaskedTranslucentRGBAImage(surface, image, region, opacity, _ialpha);
            } else if (surface->compositor->method == CompositeMethod::LumaMask) {
                return _rasterDirectMaskedTranslucentRGBAImage(surface, image, region, opacity, surface->blender.lumaValue);
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
        else return _transformedRleRGBAImage(surface, image, transform, opacity);
    //Whole Image
    } else {
        if (image->direct) return _directRGBAImage(surface, image, region, opacity);
        else if (image->scaled) return _scaledRGBAImage(surface, image, transform, region, opacity);
        else return _transformedRGBAImage(surface, image, transform, region, opacity);
    }
}


/************************************************************************/
/* Rect Linear Gradient                                                 */
/************************************************************************/

static bool _rasterLinearGradientMaskedRect(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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
            *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    auto sbuffer = static_cast<uint32_t*>(alloca(w * sizeof(uint32_t)));
    if (!sbuffer) return false;

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = buffer;
        fillFetchLinear(fill, sbuffer, region.min.y + y, region.min.x, w);
        for (uint32_t x = 0; x < w; ++x, ++dst) {
            *dst = surface->blender.blend(sbuffer[x], *dst, _alpha(sbuffer[x]), _ialpha(sbuffer[x]), sbuffer[x]);
        }
        buffer += surface->stride;
    }
    return true;
}


static bool _rasterSolidLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);

    for (uint32_t y = 0; y < h; ++y) {
        fillFetchLinear(fill, buffer + y * surface->stride, region.min.y + y, region.min.x, w);
    }
    return true;
}


static bool _rasterLinearGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterLinearGradientMaskedRect(surface, region, fill, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterLinearGradientMaskedRect(surface, region, fill, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterLinearGradientMaskedRect(surface, region, fill, surface->blender.lumaValue);
        }
    } else {
        if (fill->translucent) return _rasterTranslucentLinearGradientRect(surface, region, fill);
        else _rasterSolidLinearGradientRect(surface, region, fill);
    }
    return false;
}


/************************************************************************/
/* Rle Linear Gradient                                                  */
/************************************************************************/

static bool _rasterLinearGradientMaskedRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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
                *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        } else {
            auto ialpha = 255 - span->coverage;
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++src) {
                auto tmp = ALPHA_BLEND(*src, blendMethod(*cmp));
                tmp = ALPHA_BLEND(tmp, span->coverage) + ALPHA_BLEND(*dst, ialpha);
                *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    }
    return true;
}


static bool _rasterTranslucentLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (fill->linear.len < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        fillFetchLinear(fill, buffer, span->y, span->x, span->len);
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                *dst = surface->blender.blend(buffer[x], *dst, _alpha(buffer[x]), _ialpha(buffer[x]), buffer[x]);
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                auto tmp = ALPHA_BLEND(buffer[x], span->coverage);
                *dst = surface->blender.blend(buffer[x], *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    }
    return true;
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
            auto dst = &surface->buffer[span->y * surface->stride + span->x];
            for (uint32_t x = 0; x < span->len; ++x) {
                dst[x] = INTERPOLATE(span->coverage, buf[x], dst[x]);
            }
        }
    }
    return true;
}


static bool _rasterLinearGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (!rle) return false;

    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterLinearGradientMaskedRle(surface, rle, fill, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterLinearGradientMaskedRle(surface, rle, fill, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterLinearGradientMaskedRle(surface, rle, fill, surface->blender.lumaValue);
        }
    } else {
        if (fill->translucent) return _rasterTranslucentLinearGradientRle(surface, rle, fill);
        else return _rasterSolidLinearGradientRle(surface, rle, fill);
    }
    return false;
}


/************************************************************************/
/* Rect Radial Gradient                                                 */
/************************************************************************/

static bool _rasterRadialGradientMaskedRect(SwSurface* surface, const SwBBox& region, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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
            *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
        }
        buffer += surface->stride;
        cbuffer += surface->stride;
    }
    return true;
}


static bool _rasterTranslucentRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto buffer = surface->buffer + (region.min.y * surface->stride) + region.min.x;
    auto h = static_cast<uint32_t>(region.max.y - region.min.y);
    auto w = static_cast<uint32_t>(region.max.x - region.min.x);

    auto sbuffer = static_cast<uint32_t*>(alloca(w * sizeof(uint32_t)));
    if (!sbuffer) return false;

    for (uint32_t y = 0; y < h; ++y) {
        auto dst = buffer;
        fillFetchRadial(fill, sbuffer, region.min.y + y, region.min.x, w);
        for (uint32_t x = 0; x < w; ++x, ++dst) {
            *dst = surface->blender.blend(sbuffer[x], *dst, _alpha(sbuffer[x]), _ialpha(sbuffer[x]), sbuffer[x]);
        }
        buffer += surface->stride;
    }
    return true;
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


static bool _rasterRadialGradientRect(SwSurface* surface, const SwBBox& region, const SwFill* fill)
{
    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterRadialGradientMaskedRect(surface, region, fill, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterRadialGradientMaskedRect(surface, region, fill, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterRadialGradientMaskedRect(surface, region, fill, surface->blender.lumaValue);
        }
    } else {
        if (fill->translucent) return _rasterTranslucentRadialGradientRect(surface, region, fill);
        else return _rasterSolidRadialGradientRect(surface, region, fill);
    }
    return false;
}


/************************************************************************/
/* RLE Radial Gradient                                                  */
/************************************************************************/

static bool _rasterRadialGradientMaskedRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill, uint32_t (*blendMethod)(uint32_t))
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
                *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        } else {
            for (uint32_t x = 0; x < span->len; ++x, ++dst, ++cmp, ++src) {
                auto tmp = INTERPOLATE(span->coverage, ALPHA_BLEND(*src, blendMethod(*cmp)), *dst);
                *dst = surface->blender.blend(*src, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    }
    return true;
}


static bool _rasterTranslucentRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (fill->radial.a < FLT_EPSILON) return false;

    auto span = rle->spans;
    auto buffer = static_cast<uint32_t*>(alloca(surface->w * sizeof(uint32_t)));
    if (!buffer) return false;

    for (uint32_t i = 0; i < rle->size; ++i, ++span) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        fillFetchRadial(fill, buffer, span->y, span->x, span->len);
        if (span->coverage == 255) {
            for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                *dst = surface->blender.blend(buffer[x], *dst, _alpha(buffer[x]), _ialpha(buffer[x]), buffer[x]);
            }
        } else {
           for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                auto tmp = ALPHA_BLEND(buffer[x], span->coverage);
                *dst = surface->blender.blend(buffer[x], *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    }
    return true;
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
            for (uint32_t x = 0; x < span->len; ++x, ++dst) {
                auto tmp = ALPHA_BLEND(buf[x], span->coverage) + ALPHA_BLEND(*dst, ialpha);
                // TODO: (@Projectitis) Is this correct @hermet @mmaciola
                *dst = surface->blender.blend(tmp, *dst, _alpha(tmp), _ialpha(tmp), tmp);
            }
        }
    }
    return true;
}


static bool _rasterRadialGradientRle(SwSurface* surface, const SwRleData* rle, const SwFill* fill)
{
    if (!rle) return false;

    if (_compositing(surface)) {
        if (surface->compositor->method == CompositeMethod::AlphaMask) {
            return _rasterRadialGradientMaskedRle(surface, rle, fill, _alpha);
        } else if (surface->compositor->method == CompositeMethod::InvAlphaMask) {
            return _rasterRadialGradientMaskedRle(surface, rle, fill, _ialpha);
        } else if (surface->compositor->method == CompositeMethod::LumaMask) {
            return _rasterRadialGradientMaskedRle(surface, rle, fill, surface->blender.lumaValue);
        }
    } else {
        if (fill->translucent) _rasterTranslucentRadialGradientRle(surface, rle, fill);
        else return _rasterSolidRadialGradientRle(surface, rle, fill);
    }
    return false;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

uint32_t blendNormal(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Normal: A + B
    return src_blended + ALPHA_BLEND(dst, ialpha);
}

uint32_t blendScreen(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Screen: 1 - (1-A)*(1-B) = A + B - A*B
    return (LIMIT_BYTE(((src_blended >> 16) & 0xff) - ((((src_blended >> 16) & 0xff) * ((dst >> 16) & 0xff)) >> 8) + ((dst >> 16) & 0xff)) << 16)
            | (LIMIT_BYTE(((src_blended >> 8) & 0xff) - ((((src_blended >> 8) & 0xff) * ((dst >> 8) & 0xff)) >> 8) + ((dst >> 8) & 0xff)) << 8)
            | LIMIT_BYTE((src_blended & 0xff) - (((src_blended & 0xff) * (dst & 0xff)) >> 8) + (dst & 0xff));
}

uint32_t blendMultiply(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Multiply: A*B
    return (LIMIT_BYTE(((dst >> 16) & 0xff) * (((src_blended >> 16) & 0xff) + ialpha) >> 8) << 16)
            | (LIMIT_BYTE(((dst >> 8) & 0xff) * (((src_blended >> 8) & 0xff) + ialpha) >> 8) << 8)
            | LIMIT_BYTE((dst & 0xff) * ((src_blended & 0xff) + ialpha) >> 8);
}

uint32_t blendOverlay(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Overlay: B<=0.5: 2*A*B or B>0.5: 1-2*(1-A)*(1-B)
    uint32_t result = ((dst & 0xff0000) <= 0x800000)
                ? ((2 * ((src >> 16) & 0xff) * ((dst >> 16) & 0xff)) >> 8) << 16 // multiply
                : (0xff - (2*(0xff - ((src >> 16) & 0xff))*(0xff - ((dst >> 16) & 0xff)) >> 8)) << 16; // screen
    result |= ((dst & 0x00ff00) <= 0x008000)
                ? ((2 * ((src >> 8) & 0xff) * ((dst >> 8) & 0xff)) >> 8) << 8 // multiply
                : (0xff - (2*(0xff - ((src >> 8) & 0xff))*(0xff - ((dst >> 8) & 0xff)) >> 8)) << 8; // screen
    result |= ((dst & 0x0000ff) <= 0x000080)
                ? ((2 * (src & 0xff) * (dst & 0xff)) >> 8) // multiply
                : (0xff - (2*(0xff - (src & 0xff))*(0xff - (dst & 0xff)) >> 8)); // screen
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendDarken(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Darken: min(A, B)
    uint32_t result = min(src & 0xff0000, dst & 0xff0000)
            | min(src & 0x00ff00, dst & 0x00ff00)
            | min(src & 0x0000ff, dst & 0x0000ff);
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendLighten(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Lighten: max(A, B)
    uint32_t result = max(src & 0xff0000, dst & 0xff0000)
            | max(src & 0x00ff00, dst & 0x00ff00)
            | max(src & 0x0000ff, dst & 0x0000ff);
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendColorDodge(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // ColorDodge: B / (1-A)
    uint32_t src_inverted = 0xffffffff - src;
    uint32_t result = ((src_inverted & 0xff0000) == 0) ? (dst & 0xff0000) : (LIMIT_BYTE(((dst >> 8) & 0xff00) / ((src_inverted >> 16) & 0xff)) << 16);
    result |= ((src_inverted & 0x00ff00) == 0) ? (dst & 0x00ff00) : (LIMIT_BYTE((dst & 0xff00) / ((src_inverted >> 8) & 0xff)) << 8);
    result |= ((src_inverted & 0x0000ff) == 0) ? (dst & 0x0000ff) : LIMIT_BYTE(((dst & 0xff) << 8) / (src_inverted & 0xff));
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendColorBurn(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // ColorBurn: 1 - (1-B) / A
    uint32_t dst_inverted = 0xffffffff - dst;
    uint32_t result = (((src >> 16) & 0xff) == 0) ? (dst & 0xff0000) : (LIMIT_BYTE_LOW(0xff - ((((dst_inverted >> 16) & 0xff) << 8) / ((src >> 16) & 0xff))) << 16);
    result |= (((src >> 8) & 0xff) == 0) ? (dst & 0xff00) : (LIMIT_BYTE_LOW(0xff - ((((dst_inverted >> 8) & 0xff) << 8) / ((src >> 8) & 0xff))) << 8);
    result |= ((src & 0xff) == 0) ? (dst & 0xff) : (LIMIT_BYTE_LOW(0xff - (((dst_inverted & 0xff) << 8) / (src & 0xff))));
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendHardLight(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // HardLight: Layers-inverted overlay
    uint32_t result = ((src & 0xff0000) <= 0x800000)
                ? ((2 * ((dst >> 16) & 0xff) * ((src >> 16) & 0xff)) >> 8) << 16 // multiply
                : (0xff - (2*(0xff - ((dst >> 16) & 0xff))*(0xff - ((src >> 16) & 0xff)) >> 8)) << 16; // screen
    result |= ((src & 0x00ff00) <= 0x008000)
                ? ((2 * ((dst >> 8) & 0xff) * ((src >> 8) & 0xff)) >> 8) << 8 // multiply
                : (0xff - (2*(0xff - ((dst >> 8) & 0xff))*(0xff - ((src >> 8) & 0xff)) >> 8)) << 8; // screen
    result |= ((src & 0x0000ff) <= 0x000080)
                ? ((2 * (dst & 0xff) * (src & 0xff)) >> 8) // multiply
                : (0xff - (2*(0xff - (dst & 0xff))*(0xff - (src & 0xff)) >> 8)); // screen
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendSoftLight(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // SoftLight: A<=0.5: (2*A-1)*(B-B^2)+B or A>0.5: (2*A-1)*(sqrt(B)-B)+B
    uint32_t result = 0x000000;
    for (uint8_t i = 0; i <= 16; i += 8) {
        uint8_t src_b = (src >> i) & 0xff;
        uint8_t dst_b = (dst >> i) & 0xff;
        uint8_t soft = ((src_b < 0x80) ? ((2 * dst_b * src_b >> 8) + (dst_b * dst_b * (0xff - 2 * src_b) >> 16)) : (sqrt((float)dst_b/255) * (2 * src_b - 0xff) + (2 * dst_b * (0xff - src_b) >> 8)));
        result |= soft << i;
    }
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendDifference(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Difference: |A - B|
    uint32_t result = ABS_DIFFERENCE(src & 0xff0000, dst & 0xff0000)
        | ABS_DIFFERENCE(src & 0x00ff00, dst & 0x00ff00)
        | ABS_DIFFERENCE(src & 0x0000ff, dst & 0x0000ff);
    return BLEND_COLORS(result, dst, alpha, ialpha);
}

uint32_t blendExclusion(uint32_t src, uint32_t dst, uint8_t alpha, uint8_t ialpha, uint32_t src_blended)
{
    // Exclusion: 0.5 - 2*(A-0.5)*(B-0.5) = A + B - 2AB
    return (LIMIT_BYTE(((src_blended >> 16) & 0xff) - ((((src_blended >> 16) & 0xff) * ((dst >> 16) & 0xff) * 2) >> 8) + ((dst >> 16) & 0xff)) << 16)
            | (LIMIT_BYTE(((src_blended >> 8) & 0xff) - ((((src_blended >> 8) & 0xff) * ((dst >> 8) & 0xff) * 2) >> 8) + ((dst >> 8) & 0xff)) << 8)
            | LIMIT_BYTE((src_blended & 0xff) - (((src_blended & 0xff) * (dst & 0xff) * 2) >> 8) + (dst & 0xff));
}

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
        surface->blender.lumaValue = _abgrLumaValue;
    } else if (surface->cs == SwCanvas::ARGB8888 || surface->cs == SwCanvas::ARGB8888_STRAIGHT) {
        surface->blender.join = _argbJoin;
        surface->blender.lumaValue = _argbLumaValue;
    } else {
        //What Color Space ???
        return false;
    }
    return true;
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
    //OPTIMIZE_ME: +SIMD
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


bool rasterGradientShape(SwSurface* surface, SwShape* shape, unsigned id)
{
    if (!shape->fill) return false;

    if (shape->fastTrack) {
        if (id == TVG_CLASS_ID_LINEAR) return _rasterLinearGradientRect(surface, shape->bbox, shape->fill);
        else if (id == TVG_CLASS_ID_RADIAL)return _rasterRadialGradientRect(surface, shape->bbox, shape->fill);
    } else {
        if (id == TVG_CLASS_ID_LINEAR) return _rasterLinearGradientRle(surface, shape->rle, shape->fill);
        else if (id == TVG_CLASS_ID_RADIAL) return _rasterRadialGradientRle(surface, shape->rle, shape->fill);
    }
    return false;
}


bool rasterGradientStroke(SwSurface* surface, SwShape* shape, unsigned id)
{
    if (!shape->stroke || !shape->stroke->fill || !shape->strokeRle) return false;

    if (id == TVG_CLASS_ID_LINEAR) return _rasterLinearGradientRle(surface, shape->strokeRle, shape->stroke->fill);
    else if (id == TVG_CLASS_ID_RADIAL) return _rasterRadialGradientRle(surface, shape->strokeRle, shape->stroke->fill);

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

    if (shape->fastTrack) return _rasterRect(surface, shape->bbox, color, a);
    else return _rasterRle(surface, shape->rle, color, a);
}


bool rasterStroke(SwSurface* surface, SwShape* shape, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (a < 255) {
        r = _multiplyAlpha(r, a);
        g = _multiplyAlpha(g, a);
        b = _multiplyAlpha(b, a);
    }

    auto color = surface->blender.join(r, g, b, a);

    return _rasterRle(surface, shape->strokeRle, color, a);
}


bool rasterImage(SwSurface* surface, SwImage* image, const Matrix* transform, const SwBBox& bbox, uint32_t opacity)
{
    //Verify Boundary
    if (bbox.max.x < 0 || bbox.max.y < 0 || bbox.min.x >= static_cast<SwCoord>(surface->w) || bbox.min.y >= static_cast<SwCoord>(surface->h)) return false;

    //TOOD: switch (image->format)
    //TODO: case: _rasterRGBImage()
    //TODO: case: _rasterGrayscaleImage()
    //TODO: case: _rasterAlphaImage()
    return _rasterRGBAImage(surface, image, transform, bbox, opacity);
}

bool rasterImageMesh(SwSurface* surface, SwImage* image, const Polygon* triangles, const uint32_t count, const Matrix* transform, const SwBBox& bbox, uint32_t opacity)
{
    //Verify Boundary
    if (bbox.max.x < 0 || bbox.max.y < 0 || bbox.min.x >= static_cast<SwCoord>(surface->w) || bbox.min.y >= static_cast<SwCoord>(surface->h)) return false;

    //TOOD: switch (image->format)
    //TODO: case: _rasterRGBImageMesh()
    //TODO: case: _rasterGrayscaleImageMesh()
    //TODO: case: _rasterAlphaImageMesh()
    return _transformedRGBAImageMesh(surface, image, triangles, count, transform, &bbox, opacity);
}