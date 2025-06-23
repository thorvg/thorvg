/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_FILL_H_
#define _TVG_FILL_H_

#include "tvgCommon.h"
#include "tvgMath.h"

#define LINEAR(A) static_cast<LinearGradientImpl*>(A)
#define CONST_LINEAR(A) static_cast<const LinearGradientImpl*>(A)

#define RADIAL(A) static_cast<RadialGradientImpl*>(A)
#define CONST_RADIAL(A) static_cast<const RadialGradientImpl*>(A)

struct Fill::Impl
{
    ColorStop* colorStops = nullptr;
    Matrix transform = tvg::identity();
    uint16_t cnt = 0;
    FillSpread spread = FillSpread::Pad;

    virtual ~Impl()
    {
        tvg::free(colorStops);
    }

    void copy(const Fill::Impl& dup)
    {
        cnt = dup.cnt;
        spread = dup.spread;
        colorStops = tvg::malloc<ColorStop*>(sizeof(ColorStop) * dup.cnt);
        if (dup.cnt > 0) memcpy(colorStops, dup.colorStops, sizeof(ColorStop) * dup.cnt);
        transform = dup.transform;
    }

    Result update(const ColorStop* colorStops, uint32_t cnt)
    {
        if ((!colorStops && cnt > 0) || (colorStops && cnt == 0)) return Result::InvalidArguments;

        if (cnt == 0) {
            if (this->colorStops) {
                tvg::free(this->colorStops);
                this->colorStops = nullptr;
                this->cnt = 0;
            }
            return Result::Success;
        }

        if (cnt != this->cnt) {
            this->colorStops = tvg::realloc<ColorStop*>(this->colorStops, cnt * sizeof(ColorStop));
        }

        this->cnt = cnt;
        memcpy(this->colorStops, colorStops, cnt * sizeof(ColorStop));

        return Result::Success;
    }
};


struct RadialGradientImpl : RadialGradient
{
    Fill::Impl impl;

    float cx = 0.0f, cy = 0.0f;
    float fx = 0.0f, fy = 0.0f;
    float r = 0.0f, fr = 0.0f;

    RadialGradientImpl()
    {
        Fill::pImpl = &impl;
    }

    Fill* duplicate() const
    {
        auto ret = RadialGradient::gen();
        RADIAL(ret)->impl.copy(this->impl);
        RADIAL(ret)->cx = cx;
        RADIAL(ret)->cy = cy;
        RADIAL(ret)->r = r;
        RADIAL(ret)->fx = fx;
        RADIAL(ret)->fy = fy;
        RADIAL(ret)->fr = fr;

        return ret;
    }

    Result radial(float cx, float cy, float r, float fx, float fy, float fr)
    {
        if (r < 0 || fr < 0) return Result::InvalidArguments;

        this->cx = cx;
        this->cy = cy;
        this->r = r;
        this->fx = fx;
        this->fy = fy;
        this->fr = fr;

        return Result::Success;
    }

    Result radial(float* cx, float* cy, float* r, float* fx, float* fy, float* fr) const
    {
        if (cx) *cx = this->cx;
        if (cy) *cy = this->cy;
        if (r) *r = this->r;
        if (fx) *fx = this->fx;
        if (fy) *fy = this->fy;
        if (fr) *fr = this->fr;

        return Result::Success;
    }

    //TODO: remove this logic once SVG 2.0 is adopted by sw and wg engines (gl already supports it); lottie-specific handling will then be delegated entirely to the loader
    //clamp focal point and shrink start circle if needed to avoid invalid gradient setup
    bool correct(float& fx, float& fy, float& fr) const
    {
        constexpr float precision = 0.01f;

        //a solid fill case. It can be handled by engine.
        if (this->r < precision) return false;

        fx = this->fx;
        fy = this->fy;
        fr = this->fr;

        auto dx = this->cx - this->fx;
        auto dy = this->cy - this->fy;
        auto dist = sqrtf(dx * dx + dy * dy);

        //move the focal point to the edge (just inside) if it's outside the end circle
        if (this->r - dist <  precision) {
            //handle special case: small radius and small distance -> shift focal point to avoid div-by-zero
            if (dist < precision) dist = dx = precision;
            auto scale = this->r * (1.0f - precision) / dist;
            fx = this->cx - dx * scale;
            fy = this->cy - dy * scale;
            dx = this->cx - fx;
            dy = this->cy - fy;
            dist = sqrtf(dx * dx + dy * dy);
        }

        //if the start circle doesn't fit entirely within the end circle, shrink it (with epsilon margin)
        auto maxFr = (this->r - dist) * (1.0f - precision);
        if (this->fr > maxFr) fr = std::max(0.0f, maxFr);

        return true;
    }
};


struct LinearGradientImpl :  LinearGradient
{
    Fill::Impl impl;

    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;

    LinearGradientImpl()
    {
        Fill::pImpl = &impl;
    }

    Fill* duplicate() const
    {
        auto ret = LinearGradient::gen();
        LINEAR(ret)->impl.copy(this->impl);
        LINEAR(ret)->x1 = x1;
        LINEAR(ret)->y1 = y1;
        LINEAR(ret)->x2 = x2;
        LINEAR(ret)->y2 = y2;

        return ret;
    }

    Result linear(float x1, float y1, float x2, float y2) noexcept
    {
        this->x1 = x1;
        this->y1 = y1;
        this->x2 = x2;
        this->y2 = y2;

        return Result::Success;
    }

    Result linear(float* x1, float* y1, float* x2, float* y2) const noexcept
    {
        if (x1) *x1 = this->x1;
        if (x2) *x2 = this->x2;
        if (y1) *y1 = this->y1;
        if (y2) *y2 = this->y2;

        return Result::Success;
    }
};


#endif  //_TVG_FILL_H_
