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

#ifndef _TVG_TEST_ENGINE_METHOD_H_
#define _TVG_TEST_ENGINE_METHOD_H_

#include <algorithm>
#include <memory>
#include <vector>
#include <thorvg.h>
#include "config.h"

#ifdef THORVG_GL_TEST_SUPPORT
    #include <EGL/egl.h>
    #include <EGL/eglext.h>
#endif

#ifdef THORVG_WG_TEST_SUPPORT
    #include <webgpu/webgpu.h>
#endif

using namespace tvg;

struct TvgTestEngine
{
    virtual ~TvgTestEngine() {}
    virtual bool init(uint32_t w, uint32_t h) = 0;
    virtual bool init(uint32_t w, uint32_t h, ColorSpace cs) = 0;
    virtual std::unique_ptr<Canvas> canvas() = 0;
    // Sets the canvas target to this test engine's fixed headless buffer size.
    virtual Result target(Canvas* canvas) = 0;
    virtual bool rendered() = 0;
};

#ifdef THORVG_CPU_ENGINE_SUPPORT

struct TvgSwTestEngine : TvgTestEngine
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    ColorSpace colorSpace = ColorSpace::ARGB8888;
    std::vector<uint32_t> buffer;

    bool init(uint32_t w, uint32_t h) override
    {
        return init(w, h, ColorSpace::ARGB8888);
    }

    bool init(uint32_t w, uint32_t h, ColorSpace cs) override
    {
        width = w;
        height = h;
        stride = w;
        colorSpace = cs;
        buffer.resize(static_cast<size_t>(w) * h);
        clear();
        return true;
    }

    std::unique_ptr<Canvas> canvas() override
    {
        return std::unique_ptr<Canvas>(SwCanvas::gen());
    }

    Result target(Canvas* canvas) override
    {
        if (!canvas) return Result::InvalidArguments;
        clear();
        return static_cast<SwCanvas*>(canvas)->target(buffer.data(), stride, width, height, colorSpace);
    }

    bool rendered() override
    {
        return std::any_of(buffer.begin(), buffer.end(), [](uint32_t value) { return value != 0; });
    }

    void clear()
    {
        std::fill(buffer.begin(), buffer.end(), 0);
    }
};

#endif

#if defined(THORVG_GL_ENGINE_SUPPORT) && defined(THORVG_GL_TEST_SUPPORT)

struct TvgGlTestEngine : TvgTestEngine
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    ColorSpace colorSpace = ColorSpace::ABGR8888S;

    TvgGlTestEngine() {}
    ~TvgGlTestEngine();

    bool init(uint32_t w, uint32_t h) override;
    bool init(uint32_t w, uint32_t h, ColorSpace cs) override;
    std::unique_ptr<Canvas> canvas() override;
    Result target(Canvas* canvas) override;
    bool rendered() override;

private:
    bool setup();
    bool initDisplay();
    bool initConfig();
    bool initSurface();
    bool initContext();
    bool makeCurrent();
    void release();
    void clear();
};

#endif

#if defined(THORVG_WG_ENGINE_SUPPORT) && defined(THORVG_WG_TEST_SUPPORT)

struct TvgWgTestEngine : TvgTestEngine
{
    WGPUInstance instance = nullptr;
    WGPUDevice device = nullptr;
    WGPUTexture texture = nullptr;
    WGPUTextureFormat format = WGPUTextureFormat_BGRA8Unorm;
    uint32_t width = 0;
    uint32_t height = 0;
    ColorSpace colorSpace = ColorSpace::ABGR8888S;

    TvgWgTestEngine() {}
    ~TvgWgTestEngine();

    bool init(uint32_t w, uint32_t h) override;
    bool init(uint32_t w, uint32_t h, ColorSpace cs) override;
    std::unique_ptr<Canvas> canvas() override;
    Result target(Canvas* canvas) override;
    bool rendered() override;

private:
    bool setup();
    bool recreateTarget();
    void release();
    void releaseTexture();
    void clear();
};

#endif

#endif
