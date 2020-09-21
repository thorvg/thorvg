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
#include "tvgPaint.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

Paint :: Paint() : pImpl(new Impl())
{
}


Paint :: ~Paint()
{
    delete(pImpl);
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Result Paint::rotate(float degree) noexcept
{
    if (pImpl->rotate(degree)) return Result::Success;
    return Result::FailedAllocation;
}


Result Paint::scale(float factor) noexcept
{
    if (pImpl->scale(factor)) return Result::Success;
    return Result::FailedAllocation;
}


Result Paint::translate(float x, float y) noexcept
{
    if (pImpl->translate(x, y)) return Result::Success;
    return Result::FailedAllocation;
}


Result Paint::transform(const Matrix& m) noexcept
{
    if (pImpl->transform(m)) return Result::Success;
    return Result::FailedAllocation;
}

Matrix Paint::transform() noexcept
{
    return pImpl->transform();
}

Result Paint::bounds(float* x, float* y, float* w, float* h) const noexcept
{
    if (pImpl->bounds(x, y, w, h)) return Result::Success;
    return Result::InsufficientCondition;
}

Paint* Paint::duplicate() const noexcept
{
    return pImpl->duplicate();
}

Result Paint::composite(std::unique_ptr<Paint> comp, CompMethod method) const noexcept
{
    if (pImpl->composite(move(comp), method)) return Result::Success;
    return Result::InsufficientCondition;
}

Paint* Paint::composite() const noexcept
{
    return pImpl->composite();
}

CompMethod Paint::compositeMethod() const noexcept
{
    return pImpl->compositeMethod();
}

