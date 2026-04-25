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

#include <thorvg.h>
#include "config.h"
#include "catch.hpp"

using namespace tvg;
using namespace std;

TEST_CASE("Scene Creation", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    REQUIRE(scene->type() == Type::Scene);

    Paint::rel(scene);
}

TEST_CASE("Pushing Paints Into Scene", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->parent() == nullptr);

    Paint* paints[3];

    //Pushing Paints
    paints[0] = Shape::gen();
    paints[0]->ref();
    REQUIRE(paints[0]->parent() == nullptr);
    REQUIRE(scene->add(paints[0]) == Result::Success);
    REQUIRE(paints[0]->parent() == scene);

    paints[1] = Picture::gen();
    paints[1]->ref();
    REQUIRE(paints[1]->parent() == nullptr);
    REQUIRE(scene->add(paints[1]) == Result::Success);
    REQUIRE(paints[1]->parent() == scene);

    paints[2] = Picture::gen();
    paints[2]->ref();
    REQUIRE(paints[2]->parent() == nullptr);
    REQUIRE(scene->add(paints[2]) == Result::Success);
    REQUIRE(paints[2]->parent() == scene);

    //Pushing Null Pointer
    REQUIRE(scene->add(nullptr) == Result::InvalidArguments);

    //Pushing Invalid Paint
    REQUIRE(scene->add(nullptr) == Result::InvalidArguments);

    REQUIRE(scene->remove(paints[0]) == Result::Success);
    REQUIRE(scene->remove(paints[0]) == Result::InsufficientCondition);
    REQUIRE(scene->add(paints[0], paints[1]) == Result::Success);
    REQUIRE(scene->remove(paints[1]) == Result::Success);
    REQUIRE(scene->remove(paints[1]) == Result::InsufficientCondition);
    REQUIRE(scene->remove(paints[2]) == Result::Success);
    REQUIRE(scene->remove(paints[0]) == Result::Success);

    paints[0]->unref();
    paints[1]->unref();
    paints[2]->unref();

    Paint::rel(scene);
}

TEST_CASE("Scene Clear", "[tvgScene]")
{
    auto scene = Scene::gen();
    REQUIRE(scene);

    REQUIRE(scene->add(Shape::gen()) == Result::Success);
    REQUIRE(scene->remove() == Result::Success);

    Paint::rel(scene);
}

TEST_CASE("Scene Clear And Reuse Shape", "[tvgScene]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100] = {};
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto scene = Scene::gen();
        REQUIRE(scene);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->ref() == 1);

        REQUIRE(scene->add(shape) == Result::Success);
        REQUIRE(canvas->add(scene) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);

        //No deallocate shape.
        REQUIRE(scene->remove() == Result::Success);

        //Reuse shape.
        REQUIRE(scene->add(shape) == Result::Success);
        REQUIRE(shape->unref() == 1); //The scene still holds 1.
    }
    REQUIRE(Initializer::term() == Result::Success);
}
