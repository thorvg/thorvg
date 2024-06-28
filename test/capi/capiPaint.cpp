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


TEST_CASE("Paint Transform", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix_set = {1, 0, 0, 0, 1, 0, 0, 0, 1}, matrix_get;

    REQUIRE(tvg_paint_set_transform(paint, &matrix_set) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix_get) == TVG_RESULT_SUCCESS);
    REQUIRE(matrix_get.e11 == Approx(matrix_set.e11).margin(0.000001));
    REQUIRE(matrix_get.e12 == Approx(matrix_set.e12).margin(0.000001));
    REQUIRE(matrix_get.e13 == Approx(matrix_set.e13).margin(0.000001));
    REQUIRE(matrix_get.e21 == Approx(matrix_set.e21).margin(0.000001));
    REQUIRE(matrix_get.e22 == Approx(matrix_set.e22).margin(0.000001));
    REQUIRE(matrix_get.e23 == Approx(matrix_set.e23).margin(0.000001));
    REQUIRE(matrix_get.e31 == Approx(matrix_set.e31).margin(0.000001));
    REQUIRE(matrix_get.e32 == Approx(matrix_set.e32).margin(0.000001));
    REQUIRE(matrix_get.e33 == Approx(matrix_set.e33).margin(0.000001));

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Translate", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix;

    REQUIRE(tvg_paint_translate(paint, 20, 30) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix) == TVG_RESULT_SUCCESS);
    REQUIRE(matrix.e11 == Approx(1).margin(0.000001));
    REQUIRE(matrix.e12 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e13 == Approx(20).margin(0.000001));
    REQUIRE(matrix.e21 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e22 == Approx(1).margin(0.000001));
    REQUIRE(matrix.e23 == Approx(30).margin(0.000001));
    REQUIRE(matrix.e31 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e32 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e33 == Approx(1).margin(0.000001));

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Scale", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix;

    REQUIRE(tvg_paint_scale(paint, 2.5f) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix) == TVG_RESULT_SUCCESS);
    REQUIRE(matrix.e11 == Approx(2.5).margin(0.000001));
    REQUIRE(matrix.e12 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e13 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e21 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e22 == Approx(2.5).margin(0.000001));
    REQUIRE(matrix.e23 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e31 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e32 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e33 == Approx(1).margin(0.000001));

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Rotate", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Matrix matrix;

    REQUIRE(tvg_paint_rotate(paint, 180.0f) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_transform(paint, &matrix) == TVG_RESULT_SUCCESS);
    REQUIRE(matrix.e11 == Approx(-1).margin(0.000001));
    REQUIRE(matrix.e12 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e13 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e21 == Approx(-0).margin(0.000001));
    REQUIRE(matrix.e22 == Approx(-1).margin(0.000001));
    REQUIRE(matrix.e23 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e31 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e32 == Approx(0).margin(0.000001));
    REQUIRE(matrix.e33 == Approx(1).margin(0.000001));

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Opacity", "[capiPaint]")
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

