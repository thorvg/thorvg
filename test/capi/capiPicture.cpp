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
#include <string.h>
#include "config.h"
#include "../catch.hpp"


TEST_CASE("Load Raw file in Picture", "[capiPicture]")
{
    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);

    Tvg_Type type = TVG_TYPE_UNDEF;
    REQUIRE(tvg_paint_get_type(picture, &type) == TVG_RESULT_SUCCESS);
    REQUIRE(type == TVG_TYPE_PICTURE);
    REQUIRE(type != TVG_TYPE_SHAPE);
    REQUIRE(type != TVG_TYPE_SCENE);

    //Load Raw Data
    FILE* fp = fopen(TEST_DIR"/rawimage_200x300.raw", "r");
    if (!fp) return;

    uint32_t* data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));

    if (data && fread(data, sizeof(uint32_t), 200*300, fp) > 0) {
        //Negative
        REQUIRE(tvg_picture_load_raw(picture, nullptr, 100, 100, true, true) == TVG_RESULT_INVALID_ARGUMENT);
        REQUIRE(tvg_picture_load_raw(nullptr, data, 200, 300, true, true) == TVG_RESULT_INVALID_ARGUMENT);
        REQUIRE(tvg_picture_load_raw(picture, data, 0, 0, true, true) == TVG_RESULT_INVALID_ARGUMENT);

        //Positive
        REQUIRE(tvg_picture_load_raw(picture, data, 200, 300, true, true) == TVG_RESULT_SUCCESS);
        REQUIRE(tvg_picture_load_raw(picture, data, 200, 300, true, false) == TVG_RESULT_SUCCESS);

        //Verify Size
        float w, h;
        REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
        REQUIRE(w == Approx(200).epsilon(0.0000001));
        REQUIRE(h == Approx(300).epsilon(0.0000001));
        float wNew = 500.0f, hNew = 500.0f;
        REQUIRE(tvg_picture_set_size(picture, wNew, hNew) == TVG_RESULT_SUCCESS);
        REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
        REQUIRE(w == Approx(wNew).epsilon(0.0000001));
        REQUIRE(h == Approx(hNew).epsilon(0.0000001));
    }

    REQUIRE(tvg_paint_del(picture) == TVG_RESULT_SUCCESS);

    fclose(fp);
    free(data);
}

#ifdef THORVG_SVG_LOADER_SUPPORT

