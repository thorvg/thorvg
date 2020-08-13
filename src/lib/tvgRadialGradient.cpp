/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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
#ifndef _TVG_RADIAL_GRADIENT_CPP_
#define _TVG_RADIAL_GRADIENT_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct RadialGradient::Impl
{
    float cx, cy, radius;
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

RadialGradient::RadialGradient():pImpl(make_unique<Impl>())
{
    _id = FILL_ID_RADIAL;
}


RadialGradient::~RadialGradient()
{
}


Result RadialGradient::radial(float cx, float cy, float radius) noexcept
{
    if (radius < FLT_EPSILON) return Result::InvalidArguments;

    IMPL->cx = cx;
    IMPL->cy = cy;
    IMPL->radius = radius;

    return Result::Success;
}


Result RadialGradient::radial(float* cx, float* cy, float* radius) const noexcept
{
    if (cx) *cx = IMPL->cx;
    if (cy) *cy = IMPL->cy;
    if (radius) *radius = IMPL->radius;

    return Result::Success;
}


unique_ptr<RadialGradient> RadialGradient::gen() noexcept
{
    auto fill = unique_ptr<RadialGradient>(new RadialGradient);
    assert(fill);

    return fill;
}

#endif /* _TVG_RADIAL_GRADIENT_CPP_ */