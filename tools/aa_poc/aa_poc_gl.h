/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef THORVG_TOOLS_AA_POC_GL_H
#define THORVG_TOOLS_AA_POC_GL_H

#include <cstddef>
#include <cstdint>
#include <string>

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION 1
#endif
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#if defined(AA_POC_GLES)
#include <GLES3/gl3.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif

namespace aa_poc
{

class GlContext
{
public:
    GlContext() = default;
    ~GlContext();

    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;

    bool init();
    bool makeCurrent();

    template<typename Canvas, typename ColorSpace>
    auto target(Canvas& canvas, GLuint framebuffer, uint32_t width, uint32_t height,
                ColorSpace colorSpace)
    {
        makeCurrent();
#ifdef __APPLE__
        return canvas.target(nullptr, nullptr, context, static_cast<int32_t>(framebuffer),
                             width, height, colorSpace);
#else
        return canvas.target(display, surface, context, static_cast<int32_t>(framebuffer),
                             width, height, colorSpace);
#endif
    }

private:
#ifdef __APPLE__
    CGLContextObj context = nullptr;
#else
    bool initDisplay();

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
#endif
};

class RenderTarget
{
public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    bool init(uint32_t width, uint32_t height, uint32_t samples,
              const char* diagnosticName);
    GLuint framebuffer() const { return fbo; }

private:
    GLuint fbo = 0;
    GLuint color = 0;
    GLuint depthStencil = 0;
};

struct AttributeBinding
{
    GLuint index;
    const char* name;
};

GLuint createProgram(const char* vertexSource, const char* fragmentSource,
                     const AttributeBinding* bindings, size_t bindingCount,
                     const char* diagnosticName);

// Establishes only the state required for a full white color/depth/stencil clear.
// Renderers keep ownership of their depth, blend, cull, and draw-state policy.
void clearFramebuffer(GLuint framebuffer, uint32_t width, uint32_t height);

bool writeFramebufferPng(const std::string& filename, GLuint framebuffer,
                         uint32_t width, uint32_t height, uint32_t downsample,
                         const char* diagnosticName);

void printGlInfo();

} // namespace aa_poc

#endif // THORVG_TOOLS_AA_POC_GL_H
