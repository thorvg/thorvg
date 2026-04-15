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

#include <memory>
#include <thorvg.h>
#include "catch.hpp"
#include "testCanvas.h"
#include "testGlWindow.h"

using namespace tvg;

#if defined(THORVG_GL_ENGINE_SUPPORT) && defined(THORVG_GL_TEST_SUPPORT)

TEST_CASE("GL Missing Initialization", "[tvgGlCanvas]")
{
    TestCanvas::missingInitialization([]() {
        return std::unique_ptr<GlCanvas>(GlCanvas::gen());
    });
}

TEST_CASE("GL Basic Creation", "[tvgGlCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWindow(32, 32);
        (void)window;

        TestCanvas::basicCreation([]() {
            return std::unique_ptr<GlCanvas>(GlCanvas::gen());
        });
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Pushing Paints", "[tvgGlCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWindow(100, 100);

        TestCanvas::pushingPaints([]() {
            return std::unique_ptr<GlCanvas>(GlCanvas::gen());
        }, [&](GlCanvas& canvas) {
            REQUIRE(window->target(&canvas) == Result::Success);
        });
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Update", "[tvgGlCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWindow(100, 100);

        TestCanvas::update([]() {
            return std::unique_ptr<GlCanvas>(GlCanvas::gen());
        }, [&](GlCanvas& canvas) {
            REQUIRE(window->target(&canvas) == Result::Success);
        });
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Synchronized Drawing", "[tvgGlCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWindow(100, 100);

        TestCanvas::synchronizedDrawing([]() {
            return std::unique_ptr<GlCanvas>(GlCanvas::gen());
        }, [&](GlCanvas& canvas) {
            REQUIRE(window->target(&canvas) == Result::Success);
        }, [&]() {
            REQUIRE(window->hasRenderedContent());
        });
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Viewport", "[tvgGlCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto window = genWindow(100, 100);

        TestCanvas::viewport([]() {
            return std::unique_ptr<GlCanvas>(GlCanvas::gen());
        }, [&](GlCanvas& canvas) {
            REQUIRE(window->target(&canvas) == Result::Success);
        }, [&]() {
            REQUIRE(window->hasRenderedContent());
        });
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
