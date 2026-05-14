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
#include "testGlEngine.h"

using namespace tvg;
using namespace std;

#if defined(THORVG_GL_TEST_SUPPORT)

TEST_CASE("GL Missing Initialization", "[tvgGlCanvas]")
{
    auto canvas = unique_ptr<GlCanvas>(GlCanvas::gen());
    REQUIRE(canvas == nullptr);
}

TEST_CASE("GL Basic Creation", "[tvgGlCanvas]")
{
    TestGLEngine engine;

    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<GlCanvas>(GlCanvas::gen());
        REQUIRE(canvas);

        auto canvas2 = unique_ptr<GlCanvas>(GlCanvas::gen());
        REQUIRE(canvas2);

        auto canvas3 = unique_ptr<GlCanvas>(GlCanvas::gen());
        REQUIRE(canvas3);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Pushing Paints", "[tvgGlCanvas]")
{
    TestGLEngine engine;

    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<GlCanvas>(GlCanvas::gen());
        REQUIRE(canvas);

        engine.target(canvas.get());

        // Try all types of paints.
        REQUIRE(canvas->add(Shape::gen()) == Result::Success);
        REQUIRE(canvas->add(Picture::gen()) == Result::Success);
        REQUIRE(canvas->add(Scene::gen()) == Result::Success);

        // Cases by contexts.
        REQUIRE(canvas->update() == Result::Success);

        REQUIRE(canvas->add(Shape::gen()) == Result::Success);
        REQUIRE(canvas->add(Shape::gen()) == Result::Success);

        REQUIRE(canvas->remove() == Result::Success);

        Paint* paints[2];

        paints[0] = Shape::gen();
        REQUIRE(canvas->add(paints[0]) == Result::Success);

        // Negative case 1
        REQUIRE(canvas->add(nullptr) == Result::InvalidArguments);

        paints[1] = Shape::gen();
        REQUIRE(canvas->add(paints[1]) == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);

        // Check list of paints
        auto list = canvas->paints();
        REQUIRE(list.size() == 2);
        int idx = 0;
        for (auto paint : list) {
            REQUIRE(paints[idx] == paint);
            ++idx;
        }
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Update", "[tvgGlCanvas]")
{
    TestGLEngine engine;

    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<GlCanvas>(GlCanvas::gen());
        REQUIRE(canvas);

        engine.target(canvas.get());

        REQUIRE(canvas->update() == Result::Success);

        REQUIRE(canvas->add(Shape::gen()) == Result::Success);

        // No added shape
        auto shape = Shape::gen();

        // Normal case
        REQUIRE(canvas->add(shape) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->update() == Result::InsufficientCondition);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(canvas->update() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Synchronized Drawing", "[tvgGlCanvas]")
{
    TestGLEngine engine;

    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<GlCanvas>(GlCanvas::gen());
        REQUIRE(canvas);

        REQUIRE(canvas->sync() == Result::Success);
        REQUIRE(canvas->draw() == Result::InsufficientCondition);

        engine.target(canvas.get());

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        // Invalid Shape
        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(canvas->add(shape) == Result::Success);

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        auto shape2 = Shape::gen();
        REQUIRE(shape2);
        REQUIRE(shape2->appendRect(0, 0, 100, 100) == Result::Success);
        REQUIRE(shape2->fill(255, 255, 255, 255) == Result::Success);

        REQUIRE(canvas->add(shape2) == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("GL Viewport", "[tvgGlCanvas]")
{
    TestGLEngine engine;

    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<GlCanvas>(GlCanvas::gen());
        REQUIRE(canvas);

        REQUIRE(canvas->viewport(25, 25, 100, 100) == Result::Success);

        engine.target(canvas.get());

        REQUIRE(canvas->viewport(25, 25, 50, 50) == Result::Success);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->appendRect(0, 0, 100, 100) == Result::Success);
        REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);

        REQUIRE(canvas->add(shape) == Result::Success);

        // Negative, not allowed
        REQUIRE(canvas->viewport(15, 25, 5, 5) == Result::InsufficientCondition);

        REQUIRE(canvas->draw() == Result::Success);

        // Negative, not allowed
        REQUIRE(canvas->viewport(25, 25, 10, 10) == Result::InsufficientCondition);

        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
