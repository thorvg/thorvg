/*
 * Copyright (c) 2021 - 2025 the ThorVG project. All rights reserved.

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
    REQUIRE(scene->parent() == nullptr);

    Paint* paints[3];

    //Pushing Paints
    paints[0] = Shape::gen();
    REQUIRE(paints[0]->parent() == nullptr);
    REQUIRE(scene->push(paints[0]) == Result::Success);
    REQUIRE(paints[0]->parent() == scene.get());

    paints[1] = Picture::gen();
    REQUIRE(paints[1]->parent() == nullptr);
    REQUIRE(scene->push(paints[1]) == Result::Success);
    REQUIRE(paints[1]->parent() == scene.get());

    paints[2] = Picture::gen();
    REQUIRE(paints[2]->parent() == nullptr);
    REQUIRE(scene->push(paints[2]) == Result::Success);
    REQUIRE(paints[2]->parent() == scene.get());

    //Pushing Null Pointer
    REQUIRE(scene->push(nullptr) == Result::InvalidArguments);

    //Pushing Invalid Paint
    REQUIRE(scene->push(nullptr) == Result::InvalidArguments);
}

TEST_CASE("Scene Clear", "[tvgScene]")
{
    auto scene = unique_ptr<Scene>(Scene::gen());
    REQUIRE(scene);

    REQUIRE(scene->push(Shape::gen()) == Result::Success);
    REQUIRE(scene->remove() == Result::Success);
}

TEST_CASE("Scene Clear And Reuse Shape", "[tvgScene]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100];
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto scene = Scene::gen();
        REQUIRE(scene);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->ref() == 1);

        REQUIRE(scene->push(shape) == Result::Success);
        REQUIRE(canvas->push(scene) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);

        //No deallocate shape.
        REQUIRE(scene->remove() == Result::Success);

        //Reuse shape.
        REQUIRE(scene->push(shape) == Result::Success);
        REQUIRE(shape->unref() == 1); //The scene still holds 1.
    }
    REQUIRE(Initializer::term() == Result::Success);
}
