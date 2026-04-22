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

#include <thorvg.h>
#include <memory>
#include "catch.hpp"
#include "testRenderer.h"
#include "testWgWindow.h"

using namespace tvg;

#if defined(THORVG_WG_ENGINE_SUPPORT) && defined(THORVG_WG_TEST_SUPPORT)

TEST_CASE("WG Basic draw", "[tvgWgEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWgWindow(100, 100);
        auto canvas = std::unique_ptr<WgCanvas>(WgCanvas::gen());
        REQUIRE(canvas);
        REQUIRE(window->target(canvas.get()) == Result::Success);

        TestRenderer::basic(*canvas,
                            {BlendMethod::Normal, BlendMethod::Multiply, BlendMethod::Screen},
                            {MaskMethod::None, MaskMethod::Alpha});
        REQUIRE(window->hasRenderedContent());
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("WG Image Draw", "[tvgWgEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWgWindow(100, 100);
        auto canvas = std::unique_ptr<WgCanvas>(WgCanvas::gen());
        REQUIRE(canvas);
        REQUIRE(window->target(canvas.get()) == Result::Success);

        REQUIRE(TestRenderer::image(*canvas,
                                    {BlendMethod::Normal, BlendMethod::Multiply, BlendMethod::Screen},
                                    {MaskMethod::None, MaskMethod::Alpha}));
        REQUIRE(window->hasRenderedContent());
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("WG Filling Draw", "[tvgWgEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWgWindow(100, 100);
        auto canvas = std::unique_ptr<WgCanvas>(WgCanvas::gen());
        REQUIRE(canvas);
        REQUIRE(window->target(canvas.get()) == Result::Success);

        TestRenderer::filling(*canvas);
        REQUIRE(window->hasRenderedContent());
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("WG Image Rotation", "[tvgWgEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWgWindow(960, 960);
        auto canvas = std::unique_ptr<WgCanvas>(WgCanvas::gen());
        REQUIRE(canvas);
        REQUIRE(window->target(canvas.get()) == Result::Success);

        REQUIRE(TestRenderer::imageRotation(*canvas));
        REQUIRE(window->hasRenderedContent());
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
