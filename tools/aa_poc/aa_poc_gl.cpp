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

#include "aa_poc_gl.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "lodepng.h"

namespace aa_poc
{
namespace
{

GLuint compileShader(GLenum type, const char* source, const char* diagnosticName)
{
    auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;

    char log[4096] = {};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::fprintf(stderr, "%s: shader compilation failed:\n%s\n", diagnosticName, log);
    glDeleteShader(shader);
    return 0;
}

} // namespace

#ifdef __APPLE__

bool GlContext::init()
{
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAOpenGLProfile, static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_GL3_Core),
        kCGLPFAColorSize, static_cast<CGLPixelFormatAttribute>(32),
        kCGLPFADepthSize, static_cast<CGLPixelFormatAttribute>(24),
        kCGLPFAStencilSize, static_cast<CGLPixelFormatAttribute>(8),
        static_cast<CGLPixelFormatAttribute>(0)
    };

    CGLPixelFormatObj pixelFormat = nullptr;
    GLint count = 0;
    if (CGLChoosePixelFormat(attrs, &pixelFormat, &count) != kCGLNoError || !pixelFormat) {
        return false;
    }
    auto result = CGLCreateContext(pixelFormat, nullptr, &context);
    CGLDestroyPixelFormat(pixelFormat);
    if (result != kCGLNoError || !context) return false;
    return makeCurrent();
}

bool GlContext::makeCurrent()
{
    return CGLSetCurrentContext(context) == kCGLNoError;
}

GlContext::~GlContext()
{
    if (!context) return;
    CGLSetCurrentContext(nullptr);
    CGLDestroyContext(context);
}

#else

bool GlContext::initDisplay()
{
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr) == EGL_TRUE) {
        return true;
    }

    display = EGL_NO_DISPLAY;
    auto queryDevices = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(
        eglGetProcAddress("eglQueryDevicesEXT"));
    auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!getPlatformDisplay) {
        getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplay"));
    }
    if (!queryDevices || !getPlatformDisplay) return false;

    EGLDeviceEXT device = nullptr;
    EGLint count = 0;
    if (queryDevices(1, &device, &count) != EGL_TRUE || count <= 0) return false;
    display = getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, device, nullptr);
    return display != EGL_NO_DISPLAY &&
           eglInitialize(display, nullptr, nullptr) == EGL_TRUE;
}

bool GlContext::init()
{
    if (!initDisplay()) return false;

#if defined(AA_POC_GLES)
    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) return false;
    constexpr EGLint renderable = EGL_OPENGL_ES3_BIT;
#else
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) return false;
    constexpr EGLint renderable = EGL_OPENGL_BIT;
#endif

    const EGLint configAttrs[] = {
        EGL_RENDERABLE_TYPE, renderable,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (eglChooseConfig(display, configAttrs, &config, 1, &count) != EGL_TRUE || count <= 0) {
        return false;
    }

#if defined(AA_POC_GLES)
    const EGLint contextAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
#else
    const EGLint contextAttrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
#endif
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
    if (context == EGL_NO_CONTEXT) return false;

    const EGLint surfaceAttrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, surfaceAttrs);
    if (surface == EGL_NO_SURFACE) return false;
    return makeCurrent();
}

bool GlContext::makeCurrent()
{
    return eglMakeCurrent(display, surface, surface, context) == EGL_TRUE;
}

GlContext::~GlContext()
{
    if (display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
    eglTerminate(display);
    eglReleaseThread();
}

#endif

RenderTarget::~RenderTarget()
{
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (color) glDeleteRenderbuffers(1, &color);
    if (depthStencil) glDeleteRenderbuffers(1, &depthStencil);
}

bool RenderTarget::init(uint32_t width, uint32_t height, uint32_t samples,
                        const char* diagnosticName)
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    if (samples > 1) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);

    glGenRenderbuffers(1, &depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    if (samples > 1) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8,
                                         width, height);
    } else {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) return true;
    std::fprintf(stderr, "%s: framebuffer incomplete (0x%x)\n", diagnosticName, status);
    return false;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource,
                     const AttributeBinding* bindings, size_t bindingCount,
                     const char* diagnosticName)
{
    auto vertex = compileShader(GL_VERTEX_SHADER, vertexSource, diagnosticName);
    auto fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, diagnosticName);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }

    auto program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    for (size_t i = 0; i < bindingCount; ++i) {
        glBindAttribLocation(program, bindings[i].index, bindings[i].name);
    }
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) return program;

    char log[4096] = {};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::fprintf(stderr, "%s: shader link failed:\n%s\n", diagnosticName, log);
    glDeleteProgram(program);
    return 0;
}

