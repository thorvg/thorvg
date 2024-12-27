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

#ifdef __EMSCRIPTEN__
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

class GlGeometry;

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
  unique_ptr<GlGeometry> geometry;
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
