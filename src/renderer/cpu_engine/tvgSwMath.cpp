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
/* Internal Class Implementation                                        */
/************************************************************************/

static inline bool _tiny(const Point& pt)
{
    constexpr float EPSILON = 2.0f / 64.0f;

    return fabsf(pt.x) < EPSILON && fabsf(pt.y) < EPSILON;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

float mathMean(float angle1, float angle2)
{
    return angle1 + mathDiff(angle1, angle2) * 0.5f;
}

int mathCubicAngle(const Point* pts, float& angleIn, float& angleMid, float& angleOut)
{
    auto d1 = pts[2] - pts[3];
    auto d2 = pts[1] - pts[2];
    auto d3 = pts[0] - pts[1];

    if (_tiny(d1)) {
        if (_tiny(d2)) {
            if (_tiny(d3)) {
                angleIn = angleMid = angleOut = 0.0f;
                return -1;   // ignorable
            } else {
                angleIn = angleMid = angleOut = mathAtan(d3);
            }
        } else {
            if (_tiny(d3)) {
                angleIn = angleMid = angleOut = mathAtan(d2);
            } else {
                angleIn = angleMid = mathAtan(d2);
                angleOut = mathAtan(d3);
            }
        }
    } else {
        if (_tiny(d2)) {
            if (_tiny(d3)) {
                angleIn = angleMid = angleOut = mathAtan(d1);
            } else {
                angleIn = mathAtan(d1);
                angleOut = mathAtan(d3);
                angleMid = mathMean(angleIn, angleOut);
            }
        } else {
            if (_tiny(d3)) {
                angleIn = mathAtan(d1);
                angleMid = angleOut = mathAtan(d2);
            } else {
                angleIn = mathAtan(d1);
                angleMid = mathAtan(d2);
                angleOut = mathAtan(d3);
            }
        }
    }

    auto theta1 = fabsf(mathDiff(angleIn, angleMid));
    auto theta2 = fabsf(mathDiff(angleMid, angleOut));

    if ((theta1 < (SW_PI / 8.0f)) && (theta2 < (SW_PI / 8.0f))) return 0;   // small curvature

    return 1;
}

void mathRotate(Point& pt, float degree)
{
    if (tvg::zero(degree) || tvg::zero(pt)) return;

    auto radian = deg2rad(degree);
    auto cosv = cosf(radian);
    auto sinv = sinf(radian);

    auto x = pt.x;
    auto y = pt.y;

    pt.x = x * cosv - y * sinv;
    pt.y = x * sinv + y * cosv;
}

float mathTan(float degree)
{
    if (tvg::zero(degree)) return 0.0f;
    return tanf(deg2rad(degree));
}

float mathAtan(const Point& pt)
{
    if (tvg::zero(pt)) return 0.0f;
    return rad2deg(tvg::atan2(pt.y, pt.x));
}

float mathSin(float degree)
{
    if (tvg::zero(degree)) return 0.0f;
    return mathCos(SW_PI2 - degree);
}

float mathCos(float degree)
{
    return cosf(deg2rad(degree));
}

void mathSplitCubic(Point* pts)
{
    auto p01 = (pts[0] + pts[1]) * 0.5f;
    auto p12 = (pts[1] + pts[2]) * 0.5f;
    auto p23 = (pts[2] + pts[3]) * 0.5f;

    auto p012 = (p01 + p12) * 0.5f;
    auto p123 = (p12 + p23) * 0.5f;

    auto p0123 = (p012 + p123) * 0.5f;

    // left curve
    pts[1] = p01;
    pts[2] = p012;
    pts[3] = p0123;

    // right curve
    pts[4] = p123;
    pts[5] = p23;
    pts[6] = pts[3];
}

float mathDiff(float angle1, float angle2)
{
    auto delta = fmodf(angle2 - angle1, 360.0f);

    if (delta < 0.0f) delta += 360.0f;
    if (delta > 180.0f) delta -= 360.0f;

    return delta;
}

bool mathUpdateOutlineBBox(const SwOutline* outline, const RenderRegion& clipBox, RenderRegion& renderBox, bool fastTrack)
{
    if (!outline || outline->pts.empty()) {
        renderBox.reset();
        return false;
    }

    auto pt = outline->pts.begin();

    auto xMin = pt->x;
    auto xMax = pt->x;
    auto yMin = pt->y;
    auto yMax = pt->y;

    for (++pt; pt < outline->pts.end(); ++pt) {
        if (xMin > pt->x) xMin = pt->x;
        if (xMax < pt->x) xMax = pt->x;
        if (yMin > pt->y) yMin = pt->y;
        if (yMax < pt->y) yMax = pt->y;
    }

    if (fastTrack) {
        renderBox.min = {int32_t(roundf(xMin)), int32_t(roundf(yMin))};
        renderBox.max = {int32_t(roundf(xMax)), int32_t(roundf(yMax))};
    } else {
        renderBox.min = {int32_t(floorf(xMin)), int32_t(floorf(yMin))};
        renderBox.max = {int32_t(ceilf(xMax)), int32_t(ceilf(yMax))};
    }

    renderBox.intersect(clipBox);
    return renderBox.valid();
}