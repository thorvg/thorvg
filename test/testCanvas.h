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

#ifndef _TVG_TEST_CANVAS_H_
#define _TVG_TEST_CANVAS_H_

#include <atomic>
#include <thread>
#include <vector>
#include <thorvg.h>
#include "catch.hpp"
#include "engine.h"

using namespace tvg;

struct TestCanvas
{
    static constexpr uint32_t CreationSize = 32;
    static constexpr uint32_t RenderSize = 100;

    static void missingInitialization(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(CreationSize, CreationSize));

        auto canvas = engine.canvas();
        REQUIRE(canvas == nullptr);
    }

    static void basicCreation(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(CreationSize, CreationSize));

        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);

            auto canvas2 = engine.canvas();
            REQUIRE(canvas2);

            auto canvas3 = engine.canvas();
            REQUIRE(canvas3);
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static void pushingPaints(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));

        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);

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

    static void update(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));

        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);

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

    static void synchronizedDrawing(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));

        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);

            REQUIRE(canvas->sync() == Result::Success);
            REQUIRE(canvas->draw() == Result::InsufficientCondition);

            REQUIRE(engine.target(canvas.get()) == Result::Success);

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
            REQUIRE(engine.rendered());
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static void asynchronousDrawing(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));

        REQUIRE(Initializer::init(2) == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);

            REQUIRE(engine.target(canvas.get()) == Result::Success);

            for (int i = 0; i < 3; ++i) {
                auto shape = Shape::gen();
                REQUIRE(shape);
                REQUIRE(shape->appendRect(0, 0, 100, 100) == Result::Success);
                REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);
                REQUIRE(canvas->add(shape) == Result::Success);
            }

            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
            REQUIRE(engine.rendered());
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static void viewport(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));

        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);

            REQUIRE(canvas->viewport(25, 25, 100, 100) == Result::Success);
            REQUIRE(engine.target(canvas.get()) == Result::Success);
            REQUIRE(canvas->viewport(25, 25, 50, 50) == Result::Success);

            auto shape = Shape::gen();
            REQUIRE(shape);
            REQUIRE(shape->appendRect(0, 0, 100, 100) == Result::Success);
            REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);
            REQUIRE(canvas->add(shape) == Result::Success);

            REQUIRE(canvas->viewport(15, 25, 5, 5) == Result::InsufficientCondition);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->viewport(25, 25, 10, 10) == Result::InsufficientCondition);
            REQUIRE(canvas->sync() == Result::Success);
            REQUIRE(engine.rendered());
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static void multiThreading(TvgTestEngine& engine)
    {
        REQUIRE(Initializer::init() == Result::Success);
        {
            std::vector<std::thread> workers;
            std::atomic<unsigned int> created{0};
            auto count = std::thread::hardware_concurrency();

            auto worker = [&]() {
                auto canvas = engine.canvas();
                if (canvas) ++created;
            };

            for (unsigned int i = 0; i < count; ++i) {
                workers.emplace_back(worker);
            }

            for (auto& t : workers) {
                t.join();
            }

            REQUIRE(created.load() == count);
        }
        REQUIRE(Initializer::term() == Result::Success);
    }
};

#endif
