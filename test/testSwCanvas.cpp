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

#include <thread>
#include <vector>
#include <thorvg.h>
#include "config.h"
#include "catch.hpp"
#include "testCanvas.h"

using namespace tvg;
using namespace std;

#ifdef THORVG_CPU_ENGINE_SUPPORT

TEST_CASE("Missing Initialization", "[tvgSwCanvas]")
{
    TestCanvas::missingInitialization([]() {
        return unique_ptr<SwCanvas>(SwCanvas::gen());
    });
}

TEST_CASE("Basic Creation", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    TestCanvas::basicCreation([]() {
        return unique_ptr<SwCanvas>(SwCanvas::gen());
    });
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Target Buffer", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100];
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

TEST_CASE("Pushing Paints", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    uint32_t buffer[100 * 100];
    TestCanvas::pushingPaints([]() {
        return unique_ptr<SwCanvas>(SwCanvas::gen());
    }, [&](SwCanvas& canvas) {
        REQUIRE(canvas.target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);
    });
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Update", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    uint32_t buffer[100 * 100];
    TestCanvas::update([]() {
        return unique_ptr<SwCanvas>(SwCanvas::gen());
    }, [&](SwCanvas& canvas) {
        REQUIRE(canvas.target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);
    });
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Synchronized Drawing", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    uint32_t buffer[100 * 100];
    TestCanvas::synchronizedDrawing([]() {
        return unique_ptr<SwCanvas>(SwCanvas::gen());
    }, [&](SwCanvas& canvas) {
        REQUIRE(canvas.target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);
    }, []() {});
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Asynchronous Drawing", "[tvgSwCanvas]")
{
    //Use multi-threading
    REQUIRE(Initializer::init(2) == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100];
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        for (int i = 0; i < 3; ++i) {
            auto shape = Shape::gen();
            REQUIRE(shape);
            REQUIRE(shape->appendRect(0, 0, 100, 100) == Result::Success);
            REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);
            REQUIRE(canvas->add(shape) == Result::Success);
        }

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Viewport", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    uint32_t buffer[100 * 100];
    TestCanvas::viewport([]() {
        return unique_ptr<SwCanvas>(SwCanvas::gen());
    }, [&](SwCanvas& canvas) {
        REQUIRE(canvas.target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);
    }, []() {});
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Multi-Threading", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        std::vector<thread> workers;

        auto worker = []() {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        };

        for (unsigned int i = 0; i < thread::hardware_concurrency(); ++i) {
            workers.emplace_back(worker);
        }

        for (auto& t : workers) {
            t.join();
        }
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
