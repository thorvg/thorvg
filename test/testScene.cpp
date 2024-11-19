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
using namespace std;

TEST_CASE("Scene Creation", "[tvgScene]")
{
    auto scene = unique_ptr<Scene>(Scene::gen());
    REQUIRE(scene);

    REQUIRE(scene->type() == Type::Scene);
}

TEST_CASE("Pushing Paints Into Scene", "[tvgScene]")
{
    auto scene = unique_ptr<Scene>(Scene::gen());
    REQUIRE(scene);

    Paint* paints[3];

    //Pushing Paints
    paints[0] = Shape::gen();
    REQUIRE(scene->push(paints[0]) == Result::Success);

    paints[1] = Picture::gen();
    REQUIRE(scene->push(paints[1]) == Result::Success);

    paints[2] = Picture::gen();
    REQUIRE(scene->push(paints[2]) == Result::Success);

    //Pushing Null Pointer
    REQUIRE(scene->push(nullptr) == Result::InvalidArguments);

    //Pushing Invalid Paint
    REQUIRE(scene->push(nullptr) == Result::InvalidArguments);

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
    auto scene = unique_ptr<Scene>(Scene::gen());
    REQUIRE(scene);

    REQUIRE(scene->push(Shape::gen()) == Result::Success);
    REQUIRE(scene->clear() == Result::Success);
}

TEST_CASE("Scene Clear And Reuse Shape", "[tvgScene]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
    REQUIRE(canvas);

    auto scene = Scene::gen();
    REQUIRE(scene);

    auto shape = Shape::gen();
    REQUIRE(shape);

    REQUIRE(scene->push(shape) == Result::Success);
    REQUIRE(canvas->push(scene) == Result::Success);
    REQUIRE(canvas->update() == Result::Success);

    //No deallocate shape.
    REQUIRE(scene->clear(false) == Result::Success);

    //Reuse shape.
    REQUIRE(scene->push(shape) == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}
