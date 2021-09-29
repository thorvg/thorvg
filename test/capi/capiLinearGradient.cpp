/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include "../catch.hpp"

TEST_CASE("Linear Gradient Basic Create", "[capiLinearGradient]")
{
    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);
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
    REQUIRE(tvg_shape_set_linear_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Gradient *gradient = tvg_linear_gradient_new();
    REQUIRE(gradient);
    REQUIRE(tvg_shape_set_linear_gradient(NULL, gradient) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Paint *shape = tvg_shape_new();
    REQUIRE(shape);

    REQUIRE(tvg_shape_set_linear_gradient(shape, gradient) == TVG_RESULT_SUCCESS);

    Tvg_Gradient *gradient_ret = NULL;
    REQUIRE(tvg_shape_get_gradient(shape, &gradient_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(gradient_ret);

    REQUIRE(tvg_shape_set_linear_gradient(shape, NULL) == TVG_RESULT_MEMORY_CORRUPTION);
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
    REQUIRE(color_stops_ret == NULL);
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

    REQUIRE(tvg_shape_set_stroke_linear_gradient(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_shape_set_stroke_linear_gradient(NULL, gradient) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_shape_set_stroke_linear_gradient(shape, gradient) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_get_stroke_gradient(shape, &gradient_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(gradient_ret);

    REQUIRE(tvg_gradient_get_color_stops(gradient_ret, &color_stops_ret,  &color_stops_count_ret) == TVG_RESULT_SUCCESS);
    REQUIRE(color_stops_ret);
    REQUIRE(color_stops_count_ret == 2);

    REQUIRE(tvg_paint_del(shape) == TVG_RESULT_SUCCESS);
}
