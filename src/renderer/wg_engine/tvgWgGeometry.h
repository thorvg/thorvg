/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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
#include "tvgRender.h"
#include "tvgArray.h"

class WgPoint
{
public:
    float x;
    float y;

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
    inline void negate() { x = -x; y = -y; }
    inline float length() const { return sqrt(x*x + y*y); }
    inline float length2() const { return x*x + y*y; }
    inline float dot(const WgPoint& p) const { return x * p.x + y * p.y; }
    inline float dist(const WgPoint& p) const { return sqrt(dist2(p)); }
    inline float dist2(const WgPoint& p) const { return ((p.x - x)*(p.x - x) + (p.y - y)*(p.y - y)); }
    inline bool equal(const WgPoint& p) const { return mathEqual(x, p.x) && mathEqual(y, p.y); }
    inline void normalize() { float rlen = 1.0f / length(); x *= rlen; y *= rlen; }
    inline WgPoint normal() const { float rlen = 1.0f / length(); return { x * rlen, y * rlen }; }
    inline WgPoint lerp(const WgPoint& p, float t) const { return { x + (p.x - x) * t, y + (p.y - y) * t }; };
    inline WgPoint trans(const Matrix& m) const { return { x * m.e11 + y * m.e12, x * m.e21 + y * m.e22 }; };
};

struct WgMath
{
    Array<float> sinus;
    Array<float> cosin;
    bool initialized{};

    void initialize();
    void release();
};


struct WgPolyline
{
    Array<WgPoint> pts;
    Array<float> dist;
    // polyline bbox points indexes
    uint32_t iminx{};
    uint32_t iminy{};
    uint32_t imaxx{};
    uint32_t imaxy{};
    // total polyline length
    float len{};
    bool closed{};

    WgPolyline();

    void appendPoint(WgPoint pt);
    void appendCubic(WgPoint p1, WgPoint p2, WgPoint p3, size_t nsegs = 16);

    void trim(WgPolyline* polyline, float trimBegin, float trimEnd) const;

    void close();
    void clear();

    void getBBox(WgPoint& pmin, WgPoint& pmax) const;
};


struct WgGeometryData
{
    static WgMath* gMath;

    WgPolyline positions{};
    Array<WgPoint> texCoords{};
    Array<uint32_t> indexes{};

    WgGeometryData();
    void clear();

    void appendCubic(WgPoint p1, WgPoint p2, WgPoint p3);
    void appendBox(WgPoint pmin, WgPoint pmax);
    void appendRect(WgPoint p0, WgPoint p1, WgPoint p2, WgPoint p3);
    void appendCircle(WgPoint center, float radius);
    void appendImageBox(float w, float h);
    void appendBlitBox();
    void appendStrokeDashed(const WgPolyline* polyline, const RenderStroke *stroke);
    void appendStrokeJoin(const WgPoint& v0, const WgPoint& v1, const WgPoint& v2,
                          StrokeJoin join, float halfWidth, float miterLimit);
    void appendStroke(const WgPolyline* polyline, const RenderStroke *stroke);
};

#endif // _TVG_WG_GEOMETRY_H_