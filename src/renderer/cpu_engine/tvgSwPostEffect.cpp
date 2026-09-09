/*
 * Copyright (c) 2024 - 2026 ThorVG project. All rights reserved.

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
#include "tvgSwCommon.h"

/************************************************************************/
/* Gaussian Blur Implementation                                         */
/************************************************************************/

struct SwGaussianBlur
{
    static constexpr int MAX_LEVEL = 3;
    int level;
    int kernel[MAX_LEVEL];
    int extends;
    int downsample;
    int reducedKernel[MAX_LEVEL];
};


static inline int _gaussianEdgeWrap(int end, int idx)
{
    auto r = idx % (end + 1);
    return (r < 0) ? (end + 1) + r : r;
}


static inline int _gaussianEdgeExtend(int end, int idx)
{
    if (idx < 0) return 0;
    else if (idx > end) return end;
    return idx;
}


template<int border>
static inline int _gaussianRemap(int end, int idx)
{
    if (border == 1) return _gaussianEdgeWrap(end, idx);
    return _gaussianEdgeExtend(end, idx);
}


//TODO: SIMD OPTIMIZATION?
template<int border = 0>
static void _gaussianFilter(uint8_t* dst, uint8_t* src, int32_t stride, int32_t w, int32_t h, const RenderRegion& bbox, int32_t dimension, bool flipped)
{
    if (flipped) {
        src += (bbox.min.x * stride + bbox.min.y) << 2;
        dst += (bbox.min.x * stride + bbox.min.y) << 2;
    } else {
        src += (bbox.min.y * stride + bbox.min.x) << 2;
        dst += (bbox.min.y * stride + bbox.min.x) << 2;
    }

    auto iarr = 1.0f / (dimension + dimension + 1);
    auto end = w - 1;

    #pragma omp parallel for
    for (int y = 0; y < h; ++y) {
        auto p = y * stride;
        auto i = p * 4;                 //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc[4] = {0, 0, 0, 0};      //sliding accumulator

        //initial accumulation
        for (int x = l; x < r; ++x) {
            auto id = (_gaussianRemap<border>(end, x) + p) * 4;
            acc[0] += src[id++];
            acc[1] += src[id++];
            acc[2] += src[id++];
            acc[3] += src[id];
        }
        //perform filtering
        for (int x = 0; x < w; ++x, ++r, ++l) {
            auto rid = (_gaussianRemap<border>(end, r) + p) * 4;
            auto lid = (_gaussianRemap<border>(end, l) + p) * 4;
            acc[0] += src[rid++] - src[lid++];
            acc[1] += src[rid++] - src[lid++];
            acc[2] += src[rid++] - src[lid++];
            acc[3] += src[rid] - src[lid];
            //ignored rounding for the performance. It should be originally: acc[idx] * iarr + 0.5f
            dst[i++] = static_cast<uint8_t>(acc[0] * iarr);
            dst[i++] = static_cast<uint8_t>(acc[1] * iarr);
            dst[i++] = static_cast<uint8_t>(acc[2] * iarr);
            dst[i++] = static_cast<uint8_t>(acc[3] * iarr);
        }
    }
}


//Fast Almost-Gaussian Filtering Method by Peter Kovesi
static int _gaussianInit(SwGaussianBlur* data, float sigma, int quality)
{
    const auto MAX_LEVEL = SwGaussianBlur::MAX_LEVEL;

    if (tvg::zero(sigma)) return 0;

    data->level = int(SwGaussianBlur::MAX_LEVEL * ((quality - 1) * 0.01f)) + 1;

    //compute box kernel sizes
    auto wl = (int) sqrt((12 * sigma / MAX_LEVEL) + 1);
    if (wl % 2 == 0) --wl;
    auto wu = wl + 2;
    auto mi = (12 * sigma - MAX_LEVEL * wl * wl - 4 * MAX_LEVEL * wl - 3 * MAX_LEVEL) / (-4 * wl - 4);
    auto m = int(mi + 0.5f);
    auto extends = 0;

    for (int i = 0; i < data->level; i++) {
        data->kernel[i] = ((i < m ? wl : wu) - 1) / 2;
        extends += data->kernel[i];
    }

    return extends;
}


