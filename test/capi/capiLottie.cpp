/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#ifdef THORVG_LOTTIE_LOADER_SUPPORT

TEST_CASE("Lottie Slot", "[capiLottie]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Animation* animation = tvg_lottie_animation_new();
    REQUIRE(animation);

    Tvg_Paint* picture = tvg_animation_get_picture(animation);
    REQUIRE(picture);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_paint_get_identifier(picture, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_PICTURE);

    const char* slotJson = R"({"gradient_fill":{"p":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}})";

    //Slot override before loaded
    REQUIRE(tvg_lottie_animation_override(animation, slotJson) == TVG_RESULT_INSUFFICIENT_CONDITION);

    //Animation load
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/lottieslot.json") == TVG_RESULT_SUCCESS);

    //Slot revert before overriding
    REQUIRE(tvg_lottie_animation_override(animation, nullptr) == TVG_RESULT_SUCCESS);

    //Slot override
    REQUIRE(tvg_lottie_animation_override(animation, slotJson) == TVG_RESULT_SUCCESS);

    //Slot revert
    REQUIRE(tvg_lottie_animation_override(animation, nullptr) == TVG_RESULT_SUCCESS);

    //Slot override after reverting
    REQUIRE(tvg_lottie_animation_override(animation, slotJson) == TVG_RESULT_SUCCESS);

    //Slot override with invalid JSON
    REQUIRE(tvg_lottie_animation_override(animation, "") == TVG_RESULT_INVALID_ARGUMENT);

    REQUIRE(tvg_animation_del(animation) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Lottie Slot 2", "[capiLottie]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Animation* animation = tvg_lottie_animation_new();
    REQUIRE(animation);

    Tvg_Paint* picture = tvg_animation_get_picture(animation);
    REQUIRE(picture);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_paint_get_identifier(picture, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_PICTURE);

    const char* slotJson = R"({"lottie-icon-outline":{"p":{"a":0,"k":[1,1,0]}},"lottie-icon-solid":{"p":{"a":0,"k":[0,0,1]}}})";

    //Animation load
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/lottieslotkeyframe.json") == TVG_RESULT_SUCCESS);

    //Slot override
    REQUIRE(tvg_lottie_animation_override(animation, slotJson) == TVG_RESULT_SUCCESS);

    //Slot revert
    REQUIRE(tvg_lottie_animation_override(animation, nullptr) == TVG_RESULT_SUCCESS);

    //Slot override after reverting
    REQUIRE(tvg_lottie_animation_override(animation, slotJson) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_animation_del(animation) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

TEST_CASE("Lottie Marker", "[capiLottie]")
{
    REQUIRE(tvg_engine_init(TVG_ENGINE_SW, 0) == TVG_RESULT_SUCCESS);

    Tvg_Animation* animation = tvg_lottie_animation_new();
    REQUIRE(animation);

    Tvg_Paint* picture = tvg_animation_get_picture(animation);
    REQUIRE(picture);

    Tvg_Identifier id = TVG_IDENTIFIER_UNDEF;
    REQUIRE(tvg_paint_get_identifier(picture, &id) == TVG_RESULT_SUCCESS);
    REQUIRE(id == TVG_IDENTIFIER_PICTURE);

    //Set marker before loaded
    REQUIRE(tvg_lottie_animation_set_marker(animation, "sectionC") == TVG_RESULT_INSUFFICIENT_CONDITION);

    //Animation load
    REQUIRE(tvg_picture_load(picture, TEST_DIR"/lottiemarker.json") == TVG_RESULT_SUCCESS);
    
    //Set marker
    REQUIRE(tvg_lottie_animation_set_marker(animation, "sectionA") == TVG_RESULT_SUCCESS);

    //Set marker by invalid name
    REQUIRE(tvg_lottie_animation_set_marker(animation, "") == TVG_RESULT_INVALID_ARGUMENT);

    //Get marker count
    uint32_t cnt = 0;
    REQUIRE(tvg_lottie_animation_get_markers_cnt(animation, &cnt) == TVG_RESULT_SUCCESS);
    REQUIRE(cnt == 3);

    //Get marker name by index
    const char* name = nullptr;
    REQUIRE(tvg_lottie_animation_get_marker(animation, 1, &name) == TVG_RESULT_SUCCESS);
    REQUIRE(!strcmp(name, "sectionB"));

    //Get marker name by invalid index
    REQUIRE(tvg_lottie_animation_get_marker(animation, -1, &name) == TVG_RESULT_INVALID_ARGUMENT);

    REQUIRE(tvg_animation_del(animation) == TVG_RESULT_SUCCESS);

    REQUIRE(tvg_engine_term(TVG_ENGINE_SW) == TVG_RESULT_SUCCESS);
}

#endif