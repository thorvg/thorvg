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

#ifndef _TVG_GL_GEOMETRY_H_
#define _TVG_GL_GEOMETRY_H_

#include "tvgGlCommon.h"

class GlPoint
{
public:
    float x = 0.0f;
    float y = 0.0f;

    GlPoint() = default;

    GlPoint(float pX, float pY):x(pX), y(pY)
    {}

    GlPoint(const Point& rhs):GlPoint(rhs.x, rhs.y)
    {}

    GlPoint(const GlPoint& rhs) = default;
    GlPoint(GlPoint&& rhs) = default;

    GlPoint& operator= (const GlPoint& rhs) = default;
    GlPoint& operator= (GlPoint&& rhs) = default;

    GlPoint& operator= (const Point& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }

    bool operator== (const GlPoint& rhs)
    {
        if (&rhs == this)
            return true;
        if (rhs.x == this->x && rhs.y == this->y)
            return true;
        return false;
    }

    bool operator!= (const GlPoint& rhs)
    {
        if (&rhs == this)
            return true;
        if (rhs.x != this->x || rhs.y != this->y)
            return true;
        return false;
    }

    GlPoint operator+ (const GlPoint& rhs) const
    {
        return GlPoint(x + rhs.x, y + rhs.y);
    }

    GlPoint operator+ (const float c) const
    {
        return GlPoint(x + c, y + c);
    }

    GlPoint operator- (const GlPoint& rhs) const
    {
        return GlPoint(x - rhs.x, y - rhs.y);
    }

    GlPoint operator- (const float c) const
    {
        return GlPoint(x - c, y - c);
    }

    GlPoint operator* (const GlPoint& rhs) const
    {
        return GlPoint(x * rhs.x, y * rhs.y);
    }

    GlPoint operator* (const float c) const
    {
        return GlPoint(x * c, y * c);
    }

    GlPoint operator/ (const GlPoint& rhs) const
    {
        return GlPoint(x / rhs.x, y / rhs.y);
    }

    GlPoint operator/ (const float c) const
    {
        return GlPoint(x / c, y / c);
    }

    void mod()
    {
        x = fabsf(x);
        y = fabsf(y);
    }

    void normalize()
    {
        auto length = sqrtf( (x * x) + (y * y) );
        if (length != 0.0f)
        {
            const auto inverseLen = 1.0f / length;
            x *= inverseLen;
            y *= inverseLen;
        }
    }
};

typedef GlPoint GlSize;

struct SmoothPoint
{
    GlPoint orgPt;
    GlPoint fillOuterBlur;
    GlPoint fillOuter;
    GlPoint strokeOuterBlur;
    GlPoint strokeOuter;
    GlPoint strokeInnerBlur;
    GlPoint strokeInner;

    SmoothPoint(GlPoint pt)
        :orgPt(pt),
        fillOuterBlur(pt),
        fillOuter(pt),
        strokeOuterBlur(pt),
        strokeOuter(pt),
        strokeInnerBlur(pt),
        strokeInner(pt)
    {
    }
};

struct PointNormals
{
    GlPoint normal1;
    GlPoint normal2;
    GlPoint normalF;
};

struct VertexData
{
   GlPoint point;
   float opacity = 0.0f;
};

struct VertexDataArray
{
    vector<VertexData>   vertices;
    vector<uint32_t>     indices;
};

struct GlPrimitive
{
    vector<SmoothPoint> mAAPoints;
    VertexDataArray mFill;
    VertexDataArray mStroke;
    GlPoint mTopLeft;
    GlPoint mBottomRight;
    bool mIsClosed = false;
};

class GlGpuBuffer;

class GlGeometry
{
public:

    uint32_t getPrimitiveCount();
    const GlSize getPrimitiveSize(const uint32_t primitiveIndex) const;
    bool decomposeOutline(const Shape& shape);
    bool generateAAPoints(TVG_UNUSED const Shape& shape, float strokeWd, RenderUpdateFlag flag);
    bool tesselate(TVG_UNUSED const Shape &shape, float viewWd, float viewHt, RenderUpdateFlag flag);
    void disableVertex(uint32_t location);
    void draw(const uint32_t location, const uint32_t primitiveIndex, RenderUpdateFlag flag);

private:
    GlPoint normalizePoint(const GlPoint &pt, float viewWd, float viewHt);
    void addGeometryPoint(VertexDataArray &geometry, const GlPoint &pt, float viewWd, float viewHt, float opacity);
    GlPoint getNormal(const GlPoint &p1, const GlPoint &p2);
    float dotProduct(const GlPoint &p1, const GlPoint &p2);
    GlPoint extendEdge(const GlPoint &pt, const GlPoint &normal, float scalar);

    void addPoint(GlPrimitive& primitve, const GlPoint &pt, GlPoint &min, GlPoint &max);
    void addTriangleFanIndices(uint32_t &curPt, vector<uint32_t> &indices);
    void addQuadIndices(uint32_t &curPt, vector<uint32_t> &indices);
    bool isBezierFlat(const GlPoint &p1, const GlPoint &c1, const GlPoint &c2, const GlPoint &p2);
    void decomposeCubicCurve(GlPrimitive& primitve, const GlPoint &pt1, const GlPoint &cpt1, const GlPoint &cpt2, const GlPoint &pt2, GlPoint &min, GlPoint &max);
    void updateBuffer(const uint32_t location, const VertexDataArray& vertexArray);

    unique_ptr<GlGpuBuffer> mGpuBuffer;
    vector<GlPrimitive>     mPrimitives;
};

#endif /* _TVG_GL_GEOMETRY_H_ */
