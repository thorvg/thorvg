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

#include <fstream>
#include <cstring>
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

    REQUIRE(text->type() == Type::Text);

    Paint::rel(text);
}

TEST_CASE("Load TTF Data from a file", "[tvgText]")
{
    Initializer::init();
    {
        auto text = Text::gen();
        REQUIRE(text);

        REQUIRE(Text::unload(TEST_DIR"/invalid.ttf") == Result::InsufficientCondition);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);
        REQUIRE(Text::load(TEST_DIR"/invalid.ttf") == Result::InvalidArguments);
        REQUIRE(Text::unload(TEST_DIR"/Arial.ttf") == Result::Success);
        REQUIRE(Text::load("") == Result::InvalidArguments);
        REQUIRE(Text::load(TEST_DIR"/NanumGothicCoding.ttf") == Result::Success);

        Paint::rel(text);
    }
    Initializer::term();
}

TEST_CASE("Load TTF Data from a memory", "[tvgText]")
{
    Initializer::init();
    {
        ifstream file(TEST_DIR"/Arial.ttf", ios::binary);
        REQUIRE(file.is_open());
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        auto data = (char*)malloc(size);
        REQUIRE(data);
        file.read(data, size);
        file.close();

        auto text = Text::gen();
        REQUIRE(text);

        static const char* svg = "<svg height=\"1000\" viewBox=\"0 0 600 600\" ></svg>";

        //load
        REQUIRE(Text::load(nullptr, data, size) == Result::InvalidArguments);
        REQUIRE(Text::load("Arial", data, 0) == Result::InvalidArguments);
        REQUIRE(Text::load("Arial", data, 0) == Result::InvalidArguments);
        REQUIRE(Text::load("ArialSvg", svg, strlen(svg), "unknown") == Result::NonSupport);
        REQUIRE(Text::load("ArialUnknown", data, size, "unknown") == Result::Success);
        REQUIRE(Text::load("ArialTtf", data, size, "ttf", true) == Result::Success);
        REQUIRE(Text::load("Arial", data, size, "") == Result::Success);

        //unload
        REQUIRE(Text::load("invalid", nullptr, 0) == Result::InsufficientCondition);
        REQUIRE(Text::load("ArialSvg", nullptr, 0) == Result::InsufficientCondition);
        REQUIRE(Text::load("ArialUnknown", nullptr, 0) == Result::Success);
        REQUIRE(Text::load("ArialTtf", nullptr, 0) == Result::Success);
        REQUIRE(Text::load("Arial", nullptr, 111) == Result::Success);

        free(data);

        Paint::rel(text);
    }
    Initializer::term();
}

TEST_CASE("Text Font", "[tvgText]")
{
    Initializer::init();
    {
        auto text = Text::gen();
        REQUIRE(text);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);

        REQUIRE(text->font("Arial") == Result::Success);
        REQUIRE(text->size(80) == Result::Success);
        REQUIRE(text->font("Arial") == Result::Success);
        REQUIRE(text->size(1) == Result::Success);
        REQUIRE(text->size(50) == Result::Success);
        REQUIRE(text->font(nullptr) == Result::Success);
        REQUIRE(text->font("InvalidFont") == Result::InsufficientCondition);

        Paint::rel(text);
    }
    Initializer::term();
}

TEST_CASE("Text Basic", "[tvgText]")
{
    Initializer::init();
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100];
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto text = Text::gen();
        REQUIRE(text);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);
        REQUIRE(text->font("Arial") == Result::Success);
        REQUIRE(text->size(80) == Result::Success);

        REQUIRE(text->text(nullptr) == Result::Success);
        REQUIRE(text->text("") == Result::Success);
        REQUIRE(text->text("ABCDEFGHIJIKLMOPQRSTUVWXYZ") == Result::Success);
        REQUIRE(text->text("THORVG Text") == Result::Success);

        REQUIRE(text->fill(255, 255, 255) == Result::Success);

        REQUIRE(canvas->push(text) == Result::Success);
    }
    Initializer::term();
}

