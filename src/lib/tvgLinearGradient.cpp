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
#ifndef _TVG_LINEAR_GRADIENT_CPP_
#define _TVG_LINEAR_GRADIENT_CPP_

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
    id = FILL_ID_LINEAR;
}


LinearGradient::~LinearGradient()
{
}


Result LinearGradient::linear(float x1, float y1, float x2, float y2) noexcept
{
    if (fabsf(x2 - x1) < FLT_EPSILON && fabsf(y2 - y1) < FLT_EPSILON)
        return Result::InvalidArguments;

    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

    impl->x1 = x1;
    impl->y1 = y1;
    impl->x2 = x2;
    impl->y2 = y2;

    return Result::Success;
}


Result LinearGradient::linear(float* x1, float* y1, float* x2, float* y2) const noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

    if (x1) *x1 = impl->x1;
    if (x2) *x2 = impl->x2;
    if (y1) *y1 = impl->y1;
    if (y2) *y2 = impl->y2;

    return Result::Success;
}


unique_ptr<LinearGradient> LinearGradient::gen() noexcept
{
    auto fill = unique_ptr<LinearGradient>(new LinearGradient);
    assert(fill);

    return fill;
}

#endif /* _TVG_LINEAR_GRADIENT_CPP_ */