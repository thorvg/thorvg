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
    auto text = unique_ptr<Text>(Text::gen());
    REQUIRE(text);

    REQUIRE(text->type() == Type::Text);
}

TEST_CASE("Load TTF Data from a file", "[tvgText]")
{
    Initializer::init();
    {
        auto text = unique_ptr<Text>(Text::gen());
        REQUIRE(text);

        REQUIRE(Text::unload(TEST_DIR"/invalid.ttf") == tvg::Result::InsufficientCondition);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == tvg::Result::Success);
        REQUIRE(Text::load(TEST_DIR"/invalid.ttf") == tvg::Result::InvalidArguments);
        REQUIRE(Text::unload(TEST_DIR"/Arial.ttf") == tvg::Result::Success);
        REQUIRE(Text::load("") == tvg::Result::InvalidArguments);
        REQUIRE(Text::load(TEST_DIR"/NanumGothicCoding.ttf") == tvg::Result::Success);
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

        auto text = unique_ptr<Text>(Text::gen());
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
    }
    Initializer::term();
}

TEST_CASE("Text Font", "[tvgText]")
{
    Initializer::init();
    {
        auto text = unique_ptr<Text>(Text::gen());
        REQUIRE(text);

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == tvg::Result::Success);

        REQUIRE(text->font("Arial", 80) == tvg::Result::Success);
        REQUIRE(text->font("Arial", 1) == tvg::Result::Success);
        REQUIRE(text->font("Arial", 50) == tvg::Result::Success);
        REQUIRE(text->font(nullptr, 50) == tvg::Result::Success);
        REQUIRE(text->font("InvalidFont", 80) == tvg::Result::InsufficientCondition);
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

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == tvg::Result::Success);
        REQUIRE(text->font("Arial", 80) == tvg::Result::Success);

        REQUIRE(text->text(nullptr) == tvg::Result::Success);
        REQUIRE(text->text("") == tvg::Result::Success);
        REQUIRE(text->text("ABCDEFGHIJIKLMOPQRSTUVWXYZ") == tvg::Result::Success);
        REQUIRE(text->text("THORVG Text") == tvg::Result::Success);

        REQUIRE(text->fill(255, 255, 255) == tvg::Result::Success);

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

        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == tvg::Result::Success);
        REQUIRE(text->font("Arial", 80) == tvg::Result::Success);

        REQUIRE(text->text("\xc5\xbb\x6f\xc5\x82\xc4\x85\x64\xc5\xba \xc8\xab") == tvg::Result::Success);

        REQUIRE(text->fill(255, 255, 255) == tvg::Result::Success);

        REQUIRE(canvas->push(text) == Result::Success);
    }
    Initializer::term();
}

TEST_CASE("Text Duplication", "[tvgText]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        REQUIRE(Text::load(TEST_DIR"/Arial.ttf") == Result::Success);

        auto text = Text::gen();
        REQUIRE(text);
        REQUIRE(text->font("Arial", 32) == Result::Success);
        REQUIRE(text->text("Original Text") == Result::Success);
        REQUIRE(text->fill(255, 0, 0) == Result::Success);

        REQUIRE(text->opacity(0) == Result::Success);
        REQUIRE(text->translate(200.0f, 100.0f) == Result::Success);
        REQUIRE(text->scale(2.2f) == Result::Success);
        REQUIRE(text->rotate(90.0f) == Result::Success);

        auto comp = Shape::gen();
        REQUIRE(comp);
        REQUIRE(text->clip(comp) == Result::Success);

        //Duplication
        auto dup = unique_ptr<Paint>(text->duplicate());
        REQUIRE(dup);

        //Compare properties
        REQUIRE(dup->type() == Type::Text);
        REQUIRE(dup->opacity() == 0);

        auto m = text->transform();
        REQUIRE(m.e11 == Approx(0.0f).margin(0.000001));
        REQUIRE(m.e12 == Approx(-2.2f).margin(0.000001));
        REQUIRE(m.e13 == Approx(200.0f).margin(0.000001));
        REQUIRE(m.e21 == Approx(2.2f).margin(0.000001));
        REQUIRE(m.e22 == Approx(0.0f).margin(0.000001));
        REQUIRE(m.e23 == Approx(100.0f).margin(0.000001));
        REQUIRE(m.e31 == Approx(0.0f).margin(0.000001));
        REQUIRE(m.e32 == Approx(0.0f).margin(0.000001));
        REQUIRE(m.e33 == Approx(1.0f).margin(0.000001));
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
