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

#ifdef THORVG_TTF_LOADER_SUPPORT

TEST_CASE("Text Creation", "[tvgText]")
{
    auto text = Text::gen();
    REQUIRE(text);

    REQUIRE(text->identifier() == Text::identifier());
    REQUIRE(text->identifier() != Shape::identifier());
    REQUIRE(text->identifier() != Scene::identifier());
    REQUIRE(text->identifier() != Picture::identifier());
}

TEST_CASE("Load TTF Data", "[tvgText]")
{
    Initializer::init(CanvasEngine::Sw, 0);

    auto text = Text::gen();
    REQUIRE(text);

    REQUIRE(Text::unload(TEST_DIR"/invalid.ttf") == tvg::Result::InsufficientCondition);

    REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == tvg::Result::Success);
    REQUIRE(Text::load(TEST_DIR"/invalid.ttf") == tvg::Result::InvalidArguments);
    REQUIRE(Text::unload(TEST_DIR"/Arial.ttf") == tvg::Result::Success);
    REQUIRE(Text::load("") == tvg::Result::InvalidArguments);
    REQUIRE(Text::load(TEST_DIR"/NanumGothicCoding.ttf") == tvg::Result::Success);

    Initializer::term(CanvasEngine::Sw);
}

TEST_CASE("Text Font", "[tvgText]")
{
    Initializer::init(CanvasEngine::Sw, 0);

    auto text = Text::gen();
    REQUIRE(text);

    REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == tvg::Result::Success);

    REQUIRE(text->font("Arial", 80) == tvg::Result::Success);
    REQUIRE(text->font("Arial", 1) == tvg::Result::Success);
    REQUIRE(text->font("Arial", 50) == tvg::Result::Success);
    REQUIRE(text->font("InvalidFont", 80) == tvg::Result::InsufficientCondition);

    Initializer::term(CanvasEngine::Sw);
}


TEST_CASE("Text Basic", "[tvgText]")
{
    Initializer::init(CanvasEngine::Sw, 0);

    auto canvas = SwCanvas::gen();

    auto text = Text::gen();
    REQUIRE(text);

    REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == tvg::Result::Success);
    REQUIRE(text->font("Arial", 80) == tvg::Result::Success);

    REQUIRE(text->text("THORVG Text") == tvg::Result::Success);
    REQUIRE(text->text("ABCDEFGHIJIKLMOPQRSTUVWXYZ") == tvg::Result::Success);
    REQUIRE(text->text("") == tvg::Result::Success);
    REQUIRE(text->text(nullptr) == tvg::Result::Success);

    REQUIRE(text->fill(255, 255, 255) == tvg::Result::Success);

    REQUIRE(canvas->push(std::move(text)) == Result::Success);

    Initializer::term(CanvasEngine::Sw);
}

#endif
