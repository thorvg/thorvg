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

#include "config.h"
#include <thorvg.h>
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
#include <thorvg_lottie.h>
#endif
#include <fstream>
#include <cstring>
#include "catch.hpp"

using namespace tvg;
using namespace std;

#ifdef THORVG_LOTTIE_LOADER_SUPPORT

TEST_CASE("Lottie Slot", "[tvgLottie]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = LottieAnimation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    const char* slotJson = R"({"gradient_fill":{"p":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}})";

    //Slot override before loaded
    REQUIRE(animation->override(slotJson) == Result::InsufficientCondition);

    //Animation load
    REQUIRE(picture->load(TEST_DIR"/lottieslot.json") == Result::Success);

    //Slot revert before overriding
    REQUIRE(animation->override(nullptr) == Result::Success);

    //Slot override
    REQUIRE(animation->override(slotJson) == Result::Success);

    //Slot revert
    REQUIRE(animation->override(nullptr) == Result::Success);

    //Slot override after reverting
    REQUIRE(animation->override(slotJson) == Result::Success);

    //Slot override with invalid JSON
    REQUIRE(animation->override("") == Result::InvalidArguments);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Marker", "[tvgLottie]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = LottieAnimation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    //Set marker name before loaded
    REQUIRE(animation->segment("sectionC") == Result::InsufficientCondition);

    //Animation load
    REQUIRE(picture->load(TEST_DIR"/lottiemarker.json") == Result::Success);

    //Set marker
    REQUIRE(animation->segment("sectionA") == Result::Success);

    //Set marker by invalid name
    REQUIRE(animation->segment("") == Result::InvalidArguments);

    //Get marker count
    REQUIRE(animation->markersCnt() == 3);

    //Get marker name by index
    REQUIRE(!strcmp(animation->marker(1), "sectionB"));

    //Get marker name by invalid index
    REQUIRE(animation->marker(-1) == nullptr);

    REQUIRE(Initializer::term() == Result::Success);
}

#endif