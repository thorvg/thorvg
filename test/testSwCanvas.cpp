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
    REQUIRE(Initializer::init(0, CanvasEngine::Sw) == Result::Success);

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
    REQUIRE(Initializer::init(0, CanvasEngine::Sw) == Result::Success);

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
    REQUIRE(Initializer::init(0, CanvasEngine::Sw) == Result::Success);

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


TEST_CASE("Pushing Paints", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    //Try all types of paints.
    REQUIRE(canvas->push(Shape::gen()) == Result::Success);
    REQUIRE(canvas->push(Picture::gen()) == Result::Success);
    REQUIRE(canvas->push(Scene::gen()) == Result::Success);

    //Cases by contexts.
    REQUIRE(canvas->update() == Result::Success);

    REQUIRE(canvas->push(Shape::gen()) == Result::Success);
    REQUIRE(canvas->push(Shape::gen()) == Result::Success);

    REQUIRE(canvas->clear() == Result::Success);

    Paint* paints[2];

    auto p1 = Shape::gen();
    paints[0] = p1.get();
    REQUIRE(canvas->push(std::move(p1)) == Result::Success);

    //Negative case 1
    REQUIRE(canvas->push(nullptr) == Result::MemoryCorruption);

    //Negative case 2
    std::unique_ptr<Shape> shape6 = nullptr;
    REQUIRE(canvas->push(std::move(shape6)) == Result::MemoryCorruption);

    auto p2 = Shape::gen();
    paints[1] = p2.get();
    REQUIRE(canvas->push(std::move(p2)) == Result::Success);
    REQUIRE(canvas->draw() == Result::Success);

    //Negative case 3
    REQUIRE(canvas->push(Shape::gen()) == Result::InsufficientCondition);

    //Check list of paints
    auto list = canvas->paints();
    REQUIRE(list.size() == 2);
    int idx = 0;
    for (auto paint : list) {
       REQUIRE(paints[idx] == paint);
       ++idx;
    }

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Clear", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    auto canvas2 = SwCanvas::gen();
    REQUIRE(canvas2);

    //Try 0: Negative
    REQUIRE(canvas->clear() == Result::InsufficientCondition);
    REQUIRE(canvas->clear(false) == Result::InsufficientCondition);
    REQUIRE(canvas->clear() == Result::InsufficientCondition);

    REQUIRE(canvas2->clear(false) == Result::InsufficientCondition);
    REQUIRE(canvas2->clear() == Result::InsufficientCondition);
    REQUIRE(canvas2->clear(false) == Result::InsufficientCondition);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    uint32_t buffer2[100*100];
    REQUIRE(canvas2->target(buffer2, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    //Try 1: Push -> Clear
    for (int i = 0; i < 5; ++i) {
        REQUIRE(canvas->push(Shape::gen()) == Result::Success);

        auto shape2 = Shape::gen();
        REQUIRE(shape2);

        REQUIRE(canvas2->push(std::move(shape2)) == Result::Success);
    }

    REQUIRE(canvas->clear() == Result::Success);
    REQUIRE(canvas->clear(false) == Result::Success);

    REQUIRE(canvas2->clear(false) == Result::Success);
    REQUIRE(canvas2->clear() == Result::Success);

    //Try 2: Push -> Update -> Clear
    for (int i = 0; i < 5; ++i) {
        REQUIRE(canvas->push(Shape::gen()) == Result::Success);

        auto shape2 = Shape::gen();
        REQUIRE(shape2);
        REQUIRE(canvas2->push(std::move(shape2)) == Result::Success);
    }

    REQUIRE(canvas->update() == Result::Success);
    REQUIRE(canvas->clear() == Result::Success);
    REQUIRE(canvas->clear(false) == Result::Success);

    REQUIRE(canvas2->update() == Result::Success);
    REQUIRE(canvas2->clear(false) == Result::Success);
    REQUIRE(canvas2->clear() == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Update", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    REQUIRE(canvas->update() == Result::InsufficientCondition);

    REQUIRE(canvas->push(Shape::gen()) == Result::Success);

    //No pushed shape
    auto shape = Shape::gen();
    REQUIRE(canvas->update(shape.get()) == Result::Success);

    //Normal case
    auto ptr = shape.get();
    REQUIRE(canvas->push(std::move(shape)) == Result::Success);
    REQUIRE(canvas->update(ptr) == Result::Success);
    REQUIRE(canvas->update() == Result::Success);
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->update() == Result::InsufficientCondition);
    REQUIRE(canvas->clear() == Result::InsufficientCondition);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(canvas->clear() == Result::Success);

    //Invalid shape
    REQUIRE(canvas->update(ptr) == Result::InsufficientCondition);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Synchronized Drawing", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

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
    REQUIRE(canvas->push(std::move(shape)) == Result::Success);

    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);
    REQUIRE(canvas->clear() == Result::Success);

    auto shape2 = Shape::gen();
    REQUIRE(shape2);
    REQUIRE(shape2->appendRect(0, 0, 100, 100) == Result::Success);
    REQUIRE(shape2->fill(255, 255, 255, 255) == Result::Success);

    REQUIRE(canvas->push(std::move(shape2)) == Result::Success);
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Asynchronized Drawing", "[tvgSwCanvas]")
{
    //Use multi-threading
    REQUIRE(Initializer::init(2) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    for (int i = 0; i < 3; ++i) {
        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->appendRect(0, 0, 100, 100) == Result::Success);
        REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);

        REQUIRE(canvas->push(std::move(shape)) == Result::Success);
    }

    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Viewport", "[tvgSwCanvas]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    REQUIRE(canvas->viewport(25, 25, 100, 100) == Result::Success);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ARGB8888) == Result::Success);

    REQUIRE(canvas->viewport(25, 25, 50, 50) == Result::Success);

    auto shape = Shape::gen();
    REQUIRE(shape);
    REQUIRE(shape->appendRect(0, 0, 100, 100) == Result::Success);
    REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);

    REQUIRE(canvas->push(std::move(shape)) == Result::Success);

    //Negative, not allowed
    REQUIRE(canvas->viewport(15, 25, 5, 5) == Result::InsufficientCondition);

    REQUIRE(canvas->draw() == Result::Success);

    //Negative, not allowed
    REQUIRE(canvas->viewport(25, 25, 10, 10) == Result::InsufficientCondition);

    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}
