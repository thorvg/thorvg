/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include "catch.hpp"

#define UTC_EPSILON 1e-6f

TEST_CASE("Paint Transform", "[capiPaintTransform]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Matrix matrix_set = {1, 0, 0, 0, 1, 0, 0, 0, 1}, matrix_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_paint_transform(paint, &matrix_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix_get) == TVG_RESULT_SUCCESS);
    REQUIRE(matrix_get.e11 == matrix_set.e11);
    REQUIRE(matrix_get.e12 == matrix_set.e12);
    REQUIRE(matrix_get.e13 == matrix_set.e13);
    REQUIRE(matrix_get.e21 == matrix_set.e21);
    REQUIRE(matrix_get.e22 == matrix_set.e22);
    REQUIRE(matrix_get.e23 == matrix_set.e23);
    REQUIRE(matrix_get.e31 == matrix_set.e31);
    REQUIRE(matrix_get.e32 == matrix_set.e32);
    REQUIRE(matrix_get.e33 == matrix_set.e33);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Translate", "[capiPaintTranslate]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Matrix matrix_get;
    float tx = 20, ty = 30;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_paint_translate(paint, tx, ty) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix_get) == TVG_RESULT_SUCCESS);
    REQUIRE(fabs(matrix_get.e11 - 1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e12 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e13 - tx) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e21 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e22 - 1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e23 - ty) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e31 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e32 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e33 - 1.0f) < UTC_EPSILON);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Scale", "[capiPaintScale]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Matrix matrix_get;
    float scale = 2.5f;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_paint_scale(paint, scale) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix_get) == TVG_RESULT_SUCCESS);
    REQUIRE(fabs(matrix_get.e11 - scale) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e12 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e13 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e21 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e22 - scale) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e23 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e31 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e32 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e33 - 1.0f) < UTC_EPSILON);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Rotate", "[capiPaintRotate]")
{
    Tvg_Paint *paint = NULL;
    Tvg_Matrix matrix_get;
    float degree = 180.0f;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_paint_rotate(paint, degree) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix_get) == TVG_RESULT_SUCCESS);
    REQUIRE(fabs(matrix_get.e11 - -1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e12 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e13 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e21 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e22 - -1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e23 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e31 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e32 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e33 - 1.0f) < UTC_EPSILON);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Opacity", "[capiPaintOpacity]")
{
    Tvg_Paint *paint = NULL;
    uint8_t opacity_set, opacity_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    opacity_set = 0;
    REQUIRE(tvg_paint_set_opacity(paint, opacity_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_opacity(paint, &opacity_get) == TVG_RESULT_SUCCESS);
    REQUIRE(opacity_get == opacity_get);

    opacity_set = 128;
    REQUIRE(tvg_paint_set_opacity(paint, opacity_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_opacity(paint, &opacity_get) == TVG_RESULT_SUCCESS);
    REQUIRE(opacity_get == opacity_get);

    opacity_set = 255;
    REQUIRE(tvg_paint_set_opacity(paint, opacity_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_opacity(paint, &opacity_get) == TVG_RESULT_SUCCESS);
    REQUIRE(opacity_get == opacity_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Bounds", "[capiPaintBounds]")
{
    Tvg_Paint *paint = NULL;
    float x = 0, y = 10, w = 20, h = 100;
    float x_get, y_get, w_get, h_get;

    paint = tvg_shape_new();
    REQUIRE(paint != NULL);

    REQUIRE(tvg_shape_append_rect(paint, x, y, w, h, 0, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_bounds(paint, &x_get, &y_get, &w_get, &h_get) == TVG_RESULT_SUCCESS);

    REQUIRE(x == x_get);
    REQUIRE(y == y_get);
    REQUIRE(w == w_get);
    REQUIRE(h == h_get);

    REQUIRE(tvg_shape_reset(paint) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_move_to(paint, x, y) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_line_to(paint, x + w, y + h) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_bounds(paint, &x_get, &y_get, &w_get, &h_get) == TVG_RESULT_SUCCESS);

    REQUIRE(x == x_get);
    REQUIRE(y == y_get);
    REQUIRE(w == w_get);
    REQUIRE(h == h_get);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}
