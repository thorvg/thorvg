/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include <math.h>
#include <vector>
#include "tvgGlCommon.h"

#define PI 3.1415926535897932384626433832795f

#define MVP_MATRIX() \
    float mvp[4*4] = { \
        2.f / w, 0.0, 0.0f, 0.0f, \
        0.0, -2.f / h, 0.0f, 0.0f, \
        0.0f, 0.0f, -1.f, 0.0f, \
        -1.f, 1.f, 0.0f, 1.0f \
    };

#define MULTIPLY_MATRIX(A, B, transform) \
    for(auto i = 0; i < 4; ++i) \
    { \
        for(auto j = 0; j < 4; ++j) \
        { \
            float sum = 0.0; \
            for (auto k = 0; k < 4; ++k) \
                sum += A[k*4+i] * B[j*4+k]; \
            transform[j*4+i] = sum; \
        } \
    }

/**
 *  mat3x3               mat4x4
 *
 * [ e11 e12 e13 ]     [ e11 e12 0 e13 ]
 * [ e21 e22 e23 ] =>  [ e21 e22 0 e23 ]
 * [ e31 e32 e33 ]     [ 0   0   1  0  ]
 *                     [ e31 e32 0 e33 ]
 *
 */

// All GPU use 4x4 matrix with column major order
#define GET_MATRIX44(mat3, mat4)    \
    do {                            \
        mat4[0] = mat3.e11;         \
        mat4[1] = mat3.e21;         \
        mat4[2] = 0;                \
        mat4[3] = mat3.e31;         \
        mat4[4] = mat3.e12;         \
        mat4[5] = mat3.e22;         \
        mat4[6] = 0;                \
        mat4[7] = mat3.e32;         \
        mat4[8] = 0;                \
        mat4[9] = 0;                \
        mat4[10] = 1;               \
        mat4[11] = 0;               \
        mat4[12] = mat3.e13;        \
        mat4[13] = mat3.e23;        \
        mat4[14] = 0;               \
        mat4[15] = mat3.e33;        \
    } while (false)

class GlPoint
{
public:
    float x = 0.0f;
    float y = 0.0f;

    GlPoint() = default;

    GlPoint(float pX, float pY):x(pX), y(pY)
    {
    }

    GlPoint(const Point& rhs):GlPoint(rhs.x, rhs.y)
    {
    }

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
        if (&rhs == this) return true;
        if (rhs.x == this->x && rhs.y == this->y) return true;
        return false;
    }

    bool operator!= (const GlPoint& rhs)
    {
        if (&rhs == this) return true;
        if (rhs.x != this->x || rhs.y != this->y) return true;
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
        auto length = sqrtf((x * x) + (y * y));
        if (length != 0.0f) {
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

    ~GlGeometry();

    uint32_t getPrimitiveCount();
    const GlSize getPrimitiveSize(const uint32_t primitiveIndex) const;
    bool decomposeOutline(const RenderShape& rshape);
    bool generateAAPoints(TVG_UNUSED const RenderShape& rshape, float strokeWd, RenderUpdateFlag flag);
    bool tesselate(TVG_UNUSED const RenderShape& rshape, float viewWd, float viewHt, RenderUpdateFlag flag);
    void disableVertex(uint32_t location);
    void draw(const uint32_t location, const uint32_t primitiveIndex, RenderUpdateFlag flag);
    void updateTransform(const RenderTransform* transform, float w, float h);
    float* getTransforMatrix();

private:
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

    GLuint mVao = 0;
    std::unique_ptr<GlGpuBuffer> mVertexBuffer;
    std::unique_ptr<GlGpuBuffer> mIndexBuffer;
    vector<GlPrimitive>     mPrimitives;
    float                   mTransform[16];
};

#endif /* _TVG_GL_GEOMETRY_H_ */
