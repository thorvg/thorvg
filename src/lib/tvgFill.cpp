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
#ifndef _TVG_FILL_CPP_
#define _TVG_FILL_CPP_

#include "tvgCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Fill::Impl
{
    ColorStop* colorStops = nullptr;
    uint32_t cnt = 0;
    FillSpread spread;

    ~Impl()
    {
        if (colorStops) free(colorStops);
    }
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Fill::Fill():pImpl(make_unique<Impl>())
{
}


Fill::~Fill()
{
}


Result Fill::colorStops(const ColorStop* colorStops, uint32_t cnt) noexcept
{
    auto impl = pImpl.get();

    if (cnt == 0) {
        if (impl->colorStops) {
            free(impl->colorStops);
            impl->colorStops = nullptr;
            impl->cnt = cnt;
        }
        return Result::Success;
    }

    if (impl->cnt != cnt) {
        impl->colorStops = static_cast<ColorStop*>(realloc(impl->colorStops, cnt * sizeof(ColorStop)));
    }

    impl->cnt = cnt;
    memcpy(impl->colorStops, colorStops, cnt * sizeof(ColorStop));

    return Result::Success;
}


uint32_t Fill::colorStops(const ColorStop** colorStops) const noexcept
{
    if (colorStops) *colorStops = IMPL->colorStops;

    return IMPL->cnt;
}


Result Fill::spread(FillSpread s) noexcept
{
    IMPL->spread = s;

    return Result::Success;
}


FillSpread Fill::spread() const noexcept
{
    return IMPL->spread;
}

#endif /* _TVG_FILL_CPP_ */