/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include <float.h>
#include "tvgFill.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct RadialGradient::Impl
{
    float cx = 0.0f, cy = 0.0f;
    float fx = 0.0f, fy = 0.0f;
    float r = 0.0f, fr = 0.0f;

    Fill* duplicate()
    {
        auto ret = RadialGradient::gen();
        if (!ret) return nullptr;

        ret->pImpl->cx = cx;
        ret->pImpl->cy = cy;
        ret->pImpl->r = r;

        ret->pImpl->fx = fx;
        ret->pImpl->fy = fy;
        ret->pImpl->fr = fr;

        return ret.release();
    }
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

RadialGradient::RadialGradient():pImpl(new Impl())
{
    Fill::pImpl->id = TVG_CLASS_ID_RADIAL;
    Fill::pImpl->method(new FillDup<RadialGradient::Impl>(pImpl));
}


RadialGradient::~RadialGradient()
{
    delete(pImpl);
}


Result RadialGradient::radial(float cx, float cy, float r) noexcept
{
    if (r < 0) return Result::InvalidArguments;

    pImpl->cx = pImpl->fx = cx;
    pImpl->cy = pImpl->fy = cy;
    pImpl->r = r;
    pImpl->fr = 0.0f;

    return Result::Success;
}


Result RadialGradient::radial(float cx, float cy, float r, float fx, float fy, float fr) noexcept
{
    if (r < 0 || fr < 0) return Result::InvalidArguments;

    pImpl->cx = cx;
    pImpl->cy = cy;
    pImpl->r = r;
    pImpl->fx = fx;
    pImpl->fy = fy;
    pImpl->fr = fr;

    return Result::Success;
}


Result RadialGradient::radial(float* cx, float* cy, float* r) const noexcept
{
    if (cx) *cx = pImpl->cx;
    if (cy) *cy = pImpl->cy;
    if (r) *r = pImpl->r;

    return Result::Success;
}


Result RadialGradient::radial(float* cx, float* cy, float* r, float* fx, float* fy, float* fr) const noexcept
{
    if (cx) *cx = pImpl->cx;
    if (cy) *cy = pImpl->cy;
    if (r) *r = pImpl->r;
    if (fx) *fx = pImpl->fx;
    if (fy) *fy = pImpl->fy;
    if (fr) *fr = pImpl->fr;

    return Result::Success;
}


unique_ptr<RadialGradient> RadialGradient::gen() noexcept
{
    return unique_ptr<RadialGradient>(new RadialGradient);
}


uint32_t RadialGradient::identifier() noexcept
{
    return TVG_CLASS_ID_RADIAL;
}
