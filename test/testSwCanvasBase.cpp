/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include "catch.hpp"

using namespace tvg;


TEST_CASE("Memory Reservation", "[tvgSwCanvasBase]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    //Check Growth / Reduction
    REQUIRE(canvas->reserve(10) == Result::Success);
    REQUIRE(canvas->reserve(1000) == Result::Success);
    REQUIRE(canvas->reserve(100) == Result::Success);
    REQUIRE(canvas->reserve(0) == Result::Success);

    //Too big size
    REQUIRE(canvas->reserve(UINT32_MAX) == Result::FailedAllocation);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Pushing Paints", "[tvgSwCanvasBase]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    //Try all types of paints.
    REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);
    REQUIRE(canvas->push(move(Picture::gen())) == Result::Success);
    REQUIRE(canvas->push(move(Scene::gen())) == Result::Success);

    //Cases by contexts.
    REQUIRE(canvas->update() == Result::Success);

    REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);
    REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);

    REQUIRE(canvas->clear() == Result::Success);

    REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);

    //Negative case 1
    REQUIRE(canvas->push(nullptr) == Result::MemoryCorruption);

    //Negative case 2
    std::unique_ptr<Shape> shape6 = nullptr;
    REQUIRE(canvas->push(move(shape6)) == Result::MemoryCorruption);

    //Negative case 3
    REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->push(move(Shape::gen())) == Result::InsufficientCondition);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Clear", "[tvgSwCanvasBase]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);
    auto canvas2 = SwCanvas::gen();
    REQUIRE(canvas2);

    //Try 0: Clear
    REQUIRE(canvas->clear() == Result::Success);
    REQUIRE(canvas->clear(false) == Result::Success);
    REQUIRE(canvas->clear() == Result::Success);

    REQUIRE(canvas2->clear(false) == Result::Success);
    REQUIRE(canvas2->clear() == Result::Success);
    REQUIRE(canvas2->clear(false) == Result::Success);

    Shape* ptrs[5];

    //Try 1: Push -> Clear
    for (int i = 0; i < 5; ++i) {
        REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);

        auto shape2 = Shape::gen();
        REQUIRE(shape2);
        ptrs[i] = shape2.get();

        REQUIRE(canvas2->push(move(shape2)) == Result::Success);
    }

    REQUIRE(canvas->clear() == Result::Success);
    REQUIRE(canvas->clear(false) == Result::Success);

    REQUIRE(canvas2->clear(false) == Result::Success);
    REQUIRE(canvas2->clear() == Result::Success);

    for (int i = 0; i < 5; ++i) delete(ptrs[i]);

    //Try 2: Push -> Update -> Clear
    for (int i = 0; i < 5; ++i) {
        REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);

        auto shape2 = Shape::gen();
        REQUIRE(shape2);
        ptrs[i] = shape2.get();

        REQUIRE(canvas2->push(move(shape2)) == Result::Success);
    }

    REQUIRE(canvas->update() == Result::Success);
    REQUIRE(canvas->clear() == Result::Success);
    REQUIRE(canvas->clear(false) == Result::Success);

    REQUIRE(canvas2->update() == Result::Success);
    REQUIRE(canvas2->clear(false) == Result::Success);
    REQUIRE(canvas2->clear() == Result::Success);

    for (int i = 0; i < 5; ++i) delete(ptrs[i]);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Update", "[tvgSwCanvasBase]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    REQUIRE(canvas->update() == Result::InsufficientCondition);

    REQUIRE(canvas->push(move(Shape::gen())) == Result::Success);

    //No pushed shape
    auto shape = Shape::gen();
    REQUIRE(canvas->update(shape.get()) == Result::InvalidArguments);

    //Normal case
    auto ptr = shape.get();
    REQUIRE(canvas->push(move(shape)) == Result::Success);
    REQUIRE(canvas->update(ptr) == Result::Success);
    REQUIRE(canvas->update() == Result::Success);
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->update() == Result::InsufficientCondition);

    REQUIRE(canvas->clear() == Result::Success);

    //Invalid shape
    REQUIRE(canvas->update(ptr) == Result::InsufficientCondition);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Synchronized Drawing", "[tvgSwCanvasBase]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    REQUIRE(canvas->sync() == Result::InsufficientCondition);
    REQUIRE(canvas->draw() == Result::InsufficientCondition);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    REQUIRE(canvas->draw() == Result::InsufficientCondition);
    REQUIRE(canvas->sync() == Result::InsufficientCondition);

    //Invalid Shape
    auto shape = Shape::gen();
    REQUIRE(shape);
    REQUIRE(canvas->push(move(shape)) == Result::Success);

    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);
    REQUIRE(canvas->clear() == Result::Success);

    auto shape2 = Shape::gen();
    REQUIRE(shape2);
    REQUIRE(shape2->appendRect(0, 0, 100, 100, 0, 0) == Result::Success);
    REQUIRE(shape2->fill(255, 255, 255, 255) == Result::Success);

    REQUIRE(canvas->push(move(shape2)) == Result::Success);
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Asynchronized Drawing", "[tvgSwCanvasBase]")
{
    //Use multi-threading
    REQUIRE(Initializer::init(CanvasEngine::Sw, 2) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    for (int i = 0; i < 3; ++i) {
        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->appendRect(0, 0, 100, 100, 0, 0) == Result::Success);
        REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);

        REQUIRE(canvas->push(move(shape)) == Result::Success);
    }

    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}
