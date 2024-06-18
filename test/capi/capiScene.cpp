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

TEST_CASE("Create a Scene", "[capiScene]")
{
    Tvg_Paint* scene = tvg_scene_new();
    REQUIRE(scene);

    Tvg_Type type = TVG_TYPE_UNDEF;
    REQUIRE(tvg_paint_get_type(scene, &type) == TVG_RESULT_SUCCESS);
    REQUIRE(type == TVG_TYPE_SCENE);
    REQUIRE(type != TVG_TYPE_PICTURE);
    REQUIRE(type != TVG_TYPE_SHAPE);

    REQUIRE(tvg_paint_del(scene) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Paints Into a Scene", "[capiScene]")
{
    Tvg_Paint* scene = tvg_scene_new();
    REQUIRE(scene);

    //Pushing Paints
    REQUIRE(tvg_scene_push(scene, tvg_shape_new()) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_scene_push(scene, tvg_picture_new()) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_scene_push(scene, tvg_scene_new()) == TVG_RESULT_SUCCESS);

    //Pushing Null Pointer
    REQUIRE(tvg_scene_push(scene, NULL) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_scene_push(NULL, NULL) == TVG_RESULT_INVALID_ARGUMENT);

    REQUIRE(tvg_paint_del(scene) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Clear the Scene", "[capiScene]")
{
    Tvg_Paint* scene = tvg_scene_new();
    REQUIRE(scene);

    REQUIRE(tvg_scene_push(scene, tvg_shape_new()) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_scene_clear(scene, true) == TVG_RESULT_SUCCESS);

    //Invalid scene
    REQUIRE(tvg_scene_clear(NULL, false) == TVG_RESULT_INVALID_ARGUMENT);

    REQUIRE(tvg_paint_del(scene) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Scene reusing paints", "[capiScene]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Canvas* canvas = tvg_swcanvas_create();
    REQUIRE(canvas);

    uint32_t* buffer = (uint32_t*) malloc(sizeof(uint32_t) * 200 * 200);
    REQUIRE(buffer);

    REQUIRE(tvg_swcanvas_set_target(canvas, buffer, 200, 200, 200, TVG_COLORSPACE_ARGB8888) == TVG_RESULT_SUCCESS);

    Tvg_Paint* scene = tvg_scene_new();
    REQUIRE(scene);

    Tvg_Paint* shape = tvg_shape_new();
    REQUIRE(shape);

    REQUIRE(tvg_scene_push(scene, shape) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_push(canvas, scene) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_canvas_update(canvas) == TVG_RESULT_SUCCESS);

    //No deallocate shape.
    REQUIRE(tvg_scene_clear(scene, false) == TVG_RESULT_SUCCESS);

    //Reuse shape.
    REQUIRE(tvg_scene_push(scene, shape) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_canvas_destroy(canvas) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);

    free(buffer);
}
