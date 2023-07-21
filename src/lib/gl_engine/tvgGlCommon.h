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

#ifndef _TVG_GL_COMMON_H_
#define _TVG_GL_COMMON_H_

#include <assert.h>
#ifdef OS_ANDROID
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include "tvgCommon.h"
#include "tvgRender.h"


#define GL_CHECK(x) \
        x; \
        do { \
          GLenum glError = glGetError(); \
          if(glError != GL_NO_ERROR) { \
            TVGERR("GL_ENGINE", "glGetError() = %i (0x%.8x)", glError, glError); \
            assert(0); \
          } \
        } while(0)

#define EGL_CHECK(x) \
    x; \
    do { \
        EGLint eglError = eglGetError(); \
        if(eglError != EGL_SUCCESS) { \
            TVGERR("GL_ENGINE", "eglGetError() = %i (0x%.8x)", eglError, eglError); \
            assert(0); \
        } \
    } while(0)


class GlGeometry;

struct GlShape
{
  const RenderShape* rshape = nullptr;
  float viewWd;
  float viewHt;
  RenderUpdateFlag updateFlag = None;
  std::unique_ptr<GlGeometry> geometry;
};


#endif /* _TVG_GL_COMMON_H_ */
