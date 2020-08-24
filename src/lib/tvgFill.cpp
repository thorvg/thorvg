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