TEST_CASE("Text with composite glyphs", "[tvgText]")
{
    Initializer::init();
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100];
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto text = Text::gen();
        REQUIRE(text);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);
        REQUIRE(text->font("Arial") == Result::Success);
        REQUIRE(text->size(80) == Result::Success);

        REQUIRE(text->text("\xc5\xbb\x6f\xc5\x82\xc4\x85\x64\xc5\xba \xc8\xab") == Result::Success);

        REQUIRE(text->fill(255, 255, 255) == Result::Success);

        REQUIRE(canvas->push(text) == Result::Success);
    }
    Initializer::term();
}

TEST_CASE("Text Styles", "[tvgText]")
{
    Initializer::init();
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100];
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto text = Text::gen();
        REQUIRE(text);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);
        REQUIRE(text->font("Arial") == Result::Success);
        REQUIRE(text->size(80) == Result::Success);

        REQUIRE(text->text("\xc5\xbb\x6f\xc5\x82\xc4\x85\x64\xc5\xba \xc8\xab") == Result::Success);

        REQUIRE(text->fill(255, 255, 255) == Result::Success);

        REQUIRE(text->outline(0, 0, 0, 0) == Result::Success);
        REQUIRE(text->outline(3, 255, 255, 255) == Result::Success);
        REQUIRE(text->outline(0, 0, 0, 0) == Result::Success);

        REQUIRE(text->italic(-10.0f) == Result::Success);
        REQUIRE(text->italic(10000.0f) == Result::Success);
        REQUIRE(text->italic(0.0) == Result::Success);
        REQUIRE(text->italic(0.18f) == Result::Success);

        REQUIRE(canvas->push(text) == Result::Success);
    }
    Initializer::term();
}

TEST_CASE("Text Layout & Align", "[tvgText]")
{
    Initializer::init();
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100];
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto text = Text::gen();
        REQUIRE(text);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);
        REQUIRE(text->font("Arial") == Result::Success);
        REQUIRE(text->size(80) == Result::Success);
        REQUIRE(text->text("\xc5\xbb\x6f\xc5\x82\xc4\x85\x64\xc5\xba \xc8\xab") == Result::Success);

        REQUIRE(text->align(0.0f, 0.0f) == Result::Success);
        REQUIRE(text->align(0.5f, 0.5f) == Result::Success);
        REQUIRE(text->align(1.0f, 1.0f) == Result::Success);
        REQUIRE(text->align(2.0f, 2.0f) == Result::Success);
        REQUIRE(text->align(-1.0f, -1.0f) == Result::Success);

        REQUIRE(text->layout(300, 300) == Result::Success);
        REQUIRE(text->layout(0, 0) == Result::Success);
        REQUIRE(text->layout(-100, -100) == Result::Success);

        REQUIRE(text->wrap(TextWrap::Character) == Result::Success);
        REQUIRE(text->wrap(TextWrap::Ellipsis) == Result::Success);
        REQUIRE(text->wrap(TextWrap::None) == Result::Success);
        REQUIRE(text->wrap(TextWrap::Smart) == Result::Success);
        REQUIRE(text->wrap(TextWrap::Word) == Result::Success);

        REQUIRE(canvas->push(text) == Result::Success);
    }
    Initializer::term();
}

TEST_CASE("Text Spacing", "[tvgText]")
{
    Initializer::init();
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[100*100];
        canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888);

        auto text = Text::gen();
        REQUIRE(text);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);
        REQUIRE(text->font("Arial") == Result::Success);
        REQUIRE(text->size(80) == Result::Success);
        REQUIRE(text->text("\xc5\xbb\x6f\xc5\x82\xc4\x85\x64\xc5\xba \xc8\xab") == Result::Success);
        REQUIRE(text->spacing(-1.0f, -1.0f) == Result::InvalidArguments);
        REQUIRE(text->spacing(0.0f, 0.0f) == Result::Success);
        REQUIRE(text->spacing(1.5f, 1.5f) == Result::Success);
        REQUIRE(text->spacing(2.0f, 2.0f) == Result::Success);

        REQUIRE(canvas->push(text) == Result::Success);
    }
    Initializer::term();
}

#endif