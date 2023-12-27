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

TEST_CASE("Basic Create", "[capiRadialGradient]")
{
    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_gradient_get_identifier(gradient, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_RADIAL_GRAD);
    REQUIRE(id != TVG_IDENTIFIER_LINEAR_GRAD);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set gradient center point and radius", "[capiRadialGradient]")
{
    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);
    REQUIRE(tvg_radial_gradient_set(gradient, 10.0, 15.0, 30.0) == TVG_RESULT_SUCCESS);

    float cx, cy, radius;
    REQUIRE(tvg_radial_gradient_get(gradient, &cx, &cy, &radius) == TVG_RESULT_SUCCESS);
    REQUIRE(cx == Approx(10.0).margin(0.000001));
    REQUIRE(cy == Approx(15.0).margin(0.000001));
    REQUIRE(radius == Approx(30.0).margin(0.000001));

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set gradient in shape", "[capiRadialGradient]")
{
    REQUIRE(tvg_shape_set_radial_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);

    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    REQUIRE(tvg_shape_set_radial_gradient(NULL, gradient) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_shape_set_radial_gradient(shape, gradient) == TVG_RESULT_SUCCESS);

    Tvg_Gradient *gradient_ret = NULL;
    REQUIRE(tvg_shape_get_gradient(shape, &gradient_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(gradient_ret);

    REQUIRE(tvg_shape_set_radial_gradient(shape, NULL) == TVG_RESULT_MEMORY_CORRUPTION);
    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set/Get color stops", "[capiRadialGradient]")
{
    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);

    Tvg_Color_Stop color_stops[2] = {
        {0.0, 0, 0,   0, 255},
        {1.0, 0, 255, 0, 255},
    };

    const Tvg_Color_Stop *color_stops_ret;
    uint32_t color_stops_count_ret;

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 2) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_get_color_stops(gradient, &color_stops_ret, &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_count_ret == 2);

    REQUIRE(color_stops_ret[0].a == 255);
    REQUIRE(color_stops_ret[1].g == 255);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Clear gradient data", "[capiRadialGradient]")
{
    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);

    Tvg_Color_Stop color_stops[2] = {
        {0.0, 0, 0,   0, 255},
        {1.0, 0, 255, 0, 255},
    };

    const Tvg_Color_Stop *color_stops_ret;
    uint32_t color_stops_count_ret;

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 2) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_get_color_stops(gradient, &color_stops_ret, &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_ret);
    REQUIRE(color_stops_count_ret == 2);

    REQUIRE(tvg_gradient_set_color_stops(gradient, NULL, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_get_color_stops(gradient, &color_stops_ret, &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_ret == nullptr);
    REQUIRE(color_stops_count_ret == 0);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Set/Get gradient spread", "[capiRadialGradient]")
{
    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);

    REQUIRE(tvg_gradient_set_spread(gradient, TVG_STROKE_FILL_PAD) == TVG_RESULT_SUCCESS);

    Tvg_Stroke_Fill spread;
    REQUIRE(tvg_gradient_get_spread(gradient, &spread) == TVG_RESULT_SUCCESS);
    REQUIRE(spread == TVG_STROKE_FILL_PAD);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_del(NULL) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("Radial Gradient transformation", "[capiRadialGradient]")
{
    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);

    Tvg_Matrix matrix_get;

    REQUIRE(tvg_gradient_get_transform(NULL, &matrix_get) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_gradient_get_transform(gradient, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_gradient_get_transform(gradient, &matrix_get) == TVG_RESULT_SUCCESS);

    REQUIRE(matrix_get.e11 == Approx(1.0f).margin(0.000001));
    REQUIRE(matrix_get.e12 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix_get.e13 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix_get.e21 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix_get.e22 == Approx(1.0f).margin(0.000001));
    REQUIRE(matrix_get.e23 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix_get.e31 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix_get.e32 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix_get.e33 == Approx(1.0f).margin(0.000001));

    Tvg_Matrix matrix_set = {1.1f, -2.2f, 3.3f, -4.4f, 5.5f, -6.6f, 7.7f, -8.8f, 9.9f};
    REQUIRE(tvg_gradient_set_transform(NULL, &matrix_set) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_gradient_set_transform(gradient, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_gradient_set_transform(gradient, &matrix_set) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_gradient_get_transform(gradient, &matrix_get) == TVG_RESULT_SUCCESS);

    REQUIRE(matrix_get.e11 == Approx(matrix_set.e11).margin(0.000001));
    REQUIRE(matrix_get.e12 == Approx(matrix_set.e12).margin(0.000001));
    REQUIRE(matrix_get.e13 == Approx(matrix_set.e13).margin(0.000001));
    REQUIRE(matrix_get.e21 == Approx(matrix_set.e21).margin(0.000001));
    REQUIRE(matrix_get.e22 == Approx(matrix_set.e22).margin(0.000001));
    REQUIRE(matrix_get.e23 == Approx(matrix_set.e23).margin(0.000001));
    REQUIRE(matrix_get.e31 == Approx(matrix_set.e31).margin(0.000001));
    REQUIRE(matrix_get.e32 == Approx(matrix_set.e32).margin(0.000001));
    REQUIRE(matrix_get.e33 == Approx(matrix_set.e33).margin(0.000001));

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Stroke Radial Gradient", "[capiRadialGradient]")
{
    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    Tvg_Gradient *gradient = tvg_radial_gradient_new();
    REQUIRE(gradient);

    REQUIRE(tvg_radial_gradient_set(gradient, 10.0, 15.0, 30.0) == TVG_RESULT_SUCCESS);

    Tvg_Color_Stop color_stops[2] = {
        {0.0, 0, 0,   0, 255},
        {1.0, 0, 255, 0, 255},
    };

    Tvg_Gradient *gradient_ret = NULL;
    const Tvg_Color_Stop *color_stops_ret = NULL;
    uint32_t color_stops_count_ret = 0;

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 2) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_set_stroke_radial_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_shape_set_stroke_radial_gradient(NULL, gradient) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_shape_set_stroke_radial_gradient(shape, gradient) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_get_stroke_gradient(shape, &gradient_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(gradient_ret);

    REQUIRE(tvg_gradient_get_color_stops(gradient_ret, &color_stops_ret,  &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_ret);
    REQUIRE(color_stops_count_ret == 2);

    float cx, cy, radius;
    REQUIRE(tvg_radial_gradient_get(gradient_ret, &cx, &cy, &radius) == TVG_RESULT_SUCCESS);
    REQUIRE(cx == Approx(10.0).margin(0.000001));
    REQUIRE(cy == Approx(15.0).margin(0.000001));
    REQUIRE(radius == Approx(30.0).margin(0.000001));

    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}
