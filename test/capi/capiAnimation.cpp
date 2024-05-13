/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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


TEST_CASE("Animation Basic", "[capiAnimation]")
{
    Tvg_Animation* animation = tvg_animation_new();
    REQUIRE(animation);

    Tvg_Paint* picture = tvg_animation_get_picture(animation);
    REQUIRE(picture);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_paint_get_identifier(picture, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_PICTURE);

    //Negative cases
    REQUIRE(tvg_animation_set_frame(animation, 0) == TVG_RESULT_INSUFFICIENT_CONDITION);
    auto frame = 0.0f;
    REQUIRE(tvg_animation_set_frame(animation, frame) == TVG_RESULT_INSUFFICIENT_CONDITION);

    REQUIRE(tvg_animation_get_frame(animation, nullptr) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_animation_get_frame(animation, &frame) == TVG_RESULT_SUCCESS);
    REQUIRE(frame == 0.0f);

    REQUIRE(tvg_animation_get_total_frame(animation, nullptr) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_animation_get_total_frame(animation, &frame) == TVG_RESULT_SUCCESS);
    REQUIRE(frame == 0.0f);

    REQUIRE(tvg_animation_get_duration(animation, nullptr) == TVG_RESULT_INVALID_ARGUMENT);
    float duration = 0.0f;
    REQUIRE(tvg_animation_get_duration(animation, &duration) == TVG_RESULT_SUCCESS);
    REQUIRE(duration == 0.0f);

    REQUIRE(tvg_animation_del(nullptr) == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_animation_del(animation) == TVG_RESULT_SUCCESS);
}

#ifdef THORVG_LOTTIE_LOADER_SUPPORT

TEST_CASE("Animation Lottie", "[capiAnimation]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Animation* animation = tvg_animation_new();
    REQUIRE(animation);

    Tvg_Paint* picture = tvg_animation_get_picture(animation);
    REQUIRE(picture);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_paint_get_identifier(picture, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_PICTURE);

    REQUIRE(tvg_picture_load(picture, TEST_DIR"/invalid.json") == TVG_RESULT_INVALID_ARGUMENT);
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/test.json") == TVG_RESULT_SUCCESS);

    float frame;
    REQUIRE(tvg_animation_get_total_frame(animation, &frame) == TVG_RESULT_SUCCESS);
    REQUIRE(frame == Approx(120).margin(0.001f));

    REQUIRE(tvg_animation_set_frame(animation, frame - 1) == TVG_RESULT_SUCCESS);
    REQUIRE(tvg_animation_get_frame(animation, &frame) == TVG_RESULT_SUCCESS);
    REQUIRE(frame == Approx(119).margin(0.001f));

    float duration;
    REQUIRE(tvg_animation_get_duration(animation, &duration) == TVG_RESULT_SUCCESS);
    REQUIRE(duration == Approx(4.004).margin(0.001f)); //120/29.97

    REQUIRE(tvg_animation_del(animation) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}


TEST_CASE("Animation Segment", "[capiAnimation]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Animation* animation = tvg_animation_new();
    REQUIRE(animation);

    Tvg_Paint* picture = tvg_animation_get_picture(animation);
    REQUIRE(picture);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_paint_get_identifier(picture, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_PICTURE);

    float begin, end;

    //Segment by range before loaded
    REQUIRE(tvg_animation_set_segment(animation, 0, 0.5) == TVG_RESULT_INSUFFICIENT_CONDITION);

    //Get current segment before loaded
    REQUIRE(tvg_animation_get_segment(animation, &begin, &end) == TVG_RESULT_INSUFFICIENT_CONDITION);

    //Animation load
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/lottiemarker.json") == TVG_RESULT_SUCCESS);
    
    //Get current segment before segment
    REQUIRE(tvg_animation_get_segment(animation, &begin, &end) == TVG_RESULT_SUCCESS);
    REQUIRE(begin == 0.0f);
    REQUIRE(end == 1.0f);

    //Get only segment begin
    REQUIRE(tvg_animation_get_segment(animation, &begin, nullptr) == TVG_RESULT_SUCCESS);
    REQUIRE(begin == 0.0f);

    //Get only segment end
    REQUIRE(tvg_animation_get_segment(animation, nullptr, &end) == TVG_RESULT_SUCCESS);
    REQUIRE(end == 1.0f);

    //Segment by range
    REQUIRE(tvg_animation_set_segment(animation, 0.25, 0.5) == TVG_RESULT_SUCCESS);

    //Get current segment before segment
    REQUIRE(tvg_animation_get_segment(animation, &begin, &end) == TVG_RESULT_SUCCESS);
    REQUIRE(begin == 0.25);
    REQUIRE(end == 0.5);

    //Segment by invalid range
    REQUIRE(tvg_animation_set_segment(animation, -0.5, 1.5) == TVG_RESULT_INVALID_ARGUMENT);

    REQUIRE(tvg_animation_del(animation) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

#endif