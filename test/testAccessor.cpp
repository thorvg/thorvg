/*
 * Copyright (c) 2022 - 2025 the ThorVG project. All rights reserved.

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

TEST_CASE("Accessor Creation", "[tvgAccessor]")
{
    auto accessor = unique_ptr<Accessor>(Accessor::gen());
    REQUIRE(accessor);

    auto accessor2 = unique_ptr<Accessor>(Accessor::gen());
    REQUIRE(accessor2);
}

#ifdef THORVG_SVG_LOADER_SUPPORT

TEST_CASE("Set", "[tvgAccessor]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ABGR8888) == Result::Success);

    auto picture = unique_ptr<Picture>(Picture::gen());
    REQUIRE(picture);
    REQUIRE(picture->load(TEST_DIR"/logo.svg") == Result::Success);

    auto accessor = unique_ptr<Accessor>(Accessor::gen());
    REQUIRE(accessor);

    //Case 1
    REQUIRE(accessor->set(picture.get(), nullptr, nullptr) == Result::InvalidArguments);

    //Case 2
    Shape* ret = nullptr;

    auto f = [](const tvg::Paint* paint, void* data) -> bool
    {
        if (paint->type() == Type::Shape) {
            auto shape = (tvg::Shape*) paint;
            uint8_t r, g, b;
            shape->fill(&r, &g, &b);
            if (r == 37 && g == 47 && b == 53) {
                shape->fill(0, 0, 255);
                shape->id = Accessor::id("TestAccessor");
                *static_cast<Shape**>(data) = shape;
                return false;
            }
        }
        return true;
    };

    REQUIRE(accessor->set(picture.get(), f, &ret) == Result::Success);

    REQUIRE((ret && ret->id == Accessor::id("TestAccessor")));

    REQUIRE(Initializer::term() == Result::Success);
}

#endif