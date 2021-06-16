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

#define UTC_EPSILON 1e-6f

TEST_CASE("Paint Transform", "[capiPaintTransform]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix_set = {1, 0, 0, 0, 1, 0, 0, 0, 1}, matrix_get;

    REQUIRE(tvg_paint_transform(paint, &matrix_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix_get) == TVG_RESULT_SUCCESS);
    REQUIRE(fabs(matrix_get.e11 - matrix_set.e11) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e12 - matrix_set.e12) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e13 - matrix_set.e13) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e21 - matrix_set.e21) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e22 - matrix_set.e22) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e23 - matrix_set.e23) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e31 - matrix_set.e31) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e32 - matrix_set.e32) < UTC_EPSILON);
    REQUIRE(fabs(matrix_get.e33 - matrix_set.e33) < UTC_EPSILON);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Translate", "[capiPaintTranslate]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix;

    REQUIRE(tvg_paint_translate(paint, 20, 30) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix) == TVG_RESULT_SUCCESS);
    REQUIRE(fabs(matrix.e11 - 1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e12 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e13 - 20) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e21 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e22 - 1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e23 - 30) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e31 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e32 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e33 - 1.0f) < UTC_EPSILON);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Scale", "[capiPaintScale]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix;

    REQUIRE(tvg_paint_scale(paint, 2.5f) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix) == TVG_RESULT_SUCCESS);
    REQUIRE(fabs(matrix.e11 - 2.5f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e12 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e13 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e21 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e22 - 2.5f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e23 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e31 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e32 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e33 - 1.0f) < UTC_EPSILON);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Rotate", "[capiPaintRotate]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix;

    REQUIRE(tvg_paint_rotate(paint, 180.0f) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix) == TVG_RESULT_SUCCESS);
    REQUIRE(fabs(matrix.e11 - -1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e12 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e13 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e21 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e22 - -1.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e23 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e31 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e32 - 0.0f) < UTC_EPSILON);
    REQUIRE(fabs(matrix.e33 - 1.0f) < UTC_EPSILON);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Opacity", "[capiPaintOpacity]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    uint8_t opacity;

    REQUIRE(tvg_paint_set_opacity(paint, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_opacity(paint, &opacity) == TVG_RESULT_SUCCESS);
    REQUIRE(0 == opacity);

    REQUIRE(tvg_paint_set_opacity(paint, 128) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_opacity(paint, &opacity) == TVG_RESULT_SUCCESS);
    REQUIRE(128 == opacity);

    REQUIRE(tvg_paint_set_opacity(paint, 255) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_opacity(paint, &opacity) == TVG_RESULT_SUCCESS);
    REQUIRE(255 == opacity);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Bounds", "[capiPaintBounds]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    float x, y, w, h;

    REQUIRE(tvg_shape_append_rect(paint, 0, 10, 20, 100, 0, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_bounds(paint, &x, &y, &w, &h) == TVG_RESULT_SUCCESS);

    REQUIRE(x == 0);
    REQUIRE(y == 10);
    REQUIRE(w == 20);
    REQUIRE(h == 100);

    REQUIRE(tvg_shape_reset(paint) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_move_to(paint, 0, 10) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_line_to(paint, 20, 110) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_bounds(paint, &x, &y, &w, &h) == TVG_RESULT_SUCCESS);

    REQUIRE(x == 0);
    REQUIRE(y == 10);
    REQUIRE(w == 20);
    REQUIRE(h == 100);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}