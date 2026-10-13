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

using namespace tvg;
using namespace std;

#ifdef THORVG_CPU_ENGINE_SUPPORT

TEST_CASE("Missing Initialization", "[tvgSwCanvas]")
{
    auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
    REQUIRE(canvas == nullptr);
}

TEST_CASE("Basic Creation", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        auto canvas2 = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas2);

        auto canvas3 = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas3);
    }

    REQUIRE(Initializer::term() == Result::Success);
}

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

TEST_CASE("Pushing Paints", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        //Try all types of paints.
        REQUIRE(canvas->add(Shape::gen()) == Result::Success);
        REQUIRE(canvas->add(Picture::gen()) == Result::Success);
        REQUIRE(canvas->add(Scene::gen()) == Result::Success);

        //Cases by contexts.
        REQUIRE(canvas->update() == Result::Success);

        REQUIRE(canvas->add(Shape::gen()) == Result::Success);
        REQUIRE(canvas->add(Shape::gen()) == Result::Success);

        REQUIRE(canvas->remove() == Result::Success);

        Paint* paints[2];

        paints[0] = Shape::gen();
        REQUIRE(canvas->add(paints[0]) == Result::Success);

        //Negative case 1
        REQUIRE(canvas->add(nullptr) == Result::InvalidArguments);

        paints[1] = Shape::gen();
        REQUIRE(canvas->add(paints[1]) == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);

        //Check list of paints
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

TEST_CASE("Update", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        REQUIRE(canvas->update() == Result::Success);

        REQUIRE(canvas->add(Shape::gen()) == Result::Success);

        //No added shape
        auto shape = Shape::gen();

        //Normal case
        REQUIRE(canvas->add(shape) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->update() == Result::InsufficientCondition);
        REQUIRE(canvas->sync() == Result::Success);

        REQUIRE(canvas->update() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Synchronized Drawing", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        REQUIRE(canvas->sync() == Result::Success);
        REQUIRE(canvas->draw() == Result::InsufficientCondition);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        //Invalid Shape
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

TEST_CASE("Asynchronous Drawing", "[tvgSwCanvas]")
{
    //Use multi-threading
    REQUIRE(Initializer::init(2) == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
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
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        REQUIRE(canvas->viewport(25, 25, 100, 100) == Result::Success);

        uint32_t buffer[100*100] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        REQUIRE(canvas->viewport(25, 25, 50, 50) == Result::Success);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->appendRect(0, 0, 100, 100) == Result::Success);
        REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);

        REQUIRE(canvas->add(shape) == Result::Success);

        //Negative, not allowed
        REQUIRE(canvas->viewport(15, 25, 5, 5) == Result::InsufficientCondition);

        REQUIRE(canvas->draw() == Result::Success);

        //Negative, not allowed
        REQUIRE(canvas->viewport(25, 25, 10, 10) == Result::InsufficientCondition);

        REQUIRE(canvas->sync() == Result::Success);
    }
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

TEST_CASE("Dirty regions lifecycle", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(2) == Result::Success);
    {
        for (auto option : {EngineOption::SmartRender, EngineOption::None}) {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen(option));
            REQUIRE(canvas);
            Region regions[2] = {};
            REQUIRE(canvas->dirtyRegions() == 0);
            REQUIRE(canvas->dirtyRegions(regions, 2) == Result::InsufficientCondition);
            REQUIRE(canvas->dirtyRegions(nullptr, 1) == Result::InvalidArguments);
            REQUIRE(canvas->dirtyRegions(regions, 0) == Result::InvalidArguments);

            uint32_t buffer[80 * 60] = {};
            REQUIRE(canvas->target(buffer, 80, 80, 60, ColorSpace::ARGB8888) == Result::Success);
            auto shape = Shape::gen();
            REQUIRE(shape->appendRect(5, 7, 10, 12) == Result::Success);
            REQUIRE(shape->fill(255, 0, 0) == Result::Success);
            REQUIRE(canvas->add(shape) == Result::Success);

            for (bool clear : {false, true}) {
                REQUIRE(canvas->update() == Result::Success);
                REQUIRE(canvas->dirtyRegions() == 0);
                REQUIRE(canvas->dirtyRegions(regions, 2) == Result::InsufficientCondition);
                REQUIRE(canvas->draw(clear) == Result::Success);
                REQUIRE(canvas->dirtyRegions() == 1);
                REQUIRE(canvas->dirtyRegions(nullptr, 1) == Result::InvalidArguments);
                REQUIRE(canvas->dirtyRegions(regions, 0) == Result::InvalidArguments);
                REQUIRE(regions[0].xMin == 0);
                REQUIRE(canvas->dirtyRegions(regions, 2) == Result::Success);
                REQUIRE(regions[0].xMin == 0);
                REQUIRE(regions[0].yMin == 0);
                REQUIRE(regions[0].xMax == 80);
                REQUIRE(regions[0].yMax == 60);
                REQUIRE(regions[1].xMin == 0);
                REQUIRE(regions[1].yMin == 0);
                REQUIRE(regions[1].xMax == 0);
                REQUIRE(regions[1].yMax == 0);
                REQUIRE(canvas->dirtyRegions() == 1);
                REQUIRE(canvas->sync() == Result::Success);
                REQUIRE(canvas->dirtyRegions() == 0);
                REQUIRE(canvas->dirtyRegions(regions, 2) == Result::InsufficientCondition);
                regions[0] = {};
            }
        }
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#ifdef THORVG_PARTIAL_RENDER_SUPPORT
TEST_CASE("Dirty regions partial updates", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(2) == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen(EngineOption::SmartRender));
        REQUIRE(canvas);
        uint32_t buffer[100 * 80] = {};
        REQUIRE(canvas->target(buffer, 100, 100, 80, ColorSpace::ARGB8888) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
        auto shape = Shape::gen();
        REQUIRE(shape->appendCircle(15, 15, 10, 10) == Result::Success);
        REQUIRE(shape->fill(255, 0, 0) == Result::Success);
        REQUIRE(canvas->add(shape) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);

        Region region = {};
        REQUIRE(canvas->dirtyRegions() == 1);
        REQUIRE(canvas->dirtyRegions(&region, 1) == Result::Success);
        REQUIRE(region.xMin == 0);
        REQUIRE(region.yMin == 0);
        REQUIRE(region.xMax == 100);
        REQUIRE(region.yMax == 80);
        REQUIRE(canvas->sync() == Result::Success);

        shape->translate(10, 10);

        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);

        const Region expected[] = {
            {5, 5, 25, 20},
            {25, 15, 35, 20},
            {5, 20, 25, 25},
            {15, 25, 25, 35},
            {25, 20, 35, 35}};
        Region regions[5] = {};
        REQUIRE(canvas->dirtyRegions() == 5);
        REQUIRE(canvas->dirtyRegions(regions, 5) == Result::Success);
        for (uint32_t i = 0; i < 5; ++i) {
            REQUIRE(regions[i].xMin == expected[i].xMin);
            REQUIRE(regions[i].yMin == expected[i].yMin);
            REQUIRE(regions[i].xMax == expected[i].xMax);
            REQUIRE(regions[i].yMax == expected[i].yMax);
        }
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}
#endif

#endif
