/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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
#include "config.h"
#include "catch.hpp"
#include "testCanvas.h"

using namespace tvg;
using namespace std;

#ifdef THORVG_CPU_ENGINE_SUPPORT


TEST_CASE("Target Buffer", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        REQUIRE(canvas->target(nullptr, 100, 100, 100, ColorSpace::ARGB8888) == Result::InvalidArguments);
        REQUIRE(canvas->target(buffer, 0, 100, 100, ColorSpace::ARGB8888) == Result::InvalidArguments);
        REQUIRE(canvas->target(buffer, 100, 0, 100, ColorSpace::ARGB8888) == Result::InvalidArguments);
        REQUIRE(canvas->target(buffer, 100, 200, 100, ColorSpace::ARGB8888) == Result::InvalidArguments);
        REQUIRE(canvas->target(buffer, 100, 100, 0, ColorSpace::ARGB8888) == Result::InvalidArguments);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Missing Initialization", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::missingInitialization(engine);
}

TEST_CASE("Basic Creation", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::basicCreation(engine);
}

TEST_CASE("Pushing Paints", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::pushingPaints(engine);
}

TEST_CASE("Update", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::update(engine);
}

TEST_CASE("Synchronized Drawing", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::synchronizedDrawing(engine);
}

TEST_CASE("Viewport", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::viewport(engine);
}

TEST_CASE("Asynchronous Drawing", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::asynchronousDrawing(engine);
}

TEST_CASE("Multi-Threading", "[tvgSwCanvas]")
{
    TvgSwTestEngine engine;
    TestCanvas::multiThreading(engine);
}

#endif
