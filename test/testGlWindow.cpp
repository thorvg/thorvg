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

#include <algorithm>
#include <vector>
#include "catch.hpp"
#include "testGlWindow.h"
#include "testGlHelpers.h"

using namespace tvg;

#if defined(THORVG_GL_ENGINE_SUPPORT) && defined(THORVG_GL_TEST_SUPPORT)

GlWindow::~GlWindow()
{
    if (context) {
        SDL_GL_MakeCurrent(nullptr, nullptr);
        SDL_GL_DeleteContext(context);
        context = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    if (videoInitialized) {
        SDL_Quit();
        videoInitialized = false;
    }
}

bool GlWindow::init(uint32_t w, uint32_t h)
{
    width = w;
    height = h;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    videoInitialized = true;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    window = SDL_CreateWindow("thorvg-gl-test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              static_cast<int>(w), static_cast<int>(h), SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) return false;

    context = SDL_GL_CreateContext(window);
    if (!context) return false;

    if (!makeCurrent()) return false;
    SDL_GL_SetSwapInterval(0);
    clear();
    return true;
}

bool GlWindow::makeCurrent()
{
    return window && context && SDL_GL_MakeCurrent(window, context) == 0;
}

Result GlWindow::target(GlCanvas* canvas)
{
    if (!makeCurrent()) return Result::Unknown;
    clear();
    return canvas->target(nullptr, nullptr, context, 0, width, height, ColorSpace::ABGR8888S);
}

bool GlWindow::hasRenderedContent()
{
    REQUIRE_NO_GL_ERROR("before hasRenderedContent");

    if (!makeCurrent()) return false;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    REQUIRE_GL(glFinish());
    REQUIRE_GL(glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
    return std::any_of(pixels.begin(), pixels.end(), [](uint8_t value) { return value != 0; });
}

void GlWindow::clear()
{
    REQUIRE_NO_GL_ERROR("before clear");

    REQUIRE_GL(glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height)));
    REQUIRE_GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    REQUIRE_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
}

std::unique_ptr<GlWindow> genWindow(uint32_t w, uint32_t h)
{
    auto window = std::make_unique<GlWindow>();
    INFO("Failed to create an offscreen OpenGL context.");
    REQUIRE(window->init(w, h));
    return window;
}

#endif
