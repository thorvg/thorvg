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
#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct LinearGradient::Impl
{
    float x1, y1, x2, y2;
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LinearGradient::LinearGradient():pImpl(make_unique<Impl>())
{
    _id = FILL_ID_LINEAR;
}


LinearGradient::~LinearGradient()
{
}


Result LinearGradient::linear(float x1, float y1, float x2, float y2) noexcept
{
    if (fabsf(x2 - x1) < FLT_EPSILON && fabsf(y2 - y1) < FLT_EPSILON)
        return Result::InvalidArguments;

    IMPL->x1 = x1;
    IMPL->y1 = y1;
    IMPL->x2 = x2;
    IMPL->y2 = y2;

    return Result::Success;
}


Result LinearGradient::linear(float* x1, float* y1, float* x2, float* y2) const noexcept
{
    if (x1) *x1 = IMPL->x1;
    if (x2) *x2 = IMPL->x2;
    if (y1) *y1 = IMPL->y1;
    if (y2) *y2 = IMPL->y2;

    return Result::Success;
}


unique_ptr<LinearGradient> LinearGradient::gen() noexcept
{
    return unique_ptr<LinearGradient>(new LinearGradient);
}