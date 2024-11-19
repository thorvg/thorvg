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

TEST_CASE("Linear Gradient Basic Create", "[capiLinearGradient]")
{
    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);

    Tvg_Type type = TVG_TYPE_UNDEF;
    REQUIRE(tvg_gradient_get_type(gradient, &type) == TVG_RESULT_SUCCESS);
    REQUIRE(type == TVG_TYPE_LINEAR_GRAD);
    REQUIRE(type != TVG_TYPE_RADIAL_GRAD);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Linear Gradient start and end position", "[capiLinearGradient]")
{
    Tvg_Gradient *gradient = tvg_linear_gradient_new();

    REQUIRE(gradient);
    REQUIRE(tvg_linear_gradient_set(gradient, 10.0, 20.0, 50.0, 40.0) == TVG_RESULT_SUCCESS);

    float x1, y1, x2, y2;
    REQUIRE(tvg_linear_gradient_get(gradient, &x1, &y1, &x2, &y2) == TVG_RESULT_SUCCESS);
    REQUIRE(x1 == 10.0);
    REQUIRE(y1 == 20.0);
    REQUIRE(x2 == 50.0);
    REQUIRE(y2 == 40.0);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Linear Gradient in shape", "[capiLinearGradient]")
{
    REQUIRE(tvg_shape_set_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);
    REQUIRE(tvg_shape_set_gradient(NULL, gradient) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    REQUIRE(tvg_shape_set_gradient(shape, gradient) == TVG_RESULT_SUCCESS);

    Tvg_Gradient *gradient_ret = NULL;
    REQUIRE(tvg_shape_get_gradient(shape, &gradient_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(gradient_ret);

    REQUIRE(tvg_shape_set_gradient(shape, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Linear Gradient color stops", "[capiLinearGradient]")
{
    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);

    Tvg_Color_Stop color_stops[2] =
    {
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

TEST_CASE("Linear Gradient duplicate", "[capiLinearGradient]")
{
    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);

    Tvg_Color_Stop color_stops[3] =
    {
        {0.0f, 255,   0,   0, 155},
        {0.8f,   0, 255,   0, 155},
        {1.0f, 128,   0, 128, 155},
    };
    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 3) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_linear_gradient_set(gradient, 11.1f, 22.2f, 33.3f, 44.4f) == TVG_RESULT_SUCCESS);

    Tvg_Gradient *gradient_dup = tvg_gradient_duplicate(gradient);
    REQUIRE(gradient_dup);

    const Tvg_Color_Stop *color_stops_dup = NULL;
    uint32_t color_stops_count_dup = 0;
    REQUIRE(tvg_gradient_get_color_stops(gradient_dup, &color_stops_dup,  &color_stops_count_dup) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_count_dup == 3);
    REQUIRE(color_stops_dup);
    REQUIRE(color_stops_dup[1].offset == Approx(0.8f).margin(0.000001));
    REQUIRE(color_stops_dup[2].a == 155);
    REQUIRE(color_stops_dup[2].r == 128);

    float x1, y1, x2, y2;
    REQUIRE(tvg_linear_gradient_get(gradient_dup, &x1, &y1, &x2, &y2) == TVG_RESULT_SUCCESS);
    REQUIRE(x1 == Approx(11.1f).margin(0.000001));
    REQUIRE(y1 == Approx(22.2f).margin(0.000001));
    REQUIRE(x2 == Approx(33.3f).margin(0.000001));
    REQUIRE(y2 == Approx(44.4f).margin(0.000001));

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_del(gradient_dup) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Linear Gradient type", "[capiLinearGradient]")
{
    Tvg_Gradient* grad = tvg_linear_gradient_new();
    REQUIRE(grad);

    Tvg_Gradient* grad_copy = tvg_gradient_duplicate(grad);
    REQUIRE(grad_copy);

    Tvg_Type type = TVG_TYPE_UNDEF;
    Tvg_Type type2 = TVG_TYPE_UNDEF;

    REQUIRE(tvg_gradient_get_type(nullptr, &type) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_gradient_get_type(grad, nullptr) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_gradient_get_type(grad, &type) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_get_type(grad_copy, &type2) == TVG_RESULT_SUCCESS);
    REQUIRE(type2 == type);

    REQUIRE(tvg_gradient_del(grad_copy) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_del(grad) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Linear Gradient clear data", "[capiLinearGradient]")
{
    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);

    Tvg_Color_Stop color_stops[2] =
    {
        {0.0, 0, 0,   0, 255},
        {1.0, 0, 255, 0, 255},
    };

    const Tvg_Color_Stop *color_stops_ret = NULL;
    uint32_t color_stops_count_ret = 0;

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 2) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_get_color_stops(gradient, &color_stops_ret,  &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_ret);
    REQUIRE(color_stops_count_ret == 2);

    REQUIRE(tvg_gradient_set_color_stops(gradient, NULL, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_get_color_stops(gradient, &color_stops_ret, &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_ret == nullptr);
    REQUIRE(color_stops_count_ret == 0);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Linear Gradient spread", "[capiLinearGradient]")
{
    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);

    Tvg_Stroke_Fill spread;
    REQUIRE(tvg_gradient_set_spread(gradient, TVG_STROKE_FILL_PAD) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_get_spread(gradient, &spread) == TVG_RESULT_SUCCESS);
    REQUIRE(spread == TVG_STROKE_FILL_PAD);

    REQUIRE(tvg_gradient_del(gradient) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_gradient_del(NULL) == TVG_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("Linear Gradient transformation", "[capiLinearGradient]")
{
    Tvg_Gradient *gradient = tvg_linear_gradient_new();
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

TEST_CASE("Stroke Linear Gradient", "[capiLinearGradient]")
{
    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);

    Tvg_Color_Stop color_stops[2] =
    {
        {0.0, 0, 0,   0, 255},
        {1.0, 0, 255, 0, 255},
    };

    Tvg_Gradient *gradient_ret = NULL;
    const Tvg_Color_Stop *color_stops_ret = NULL;
    uint32_t color_stops_count_ret = 0;

    REQUIRE(tvg_gradient_set_color_stops(gradient, color_stops, 2) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_set_stroke_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_shape_set_stroke_gradient(NULL, gradient) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_shape_set_stroke_gradient(shape, gradient) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_get_stroke_gradient(shape, &gradient_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(gradient_ret);

    REQUIRE(tvg_gradient_get_color_stops(gradient_ret, &color_stops_ret,  &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_ret);
    REQUIRE(color_stops_count_ret == 2);

    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}