bool effectGaussianBlurRegion(RenderEffectGaussianBlur* params)
{
    //region expansion for feathering
    auto& bbox = params->extend;
    auto extra = static_cast<SwGaussianBlur*>(params->rd)->extends;

    if (params->direction != 2) {
        bbox.min.x = -extra;
        bbox.max.x = extra;
    }
    if (params->direction != 1) {
        bbox.min.y = -extra;
        bbox.max.y = extra;
    }

    return true;
}


void effectGaussianBlurUpdate(RenderEffectGaussianBlur* params, const Matrix& transform)
{
    if (!params->rd) params->rd = tvg::malloc<SwGaussianBlur>(sizeof(SwGaussianBlur));
    auto rd = static_cast<SwGaussianBlur*>(params->rd);

    //compute box kernel sizes
    auto scaleSquared = transform.e11 * transform.e11 + transform.e12 * transform.e12;
    auto variance = params->sigma * params->sigma * scaleSquared;
    rd->extends = _gaussianInit(rd, variance, params->quality);
    rd->downsample = 1;
    if (params->direction == 0 && variance >= 256.0f) {
        rd->downsample = (variance >= 1024.0f) ? 4 : 2;
        SwGaussianBlur reduced;
        auto extra = _gaussianInit(&reduced, variance / (rd->downsample * rd->downsample), params->quality);
        for (int i = 0; i < rd->level; ++i) rd->reducedKernel[i] = reduced.kernel[i];
        //Include the resampling footprint as well as the reduced box support.
        rd->extends = std::max(rd->extends, (extra + 1) * rd->downsample);
    }

    params->valid = (rd->extends > 0);
}


template<int sample>
static void _gaussianDownsample(uint32_t* dst, const uint32_t* src, int stride, int w, int h, int reducedStride)
{
    auto rw = (w + sample - 1) / sample;
    auto rh = (h + sample - 1) / sample;
    for (int y = 0; y < rh; ++y) {
        for (int x = 0; x < rw; ++x) {
            uint32_t acc[4] = {};
            for (int yy = 0; yy < sample; ++yy) {
                auto row = src + std::min(y * sample + yy, h - 1) * stride;
                for (int xx = 0; xx < sample; ++xx) {
                    auto pixel = row[std::min(x * sample + xx, w - 1)];
                    for (int c = 0; c < 4; ++c) acc[c] += (pixel >> (c * 8)) & 255;
                }
            }
            uint32_t pixel = 0;
            auto count = sample * sample;
            for (int c = 0; c < 4; ++c) pixel |= ((acc[c] + count / 2) / count) << (c * 8);
            dst[y * reducedStride + x] = pixel;
        }
    }
}


template<int sample>
static void _gaussianUpsample(uint32_t* dst, const uint32_t* src, int stride, int w, int h, int reducedStride)
{
    auto rw = (w + sample - 1) / sample;
    auto rh = (h + sample - 1) / sample;
    auto unit = 2 * sample;
    auto divisor = unit * unit;
    for (int y = 0; y < h; ++y) {
        //Map pixel centers to the uniform, edge-extended downsampling grid.
        auto sy = std::max(0, 2 * y + 1 - sample);
        auto y0 = std::min(sy / unit, rh - 1);
        auto y1 = std::min(y0 + 1, rh - 1);
        auto fy = sy % unit;
        for (int x = 0; x < w; ++x) {
            auto sx = std::max(0, 2 * x + 1 - sample);
            auto x0 = std::min(sx / unit, rw - 1);
            auto x1 = std::min(x0 + 1, rw - 1);
            auto fx = sx % unit;
            auto a = src[y0 * reducedStride + x0];
            auto b = src[y0 * reducedStride + x1];
            auto c = src[y1 * reducedStride + x0];
            auto d = src[y1 * reducedStride + x1];
            uint32_t pixel = 0;
            for (int channel = 0; channel < 4; ++channel) {
                auto shift = channel * 8;
                auto top = ((a >> shift) & 255) * (unit - fx) + ((b >> shift) & 255) * fx;
                auto bottom = ((c >> shift) & 255) * (unit - fx) + ((d >> shift) & 255) * fx;
                pixel |= ((top * (unit - fy) + bottom * fy + divisor / 2) / divisor) << shift;
            }
            dst[y * stride + x] = pixel;
        }
    }
}


