/*
 * Copyright (c) 2024 - 2026 ThorVG project. All rights reserved.

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

TEST_CASE("Lottie Coverages", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        #define TEST_CNT 10

        const char* names[TEST_CNT] = {
            "test3.lot",
            "test4.lot",
            "test5.lot",
            "test6.lot",
            "test7.lot",
            "test8.lot",
            "test9.lot",
            "test10.lot",
            "test11.lot",
            "test12.lot"
        };

        auto animation = unique_ptr<Animation>(Animation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        for (int i = 0; i < TEST_CNT; ++i) {
            char buf[100];
            snprintf(buf, sizeof(buf), TEST_DIR"/%s", names[i]);
            REQUIRE(picture->load(buf) == Result::Success);
            REQUIRE(animation->frame(0.0f) == Result::InsufficientCondition);
            REQUIRE(animation->frame(animation->totalFrame() * 0.5f) == Result::Success);
            REQUIRE(animation->frame(animation->totalFrame()) == Result::Success);
        }
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Slot", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        //Slot Test 1
        const char* slotJson = R"({"gradient_fill":{"p":{"p":2,"k":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}}})";

        //Negative: slot generation before loaded
        REQUIRE(animation->gen(slotJson) == 0);

        REQUIRE(picture->load(TEST_DIR"/slot.lot") == Result::Success);

        auto id = animation->gen(slotJson);
        REQUIRE(id > 0);

        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->apply(id) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->apply(id) == Result::Success);
        REQUIRE(animation->gen("") == 0);
        REQUIRE(animation->del(id) == Result::Success);

        //Slot Test 2
        const char* slotJson2 = R"({"lottie-icon-outline":{"p":{"a":0,"k":[1,1,0]}},"lottie-icon-solid":{"p":{"a":0,"k":[0,0,1]}}})";

        auto id2 = animation->gen(slotJson2);
        REQUIRE(id2 > 0);

        REQUIRE(animation->apply(id2) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->apply(id2) == Result::Success);
        REQUIRE(animation->del(id2) == Result::Success);

        //Slot Test 3 (Transform)
        const char* positionSlot = R"({"transform_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[100,100],"t":0},{"s":[200,300],"t":100}]}}})";
        auto id3 = animation->gen(positionSlot);
        REQUIRE(id3 > 0);
        REQUIRE(animation->apply(id3) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(id3) == Result::Success);

        const char* scaleSlot = R"({"transform_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0,0],"t":0},{"s":[100,100],"t":100}]}}})";
        auto id4 = animation->gen(scaleSlot);
        REQUIRE(id4 > 0);
        REQUIRE(animation->apply(id4) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(id4) == Result::Success);

        const char* rotationSlot = R"({"transform_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[180],"t":100}]}}})";
        auto id5 = animation->gen(rotationSlot);
        REQUIRE(id5 > 0);
        REQUIRE(animation->apply(id5) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(id5) == Result::Success);

        const char* opacitySlot = R"({"transform_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[100],"t":100}]}}})";
        auto id6 = animation->gen(opacitySlot);
        REQUIRE(id6 > 0);
        REQUIRE(animation->apply(id6) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(id6) == Result::Success);

        //Slot Test 4: Expression
        const char* expressionSlot = R"({"rect_rotation":{"p":{"x":"var $bm_rt = time * 360;"}},"rect_scale":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}},"rect_position":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}}})";
        auto id7 = animation->gen(expressionSlot);
        REQUIRE(id7 > 0);
        REQUIRE(animation->apply(id7) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(id7) == Result::Success);

        //Slot Test 5: Text
        const char* textSlot = R"({"text_doc":{"p":{"k":[{"s":{"f":"Ubuntu Light Italic","t":"ThorVG!","j":0,"s":48,"fc":[1,1,1]},"t":0}]}}})";
        auto id8 = animation->gen(textSlot);
        REQUIRE(id8 > 0);
        REQUIRE(animation->apply(id8) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(id8) == Result::Success);

        //Slot Test 6: Image
        const char* imageSlot = R"({"path_img":{"p":{"id":"image_0","w":200,"h":300,"u":"images/","p":"logo.png","e":0}}})";
        auto id9 = animation->gen(imageSlot);
        REQUIRE(id9 > 0);
        REQUIRE(animation->apply(id9) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(id9) == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Marker", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        //Set marker name before loaded
        REQUIRE(animation->segment("sectionC") == Result::InsufficientCondition);

        //Get marker info before loaded
        float b, e;
        REQUIRE(animation->marker(0, &b, &e) == nullptr);

        //Animation load
        REQUIRE(picture->load(TEST_DIR"/segment.lot") == Result::Success);

        //Set marker
        REQUIRE(animation->segment("sectionA") == Result::Success);

        //Set marker by invalid name
        REQUIRE(animation->segment("") == Result::InvalidArguments);

        //Get marker count
        REQUIRE(animation->markersCnt() == 3);

        //Get marker name by index
        REQUIRE(!strcmp(animation->marker(1), "sectionB"));

        //Get marker name and segment by index
        float begin, end;
        REQUIRE(!strcmp(animation->marker(0, &begin, &end), "sectionA"));
        REQUIRE(begin == 0.0f);
        REQUIRE(end == 22.0f);

        REQUIRE(!strcmp(animation->marker(1, &begin, &end), "sectionB"));
        REQUIRE(begin == 22.0f);
        REQUIRE(end == 33.0f);

        REQUIRE(!strcmp(animation->marker(2, &begin, &end), "sectionC"));
        REQUIRE(begin == 33.0f);
        REQUIRE(end == 63.0f);

        //Get marker with only begin
        REQUIRE(!strcmp(animation->marker(0, &begin, nullptr), "sectionA"));
        REQUIRE(begin == 0.0f);

        //Get marker with only end
        REQUIRE(!strcmp(animation->marker(0, nullptr, &end), "sectionA"));
        REQUIRE(end == 22.0f);

        //Get marker by invalid index
        REQUIRE(animation->marker(-1) == nullptr);
        REQUIRE(animation->marker(-1, &begin, &end) == nullptr);

        REQUIRE(animation->segment(nullptr) == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Tween", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        REQUIRE(animation->tween(0.0f, 10.0f, 0.5f) == Result::InsufficientCondition);

        REQUIRE(picture->load(TEST_DIR"/test.lot") == Result::Success);

        //Set initial frame to avoid frame difference being too small
        REQUIRE(animation->frame(5.0f) == Result::Success);

        //Tween between frames with different progress values
        REQUIRE(animation->tween(0.0f, 10.0f, 0.5f) == Result::Success);
        REQUIRE(animation->tween(10.0f, 20.0f, 0.0f) == Result::Success);
        REQUIRE(animation->tween(20.0f, 30.0f, 1.0f) == Result::Success);

        //Tween with different frame ranges
        REQUIRE(animation->tween(10.0f, 50.0f, 0.25f) == Result::Success);
        REQUIRE(animation->tween(50.0f, 100.0f, 0.75f) == Result::Success);

        //Tween between distant frames
        REQUIRE(animation->tween(0.0f, 100.0f, 0.5f) == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Quality", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        REQUIRE(animation->quality(50) == Result::InsufficientCondition);

        REQUIRE(picture->load(TEST_DIR"/test.lot") == Result::Success);

        //Set quality with minimum value
        REQUIRE(animation->quality(0) == Result::Success);

        //Set quality with default value
        REQUIRE(animation->quality(50) == Result::Success);

        //Set quality with maximum value
        REQUIRE(animation->quality(100) == Result::Success);

        //Set quality with various values
        REQUIRE(animation->quality(25) == Result::Success);
        REQUIRE(animation->quality(75) == Result::Success);

        //Set quality with invalid value (> 100)
        REQUIRE(animation->quality(101) == Result::InvalidArguments);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Asset Resolver", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        auto resolver = [](Paint* p, const char* src, void* data) -> bool {
            if (p->type() == Type::Picture) {
                string resolvedPath = string(TEST_DIR) + "/image/test.png";
                auto ret = static_cast<Picture*>(p)->load(resolvedPath.c_str());
                return (ret == Result::Success);
            } else if (p->type() == Type::Text) {
                string fontPath = string(TEST_DIR) + "/font/Arial.ttf";
                if (Text::load(fontPath.c_str()) != Result::Success) return false;
                auto ret = static_cast<Text*>(p)->font("Arial");
                return (ret == Result::Success);
            }
            return false;
        };

        // Test unset resolver
        REQUIRE(picture->resolver(resolver, nullptr) == Result::Success);
        REQUIRE(picture->resolver(nullptr, nullptr) == Result::Success);

        //Resolver Test (Image and Font)
        REQUIRE(picture->resolver(resolver, nullptr) == Result::Success);
        REQUIRE(picture->load(TEST_DIR"/resolver.json") == Result::Success);
        REQUIRE(animation->frame(animation->totalFrame() * 0.5f) == Result::Success);

        //Test that setting/unsetting resolver after load
        REQUIRE(picture->resolver(resolver, nullptr) == Result::InsufficientCondition);
        REQUIRE(picture->resolver(nullptr, nullptr) == Result::InsufficientCondition);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif