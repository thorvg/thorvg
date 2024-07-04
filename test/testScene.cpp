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

TEST_CASE("Scene Creation", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    REQUIRE(scene->type() == Scene::type());
    REQUIRE(scene->type() != Shape::type());
    REQUIRE(scene->type() != Picture::type());
}

TEST_CASE("Pushing Paints Into Scene", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    Paint* paints[3];

    //Pushing Paints
    auto p1 = Shape::gen();
    paints[0] = p1.get();
    REQUIRE(scene->push(std::move(p1)) == Result::Success);

    auto p2 = Picture::gen();
    paints[1] = p2.get();
    REQUIRE(scene->push(std::move(p2)) == Result::Success);

    auto p3 = Picture::gen();
    paints[2] = p3.get();
    REQUIRE(scene->push(std::move(p3)) == Result::Success);

    //Pushing Null Pointer
    REQUIRE(scene->push(nullptr) == Result::MemoryCorruption);

    //Pushing Invalid Paint
    std::unique_ptr<Shape> shape = nullptr;
    REQUIRE(scene->push(std::move(shape)) == Result::MemoryCorruption);

    //Check list of paints
    auto list = scene->paints();
    REQUIRE(list.size() == 3);
    int idx = 0;
    for (auto paint : list) {
       REQUIRE(paints[idx] == paint);
       ++idx;
    }
}

TEST_CASE("Scene Clear", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    REQUIRE(scene->push(Shape::gen()) == Result::Success);
    REQUIRE(scene->clear() == Result::Success);
}

TEST_CASE("Scene Clear And Reuse Shape", "[tvgScene]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    auto scene = Scene::gen();
    REQUIRE(scene);
    Scene *pScene = scene.get();

    auto shape = Shape::gen();
    REQUIRE(shape);
    Shape* pShape = shape.get();

    REQUIRE(scene->push(std::move(shape)) == Result::Success);
    REQUIRE(canvas->push(std::move(scene)) == Result::Success);
    REQUIRE(canvas->update() == Result::Success);

    //No deallocate shape.
    REQUIRE(pScene->clear(false) == Result::Success);

    //Reuse shape.
    REQUIRE(pScene->push(std::unique_ptr<Shape>(pShape)) == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}
