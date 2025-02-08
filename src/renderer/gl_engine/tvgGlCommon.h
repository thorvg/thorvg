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

#include <assert.h>
#include <memory>
#if defined (THORVG_GL_TARGET_GLES)
    #include <GLES3/gl3.h>
    #define TVG_REQUIRE_GL_MAJOR_VER 3
    #define TVG_REQUIRE_GL_MINOR_VER 0
#else
    #if defined(__APPLE__) || defined(__MACH__)
        #include <OpenGL/gl3.h>
    #else
        #define GL_GLEXT_PROTOTYPES 1
        #include <GL/gl.h>
    #endif
    #define TVG_REQUIRE_GL_MAJOR_VER 3
    #define TVG_REQUIRE_GL_MINOR_VER 3
#endif
#include "tvgCommon.h"
#include "tvgRender.h"
#include "tvgMath.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten/html5_webgl.h>
    // query GL Error on WebGL is very slow, so disable it on WebGL
    #define GL_CHECK(x) x
#else
    #define GL_CHECK(x) \
        x; \
        do { \
          GLenum glError = glGetError(); \
          if(glError != GL_NO_ERROR) { \
            TVGERR("GL_ENGINE", "glGetError() = %i (0x%.8x)", glError, glError); \
            assert(0); \
          } \
        } while(0)
#endif

#define MIN_GL_STROKE_WIDTH 1.0f

#define MVP_MATRIX(w, h) \
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



static inline float getScaleFactor(const Matrix& m)
{
    return sqrtf(m.e11 * m.e11 + m.e21 * m.e21);
}

enum class GlStencilMode {
    None,
    FillNonZero,
    FillEvenOdd,
    Stroke,
};


class GlStageBuffer;
class GlRenderTask;

class GlGeometry
{
public:
    bool tesselate(const RenderShape& rshape, RenderUpdateFlag flag);
    bool tesselate(const RenderSurface* image, RenderUpdateFlag flag);
    void disableVertex(uint32_t location);
    bool draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag);
    void updateTransform(const Matrix& m);
    void setViewport(const RenderRegion& viewport);
    const RenderRegion& getViewport();
    const Matrix& getTransformMatrix();
    GlStencilMode getStencilMode(RenderUpdateFlag flag);
    RenderRegion getBounds() const;

private:
    RenderRegion viewport = {};
    Array<float> fillVertex;
    Array<float> strokeVertex;
    Array<uint32_t> fillIndex;
    Array<uint32_t> strokeIndex;
    Matrix mMatrix = {};
    FillRule mFillRule = FillRule::NonZero;
    RenderRegion mBounds = {};
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
    BlendMethod blend = BlendMethod::Normal;
    GlCompositor(const RenderRegion& box) : bbox(box) {}
};

#endif /* _TVG_GL_COMMON_H_ */
