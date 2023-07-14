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

#include "tvgArray.h"
#include "tvgGlCommon.h"
#include "tvgGlGpuBuffer.h"

#define PI 3.1415926535897932384626433832795f

#define MVP_MATRIX(w, h)                                                         \
    float mvp[4 * 4] = {2.f / w, 0.0,  0.0f,  0.0f, 0.0f,  -2.f / h, 0.0f, 0.0f, \
                        0.0f,    0.0f, -1.0f, 0.0f, -1.0f, 1.0f,     0.0f, 1.0f};

#define ROTATION_MATRIX(xPivot, yPivot)                                    \
    auto  radian = mTransform.angle / 180.0f * PI;                         \
    auto  cosVal = cosf(radian);                                           \
    auto  sinVal = sinf(radian);                                           \
    float rotate[4 * 4] = {cosVal,                                         \
                           -sinVal,                                        \
                           0.0f,                                           \
                           0.0f,                                           \
                           sinVal,                                         \
                           cosVal,                                         \
                           0.0f,                                           \
                           0.0f,                                           \
                           0.0f,                                           \
                           0.0f,                                           \
                           1.0f,                                           \
                           0.0f,                                           \
                           (xPivot * (1.0f - cosVal)) - (yPivot * sinVal), \
                           (yPivot * (1.0f - cosVal)) + (xPivot * sinVal), \
                           0.0f,                                           \
                           1.0f};

#define MULTIPLY_MATRIX(A, B, transform)                                     \
    for (auto i = 0; i < 4; ++i) {                                           \
        for (auto j = 0; j < 4; ++j) {                                       \
            float sum = 0.0;                                                 \
            for (auto k = 0; k < 4; ++k) sum += A[k * 4 + i] * B[j * 4 + k]; \
            transform[j * 4 + i] = sum;                                      \
        }                                                                    \
    }

#define GET_TRANSFORMATION(xPivot, yPivot, transform) \
    MVP_MATRIX();                                     \
    ROTATION_MATRIX(xPivot, yPivot);                  \
    MULTIPLY_MATRIX(mvp, rotate, transform);

class GlPoint
{
public:
    float x = 0.0f;
    float y = 0.0f;

    GlPoint() = default;

    GlPoint(float pX, float pY) : x(pX), y(pY)
    {
    }

    GlPoint(const Point &rhs) : GlPoint(rhs.x, rhs.y)
    {
    }

    GlPoint(const GlPoint &rhs) = default;
    GlPoint(GlPoint &&rhs) = default;

    GlPoint &operator=(const GlPoint &rhs) = default;
    GlPoint &operator=(GlPoint &&rhs) = default;

    GlPoint &operator=(const Point &rhs)
    {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }

    bool operator==(const GlPoint &rhs)
    {
        if (&rhs == this) return true;
        if (rhs.x == this->x && rhs.y == this->y) return true;
        return false;
    }

    bool operator!=(const GlPoint &rhs)
    {
        if (&rhs == this) return true;
        if (rhs.x != this->x || rhs.y != this->y) return true;
        return false;
    }

    GlPoint operator+(const GlPoint &rhs) const
    {
        return GlPoint(x + rhs.x, y + rhs.y);
    }

    GlPoint operator+(const float c) const
    {
        return GlPoint(x + c, y + c);
    }

    GlPoint operator-(const GlPoint &rhs) const
    {
        return GlPoint(x - rhs.x, y - rhs.y);
    }

    GlPoint operator-(const float c) const
    {
        return GlPoint(x - c, y - c);
    }

    GlPoint operator*(const GlPoint &rhs) const
    {
        return GlPoint(x * rhs.x, y * rhs.y);
    }

    GlPoint operator*(const float c) const
    {
        return GlPoint(x * c, y * c);
    }

    GlPoint operator/(const GlPoint &rhs) const
    {
        return GlPoint(x / rhs.x, y / rhs.y);
    }

    GlPoint operator/(const float c) const
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


class GlGeometry
{
public:
    GlGeometry();
    ~GlGeometry();

    bool   tessellate(const RenderShape &rshape, RenderUpdateFlag flag, GLStageBuffer *vertexBuffer,
                      GLStageBuffer *indexBuffer);
    void   draw(RenderUpdateFlag flag);
    void   updateTransform(const RenderTransform *transform, float w, float h);
    float *getTransforMatrix();
    GlSize getPrimitiveSize() const;

private:
    void bindBuffers();

private:
    GLuint mVao = 0;
    float  mTransform[16] = {0.f};

    GlGpuBufferView mVertexBufferView = {};
    GlGpuBufferView mIndexBufferView = {};

    uint32_t mDrawCount = 0;
};

#endif /* _TVG_GL_GEOMETRY_H_ */