bool effectGaussianBlur(SwCompositor* cmp, SwSurface* surface, const RenderEffectGaussianBlur* params)
{
    auto& buffer = surface->compositor->image;
    auto data = static_cast<SwGaussianBlur*>(params->rd);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);
    auto stride = cmp->image.stride;
    auto front = cmp->image.buf32;
    auto back = buffer.buf32;
    auto swapped = false;

    if (data->downsample > 1 && w >= 4 && h >= 4) {
        auto sample = data->downsample;
        auto rw = (w + sample - 1) / sample;
        auto rh = (h + sample - 1) / sample;
        auto reducedStride = std::max(rw, rh);
        auto size = size_t(reducedStride) * reducedStride;
        //Keep both reduced ping-pong images in the existing scratch surface.
        if (2 * size <= size_t(buffer.stride) * buffer.h) {
            auto a = back;
            auto b = back + size;
            auto origin = front + bbox.min.y * stride + bbox.min.x;
            RenderRegion reduced = {{0, 0}, {rw, rh}};
            if (sample == 2) _gaussianDownsample<2>(a, origin, stride, w, h, reducedStride);
            else _gaussianDownsample<4>(a, origin, stride, w, h, reducedStride);
            for (int i = 0; i < data->level; ++i) {
                _gaussianFilter(reinterpret_cast<uint8_t*>(b), reinterpret_cast<uint8_t*>(a), reducedStride, rw, rh, reduced, data->reducedKernel[i], false);
                std::swap(a, b);
            }
            rasterXYFlip(a, b, reducedStride, rw, rh, reduced, false);
            std::swap(a, b);
            for (int i = 0; i < data->level; ++i) {
                _gaussianFilter(reinterpret_cast<uint8_t*>(b), reinterpret_cast<uint8_t*>(a), reducedStride, rh, rw, reduced, data->reducedKernel[i], true);
                std::swap(a, b);
            }
            rasterXYFlip(a, b, reducedStride, rh, rw, reduced, true);
            if (sample == 2) _gaussianUpsample<2>(origin, b, stride, w, h, reducedStride);
            else _gaussianUpsample<4>(origin, b, stride, w, h, reducedStride);
            return true;
        }
    }

    TVGLOG("SW_ENGINE", "GaussianFilter region(%d, %d, %d, %d) params(%f %d %d), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->sigma, params->direction, params->border, data->level);

    /* It is best to take advantage of the Gaussian blur’s separable property
       by dividing the process into two passes. horizontal and vertical.
       We can expect fewer calculations. */

    //horizontal
    if (params->direction != 2) {
        for (int i = 0; i < data->level; ++i) {
            _gaussianFilter(reinterpret_cast<uint8_t*>(back), reinterpret_cast<uint8_t*>(front), stride, w, h, bbox, data->kernel[i], false);
            std::swap(front, back);
            swapped = !swapped;
        }
    }

    //vertical. x/y flipping and horionztal access is pretty compatible with the memory architecture.
    if (params->direction != 1) {
        rasterXYFlip(front, back, stride, w, h, bbox, false);
        std::swap(front, back);

        for (int i = 0; i < data->level; ++i) {
            _gaussianFilter(reinterpret_cast<uint8_t*>(back), reinterpret_cast<uint8_t*>(front), stride, h, w, bbox, data->kernel[i], true);
            std::swap(front, back);
            swapped = !swapped;
        }

        rasterXYFlip(front, back, stride, h, w, bbox, true);
        std::swap(front, back);
    }

    if (swapped) std::swap(cmp->image.buf8, buffer.buf8);

    return true;
}

/************************************************************************/
/* Drop Shadow Implementation                                           */
/************************************************************************/

struct SwDropShadow : SwGaussianBlur
{
    SwPoint offset;
};


