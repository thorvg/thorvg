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

#include "engine.h"
#include "testGlHelpers.h"

using namespace tvg;

#if defined(THORVG_GL_ENGINE_SUPPORT) && defined(THORVG_GL_TEST_SUPPORT)

TvgGlTestEngine::~TvgGlTestEngine()
{
    release();
    eglReleaseThread();
}

bool TvgGlTestEngine::init(uint32_t w, uint32_t h, ColorSpace cs)
{
    release();
    width = w;
    height = h;
    colorSpace = cs;
    return setup();
}

bool TvgGlTestEngine::init(uint32_t w, uint32_t h)
{
    return init(w, h, ColorSpace::ABGR8888S);
}

std::unique_ptr<Canvas> TvgGlTestEngine::canvas()
{
    return std::unique_ptr<Canvas>(GlCanvas::gen());
}

Result TvgGlTestEngine::target(Canvas* canvas)
{
    if (!canvas) return Result::InvalidArguments;
    if (!makeCurrent()) return Result::Unknown;
    clear();
    return static_cast<GlCanvas*>(canvas)->target(display, surface, context, 0, width, height, colorSpace);
}

bool TvgGlTestEngine::rendered()
{
    if (!makeCurrent()) return false;
    REQUIRE_NO_GL_ERROR("before rendered");

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    REQUIRE_GL(glFinish());
    REQUIRE_GL(glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
    return std::any_of(pixels.begin(), pixels.end(), [](uint8_t value) { return value != 0; });
}

bool TvgGlTestEngine::setup()
{
    if (!initDisplay()) return false;
    if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) return false;
    if (!initConfig()) return false;
    if (!initSurface()) return false;
    if (!initContext()) return false;
    if (!makeCurrent()) return false;
    clear();
    return true;
}

void TvgGlTestEngine::release()
{
    if (display == EGL_NO_DISPLAY) return;

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }
    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    eglTerminate(display);
    display = EGL_NO_DISPLAY;
    config = nullptr;
}

bool TvgGlTestEngine::initDisplay()
{
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) return false;
    if (eglInitialize(display, nullptr, nullptr) == EGL_TRUE) return true;

    display = EGL_NO_DISPLAY;

    auto queryDevicesExt = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    if (!queryDevicesExt) return false;

    auto getPlatformDisplayExt = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!getPlatformDisplayExt) {
        getPlatformDisplayExt = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplay"));
    }

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

bool TvgGlTestEngine::initConfig()
{
    const EGLint attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLint count = 0;
    return eglChooseConfig(display, attrs, &config, 1, &count) == EGL_TRUE && count > 0;
}

bool TvgGlTestEngine::initContext()
{
    const EGLint attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    return context != EGL_NO_CONTEXT;
}

bool TvgGlTestEngine::initSurface()
{
    const EGLint attrs[] = {
        EGL_WIDTH, static_cast<EGLint>(width),
        EGL_HEIGHT, static_cast<EGLint>(height),
        EGL_NONE
    };

    surface = eglCreatePbufferSurface(display, config, attrs);
    return surface != EGL_NO_SURFACE;
}

bool TvgGlTestEngine::makeCurrent()
{
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT) return false;
    return eglMakeCurrent(display, surface, surface, context) == EGL_TRUE;
}

void TvgGlTestEngine::clear()
{
    REQUIRE_NO_GL_ERROR("before clear");

    REQUIRE_GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    REQUIRE_GL(glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height)));
    REQUIRE_GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    REQUIRE_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
}

#endif
