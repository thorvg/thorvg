/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_UTIL_H_
#define _TVG_UTIL_H_

#include "tvgCommon.h"
#include "tvgColor.h"
#include "tvgMath.h"

namespace tvg
{

struct ColorStops
{
    Fill::ColorStop* stops = nullptr;
    uint32_t cnt = 0;
};

struct Float3
{
    float v[3];
};

struct Float4
{
    float v[4];
};

static inline Float3 lerp(const Float3& from, const Float3& to, float t)
{
    return {{lerp(from.v[0], to.v[0], t), lerp(from.v[1], to.v[1], t), lerp(from.v[2], to.v[2], t)}};
}

static inline Float4 lerp(const Float4& from, const Float4& to, float t)
{
    return {{lerp(from.v[0], to.v[0], t), lerp(from.v[1], to.v[1], t), lerp(from.v[2], to.v[2], t), lerp(from.v[3], to.v[3], t)}};
}

static inline float pos(float p, float a, float b)
{
    return (p - a) / (b - a);
}

uint32_t bsearch(const void* data, uint32_t cnt, size_t stride, float key);

}  // namespace tvg

#endif  // _TVG_TVG_UTIL_H_