//TODO: SIMD OPTIMIZATION?
static void _dropShadowFilter(uint32_t* dst, uint32_t* src, int stride, int w, int h, const RenderRegion& bbox, int32_t dimension, uint32_t color, bool flipped)
{
    if (flipped) {
        src += (bbox.min.x * stride + bbox.min.y);
        dst += (bbox.min.x * stride + bbox.min.y);
    } else {
        src += (bbox.min.y * stride + bbox.min.x);
        dst += (bbox.min.y * stride + bbox.min.x);
    }
    auto iarr = 1.0f / (dimension + dimension + 1);
    auto end = w - 1;

    #pragma omp parallel for
    for (int y = 0; y < h; ++y) {
        auto p = y * stride;
        auto i = p;                     //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc = 0;                    //sliding accumulator

        //initial accumulation
        for (int x = l; x < r; ++x) {
            auto id = _gaussianEdgeExtend(end, x) + p;
            acc += A(src[id]);
        }
        //perform filtering
        for (int x = 0; x < w; ++x, ++r, ++l) {
            auto rid = _gaussianEdgeExtend(end, r) + p;
            auto lid = _gaussianEdgeExtend(end, l) + p;
            acc += A(src[rid]) - A(src[lid]);
            //ignored rounding for the performance. It should be originally: acc * iarr
            dst[i++] = ALPHA_BLEND(color, static_cast<uint8_t>(acc * iarr));
        }
    }
}

static void _shift(uint32_t** dst, uint32_t** src, int dstride, int sstride, int wmax, int hmax, const RenderRegion& bbox, SwPoint offset, SwSize& size)
{
    size.w = bbox.max.x - bbox.min.x;
    size.h = bbox.max.y - bbox.min.y;

    //shift
    if (offset.x < 0) {
        *src -= offset.x;
        size.w += offset.x;
    } else {
        *dst += offset.x;
        size.w -= offset.x;
    }

    if (offset.y < 0) {
        *src -= (offset.y * sstride);
        size.h += offset.y;
    } else {
        *dst += (offset.y * dstride);
        size.h -= offset.y;
    }
}


static void _dropShadowNoFilter(uint32_t* dst, uint32_t* src, int dstride, int sstride, int dw, int dh, const RenderRegion& bbox, const SwPoint& offset, uint32_t color, uint8_t opacity, bool direct)
{
    src += (bbox.min.y * sstride + bbox.min.x);
    dst += (bbox.min.y * dstride + bbox.min.x);

    SwSize size;
    _shift(&dst, &src, dstride, sstride, dw, dh, bbox, offset, size);

    for (auto y = 0; y < size.h; ++y) {
        auto s2 = src;
        auto d2 = dst;
        for (int x = 0; x < size.w; ++x, ++d2, ++s2) {
            auto a = MULTIPLY(opacity, A(*s2));
            if (!direct || a == 255) *d2 = ALPHA_BLEND(color, a);
            else *d2 = INTERPOLATE(color, *d2, a);
        }
        src += sstride;
        dst += dstride;
    }
}


static void _dropShadowNoFilter(SwImage* dimg, SwImage* simg, const RenderRegion& bbox, const SwPoint& offset, uint32_t color)
{
    int dstride = dimg->stride;
    int sstride = simg->stride;

    //shadow image
    _dropShadowNoFilter(dimg->buf32, simg->buf32, dstride, sstride, dimg->w, dimg->h, bbox, offset, color, 255, false);

    //original image
    auto src = simg->buf32 + (bbox.min.y * sstride + bbox.min.x);
    auto dst = dimg->buf32 + (bbox.min.y * dstride + bbox.min.x);

    for (auto y = 0; y < (bbox.max.y - bbox.min.y); ++y) {
        auto s = src;
        auto d = dst;
        for (int x = 0; x < (bbox.max.x - bbox.min.x); ++x, ++d, ++s) {
            *d = *s + ALPHA_BLEND(*d, IA(*s));
        }
        src += sstride;
        dst += dstride;
    }
}


static void _dropShadowShift(uint32_t* dst, uint32_t* src, int dstride, int sstride, int dw, int dh, const RenderRegion& bbox, const SwPoint& offset, uint8_t opacity, bool direct)
{
    src += (bbox.min.y * sstride + bbox.min.x);
    dst += (bbox.min.y * dstride + bbox.min.x);

    SwSize size;
    _shift(&dst, &src, dstride, sstride, dw, dh, bbox, offset, size);

    for (auto y = 0; y < size.h; ++y) {
        if (direct) rasterTranslucentPixel32(dst, src, size.w, opacity);
        else rasterPixel32(dst, src, size.w, opacity);
        src += sstride;
        dst += dstride;
    }
}


