/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_PAINT_CPP_
#define _TVG_PAINT_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

Paint :: Paint() : pImpl(make_unique<Impl>())
{
}


Paint :: ~Paint()
{
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Result Paint::rotate(float degree) noexcept
{
    if (IMPL->method->rotate(degree)) return Result::Success;
    return Result::FailedAllocation;
}


Result Paint::scale(float factor) noexcept
{
    if (IMPL->method->scale(factor)) return Result::Success;
    return Result::FailedAllocation;
}


Result Paint::translate(float x, float y) noexcept
{
    if (IMPL->method->translate(x, y)) return Result::Success;
    return Result::FailedAllocation;
}


Result Paint::transform(const Matrix& m) noexcept
{
    if (IMPL->method->transform(m)) return Result::Success;
    return Result::FailedAllocation;
}


Result Paint::bounds(float* x, float* y, float* w, float* h) const noexcept
{
    if (IMPL->method->bounds(x, y, w, h)) return Result::Success;
    return Result::InsufficientCondition;
}

#endif //_TVG_PAINT_CPP_