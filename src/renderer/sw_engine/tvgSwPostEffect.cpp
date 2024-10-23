/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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
};


static void _gaussianExtendRegion(RenderRegion& region, int extra, int8_t direction)
{
    //bbox region expansion for feathering
    if (direction != 2) {
        region.x = -extra;
        region.w = extra * 2;
    }
    if (direction != 1) {
        region.y = -extra;
        region.h = extra * 2;
    }
}


static int _gaussianRemap(int end, int idx, int border)
{
    //wrap
    if (border == 1) return idx % end;

    //duplicate
    if (idx < 0) return 0;
    else if (idx >= end) return end - 1;
    return idx;
}


//TODO: SIMD OPTIMIZATION?
static void _gaussianFilter(uint8_t* src, uint8_t* dst, int32_t stride, int32_t w, int32_t h, const SwBBox& bbox, int32_t dimension, int border, bool flipped)
{
    if (flipped) {
        src += (bbox.min.x * stride + bbox.min.y) << 2;
        dst += (bbox.min.x * stride + bbox.min.y) << 2;
    } else {
        src += (bbox.min.y * stride + bbox.min.x) << 2;
        dst += (bbox.min.y * stride + bbox.min.x) << 2;
    }

    auto iarr = 1.0f / (dimension + dimension + 1);

    #pragma omp parallel for
    for (int x = 0; x < h; x++) {
        auto p = x * stride;
        auto i = p * 4;                 //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc[4] = {0, 0, 0, 0};      //sliding accumulator

        //initial acucmulation
        for (int x2 = l; x2 < r; ++x2) {
            auto id = (_gaussianRemap(w, x2, border) + p) * 4;
            acc[0] += src[id++];
            acc[1] += src[id++];
            acc[2] += src[id++];
            acc[3] += src[id];
        }
        //perform filtering
        for (int x2 = 0; x2 < w; ++x2, ++r, ++l) {
            auto rid = (_gaussianRemap(w, r, border) + p) * 4;
            auto lid = (_gaussianRemap(w, l, border) + p) * 4;
            acc[0] += src[rid++] - src[lid++];
            acc[1] += src[rid++] - src[lid++];
            acc[2] += src[rid++] - src[lid++];
            acc[3] += src[rid] - src[lid];
            dst[i++] = static_cast<uint8_t>(acc[0] * iarr + 0.5f);
            dst[i++] = static_cast<uint8_t>(acc[1] * iarr + 0.5f);
            dst[i++] = static_cast<uint8_t>(acc[2] * iarr + 0.5f);
            dst[i++] = static_cast<uint8_t>(acc[3] * iarr + 0.5f);
        }
    }
}


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


bool effectGaussianBlurPrepare(RenderEffectGaussianBlur* params)
{
    auto rd = (SwGaussianBlur*)malloc(sizeof(SwGaussianBlur));

    auto extends = _gaussianInit(rd, params->sigma * params->sigma, params->quality);

    //invalid
    if (extends == 0) {
        params->invalid = true;
        free(rd);
        return false;
    }

    _gaussianExtendRegion(params->extend, extends, params->direction);

    params->rd = rd;

    return true;
}