bool effectDropShadowRegion(RenderEffectDropShadow* params)
{
    //region expansion for feathering
    auto& bbox = params->extend;
    auto& offset = static_cast<SwDropShadow*>(params->rd)->offset;
    auto extra = static_cast<SwDropShadow*>(params->rd)->extends;

    bbox.min = {-extra, -extra};
    bbox.max = {extra, extra};

    if (offset.x < 0) bbox.min.x += (int32_t) offset.x;
    else bbox.max.x += offset.x;

    if (offset.y < 0) bbox.min.y += (int32_t) offset.y;
    else bbox.max.y += offset.y;

    return true;
}


void effectDropShadowUpdate(RenderEffectDropShadow* params, const Matrix& transform)
{
    Point offset;
    if (!params->update(transform, offset)) return;

    if (!params->rd) params->rd = tvg::malloc<SwDropShadow>(sizeof(SwDropShadow));
    auto rd = static_cast<SwDropShadow*>(params->rd);

    // The sum of the box radii covers the full blur support.
    auto scaleSquared = transform.e11 * transform.e11 + transform.e12 * transform.e12;
    rd->extends = _gaussianInit(rd, params->sigma * params->sigma * scaleSquared, params->quality);

    rd->offset = {(int32_t)offset.x, (int32_t)offset.y};
}


//A quite same integration with effectGaussianBlur(). See it for detailed comments.
//surface[0]: the original image, to overlay it into the filtered image.
//surface[1]: temporary buffer for generating the filtered image.
bool effectDropShadow(SwCompositor* cmp, SwSurface* surface[2], const RenderEffectDropShadow* params, bool direct)
{
    //FIXME: if the body is partially visible due to clipping, the shadow also becomes partially visible.

    auto data = static_cast<SwDropShadow*>(params->rd);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);

    //outside the screen
    if (abs(data->offset.x) >= w || abs(data->offset.y) >= h) return true;

    SwImage* buffer[] = {&surface[0]->compositor->image, &surface[1]->compositor->image};
    auto color = cmp->recoverSfc->join(params->color[0], params->color[1], params->color[2], 255);
    auto stride = cmp->image.stride;
    auto front = cmp->image.buf32;
    auto back = buffer[1]->buf32;

    auto opacity = direct ? MULTIPLY(params->color[3], cmp->opacity) : params->color[3];

    TVGLOG("SW_ENGINE", "DropShadow region(%d, %d, %d, %d) params(%f %f %f), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->angle, params->distance, params->sigma, data->level);

    //no filter required
    if (data->extends == 0)  {
        if (direct) {
            _dropShadowNoFilter(cmp->recoverSfc->buf32, cmp->image.buf32, cmp->recoverSfc->stride, cmp->image.stride, cmp->recoverSfc->w, cmp->recoverSfc->h, bbox, data->offset, color, opacity, direct);
        } else {
            _dropShadowNoFilter(buffer[1], &cmp->image, bbox, data->offset, color);
            std::swap(cmp->image.buf32, buffer[1]->buf32);
        }
        return true;
    }

    //saving the original image in order to overlay it into the filtered image.
    _dropShadowFilter(back, front, stride, w, h, bbox, data->kernel[0], color, false);
    std::swap(front, buffer[0]->buf32);
    std::swap(front, back);

    //horizontal
    for (int i = 1; i < data->level; ++i) {
        _dropShadowFilter(back, front, stride, w, h, bbox, data->kernel[i], color, false);
        std::swap(front, back);
    }

    //vertical
    rasterXYFlip(front, back, stride, w, h, bbox, false);
    std::swap(front, back);

    for (int i = 0; i < data->level; ++i) {
        _dropShadowFilter(back, front, stride, h, w, bbox, data->kernel[i], color, true);
        std::swap(front, back);
    }

    rasterXYFlip(front, back, stride, h, w, bbox, true);
    std::swap(cmp->image.buf32, back);

    //draw to the main surface directly
    if (direct) {
        _dropShadowShift(cmp->recoverSfc->buf32, cmp->image.buf32, cmp->recoverSfc->stride, cmp->image.stride, cmp->recoverSfc->w, cmp->recoverSfc->h, bbox, data->offset, opacity, direct);
        std::swap(cmp->image.buf32, buffer[0]->buf32);
        return true;
    }

    //draw to the intermediate surface
    rasterClear(surface[1], bbox.min.x, bbox.min.y, w, h);
    _dropShadowShift(buffer[1]->buf32, cmp->image.buf32, buffer[1]->stride, cmp->image.stride, buffer[1]->w, buffer[1]->h, bbox, data->offset, opacity, direct);
    std::swap(cmp->image.buf32, buffer[1]->buf32);

    //compositing shadow and body
    auto s = buffer[0]->buf32 + (bbox.min.y * buffer[0]->stride + bbox.min.x);
    auto d = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);

    for (auto y = 0; y < h; ++y) {
        rasterTranslucentPixel32(d, s, w, 255);
        s += buffer[0]->stride;
        d += cmp->image.stride;
    }

    return true;
}


