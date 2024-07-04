/*
 * Copyright (c) 2022 - 2024 the ThorVG project. All rights reserved.

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

TEST_CASE("Accessor Creation", "[tvgAccessor]")
{
    auto accessor = tvg::Accessor::gen();
    REQUIRE(accessor);

    auto accessor2 = tvg::Accessor::gen();
    REQUIRE(accessor2);
}

#ifdef THORVG_SVG_LOADER_SUPPORT

TEST_CASE("Set", "[tvgAccessor]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    auto picture = Picture::gen();
    REQUIRE(picture);
    REQUIRE(picture->load(TEST_DIR"/logo.svg") == Result::Success);

    auto accessor = tvg::Accessor::gen();
    REQUIRE(accessor);

    //Case 1
    picture = accessor->set(std::move(picture), nullptr);
    REQUIRE(picture);

    //Case 2
    auto f = [](const tvg::Paint* paint) -> bool
    {
        if (paint->type() == tvg::Shape::type()) {
            auto shape = (tvg::Shape*) paint;
            uint8_t r, g, b;
            shape->fillColor(&r, &g, &b);
            if (r == 37 && g == 47 && b == 53)
                shape->fill(0, 0, 255);
        }
        return true;
    };

    picture = accessor->set(std::move(picture), f);
    REQUIRE(picture);

    REQUIRE(Initializer::term() == Result::Success);
}

#endif