TEST_CASE("Paint Bounds", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    float x, y, w, h;

    REQUIRE(tvg_shape_append_rect(paint, 0, 10, 20, 100, 0, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_bounds(paint, &x, &y, &w, &h, true) == TVG_RESULT_SUCCESS);

    REQUIRE(x == 0);
    REQUIRE(y == 10);
    REQUIRE(w == 20);
    REQUIRE(h == 100);

    REQUIRE(tvg_shape_reset(paint) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_paint_translate(paint, 100, 100) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_scale(paint, 2) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_shape_move_to(paint, 0, 10) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_line_to(paint, 20, 110) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_bounds(paint, &x, &y, &w, &h, false) == TVG_RESULT_SUCCESS);

    REQUIRE(x == 0);
    REQUIRE(y == 10);
    REQUIRE(w == 20);
    REQUIRE(h == 100);

    REQUIRE(tvg_paint_get_bounds(paint, &x, &y, &w, &h, true) == TVG_RESULT_SUCCESS);

    REQUIRE(x == Approx(100.0f).margin(0.000001));
    REQUIRE(y == Approx(120.0f).margin(0.000001));
    REQUIRE(w == Approx(40.0f).margin(0.000001));
    REQUIRE(h == Approx(200.0f).margin(0.000001));

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Duplication", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    REQUIRE(tvg_paint_set_opacity(paint, 0) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_paint_translate(paint, 200, 100) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_scale(paint, 2.2f) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_rotate(paint, 90.0f) == TVG_RESULT_SUCCESS);

    Tvg_Paint* paint_copy = tvg_paint_duplicate(paint);
    REQUIRE(paint_copy);

    uint8_t opacity;
    REQUIRE(tvg_paint_get_opacity(paint_copy, &opacity) == TVG_RESULT_SUCCESS);
    REQUIRE(0 == opacity);

    Tvg_Matrix matrix;
    REQUIRE(tvg_paint_get_transform(paint, &matrix) == TVG_RESULT_SUCCESS);
    REQUIRE(matrix.e11 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix.e12 == Approx(-2.2f).margin(0.000001));
    REQUIRE(matrix.e13 == Approx(200.0f).margin(0.000001));
    REQUIRE(matrix.e21 == Approx(2.2f).margin(0.000001));
    REQUIRE(matrix.e22 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix.e23 == Approx(100.0f).margin(0.000001));
    REQUIRE(matrix.e31 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix.e32 == Approx(0.0f).margin(0.000001));
    REQUIRE(matrix.e33 == Approx(1.0f).margin(0.000001));

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_del(paint_copy) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Identifier", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Paint* paint_copy = tvg_paint_duplicate(paint);
    REQUIRE(paint_copy);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    Tvg_Identifier id_copy = TVG_IDENTIFIER_UNDEF;

    REQUIRE(tvg_paint_get_identifier(nullptr, &id) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_identifier(paint, nullptr) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_identifier(paint, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_get_identifier(paint_copy, &id_copy) == TVG_RESULT_SUCCESS);
    REQUIRE(id_copy == id);

    REQUIRE(tvg_paint_del(paint_copy) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint Clip Path Composite Method", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Paint* target = tvg_shape_new();
    REQUIRE(target);

    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_set_composite_method(paint, target, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_CLIP_PATH) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Paint* target2 = tvg_shape_new();
    REQUIRE(target2);
    REQUIRE(tvg_paint_set_composite_method(paint, target2, TVG_COMPOSITE_METHOD_CLIP_PATH) == TVG_RESULT_SUCCESS);

    const Tvg_Paint* target3 = nullptr;
    Tvg_Composite_Method method = TVG_COMPOSITE_METHOD_NONE;
    REQUIRE(tvg_paint_get_composite_method(paint, NULL, &method) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target3, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target3, &method) == TVG_RESULT_SUCCESS);
    REQUIRE(method == TVG_COMPOSITE_METHOD_CLIP_PATH);
    REQUIRE(target2 == target3);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint AlphaMask Composite Method", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Paint* target = tvg_shape_new();
    REQUIRE(target);

    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_set_composite_method(paint, target, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_ALPHA_MASK) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Paint* target2 = tvg_shape_new();
    REQUIRE(target2);
    REQUIRE(tvg_paint_set_composite_method(paint, target2, TVG_COMPOSITE_METHOD_ALPHA_MASK) == TVG_RESULT_SUCCESS);

    const Tvg_Paint* target3 = nullptr;
    Tvg_Composite_Method method = TVG_COMPOSITE_METHOD_NONE;
    REQUIRE(tvg_paint_get_composite_method(paint, NULL, &method) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target3, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target3, &method) == TVG_RESULT_SUCCESS);
    REQUIRE(method == TVG_COMPOSITE_METHOD_ALPHA_MASK);
    REQUIRE(target2 == target3);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint InvAlphaMask Composite Method", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Paint* target = tvg_shape_new();
    REQUIRE(target);

    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_set_composite_method(paint, target, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_INVERSE_ALPHA_MASK) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Paint* target2 = tvg_shape_new();
    REQUIRE(target2);
    REQUIRE(tvg_paint_set_composite_method(paint, target2, TVG_COMPOSITE_METHOD_INVERSE_ALPHA_MASK) == TVG_RESULT_SUCCESS);

    const Tvg_Paint* target3 = nullptr;
    Tvg_Composite_Method method = TVG_COMPOSITE_METHOD_NONE;
    REQUIRE(tvg_paint_get_composite_method(paint, NULL, &method) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target3, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target3, &method) == TVG_RESULT_SUCCESS);
    REQUIRE(method == TVG_COMPOSITE_METHOD_INVERSE_ALPHA_MASK);
    REQUIRE(target2 == target3);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paint LumaMask Composite Method", "[capiPaint]")
{
    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    Tvg_Paint* target = tvg_shape_new();
    REQUIRE(target);

    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_set_composite_method(paint, target, TVG_COMPOSITE_METHOD_NONE) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_LUMA_MASK) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_set_composite_method(paint, NULL, TVG_COMPOSITE_METHOD_INVERSE_LUMA_MASK) == TVG_RESULT_INVALID_ARGUMENT);

    Tvg_Paint* target2 = tvg_shape_new();
    REQUIRE(target2);
    REQUIRE(tvg_paint_set_composite_method(paint, target2, TVG_COMPOSITE_METHOD_LUMA_MASK) == TVG_RESULT_SUCCESS);

    Tvg_Paint* target3 = tvg_shape_new();
    REQUIRE(target3);
    REQUIRE(tvg_paint_set_composite_method(paint, target3, TVG_COMPOSITE_METHOD_INVERSE_LUMA_MASK) == TVG_RESULT_SUCCESS);

    const Tvg_Paint* target4 = nullptr;
    Tvg_Composite_Method method = TVG_COMPOSITE_METHOD_NONE;
    REQUIRE(tvg_paint_get_composite_method(paint, NULL, &method) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target4, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_paint_get_composite_method(paint, &target4, &method) == TVG_RESULT_SUCCESS);
    REQUIRE(method == TVG_COMPOSITE_METHOD_INVERSE_LUMA_MASK);
    REQUIRE(target3 == target4);

    REQUIRE(tvg_paint_del(paint) == TVG_RESULT_SUCCESS);
}
