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
    if (radius < FLT_EPSILON)
        return Result::InvalidArguments;

    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

    impl->cx = cx;
    impl->cy = cy;
    impl->radius = radius;

    return Result::Success;
}


Result RadialGradient::radial(float* cx, float* cy, float* radius) const noexcept
{
    auto impl = pImpl.get();
    if (!impl) return Result::MemoryCorruption;

    if (cx) *cx = impl->cx;
    if (cy) *cy = impl->cy;
    if (radius) *radius = impl->radius;

    return Result::Success;
}


unique_ptr<RadialGradient> RadialGradient::gen() noexcept
{
    auto fill = unique_ptr<RadialGradient>(new RadialGradient);
    assert(fill);

    return fill;
}

#endif /* _TVG_RADIAL_GRADIENT_CPP_ */