bool effectGaussianBlur(SwCompositor* cmp, SwImage& buffer, const RenderEffectGaussianBlur* params)
{
    if (cmp->image.channelSize != sizeof(uint32_t)) {
        TVGERR("SW_ENGINE", "Not supported grayscale Gaussian Blur!");
        return false;
    }

    auto data = static_cast<SwGaussianBlur*>(params->rd);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);
    auto stride = cmp->image.stride;
    auto threshold = (std::min(w, h) < 300) ? 2 : 1;   //fine-tuning for low-quality (experimental)
    auto front = cmp->image.buf32;
    auto back = buffer.buf32;
    auto swapped = false;

    TVGLOG("SW_ENGINE", "GaussianFilter region(%ld, %ld, %ld, %ld) params(%f %d %d), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->sigma, params->direction, params->border, data->level);

    /* It is best to take advantage of the Gaussian blur’s separable property
       by dividing the process into two passes. horizontal and vertical.
       We can expect fewer calculations. */

    //horizontal
    if (params->direction != 2) {
        for (int i = 0; i < data->level; ++i) {
            auto k = data->kernel[i] / threshold;
            if (k == 0) continue;
            _gaussianFilter(reinterpret_cast<uint8_t*>(front), reinterpret_cast<uint8_t*>(back), stride, w, h, bbox, k, params->border, false);
            std::swap(front, back);
            swapped = !swapped;
        }
    }

    //vertical. x/y flipping and horionztal access is pretty compatible with the memory architecture.
    if (params->direction != 1) {
        rasterXYFlip(front, back, stride, w, h, bbox, false);
        std::swap(front, back);

        for (int i = 0; i < data->level; ++i) {
            auto k = data->kernel[i] / threshold;
            if (k == 0) continue;
            _gaussianFilter(reinterpret_cast<uint8_t*>(front), reinterpret_cast<uint8_t*>(back), stride, h, w, bbox, k, params->border, true);
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
static void _gaussianFilter(uint32_t* src, uint32_t* dst, int stride, int w, int h, const SwBBox& bbox, const SwPoint& offset, const SwPoint& cutout, int32_t dimension, uint32_t color, bool flipped)
{
    if (flipped) {
        src += ((bbox.min.x + offset.x) * stride + (bbox.min.y + offset.y));
        dst += (bbox.min.x * stride + bbox.min.y);
    } else {
        src += (bbox.min.y * stride + bbox.min.x);
        dst += ((bbox.min.y + offset.y) * stride + (bbox.min.x + offset.x));
    }

    auto iarr = 1.0f / (dimension + dimension + 1);

    #pragma omp parallel for
    for (int x = 0; x < h - cutout.y; x++) {
        auto p = x * stride;
        auto i = p;                     //current index
        auto l = -(dimension + 1);      //left index
        auto r = dimension;             //right index
        int acc = 0;                    //sliding accumulator

        //initial acucmulation
        for (int x2 = l; x2 < r; ++x2) {
            auto id = _gaussianRemap(w, x2, 0) + p;
            acc += A(src[id]);
        }
        //perform filtering
        for (int x2 = 0; x2 < w - cutout.x; ++x2, ++r, ++l) {
            auto rid = _gaussianRemap(w, r, 0) + p;
            auto lid = _gaussianRemap(w, l, 0) + p;
            acc += A(src[rid]) - A(src[lid]);
            dst[i++] = ALPHA_BLEND(color, static_cast<uint8_t>(acc * iarr + 0.5f));
        }
    }
}


static bool _dropShadowComposite(SwImage* src, SwImage* dst, SwBBox& region, uint8_t opacity)
{
    auto sbuffer = &src->buf32[region.min.y * src->stride + region.min.x];
    auto dbuffer = &dst->buf32[region.min.y * dst->stride + region.min.x];

    for (auto y = region.min.y; y < region.max.y; ++y) {
        auto d = dbuffer;
        auto s = sbuffer;
        if (opacity == 255) {
            for (auto x = region.min.x; x < region.max.x; x++, ++d, ++s) {
                *d = *s + ALPHA_BLEND(*d, IA(*s));
            }
        } else {
            for (auto x = region.min.x; x < region.max.x; ++x, ++d, ++s) {
                *d = ALPHA_BLEND(*d, opacity);
                *d = *s + ALPHA_BLEND(*d, IA(*s));
            }
        }
        sbuffer += src->stride;
        dbuffer += dst->stride;
    }
    return true;
}


static SwPoint _dropShadowOffset(const RenderEffectDropShadow* params)
{
    return {15, 0};
}


static void _dropShadowExtendRegion(RenderRegion& region, SwPoint& offset, int extra)
{
    //expansion by blurness
    region.x = -extra;
    region.y = -extra;
    region.w = extra * 2;
    region.h = extra * 2;

    //expansion by shadow distance
    if (offset.x < 0) region.x += offset.x;
    if (offset.y < 0) region.y += offset.y;
    if (offset.x > 0) region.w += offset.x;
    if (offset.y > 0) region.h += offset.y;
}


bool effectDropShadowPrepare(RenderEffectDropShadow* params)
{
    auto rd = (SwDropShadow*)malloc(sizeof(SwDropShadow));

    //compute box kernel sizes
    auto extends = _gaussianInit(rd, params->sigma * params->sigma, params->quality);

    //invalid
    if (extends == 0 || params->color[3] == 0) {
        params->invalid = true;
        free(rd);
        return false;
    }

    //bbox region expansion for feathering
    rd->offset = _dropShadowOffset(params);
    _dropShadowExtendRegion(params->extend, rd->offset, extends);

    params->rd = rd;

    return true;
}


//A quite same integration with effectGaussianBlur(). See it for detailed comments.
bool effectDropShadow(SwCompositor* cmp, SwImage* buffer[2], const RenderEffectDropShadow* params)
{
    if (cmp->image.channelSize != sizeof(uint32_t)) {
        TVGERR("SW_ENGINE", "Not supported grayscale Drop Shadow!");
        return false;
    }

    auto data = static_cast<SwDropShadow*>(params->rd);
    auto color = cmp->recoverSfc->join(params->color[0], params->color[1], params->color[2], 255);
    auto& bbox = cmp->bbox;
    auto w = (bbox.max.x - bbox.min.x);
    auto h = (bbox.max.y - bbox.min.y);

    //FIXME: outside of the buffer range by offset
    SwPoint cutout = {w + data->offset.x - cmp->image.w, h + data->offset.y - cmp->image.h};
    if (cutout.x < 0) cutout.x = 0;
    if (cutout.y < 0) cutout.y = 0;

    auto stride = cmp->image.stride;
    auto threshold = (std::min(w, h) < 300) ? 2 : 1;      //fine-tuning for low-quality (experimental)
    auto front = cmp->image.buf32;
    auto back = buffer[1]->buf32;
    auto first = true;

    TVGLOG("SW_ENGINE", "DropShadow region(%ld, %ld, %ld, %ld) params(%f %f %f), level(%d)", bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, params->angle, params->distance, params->sigma, data->level);

    //horizontal
    for (int i = 0; i < data->level; ++i) {
        auto k = data->kernel[i] / threshold;
        if (k == 0) continue;
        _gaussianFilter(front, back, stride, w, h, bbox, {0, 0}, {0, 0}, k, color, false);

        //only first time flipping input and front to preserve the input image
        if (first) {
            std::swap(front, buffer[0]->buf32);
            first = false;
        }
        std::swap(front, back);
    }

    //vertical
    rasterXYFlip(front, back, stride, w, h, bbox, false);
    std::swap(front, back);

    for (int i = 0; i < data->level; ++i) {
        auto k = data->kernel[i] / threshold;
        if (k == 0) continue;
        _gaussianFilter(front, back, stride, h, w, bbox, {0, 0}, {0, 0}, k, color, true);
        std::swap(front, back);
    }

    rasterXYFlip(front, back, stride, h, w, bbox, true);
    std::swap(front, back);

    //finished
    std::swap(cmp->image.buf32, front);

    return _dropShadowComposite(buffer[0], &cmp->image, cmp->bbox, params->color[3]);
}