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

TEST_CASE("Scene Creation", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);
}

TEST_CASE("Pushing Paints Into Scene", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);
    SECTION("Pushing Paints") {
        REQUIRE(scene->push(move(Shape::gen())) == Result::Success);
        REQUIRE(scene->push(move(Picture::gen())) == Result::Success);
        REQUIRE(scene->push(move(Scene::gen())) == Result::Success);
    }

    SECTION("Pushing Null Pointer") {
        REQUIRE(scene->push(nullptr) == Result::MemoryCorruption);
    }

    SECTION("Pushing Invalid Object") {
        std::unique_ptr<Shape> shape = nullptr;
        REQUIRE(scene->push(move(shape)) == Result::MemoryCorruption);
    }
}

TEST_CASE("Scene Memory Reservation", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    SECTION("Check Growth / Reduction") {
        REQUIRE(scene->reserve(10) == Result::Success);
        REQUIRE(scene->reserve(1000) == Result::Success);
        REQUIRE(scene->reserve(100) == Result::Success);
        REQUIRE(scene->reserve(0) == Result::Success);
    }

    SECTION("Too Big Size") {
        REQUIRE(scene->reserve(-1) == Result::FailedAllocation);
    }
}

TEST_CASE("Scene Clear", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    REQUIRE(scene->push(move(Shape::gen())) == Result::Success);
    REQUIRE(scene->clear() == Result::Success);
}

TEST_CASE("Scene Clear And Reuse Shape", "[tvgScene]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    auto scene = Scene::gen();
    REQUIRE(scene);
    Scene *pScene = scene.get();

    auto shape = Shape::gen();
    REQUIRE(shape);
    Shape* pShape = shape.get();

    REQUIRE(scene->push(move(shape)) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);
    REQUIRE(canvas->update(nullptr) == Result::Success);

    //No deallocate shape.
    REQUIRE(pScene->clear(false) == Result::Success);

    //Reuse shape.
    REQUIRE(pScene->push(std::unique_ptr<Shape>(pShape)) == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}