void clearFramebuffer(GLuint framebuffer, uint32_t width, uint32_t height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMask(0xff);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearStencil(0);
#if defined(AA_POC_GLES)
    glClearDepthf(0.0f);
#else
    glClearDepth(0.0);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

bool writeFramebufferPng(const std::string& filename, GLuint framebuffer,
                         uint32_t width, uint32_t height, uint32_t downsample,
                         const char* diagnosticName)
{
    auto outputWidth = width / downsample;
    auto outputHeight = height / downsample;
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    std::vector<unsigned char> output(static_cast<size_t>(outputWidth) * outputHeight * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (uint32_t y = 0; y < outputHeight; ++y) {
        for (uint32_t x = 0; x < outputWidth; ++x) {
            uint32_t sum[4] = {};
            for (uint32_t sampleY = 0; sampleY < downsample; ++sampleY) {
                auto sourceY = height - 1 - (y * downsample + sampleY);
                for (uint32_t sampleX = 0; sampleX < downsample; ++sampleX) {
                    auto source = pixels.data() +
                                  (static_cast<size_t>(sourceY) * width +
                                   x * downsample + sampleX) * 4;
                    for (uint32_t channel = 0; channel < 4; ++channel) {
                        sum[channel] += source[channel];
                    }
                }
            }
            auto destination = output.data() +
                               (static_cast<size_t>(y) * outputWidth + x) * 4;
            auto sampleCount = downsample * downsample;
            for (uint32_t channel = 0; channel < 4; ++channel) {
                destination[channel] = static_cast<unsigned char>(
                    (sum[channel] + sampleCount / 2) / sampleCount);
            }
            // GL render targets contain premultiplied RGBA, while PNG alpha is
            // unassociated.  Unpremultiply only after box filtering so the
            // SSAA resolve remains linear in premultiplied space.
            auto alpha = destination[3];
            if (alpha == 0) {
                destination[0] = destination[1] = destination[2] = 0;
            } else if (alpha < 255) {
                for (uint32_t channel = 0; channel < 3; ++channel) {
                    auto straight = (static_cast<uint32_t>(destination[channel]) * 255u +
                                     alpha / 2u) /
                                    alpha;
                    destination[channel] = static_cast<unsigned char>(std::min(straight, 255u));
                }
            }
        }
    }

    lodepng::State state;
    state.encoder.auto_convert = 0;
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;
    state.info_png.color.colortype = LCT_RGBA;
    state.info_png.color.bitdepth = 8;
    std::vector<unsigned char> encoded;
    auto error = lodepng::encode(encoded, output, outputWidth, outputHeight, state);
    if (!error) error = lodepng::save_file(encoded, filename);
    if (!error) return true;
    std::fprintf(stderr, "%s: PNG encode failed for %s: %s\n", diagnosticName,
                 filename.c_str(), lodepng_error_text(error));
    return false;
}

void printGlInfo()
{
    auto version = glGetString(GL_VERSION);
    std::printf("GL: %s\n", version ? reinterpret_cast<const char*>(version) : "unknown");
#if defined(AA_POC_GLES)
    std::printf("ThorVG compatibility target: OpenGL ES 3.0 / GLSL ES 3.00\n");
#else
    std::printf("ThorVG compatibility target: OpenGL 3.3 / GLSL 3.30\n");
#endif
}

} // namespace aa_poc
