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

#ifndef _TVG_TEST_GL_ENGINE_H_
#define _TVG_TEST_GL_ENGINE_H_

#include "config.h"

#if defined(THORVG_GL_TEST_SUPPORT)

#include <thorvg.h>

#ifdef __APPLE__

// macOS has no native EGL; the engine loads Apple's OpenGL.framework, so test with CGL.
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

using namespace tvg;

struct TestGLEngine
{
    CGLContextObj context = nullptr;
    GLuint fbo = 0;
    GLuint colorBuf = 0;
    GLuint depthStencilBuf = 0;
    uint32_t width;
    uint32_t height;
    ColorSpace colorSpace;

    TestGLEngine(uint32_t w = 100, uint32_t h = 100, ColorSpace cs = ColorSpace::ABGR8888S) : width(w), height(h), colorSpace(cs)
    {
        CGLPixelFormatAttribute attrs[] = {
            kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL3_Core,
            kCGLPFAColorSize, (CGLPixelFormatAttribute)32,
            kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
            kCGLPFAStencilSize, (CGLPixelFormatAttribute)8,
            (CGLPixelFormatAttribute)0};

        CGLPixelFormatObj pixelFormat = nullptr;
        GLint numFormats = 0;
        CGLChoosePixelFormat(attrs, &pixelFormat, &numFormats);
        CGLCreateContext(pixelFormat, nullptr, &context);
        CGLDestroyPixelFormat(pixelFormat);
        CGLSetCurrentContext(context);

        // render into an offscreen fbo
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &colorBuf);
        glBindRenderbuffer(GL_RENDERBUFFER, colorBuf);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuf);
        glGenRenderbuffers(1, &depthStencilBuf);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuf);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuf);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    ~TestGLEngine()
    {
        CGLSetCurrentContext(context);
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &colorBuf);
        glDeleteRenderbuffers(1, &depthStencilBuf);
        CGLSetCurrentContext(nullptr);
        CGLDestroyContext(context);
    }

    void target(GlCanvas* canvas)
    {
        CGLSetCurrentContext(context);
        canvas->target(nullptr, nullptr, context, static_cast<int32_t>(fbo), width, height, colorSpace);
    }
};

#else

#include <EGL/egl.h>
#include <EGL/eglext.h>

using namespace tvg;

struct TestGLEngine
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    uint32_t width;
    uint32_t height;
    ColorSpace colorSpace;

    TestGLEngine(uint32_t w = 100, uint32_t h = 100, ColorSpace cs = ColorSpace::ABGR8888S) : width(w), height(h), colorSpace(cs)
    {
        if (initDisplay()) {
#if defined(THORVG_GL_TARGET_GLES)
            eglBindAPI(EGL_OPENGL_ES_API);
#else
            eglBindAPI(EGL_OPENGL_API);
#endif

            // Choose config
            const EGLint configAttrs[] = {
#if defined(THORVG_GL_TARGET_GLES)
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
#else
                EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_DEPTH_SIZE, 24,
                EGL_STENCIL_SIZE, 8,
                EGL_NONE};
            EGLint count = 0;

            if (eglChooseConfig(display, configAttrs, &config, 1, &count) != EGL_TRUE || count <= 0) return;

            // Create context
            const EGLint contextAttrs[] = {
#if defined(THORVG_GL_TARGET_GLES)
                EGL_CONTEXT_CLIENT_VERSION, 3,
#else
                EGL_CONTEXT_MAJOR_VERSION, 3,
                EGL_CONTEXT_MINOR_VERSION, 3,
                EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#endif
                EGL_NONE};

            context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
            if (context == EGL_NO_CONTEXT) return;

            // Create surface
            const EGLint surfaceAttrs[] = {
                EGL_WIDTH, static_cast<EGLint>(width),
                EGL_HEIGHT, static_cast<EGLint>(height),
                EGL_NONE};

            surface = eglCreatePbufferSurface(display, config, surfaceAttrs);
            if (surface == EGL_NO_SURFACE) return;

            eglMakeCurrent(display, surface, surface, context);
        }
    }

    ~TestGLEngine()
    {
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            eglTerminate(display);
        }
        eglReleaseThread();
    }

    void target(GlCanvas* canvas)
    {
        canvas->target(display, surface, context, 0, width, height, colorSpace);
    }

    bool initDisplay()
    {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) return false;
        if (eglInitialize(display, nullptr, nullptr) == EGL_TRUE) return true;

        display = EGL_NO_DISPLAY;

        auto queryDevicesExt = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
        if (!queryDevicesExt) return false;

        auto getPlatformDisplayExt = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (!getPlatformDisplayExt) getPlatformDisplayExt = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplay"));

        if (!getPlatformDisplayExt) return false;

        EGLDeviceEXT device = nullptr;
        EGLint numDevices = 0;
        if (queryDevicesExt(1, &device, &numDevices) != EGL_TRUE || numDevices <= 0) return false;

        display = getPlatformDisplayExt(EGL_PLATFORM_DEVICE_EXT, device, nullptr);
        if (display == EGL_NO_DISPLAY) return false;
        if (eglInitialize(display, nullptr, nullptr) != EGL_TRUE) {
            display = EGL_NO_DISPLAY;
            return false;
        }

        return true;
    }
};

#endif
#endif
#endif
