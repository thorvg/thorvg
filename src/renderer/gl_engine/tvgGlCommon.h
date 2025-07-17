/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_GL_COMMON_H_
#define _TVG_GL_COMMON_H_

#include <cassert>
#include "tvgGl.h"
#include "tvgRender.h"
#include "tvgMath.h"

#define MIN_GL_STROKE_WIDTH 1.0f

#define MVP_MATRIX(w, h)                \
    float mvp[4*4] = {                  \
        2.f / w,      0.0,  0.0f, 0.0f, \
        0.0,     -2.f / h,  0.0f, 0.0f, \
        0.0f,        0.0f, -1.0f, 0.0f, \
        -1.f,         1.f,  0.0f, 1.0f  \
    };

#define MULTIPLY_MATRIX(A, B, transform)    \
    for(auto i = 0; i < 4; ++i) {           \
        for(auto j = 0; j < 4; ++j) {       \
            float sum = 0.0;                \
            for (auto k = 0; k < 4; ++k)    \
                sum += A[k*4+i] * B[j*4+k]; \
            transform[j*4+i] = sum;         \
        }                                   \
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
#define GET_MATRIX44(mat3, mat4) \
    do {                         \
        mat4[0] = mat3.e11;      \
        mat4[1] = mat3.e21;      \
        mat4[2] = 0;             \
        mat4[3] = mat3.e31;      \
        mat4[4] = mat3.e12;      \
        mat4[5] = mat3.e22;      \
        mat4[6] = 0;             \
        mat4[7] = mat3.e32;      \
        mat4[8] = 0;             \
        mat4[9] = 0;             \
        mat4[10] = 1;            \
        mat4[11] = 0;            \
        mat4[12] = mat3.e13;     \
        mat4[13] = mat3.e23;     \
        mat4[14] = 0;            \
        mat4[15] = mat3.e33;     \
    } while (false)



enum class GlStencilMode {
    None,
    FillNonZero,
    FillEvenOdd,
    Stroke,
};


class GlStageBuffer;
class GlRenderTask;

struct GlGeometryBuffer {
    Array<float> vertex;
    Array<uint32_t> index;

    void clear()
    {
        vertex.clear();
        index.clear();
    }

};

struct GlGeometry
{
    bool tesselate(const RenderShape& rshape, RenderUpdateFlag flag);
    bool tesselate(const RenderSurface* image, RenderUpdateFlag flag);
    bool draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag);
    GlStencilMode getStencilMode(RenderUpdateFlag flag);
    RenderRegion getBounds() const;

    GlGeometryBuffer fill, stroke;
    Matrix matrix = {};
    RenderRegion viewport = {};
    RenderRegion bounds = {};
    FillRule fillRule = FillRule::NonZero;
};


struct GlShape
{
  const RenderShape* rshape = nullptr;
  float viewWd;
  float viewHt;
  uint32_t opacity = 0;
  GLuint texId = 0;
  uint32_t texFlipY = 0;
  ColorSpace texColorSpace = ColorSpace::ABGR8888;
  RenderUpdateFlag updateFlag = None;
  GlGeometry geometry;
  Array<RenderData> clips;
};

#define MAX_GRADIENT_STOPS 16

struct GlLinearGradientBlock
{
    alignas(16) float nStops[4] = {};
    alignas(16) float startPos[2] = {};
    alignas(8) float stopPos[2] = {};
    alignas(8) float stopPoints[MAX_GRADIENT_STOPS] = {};
    alignas(16) float stopColors[4 * MAX_GRADIENT_STOPS] = {};
};

struct GlRadialGradientBlock
{
    alignas(16) float nStops[4] = {};
    alignas(16) float centerPos[4] = {};
    alignas(16) float radius[2] = {};
    alignas(16) float stopPoints[MAX_GRADIENT_STOPS] = {};
    alignas(16) float stopColors[4 * MAX_GRADIENT_STOPS] = {};
};

struct GlCompositor : RenderCompositor
{
    RenderRegion bbox = {};

    GlCompositor(const RenderRegion& box) : bbox(box) {}
};


#endif /* _TVG_GL_COMMON_H_ */