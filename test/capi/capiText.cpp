/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#include <thorvg_capi.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "../catch.hpp"

#ifdef THORVG_TTF_LOADER_SUPPORT

TEST_CASE("Create text", "[capiText]")
{
    Tvg_Paint* text = tvg_text_new();
    REQUIRE(text);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_paint_get_identifier(text, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_TEXT);
    REQUIRE(id != TVG_IDENTIFIER_SHAPE);
    REQUIRE(id != TVG_IDENTIFIER_SCENE);
    REQUIRE(id != TVG_IDENTIFIER_PICTURE);

    REQUIRE(tvg_paint_del(text) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Load/unload TTF file", "[capiText]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_font_load(TEST_DIR"/invalid.ttf") == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_font_load("") == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_font_load(TEST_DIR"/Arial.ttf") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_font_unload(TEST_DIR"/Arial.ttf") == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_font_unload(TEST_DIR"/invalid.ttf") == TVG_RESULT_INSUFFICIENT_CONDITION);
    REQUIRE(tvg_font_unload("") == TVG_RESULT_INSUFFICIENT_CONDITION);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Load/unload TTF file from a memory", "[capiText]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    FILE *file = fopen(TEST_DIR"/Arial.ttf", "rb");
    REQUIRE(file);
    fseek(file, 0, SEEK_END);
    long data_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* data = (char*)malloc(data_size);
    REQUIRE(data);
    REQUIRE(fread(data, 1, data_size, file) > 0);

    static const char* svg = "<svg height=\"1000\" viewBox=\"0 0 600 600\" ></svg>";

    //load
    REQUIRE(tvg_font_load_data("Err", data, 0, "ttf", false) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_font_load_data(NULL, data, data_size, "ttf", false) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_font_load_data("Svg", svg, strlen(svg), "svg", false) == TVG_RESULT_NOT_SUPPORTED);
    REQUIRE(tvg_font_load_data("Arial1", data, data_size, "err", false) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_font_load_data("Arial2", data, data_size, "ttf", true) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_font_load_data("Arial3", data, data_size, NULL, false) == TVG_RESULT_SUCCESS);

    //unload
    REQUIRE(tvg_font_load_data("Err", NULL, data_size, "ttf", false) == TVG_RESULT_INSUFFICIENT_CONDITION);
    REQUIRE(tvg_font_load_data(NULL, NULL, data_size, "ttf", false) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_font_load_data("Arial1", NULL, 0, "ttf", false) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_font_load_data("Arial2", NULL, 0, NULL, false) == TVG_RESULT_SUCCESS);

    free(data);
    fclose(file);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set font", "[capiText]")
{
    Tvg_Paint* text = tvg_text_new();
    REQUIRE(text);

    REQUIRE(tvg_font_load(TEST_DIR"/Arial.ttf") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_text_set_font(NULL, "Arial", 10.0f, "") == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_text_set_font(text, "Unknown", 10.0f, "") == TVG_RESULT_INSUFFICIENT_CONDITION);
    REQUIRE(tvg_text_set_font(text, "Arial", 10.0f, "") == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_text_set_font(text, "Arial", 22.0f, "italic") == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_text_set_font(text, "Arial", 10.0f, "unknown") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_paint_del(text) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set text", "[capiText]")
{
    Tvg_Paint* text = tvg_text_new();
    REQUIRE(text);

    REQUIRE(tvg_font_load(TEST_DIR"/Arial.ttf") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_text_set_text(NULL, "some random text") == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_text_set_text(text, "") == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_text_set_text(text, NULL) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_text_set_text(text, "THORVG Text") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_paint_del(text) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set solid text fill", "[capiText]")
{
    Tvg_Paint* text = tvg_text_new();
    REQUIRE(text);

    REQUIRE(tvg_font_load(TEST_DIR"/Arial.ttf") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_text_set_fill_color(text, 10, 20, 30) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_text_set_font(text, "Arial", 10.0f, "") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_text_set_fill_color(NULL, 10, 20, 30) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_text_set_fill_color(text, 10, 20, 30) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_paint_del(text) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set gradient text fill", "[capiText]")
{
    Tvg_Paint* text = tvg_text_new();
    REQUIRE(text);

    Tvg_Gradient *gradientRad = tvg_radial_gradient_new();
    REQUIRE(gradientRad);
    REQUIRE(tvg_radial_gradient_set(gradientRad, 10.0, 15.0, 30.0) == TVG_RESULT_SUCCESS);

    Tvg_Gradient *gradientLin = tvg_linear_gradient_new();
    REQUIRE(gradientLin);
    REQUIRE(tvg_linear_gradient_set(gradientLin, 10.0, 20.0, 50.0, 40.0) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_font_load(TEST_DIR"/Arial.ttf") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_text_set_linear_gradient(text, NULL) == TVG_RESULT_MEMORY_CORRUPTION);
    REQUIRE(tvg_text_set_radial_gradient(text, NULL) == TVG_RESULT_MEMORY_CORRUPTION);

    REQUIRE(tvg_text_set_font(text, "Arial", 10.0f, "") == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_text_set_linear_gradient(text, NULL) == TVG_RESULT_MEMORY_CORRUPTION);
    REQUIRE(tvg_text_set_radial_gradient(text, NULL) == TVG_RESULT_MEMORY_CORRUPTION);
    REQUIRE(tvg_text_set_linear_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_text_set_radial_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_text_set_linear_gradient(text, gradientLin) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_text_set_radial_gradient(text, gradientRad) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_paint_del(text) == TVG_RESULT_SUCCESS);
}

#endif
