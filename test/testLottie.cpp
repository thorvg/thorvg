/*
 * Copyright (c) 2024 - 2025 the ThorVG project. All rights reserved.

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

    auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->type() == Type::Picture);

    const char* slotJson = R"({"gradient_fill":{"p":{"p":2,"k":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}}})";

    //Slot generation before loaded
    REQUIRE(animation->genSlot(slotJson) == 0);

    //Animation load
    REQUIRE(picture->load(TEST_DIR"/lottieslot.json") == Result::Success);

    //Slot generation
    auto slotId = animation->genSlot(slotJson);
    REQUIRE(slotId > 0);

    //Slot revert before overriding
    REQUIRE(animation->applySlot(0) == Result::Success);

    //Slot override
    REQUIRE(animation->applySlot(slotId) == Result::Success);

    //Slot revert
    REQUIRE(animation->applySlot(0) == Result::Success);

    //Slot override after reverting
    REQUIRE(animation->applySlot(slotId) == Result::Success);

    //Slot generation with invalid JSON
    REQUIRE(animation->genSlot("") == 0);

    //Slot deletion
    REQUIRE(animation->delSlot(slotId) == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Slot 2", "[tvgLottie]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
    REQUIRE(animation);

    auto picture = animation->picture();

    const char* slotJson = R"({"lottie-icon-outline":{"p":{"a":0,"k":[1,1,0]}},"lottie-icon-solid":{"p":{"a":0,"k":[0,0,1]}}})";

    //Animation load
    REQUIRE(picture->load(TEST_DIR"/lottieslotkeyframe.json") == Result::Success);

    //Slot generation
    auto slotId = animation->genSlot(slotJson);
    REQUIRE(slotId > 0);

    //Slot override
    REQUIRE(animation->applySlot(slotId) == Result::Success);

    //Slot revert
    REQUIRE(animation->applySlot(0) == Result::Success);

    //Slot override after reverting
    REQUIRE(animation->applySlot(slotId) == Result::Success);

    //Slot deletion
    REQUIRE(animation->delSlot(slotId) == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Marker", "[tvgLottie]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
    REQUIRE(animation);

    auto picture = animation->picture();

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