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

TEST_CASE("Canvas initialization", "[capiCanvas]")
{
    static uint32_t width = 200;
    static uint32_t height = 200;

    uint32_t * buffer = NULL;
    Tvg_Canvas* canvas = NULL;

    buffer = (uint32_t*) malloc(sizeof(uint32_t) * width * height);
    REQUIRE(buffer != NULL);

    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    canvas = tvg_swcanvas_create();
    REQUIRE(canvas != NULL);

    REQUIRE(tvg_swcanvas_set_target(canvas, buffer, width, width, height, TVG_COLORSPACE_ARGB8888) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_draw(canvas) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_sync(canvas) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_clear(canvas, true) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_destroy(canvas) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}
