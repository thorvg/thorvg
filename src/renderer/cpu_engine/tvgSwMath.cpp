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

float mathMean(float angle1, float angle2)
{
    return angle1 + mathDiff(angle1, angle2) * 0.5f;
}

void mathRotate(Point& pt, float radian)
{
    if (tvg::zero(radian)) return;

    auto cosv = cosf(radian);
    auto sinv = sinf(radian);

    auto x = pt.x * cosv - pt.y * sinv;
    auto y = pt.x * sinv + pt.y * cosv;

    pt = {x, y};
}

void mathSplitCubic(SwPoint* base)
{
    int32_t a, b, c, d;

    base[6].x = base[3].x;
    c = base[1].x;
    d = base[2].x;
    base[1].x = a = (base[0].x + c) >> 1;
    base[5].x = b = (base[3].x + d) >> 1;
    c = (c + d) >> 1;
    base[2].x = a = (a + c) >> 1;
    base[4].x = b = (b + c) >> 1;
    base[3].x = (a + b) >> 1;

    base[6].y = base[3].y;
    c = base[1].y;
    d = base[2].y;
    base[1].y = a = (base[0].y + c) >> 1;
    base[5].y = b = (base[3].y + d) >> 1;
    c = (c + d) >> 1;
    base[2].y = a = (a + c) >> 1;
    base[4].y = b = (b + c) >> 1;
    base[3].y = (a + b) >> 1;
}


void mathSplitLine(SwPoint* base)
{
    base[2] = base[1];
    base[1] = {(base[0].x >> 1) + (base[1].x >> 1), (base[0].y >> 1) + (base[1].y >> 1)};
}

float mathDiff(float angle1, float angle2)
{
    auto delta = fmodf((angle2 - angle1), MATH_2PI);
    if (delta < 0.0f) delta += MATH_2PI;
    if (delta > MATH_PI) delta -= MATH_2PI;
    return delta;
}

SwPoint mathTransform(const Point& to, const Matrix& transform)
{
    auto tx = to.x * transform.e11 + to.y * transform.e12 + transform.e13;
    auto ty = to.x * transform.e21 + to.y * transform.e22 + transform.e23;

    return {TO_SWCOORD(tx), TO_SWCOORD(ty)};
}


bool mathUpdateOutlineBBox(const SwOutline* outline, const RenderRegion& clipBox, RenderRegion& renderBox, bool fastTrack)
{
    if (!outline || outline->out.empty()) {
        renderBox.reset();
        return false;
    }

    auto pt = outline->out.begin();

    auto xMin = pt->x;
    auto xMax = pt->x;
    auto yMin = pt->y;
    auto yMax = pt->y;

    for (++pt; pt < outline->out.end(); ++pt) {
        if (xMin > pt->x) xMin = pt->x;
        if (xMax < pt->x) xMax = pt->x;
        if (yMin > pt->y) yMin = pt->y;
        if (yMax < pt->y) yMax = pt->y;
    }

    if (fastTrack) {
        renderBox.min = {int32_t(round(xMin / 64.0f)), int32_t(round(yMin / 64.0f))};
        renderBox.max = {int32_t(round(xMax / 64.0f)), int32_t(round(yMax / 64.0f))};
    } else {
        renderBox.min = {xMin >> 6, yMin >> 6};
        renderBox.max = {(xMax + 63) >> 6, (yMax + 63) >> 6};
    }

    renderBox.intersect(clipBox);
    return renderBox.valid();
}