TEST_CASE("Load Svg file in Picture", "[capiPicture]")
{
    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);

    //Negative
    REQUIRE(tvg_picture_load(nullptr, TEST_DIR"/logo.svg") == TVG_RESULT_INVALID_ARGUMENT);

    //Invalid file
    REQUIRE(tvg_picture_load(picture, "invalid.svg") == TVG_RESULT_INVALID_ARGUMENT);

    //Load Svg file
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/logo.svg") == TVG_RESULT_SUCCESS);

    float wNew = 500.0f, hNew = 500.0f;
    float w = 0.0f, h = 0.0f;
    REQUIRE(tvg_picture_set_size(picture, wNew, hNew) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
    REQUIRE(w == Approx(wNew).epsilon(0.0000001));
    REQUIRE(h == Approx(hNew).epsilon(0.0000001));

    REQUIRE(tvg_paint_del(picture) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Load Svg Data in Picture", "[capiPicture]")
{
    static const char* svg = "<svg height=\"1000\" viewBox=\"0 0 600 600\" width=\"1000\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M.10681413.09784845 1000.0527.01592069V1000.0851L.06005738 999.9983Z\" fill=\"#ffffff\" stroke-width=\"3.910218\"/><g fill=\"#252f35\"><g stroke-width=\"3.864492\"><path d=\"M256.61221 100.51736H752.8963V386.99554H256.61221Z\"/><path d=\"M201.875 100.51736H238.366478V386.99554H201.875Z\"/><path d=\"M771.14203 100.51736H807.633508V386.99554H771.14203Z\"/></g><path d=\"M420.82388 380H588.68467V422.805317H420.82388Z\" stroke-width=\"3.227\"/><path d=\"m420.82403 440.7101v63.94623l167.86079 25.5782V440.7101Z\"/><path d=\"M420.82403 523.07258V673.47362L588.68482 612.59701V548.13942Z\"/></g><g fill=\"#222f35\"><path d=\"M420.82403 691.37851 588.68482 630.5019 589 834H421Z\"/><path d=\"m420.82403 852.52249h167.86079v28.64782H420.82403v-28.64782 0 0\"/><path d=\"m439.06977 879.17031c0 0-14.90282 8.49429-18.24574 15.8161-4.3792 9.59153 0 31.63185 0 31.63185h167.86079c0 0 4.3792-22.04032 0-31.63185-3.34292-7.32181-18.24574-15.8161-18.24574-15.8161z\"/></g><g fill=\"#ffffff\"><path d=\"m280 140h15v55l8 10 8-10v-55h15v60l-23 25-23-25z\"/><path d=\"m335 140v80h45v-50h-25v10h10v30h-15v-57h18v-13z\"/></g></svg>";

    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);

    //Negative
    REQUIRE(tvg_picture_load_data(nullptr, svg, strlen(svg), nullptr, nullptr, true) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_picture_load_data(picture, nullptr, strlen(svg), nullptr, nullptr, true) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_picture_load_data(picture, svg, 0, nullptr, nullptr, true) == TVG_RESULT_INVALID_ARGUMENT);

    //Positive
    REQUIRE(tvg_picture_load_data(picture, svg, strlen(svg), "svg", nullptr, false) == TVG_RESULT_SUCCESS);

    //Verify Size
    float w, h;
    REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
    REQUIRE(w == Approx(1000).epsilon(0.0000001));
    REQUIRE(h == Approx(1000).epsilon(0.0000001));
    float wNew = 500.0f, hNew = 500.0f;
    REQUIRE(tvg_picture_set_size(picture, wNew, hNew) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
    REQUIRE(w == Approx(wNew).epsilon(0.0000001));
    REQUIRE(h == Approx(hNew).epsilon(0.0000001));

    REQUIRE(tvg_paint_del(picture) == TVG_RESULT_SUCCESS);
}

#endif

#ifdef THORVG_PNG_LOADER_SUPPORT

TEST_CASE("Load Png file in Picture", "[capiPicture]")
{
    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);

    //Invalid file
    REQUIRE(tvg_picture_load(picture, "invalid.png") == TVG_RESULT_INVALID_ARGUMENT);

    //Load Png file
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/test.png") == TVG_RESULT_SUCCESS);

    //Verify Size
    float wNew = 500.0f, hNew = 500.0f;
    float w = 0.0f, h = 0.0f;
    REQUIRE(tvg_picture_set_size(picture, wNew, hNew) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
    REQUIRE(w == Approx(wNew).epsilon(0.0000001));
    REQUIRE(h == Approx(hNew).epsilon(0.0000001));

    REQUIRE(tvg_paint_del(picture) == TVG_RESULT_SUCCESS);
}

#endif

#ifdef THORVG_JPG_LOADER_SUPPORT

TEST_CASE("Load Jpg file in Picture", "[capiPicture]")
{
    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);

    //Invalid file
    REQUIRE(tvg_picture_load(picture, "invalid.jpg") == TVG_RESULT_INVALID_ARGUMENT);

    //Load Png file
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/test.jpg") == TVG_RESULT_SUCCESS);

    //Verify Size
    float wNew = 500.0f, hNew = 500.0f;
    float w = 0.0f, h = 0.0f;
    REQUIRE(tvg_picture_set_size(picture, wNew, hNew) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
    REQUIRE(w == Approx(wNew).epsilon(0.0000001));
    REQUIRE(h == Approx(hNew).epsilon(0.0000001));

    REQUIRE(tvg_paint_del(picture) == TVG_RESULT_SUCCESS);
}

#endif

#ifdef THORVG_WEBP_LOADER_SUPPORT

TEST_CASE("Load Webp file in Picture", "[capiPicture]")
{
    Tvg_Paint* picture = tvg_picture_new();
    REQUIRE(picture);

    //Invalid file
    REQUIRE(tvg_picture_load(picture, "invalid.webp") == TVG_RESULT_INVALID_ARGUMENT);

    //Load Png file
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/test.webp") == TVG_RESULT_SUCCESS);

    //Verify Size
    float wNew = 500.0f, hNew = 500.0f;
    float w = 0.0f, h = 0.0f;
    REQUIRE(tvg_picture_set_size(picture, wNew, hNew) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_picture_get_size(picture, &w, &h) == TVG_RESULT_SUCCESS);
    REQUIRE(w == Approx(wNew).epsilon(0.0000001));
    REQUIRE(h == Approx(hNew).epsilon(0.0000001));

    REQUIRE(tvg_paint_del(picture) == TVG_RESULT_SUCCESS);
}

#endif
