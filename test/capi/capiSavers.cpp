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


TEST_CASE("Create and delete a Saver", "[capiSaver]")
{
    Tvg_Saver* saver = tvg_saver_new();
    REQUIRE(saver);

    REQUIRE(tvg_saver_del(nullptr) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_saver_del(saver) == TVG_RESULT_SUCCESS);
}

#ifdef THORVG_TVG_SAVER_SUPPORT

TEST_CASE("Save a paint into tvg", "[capiSaver]")
{
    Tvg_Saver* saver = tvg_saver_new();
    REQUIRE(saver);

    Tvg_Paint* paint_empty = tvg_shape_new();
    REQUIRE(paint_empty);

    Tvg_Paint* paint1 = tvg_shape_new();
    REQUIRE(paint1);
    REQUIRE(tvg_shape_append_rect(paint1, 11.1f, 22.2f, 33.3f, 44.4f, 5.5f, 6.6f) == TVG_RESULT_SUCCESS);

    Tvg_Paint* paint2 = tvg_paint_duplicate(paint1);
    REQUIRE(paint2);

    Tvg_Paint* paint3 = tvg_paint_duplicate(paint1);
    REQUIRE(paint3);

    //An invalid argument
    REQUIRE(tvg_saver_save(nullptr, paint_empty, TEST_DIR"/test.tvg", 50) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_saver_save(saver, nullptr, TEST_DIR"/test.tvg", 999999) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_saver_save(saver, paint_empty, nullptr, 100) == TVG_RESULT_INVALID_ARGUMENT);

    //Save an empty paint
    REQUIRE(tvg_saver_save(saver, paint_empty, TEST_DIR"/test.tvg", 0) == TVG_RESULT_UNKNOWN);

    //Unsupported target file format
    REQUIRE(tvg_saver_save(saver, paint1, TEST_DIR"/test.err", 0) == TVG_RESULT_NOT_SUPPORTED);

    //Correct call
    REQUIRE(tvg_saver_save(saver, paint2, TEST_DIR"/test.tvg", 100) == TVG_RESULT_SUCCESS);

    //Busy - saving some resources
    REQUIRE(tvg_saver_save(saver, paint3, TEST_DIR"/test.tvg", 100) == TVG_RESULT_INSUFFICIENT_CONDITION);

    REQUIRE(tvg_saver_del(saver) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Synchronize a Saver", "[capiSaver]")
{
    Tvg_Saver* saver = tvg_saver_new();
    REQUIRE(saver);

    Tvg_Paint* paint1 = tvg_shape_new();
    REQUIRE(paint1);
    REQUIRE(tvg_shape_append_rect(paint1, 11.1f, 22.2f, 33.3f, 44.4f, 5.5f, 6.6f) == TVG_RESULT_SUCCESS);

    Tvg_Paint* paint2 = tvg_paint_duplicate(paint1);
    REQUIRE(paint2);

    //An invalid argument
    REQUIRE(tvg_saver_sync(nullptr) == TVG_RESULT_INVALID_ARGUMENT);

    //Nothing to be synced
    REQUIRE(tvg_saver_sync(saver) == TVG_RESULT_INSUFFICIENT_CONDITION);

    REQUIRE(tvg_saver_save(saver, paint1, TEST_DIR"/test.tvg", 100) == TVG_RESULT_SUCCESS);

    //Releasing the saving task
    REQUIRE(tvg_saver_sync(saver) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_saver_save(saver, paint2, TEST_DIR"/test.tvg", 100) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_saver_del(saver) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Save scene into tvg", "[capiSaver]")
{
    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);

    Tvg_Saver* saver = tvg_saver_new();
    REQUIRE(saver);

    FILE* file = fopen(TEST_DIR"/rawimage_200x300.raw", "r");
    REQUIRE(file);
    uint32_t* data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));

    if (data && fread(data, sizeof(uint32_t), 200 * 300, file) > 0) {
        REQUIRE(tvg_picture_load_raw(picture, data, 200, 300, true, true) == TVG_RESULT_SUCCESS);
        REQUIRE(tvg_saver_save(saver, picture, TEST_DIR"/test.tvg", 88) == TVG_RESULT_SUCCESS);
        REQUIRE(tvg_saver_sync(saver) == TVG_RESULT_SUCCESS);
    }

    REQUIRE(tvg_saver_del(saver) == TVG_RESULT_SUCCESS);

    fclose(file);
    free(data);
}

#ifdef THORVG_SVG_LOADER_SUPPORT

TEST_CASE("Save svg into tvg", "[capiSaver]")
{
    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/logo.svg") == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_picture_set_size(picture, 222, 333) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_paint_translate(picture, 123.45f, 54.321f) == TVG_RESULT_SUCCESS);

    Tvg_Saver* saver = tvg_saver_new();
    REQUIRE(saver);

    REQUIRE(tvg_saver_save(saver, picture, TEST_DIR"/test.tvg", 100) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_saver_sync(saver) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_saver_del(saver) == TVG_RESULT_SUCCESS);
}

#endif

#endif


#if defined(THORVG_GIF_SAVER_SUPPORT) && defined(THORVG_LOTTIE_LOADER_SUPPORT)

TEST_CASE("Save a lottie into gif", "[capiSavers]")
{
    //TODO: GIF Save Test
}

#endif