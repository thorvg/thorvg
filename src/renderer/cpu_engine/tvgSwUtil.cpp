/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#include "tvgMath.h"
#include "tvgSwCommon.h"

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void utilExport(SwOutline* outline, const Matrix& transform, BBox& bbox)
{
    auto path = outline->path;
    auto count = path->pts.count;
    outline->out.reserve(count);
    BBox bounds = {{FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX}};

    auto out = outline->out.data;
    auto end = path->pts.end();
    for (auto pt = path->pts.begin(); pt < end; ++pt) {
        auto t = Point{pt->x * transform.e11 + pt->y * transform.e12 + transform.e13,
                       pt->x * transform.e21 + pt->y * transform.e22 + transform.e23};
        if (bounds.min.x > t.x) bounds.min.x = t.x;
        if (bounds.max.x < t.x) bounds.max.x = t.x;
        if (bounds.min.y > t.y) bounds.min.y = t.y;
        if (bounds.max.y < t.y) bounds.max.y = t.y;
        auto x = int32_t(t.x * 64.0f);
        auto y = int32_t(t.y * 64.0f);
        *out++ = {int32_t(static_cast<uint32_t>(x) << (SW_PIXEL_BITS - 6)),
                  int32_t(static_cast<uint32_t>(y) << (SW_PIXEL_BITS - 6))};
    }
    outline->out.count = count;
    bbox = bounds;
}

bool utilBBox(const BBox& bbox, const RenderRegion& clipBox, RenderRegion& renderBox, bool fastTrack)
{
    if (fastTrack) {
        renderBox.min = {(int32_t)round(bbox.min.x), (int32_t)round(bbox.min.y)};
        renderBox.max = {(int32_t)round(bbox.max.x), (int32_t)round(bbox.max.y)};
    } else {
        renderBox.min = {(int32_t)floorf(bbox.min.x), (int32_t)floorf(bbox.min.y)};
        renderBox.max = {(int32_t)ceilf(bbox.max.x), (int32_t)ceilf(bbox.max.y)};
    }
    renderBox.intersect(clipBox);
    return renderBox.valid();
}
