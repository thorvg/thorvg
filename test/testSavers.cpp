/*
 * Copyright (c) 2021 - 2023 the ThorVG project. All rights reserved.

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

TEST_CASE("Saver Creation", "[tvgSavers]")
{
    auto saver = Saver::gen();
    REQUIRE(saver);
}

#ifdef THORVG_TVG_SAVER_SUPPORT

TEST_CASE("Save empty shape", "[tvgSavers]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    auto saver = Saver::gen();
    REQUIRE(saver);

    REQUIRE(saver->save(std::move(shape), TEST_DIR"/test.tvg") == Result::Unknown);
}

TEST_CASE("Save svg into tvg", "[tvgSavers]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto picture = Picture::gen();
    REQUIRE(picture);
    REQUIRE(picture->load(TEST_DIR"/tag.svg") == Result::Success);

    auto saver = Saver::gen();
    REQUIRE(saver);

    REQUIRE(saver->save(std::move(picture), TEST_DIR"/tag.tvg") == Result::Success);
    REQUIRE(saver->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Save scene into tvg", "[tvgSavers]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto picture = tvg::Picture::gen();
    REQUIRE(picture);

    ifstream file(TEST_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    REQUIRE(picture->load(data, 200, 300, false) == Result::Success);
    REQUIRE(picture->translate(50, 0) == Result::Success);
    REQUIRE(picture->scale(2) == Result::Success);

    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(400, 400, 15, 15) == Result::Success);
    REQUIRE(mask->fill(0, 0, 0, 255) == Result::Success);
    REQUIRE(picture->composite(std::move(mask), tvg::CompositeMethod::InvAlphaMask) == Result::Success);

    auto saver = Saver::gen();
    REQUIRE(saver);
    REQUIRE(saver->save(std::move(picture), TEST_DIR"/test.tvg") == Result::Success);
    REQUIRE(saver->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);

    free(data);
}

#endif