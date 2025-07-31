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
#include <fstream>
#include "config.h"
#include "catch.hpp"

using namespace tvg;
using namespace std;
#if 0
TEST_CASE("Saver Creation", "[tvgSavers]")
{
    auto saver = unique_ptr<Saver>(Saver::gen());
    REQUIRE(saver);
}

#if defined(THORVG_GIF_SAVER_SUPPORT) && defined(THORVG_LOTTIE_LOADER_SUPPORT)

TEST_CASE("Save a lottie into gif", "[tvgSavers]")
{
    REQUIRE(Initializer::init() == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture);
    REQUIRE(picture->load(TEST_DIR"/test.json") == Result::Success);
    REQUIRE(picture->size(100, 100) == Result::Success);

    auto saver =  unique_ptr<Saver>(Saver::gen());
    REQUIRE(saver);
    REQUIRE(saver->save(animation, TEST_DIR"/test.gif") == Result::Success);
    REQUIRE(saver->sync() == Result::Success);

    //with a background
    auto animation2 = Animation::gen();
    REQUIRE(animation2);

    auto picture2 = animation2->picture();
    REQUIRE(picture2);
    REQUIRE(picture2->load(TEST_DIR"/test.json") == Result::Success);
    REQUIRE(picture2->size(100, 100) == Result::Success);

    auto bg = Shape::gen();
    REQUIRE(bg->fill(255, 255, 255) == Result::Success);
    REQUIRE(bg->appendRect(0, 0, 100, 100) == Result::Success);

    REQUIRE(saver->background(bg) == Result::Success);
    REQUIRE(saver->save(animation2, TEST_DIR"/test.gif") == Result::Success);
    REQUIRE(saver->sync() == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}
#endif
#endif