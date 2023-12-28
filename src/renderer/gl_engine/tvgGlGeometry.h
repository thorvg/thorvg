/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include <vector>
#include "tvgGlCommon.h"
#include "tvgMath.h"


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

class GlStageBuffer;
class GlRenderTask;

class GlGeometry
{
public:
    GlGeometry() = default;
    ~GlGeometry();

    bool tesselate(const RenderShape& rshape, RenderUpdateFlag flag);
    bool tesselate(const Surface* image, const RenderMesh* mesh, RenderUpdateFlag flag);
    void disableVertex(uint32_t location);
    bool draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag);
    void updateTransform(const RenderTransform* transform, float w, float h);
    void setViewport(const RenderRegion& viewport);
    float* getTransforMatrix();

private:
    RenderRegion viewport = {};
    Array<float> fillVertex = {};
    Array<float> strokeVertex = {};
    Array<uint32_t> fillIndex = {};
    Array<uint32_t> strokeIndex = {};
    float mTransform[16];
};

#endif /* _TVG_GL_GEOMETRY_H_ */
