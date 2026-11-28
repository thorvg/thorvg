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
#include <cstdio>
#include "config.h"
#include "testFramework.h"

using namespace tvg;
using namespace std;

TEST_CASE("Saver Creation", "[tvgSavers]")
{
    auto saver = unique_ptr<Saver>(Saver::gen());
    REQUIRE(saver);
}

#if defined(THORVG_GIF_SAVER_SUPPORT) && defined(THORVG_LOTTIE_LOADER_SUPPORT)

TEST_CASE("Save a lottie into gif", "[tvgSavers]") {
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = Animation::gen();
        auto picture = animation->picture();
        REQUIRE(picture->load(TEST_DIR"/test.lot") == Result::Success);
        REQUIRE(picture->size(100, 100) == Result::Success);

        auto bg = Shape::gen();
        REQUIRE(bg->fill(255, 255, 255) == Result::Success);
        REQUIRE(bg->appendRect(0, 0, 100, 100) == Result::Success);

        auto saver =  unique_ptr<Saver>(Saver::gen());
        REQUIRE(saver);

        REQUIRE(saver->background(bg) == Result::Success);
        REQUIRE(saver->save(animation, TEST_DIR"/test_saved.gif") == Result::Success);
        REQUIRE(saver->sync() == Result::Success);
        REQUIRE(remove(TEST_DIR"/test_saved.gif") == 0);
    }
    REQUIRE(Initializer::term() == Result::Success);
}
#endif

#ifdef THORVG_PNG_SAVER_SUPPORT

TEST_CASE("Save a shape into png", "[tvgSavers]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto shape = Shape::gen();
        REQUIRE(shape->appendRect(10, 20, 100, 50) == Result::Success);
        REQUIRE(shape->fill(255, 0, 0) == Result::Success);

        auto saver = unique_ptr<Saver>(Saver::gen());
        REQUIRE(saver);
        REQUIRE(saver->save(shape, TEST_DIR"/test_saved.png") == Result::Success);
        REQUIRE(saver->sync() == Result::Success);

#ifdef THORVG_PNG_LOADER_SUPPORT
        //The saved image is cropped to the paint bounds.
        auto picture = unique_ptr<Picture>(Picture::gen());
        REQUIRE(picture->load(TEST_DIR"/test_saved.png") == Result::Success);
        float w, h;
        REQUIRE(picture->size(&w, &h) == Result::Success);
        REQUIRE(w == 100.0f);
        REQUIRE(h == 50.0f);
#endif
        REQUIRE(remove(TEST_DIR"/test_saved.png") == 0);
    }
    REQUIRE(Initializer::term() == Result::Success);
}


TEST_CASE("Save a translucent shape with a background into png", "[tvgSavers]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto shape = Shape::gen();
        REQUIRE(shape->appendCircle(50, 50, 40, 30) == Result::Success);
        REQUIRE(shape->fill(0, 128, 255, 128) == Result::Success);
        REQUIRE(shape->strokeWidth(4.0f) == Result::Success);
        REQUIRE(shape->strokeFill(0, 0, 0) == Result::Success);

        auto bg = Shape::gen();
        REQUIRE(bg->appendRect(0, 0, 100, 100) == Result::Success);
        REQUIRE(bg->fill(255, 255, 255) == Result::Success);

        auto saver = unique_ptr<Saver>(Saver::gen());
        REQUIRE(saver);
        REQUIRE(saver->background(bg) == Result::Success);

        //with the compression
        REQUIRE(saver->save(shape, TEST_DIR"/test_saved.png") == Result::Success);
        REQUIRE(saver->sync() == Result::Success);
        REQUIRE(remove(TEST_DIR"/test_saved.png") == 0);

        //without the compression
        shape = Shape::gen();
        REQUIRE(shape->appendCircle(50, 50, 40, 30) == Result::Success);
        REQUIRE(shape->fill(0, 128, 255, 128) == Result::Success);
        REQUIRE(saver->save(shape, TEST_DIR"/test_saved.png", 0) == Result::Success);
        REQUIRE(saver->sync() == Result::Success);
        REQUIRE(remove(TEST_DIR"/test_saved.png") == 0);
    }
    REQUIRE(Initializer::term() == Result::Success);
}


TEST_CASE("Save invalid inputs into png", "[tvgSavers]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto saver = unique_ptr<Saver>(Saver::gen());
        REQUIRE(saver);

        //An empty paint has no bounds to be saved.
        auto empty = Shape::gen();
        REQUIRE(saver->save(empty, TEST_DIR"/test_saved.png") == Result::Unknown);
    }
    REQUIRE(Initializer::term() == Result::Success);
}
#endif
