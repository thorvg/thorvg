/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_TEST_GL_HELPERS_H_
#define _TVG_TEST_GL_HELPERS_H_

#include "catch.hpp"

#ifdef THORVG_GL_TEST_SUPPORT
    #include <SDL_opengl.h>
#endif

#if defined(THORVG_GL_ENGINE_SUPPORT) && defined(THORVG_GL_TEST_SUPPORT)

/**
 * GL_STACK_OVERFLOW and GL_STACK_UNDERFLOW are legacy desktop GL errors
 * and are not defined in OpenGL ES 2.0.
 */
inline const char* glErrorName(GLenum error)
{
    switch (error) {
        case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
#ifdef GL_STACK_OVERFLOW 
        case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
#endif
#ifdef GL_STACK_UNDERFLOW
        case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
#endif
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default: return "GL_UNKNOWN_ERROR";
    }
}

#define REQUIRE_GL(expression) \
    do { \
        expression; \
        auto error = glGetError(); \
        INFO(__FILE__ << ":" << __LINE__ << " " << #expression << " -> " << glErrorName(error)); \
        REQUIRE(error == GL_NO_ERROR); \
    } while (0)

#define REQUIRE_NO_GL_ERROR(label) \
    do { \
        auto error = glGetError(); \
        INFO(__FILE__ << ":" << __LINE__ << " " << label << " -> " << glErrorName(error)); \
        REQUIRE(error == GL_NO_ERROR); \
    } while (0)

#endif

#endif
