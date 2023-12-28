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

#ifdef THORVG_SW_RASTER_SUPPORT

TEST_CASE("Canvas missing initialization", "[capiSwCanvas]")
{
    Tvg_Canvas* canvas = tvg_swcanvas_create();
    REQUIRE(!canvas);
}

TEST_CASE("Basic canvas", "[capiSwCanvas]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Canvas* canvas = tvg_swcanvas_create();
    REQUIRE(canvas);

    Tvg_Canvas* canvas2 = tvg_swcanvas_create();
    REQUIRE(canvas2);

    REQUIRE(tvg_canvas_destroy(canvas) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_destroy(canvas2) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Canvas initialization", "[capiSwCanvas]")
{
    uint32_t* buffer = (uint32_t*) malloc(sizeof(uint32_t) * 200 * 200);
    REQUIRE(buffer);

    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Canvas* canvas = tvg_swcanvas_create();
    REQUIRE(canvas);

    REQUIRE(tvg_swcanvas_set_target(canvas, buffer, 200, 200, 200, TVG_COLORSPACE_ARGB8888) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_swcanvas_set_mempool(canvas, TVG_MEMPOOL_POLICY_DEFAULT) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_destroy(canvas) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);

    free(buffer);
}

TEST_CASE("Canvas draw", "[capiSwCanvas]")
{
    uint32_t* buffer = (uint32_t*) malloc(sizeof(uint32_t) * 200 * 200);
    REQUIRE(buffer);

    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Canvas* canvas = tvg_swcanvas_create();
    REQUIRE(canvas);

    REQUIRE(tvg_canvas_draw(canvas) == TVG_RESULT_INSUFFICIENT_CONDITION);
    REQUIRE(tvg_canvas_sync(canvas) == TVG_RESULT_INSUFFICIENT_CONDITION);

    REQUIRE(tvg_swcanvas_set_target(canvas, buffer, 200, 200, 200, TVG_COLORSPACE_ARGB8888) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_draw(canvas) == TVG_RESULT_INSUFFICIENT_CONDITION);
    REQUIRE(tvg_canvas_sync(canvas) == TVG_RESULT_INSUFFICIENT_CONDITION);

    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    REQUIRE(tvg_canvas_push(canvas, paint) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_draw(canvas) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_sync(canvas) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_clear(canvas, true) == TVG_RESULT_SUCCESS);

    Tvg_Paint* paint2 = tvg_shape_new();
    REQUIRE(paint);

    REQUIRE(tvg_shape_append_rect(paint2, 0, 0, 100, 100, 0, 0) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_shape_set_fill_color(paint2, 255, 255, 255, 255) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_push(canvas, paint2) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_draw(canvas) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_sync(canvas) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_destroy(canvas) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);

    free(buffer);
}

TEST_CASE("Canvas update, clear and reuse", "[capiSwCanvas]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Canvas* canvas = tvg_swcanvas_create();
    REQUIRE(canvas);

    Tvg_Paint* paint = tvg_shape_new();
    REQUIRE(paint);

    REQUIRE(tvg_canvas_push(canvas, paint) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_update_paint(canvas, paint) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_clear(canvas, false) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_destroy(canvas) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

#endif