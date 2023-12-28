/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

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
#include "config.h"
#include "catch.hpp"

using namespace tvg;


TEST_CASE("Missing Initialization", "[tvgSwCanvas]")
{
    auto canvas = SwCanvas::gen();
    REQUIRE(canvas == nullptr);
}

TEST_CASE("Basic Creation", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    auto canvas2 = SwCanvas::gen();
    REQUIRE(canvas2);

    auto canvas3 = SwCanvas::gen();
    REQUIRE(canvas3);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Target Buffer", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    REQUIRE(canvas->target(nullptr, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::InvalidArguments);
    REQUIRE(canvas->target(buffer, 0, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::InvalidArguments);
    REQUIRE(canvas->target(buffer, 100, 0, 100, SwCanvas::Colorspace::ARGB8888) == Result::InvalidArguments);
    REQUIRE(canvas->target(buffer, 100, 200, 100, SwCanvas::Colorspace::ARGB8888) == Result::InvalidArguments);
    REQUIRE(canvas->target(buffer, 100, 100, 0, SwCanvas::Colorspace::ARGB8888) == Result::InvalidArguments);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Memory Pool", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    REQUIRE(canvas->mempool(SwCanvas::MempoolPolicy::Default) == Result::Success);
    REQUIRE(canvas->mempool(SwCanvas::MempoolPolicy::Individual) == Result::Success);
    REQUIRE(canvas->mempool(SwCanvas::MempoolPolicy::Shareable) == Result::Success);
    REQUIRE(canvas->mempool(SwCanvas::MempoolPolicy::Default) == Result::Success);

    auto canvas2 = SwCanvas::gen();
    REQUIRE(canvas);

    REQUIRE(canvas2->mempool(SwCanvas::MempoolPolicy::Default) == Result::Success);
    REQUIRE(canvas2->mempool(SwCanvas::MempoolPolicy::Individual) == Result::Success);
    REQUIRE(canvas2->mempool(SwCanvas::MempoolPolicy::Shareable) == Result::Success);
    REQUIRE(canvas2->mempool(SwCanvas::MempoolPolicy::Shareable) == Result::Success);

    REQUIRE(canvas2->push(Shape::gen()) == Result::Success);
    REQUIRE(canvas2->mempool(SwCanvas::MempoolPolicy::Individual) == Result::InsufficientCondition);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}