/************************************************************************/
/* Fill Implementation                                                  */
/************************************************************************/

void effectFillUpdate(RenderEffectFill* params)
{
    params->valid = true;
}


bool effectFill(SwCompositor* cmp, const RenderEffectFill* params, bool direct)
{
    auto opacity = direct ? MULTIPLY(params->color[3], cmp->opacity) : params->color[3];

    auto& bbox = cmp->bbox;
    auto w = size_t(bbox.max.x - bbox.min.x);
    auto h = size_t(bbox.max.y - bbox.min.y);
    auto color = cmp->recoverSfc->join(params->color[0], params->color[1], params->color[2], 255);

    TVGLOG("SW_ENGINE", "Fill region(%d, %d, %d, %d), param(%d %d %d %d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->color[0], params->color[1], params->color[2], params->color[3]);

    if (direct) {
        auto dbuffer = cmp->recoverSfc->buf32 + (bbox.min.y * cmp->recoverSfc->stride + bbox.min.x);
        auto sbuffer = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            auto src = sbuffer;
            for (size_t x = 0; x < w; ++x, ++dst, ++src) {
                auto a = MULTIPLY(opacity, A(*src));
                auto tmp = ALPHA_BLEND(color, a);
                *dst = tmp + ALPHA_BLEND(*dst, 255 - a);
            }
            dbuffer += cmp->recoverSfc->stride;
            sbuffer += cmp->image.stride;
        }
        cmp->valid = true;  //no need the subsequent composition
    } else {
        auto dbuffer = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            for (size_t x = 0; x < w; ++x, ++dst) {
                *dst = ALPHA_BLEND(color, MULTIPLY(opacity, A(*dst)));
            }
            dbuffer += cmp->image.stride;
        }
    }
    return true;
}


/************************************************************************/
/* Tint Implementation                                                  */
/************************************************************************/

void effectTintUpdate(RenderEffectTint* params)
{
    params->valid = (params->intensity > 0);
}


bool effectTint(SwCompositor* cmp, const RenderEffectTint* params, bool direct)
{
    auto& bbox = cmp->bbox;
    auto w = size_t(bbox.max.x - bbox.min.x);
    auto h = size_t(bbox.max.y - bbox.min.y);
    auto black = cmp->recoverSfc->join(params->black[0], params->black[1], params->black[2], 255);
    auto white = cmp->recoverSfc->join(params->white[0], params->white[1], params->white[2], 255);
    auto opacity = cmp->opacity;
    auto luma = cmp->recoverSfc->alphas[2];  //luma function

    TVGLOG("SW_ENGINE", "Tint region(%d, %d, %d, %d), param(%d %d %d, %d %d %d, %d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->black[0], params->black[1], params->black[2], params->white[0], params->white[1], params->white[2], params->intensity);

    if (direct) {
        auto dbuffer = cmp->recoverSfc->buf32 + (bbox.min.y * cmp->recoverSfc->stride + bbox.min.x);
        auto sbuffer = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            auto src = sbuffer;
            for (size_t x = 0; x < w; ++x, ++dst, ++src) {
                auto val = INTERPOLATE(white, black, luma((uint8_t*)src));
                if (params->intensity < 255) val = INTERPOLATE(val, *src, params->intensity);
                *dst = INTERPOLATE(val, *dst, MULTIPLY(opacity, A(*src)));
            }
            dbuffer += cmp->recoverSfc->stride;
            sbuffer += cmp->image.stride;
        }
        cmp->valid = true;  //no need the subsequent composition
    } else {
        auto dbuffer = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            for (size_t x = 0; x < w; ++x, ++dst) {
                auto val = INTERPOLATE(white, black, luma((uint8_t*)dst));
                if (params->intensity < 255) val = INTERPOLATE(val, *dst, params->intensity);
                *dst = ALPHA_BLEND(val, MULTIPLY(opacity, A(*dst)));
            }
            dbuffer += cmp->image.stride;
        }
    }

    return true;
}


