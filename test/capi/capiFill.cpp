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

#include <thorvg_capi.h>
#include "config.h"
#include "../catch.hpp"


TEST_CASE("Set/Get fill color", "[capiShapeFill]")
{
    Tvg_Paint *paint = tvg_shape_new();
    REQUIRE(paint);

    REQUIRE(tvg_shape_set_fill_color(paint, 120, 154, 180, 100) == TVG_RESULT_SUCCESS);

    uint8_t r = 0, g = 0, b = 0, a = 0;
    REQUIRE(tvg_shape_get_fill_color(paint, &r, &g, &b, &a) == TVG_RESULT_SUCCESS);

    REQUIRE(r == 120);
    REQUIRE(g == 154);
    REQUIRE(b == 180);
    REQUIRE(a == 100);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set/Get fill color on invalid shape", "[capiShapeFill]")
{
    REQUIRE(tvg_shape_set_fill_color(NULL, 120, 154, 180, 100) == TVG_RESULT_INVALID_ARGUMENT);

    uint8_t r, g, b, a;
    REQUIRE(tvg_shape_get_fill_color(NULL, &r, &g, &b, &a) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("Set/Get shape fill rule", "[capiShapeFill]")
{
    Tvg_Paint *paint = tvg_shape_new();
    REQUIRE(paint);

    REQUIRE(tvg_shape_set_fill_rule(paint, TVG_FILL_RULE_EVEN_ODD) == TVG_RESULT_SUCCESS);

    Tvg_Fill_Rule rule = TVG_FILL_RULE_WINDING;
    REQUIRE(tvg_shape_get_fill_rule(paint, &rule) == TVG_RESULT_SUCCESS);
    REQUIRE(rule == TVG_FILL_RULE_EVEN_ODD);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set/Get shape fill rule on invalid object", "[capiShapeFill]")
{
    REQUIRE(tvg_shape_set_fill_rule(NULL, TVG_FILL_RULE_EVEN_ODD) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Fill_Rule rule;
    REQUIRE(tvg_shape_get_fill_rule(NULL, &rule) == TVG_RESULT_INVALID_ARGUMENT);
}
