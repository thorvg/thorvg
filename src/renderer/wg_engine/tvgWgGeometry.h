/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#ifndef _TVG_WG_GEOMETRY_H_
#define _TVG_WG_GEOMETRY_H_

#include <cassert>
#include "tvgMath.h"
#include "tvgCommon.h"
#include "tvgArray.h"

class WgPoint
{
public:
    float x;
    float y;

public:
    WgPoint() {}
    WgPoint(float x, float y): x(x), y(y) {}
    WgPoint(const Point& p): x(p.x), y(p.y) {}

    WgPoint operator + (const WgPoint& p) const { return { x + p.x, y + p.y }; }
    WgPoint operator - (const WgPoint& p) const { return { x - p.x, y - p.y }; }
    WgPoint operator * (const WgPoint& p) const { return { x * p.x, y * p.y }; }
    WgPoint operator / (const WgPoint& p) const { return { x / p.x, y / p.y }; }

    WgPoint operator + (const float c) const { return { x + c, y + c }; }
    WgPoint operator - (const float c) const { return { x - c, y - c }; }
    WgPoint operator * (const float c) const { return { x * c, y * c }; }
    WgPoint operator / (const float c) const { return { x / c, y / c }; }

    WgPoint negative() const { return {-x, -y}; }
    void negate() { x = -x; y = -y; }
    float length() const { return sqrt(x*x + y*y); }

    float dot(const WgPoint& p) const { return x * p.x + y * p.y; }
    float dist(const WgPoint& p) const { 
        return sqrt(
            (p.x - x)*(p.x - x) + 
            (p.y - y)*(p.y - y)
        ); 
    }

    void normalize() {
        float rlen = 1.0f / length();
        x *= rlen;
        y *= rlen;
    }

    WgPoint normal() const {
        float rlen = 1.0f / length();
        return { x * rlen, y * rlen };
    }
};

class WgVertexList {
public:
    Array<WgPoint> mVertexList;
    Array<uint32_t> mIndexList;

    void computeTriFansIndexes();
    void appendCubic(WgPoint p1, WgPoint p2, WgPoint p3);
    void appendRect(WgPoint p0, WgPoint p1, WgPoint p2, WgPoint p3);
    void appendCircle(WgPoint center, float radius);
    void close();
};

#endif // _TVG_WG_GEOMETRY_H_