/************************************************************************/
/* Tritone Implementation                                              */
/************************************************************************/

static uint32_t _trintone(uint32_t s, uint32_t m, uint32_t h, int l)
{
    if (l < 128) {
        auto a = std::min(l * 2, 255);
        return ALPHA_BLEND(s, 255 - a) + ALPHA_BLEND(m, a);
    } else {
        auto a = 2 * std::max(0, l - 128);
        return ALPHA_BLEND(m, 255 - a) + ALPHA_BLEND(h, a);
    }
}


void effectTritoneUpdate(RenderEffectTritone* params)
{
    params->valid = (params->blender < 255);
}


bool effectTritone(SwCompositor* cmp, const RenderEffectTritone* params, bool direct)
{
    auto& bbox = cmp->bbox;
    auto w = size_t(bbox.max.x - bbox.min.x);
    auto h = size_t(bbox.max.y - bbox.min.y);
    auto shadow = cmp->recoverSfc->join(params->shadow[0], params->shadow[1], params->shadow[2], 255);
    auto midtone = cmp->recoverSfc->join(params->midtone[0], params->midtone[1], params->midtone[2], 255);
    auto highlight = cmp->recoverSfc->join(params->highlight[0], params->highlight[1], params->highlight[2], 255);
    auto opacity = cmp->opacity;
    auto luma = cmp->recoverSfc->alphas[2];  //luma function

    TVGLOG("SW_ENGINE", "Tritone region(%d, %d, %d, %d), param(%d %d %d, %d %d %d, %d %d %d, %d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->shadow[0], params->shadow[1], params->shadow[2], params->midtone[0], params->midtone[1], params->midtone[2], params->highlight[0], params->highlight[1], params->highlight[2], params->blender);

    if (direct) {
        auto dbuffer = cmp->recoverSfc->buf32 + (bbox.min.y * cmp->recoverSfc->stride + bbox.min.x);
        auto sbuffer = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            auto src = sbuffer;
            if (params->blender == 0) {
                for (size_t x = 0; x < w; ++x, ++dst, ++src) {
                    *dst = INTERPOLATE(_trintone(shadow, midtone, highlight, luma((uint8_t*)src)), *dst, MULTIPLY(opacity, A(*src)));
                }
            } else {
                for (size_t x = 0; x < w; ++x, ++dst, ++src) {
                    *dst = INTERPOLATE(INTERPOLATE(*src, _trintone(shadow, midtone, highlight, luma((uint8_t*)src)), params->blender), *dst, MULTIPLY(opacity, A(*src)));
                }
            }
            dbuffer += cmp->recoverSfc->stride;
            sbuffer += cmp->image.stride;
        }
        cmp->valid = true;  //no need the subsequent composition
    } else {
        auto dbuffer = cmp->image.buf32 + (bbox.min.y * cmp->image.stride + bbox.min.x);
        for (size_t y = 0; y < h; ++y) {
            auto dst = dbuffer;
            if (params->blender == 0) {
                for (size_t x = 0; x < w; ++x, ++dst) {
                    *dst = ALPHA_BLEND(_trintone(shadow, midtone, highlight, luma((uint8_t*)dst)), MULTIPLY(A(*dst), opacity));
                }
            } else {
                for (size_t x = 0; x < w; ++x, ++dst) {
                    *dst = ALPHA_BLEND(INTERPOLATE(*dst, _trintone(shadow, midtone, highlight, luma((uint8_t*)dst)), params->blender), MULTIPLY(A(*dst), opacity));
                }
            }
            dbuffer += cmp->image.stride;
        }
    }

    return true;
}
