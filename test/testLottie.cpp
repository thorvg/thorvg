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
#include "testFramework.h"

using namespace tvg;
using namespace std;

#ifdef THORVG_LOTTIE_LOADER_SUPPORT

TEST_CASE("Lottie Coverages", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        #define TEST_CNT 12

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
            "test12.lot",
            "test13.lot",
            "test14.lot"
        };

        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
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

TEST_CASE("Lottie Negative", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        REQUIRE(picture->load(TEST_DIR"/test1.svg") == Result::Success);
        REQUIRE(animation->frame(0.0f) == Result::NonSupport);
        REQUIRE(animation->frame(animation->totalFrame() * 0.5f) == Result::NonSupport);
        REQUIRE(animation->frame(animation->totalFrame()) == Result::NonSupport);
        REQUIRE(animation->tweenTo(50.0f) == Result::InsufficientCondition);
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

        //Negative: slot generation before loaded
        REQUIRE(animation->gen(R"({"stroke_width":{"p":{"a":0,"k":10}}})") == 0);

        //Animation load with default slot
        REQUIRE(picture->load(TEST_DIR"/slot.lot") == Result::Success);

        //Negative: invalid slots
        REQUIRE(animation->gen("") == 0);
        REQUIRE(animation->gen("{}") == 0);
        REQUIRE(animation->gen(R"({"no_such_sid":{"p":{"a":0,"k":10}}})") == 0);
        REQUIRE(animation->apply(1) == Result::InvalidArguments);
        REQUIRE(animation->del(0) == Result::InvalidArguments);

        //Slot Test 1: Property Types
        const char* slots = R"({
            "stroke_width":{"p":{"a":0,"k":10}},
            "gradient_stroke_width":{"p":{"a":0,"k":30}},
            "trim_start":{"p":{"a":0,"k":25}},
            "time_remap":{"p":{"a":0,"k":1.5}},
            "rectangle_size":{"p":{"a":0,"k":[200,120]}},
            "rectangle_position":{"p":{"a":0,"k":[400,400]}},
            "rect_opacity":{"p":{"a":0,"k":50}},
            "lottie-icon-outline":{"p":{"a":0,"k":[0,0,1]}},
            "gradient_fill":{"p":{"p":2,"k":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}},
            "bezier_path":{"p":{"a":0,"k":{"c":true,"i":[[0,0],[0,0],[0,0]],"o":[[0,0],[0,0],[0,0]],"v":[[-50,-50],[50,-50],[0,50]]}}},
            "text_doc":{"p":{"k":[{"s":{"f":"Ubuntu Light Italic","t":"ThorVG!","j":0,"s":48,"fc":[1,1,1]},"t":0}]}},
            "path_img":{"p":{"id":"image_0","w":200,"h":300,"u":"images/","p":"logo.png","e":0}}
        })";

        auto id = animation->gen(slots);
        REQUIRE(id > 0);
        REQUIRE(animation->apply(id) == Result::Success);
        REQUIRE(animation->apply(id) == Result::Success); //redundant
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->apply(id) == Result::Success); //reapply
        REQUIRE(animation->del(id) == Result::Success);

        //Slot Test 2: Keyframes
        const char* animated = R"({
            "transform_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[100,100],"t":0},{"s":[200,300],"t":100}]}},
            "rectangle_radius":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[60],"t":100}]}},
            "bezier_path":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"t":0,"s":[{"c":true,"i":[[0,0],[0,0],[0,0]],"o":[[0,0],[0,0],[0,0]],"v":[[-50,-50],[50,-50],[0,50]]}]},{"t":100,"s":[{"c":true,"i":[[0,0],[0,0],[0,0]],"o":[[0,0],[0,0],[0,0]],"v":[[-80,-80],[80,-80],[0,80]]}]}]}}
        })";

        auto animatedId = animation->gen(animated);
        REQUIRE(animatedId > 0);
        REQUIRE(animation->apply(animatedId) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(animatedId) == Result::Success);

        //Slot Test 3: Expression
        const char* expressions = R"({"rect_rotation":{"p":{"x":"var $bm_rt = time * 360;"}},"rect_scale":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}},"rect_position":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}}})";

        auto expressionId = animation->gen(expressions);
        REQUIRE(expressionId > 0);
        REQUIRE(animation->apply(expressionId) == Result::Success);
        REQUIRE(animation->apply(0) == Result::Success);
        REQUIRE(animation->del(expressionId) == Result::Success);
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

        // Get marker info before loaded
        float markerBegin, markerEnd;
        REQUIRE(animation->marker(0, &markerBegin, &markerEnd) == nullptr);

        //Animation load
        REQUIRE(picture->load(TEST_DIR"/segment.lot") == Result::Success);

        //Set marker
        REQUIRE(animation->segment("sectionA") == Result::Success);

        //Set marker by invalid name
        REQUIRE(animation->segment("") == Result::InvalidArguments);

        //Get marker count
        REQUIRE(animation->markersCnt() == 3);

        // Get marker name and segment by index
        REQUIRE(!strcmp(animation->marker(0, &markerBegin, &markerEnd), "sectionA"));
        REQUIRE(markerBegin == 0.0f);
        REQUIRE(markerEnd == 22.0f);

        REQUIRE(!strcmp(animation->marker(1, &markerBegin, &markerEnd), "sectionB"));
        REQUIRE(markerBegin == 22.0f);
        REQUIRE(markerEnd == 33.0f);

        REQUIRE(!strcmp(animation->marker(2, &markerBegin, &markerEnd), "sectionC"));
        REQUIRE(markerBegin == 33.0f);
        REQUIRE(markerEnd == 63.0f);

        // Get marker with only begin
        REQUIRE(!strcmp(animation->marker(0, &markerBegin, nullptr), "sectionA"));
        REQUIRE(markerBegin == 0.0f);

        // Get marker with only end
        REQUIRE(!strcmp(animation->marker(0, nullptr, &markerEnd), "sectionA"));
        REQUIRE(markerEnd == 22.0f);

        // Get marker by invalid index
        REQUIRE(animation->marker(-1, &markerBegin, &markerEnd) == nullptr);

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

        REQUIRE(animation->tweenTo(10.0f) == Result::InsufficientCondition);
        REQUIRE(animation->tween(0.5f) == Result::InsufficientCondition);

        REQUIRE(picture->load(TEST_DIR"/tween.lot") == Result::Success);

        REQUIRE(animation->tween(0.5f) == Result::InsufficientCondition);

        REQUIRE(animation->frame(5.0f) == Result::Success);
        REQUIRE(animation->tweenTo(20.0f) == Result::Success);
        REQUIRE(animation->tween(0.0f) == Result::Success);
        REQUIRE(animation->tween(0.5f) == Result::Success);
        REQUIRE(animation->tween(1.0f) == Result::Success);

        REQUIRE(animation->tweenTo(30.0f) == Result::Success);
        REQUIRE(animation->frame(10.0f) == Result::Success);
        REQUIRE(animation->tween(0.5f) == Result::InsufficientCondition);

        REQUIRE(animation->tweenTo(40.0f) == Result::Success);
        REQUIRE(animation->tween(0.0f, 10.0f, 0.5f) == Result::Success);
        REQUIRE(animation->tween(0.75f) == Result::InsufficientCondition);
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


TEST_CASE("Lottie Audio Layer", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto animation = unique_ptr<LottieAnimation>(LottieAnimation::gen());
        REQUIRE(animation);

        auto picture = animation->picture();

        int callCount = 0;
        LottieAudioResolver received{};

        auto resolver = [&](const LottieAudioResolver& info, void*) {
            ++callCount;
            received = info;
        };

        //Negative: register resolver before loaded
        REQUIRE(animation->resolver(resolver, nullptr) == Result::InsufficientCondition);

        REQUIRE(picture->load(TEST_DIR"/audio_layer.json") == Result::Success);

        //Frame updates without a registered resolver
        REQUIRE(animation->frame(15) == Result::Success);

        //Test Audio Resolver
        REQUIRE(animation->resolver(resolver, nullptr) == Result::Success);
        REQUIRE(animation->frame(1) == Result::Success);
        REQUIRE(callCount == 1);
        REQUIRE(received.active == true);
        REQUIRE(received.offset >= 0.0f);
        REQUIRE(received.volume == Approx(100.0f).margin(0.1f));
        REQUIRE(received.embedded == false);
        REQUIRE(received.src != nullptr);
        REQUIRE(received.size == 0);
        REQUIRE(received.mimeType == nullptr);

        //Resolver should not be invoked (no state change)
        callCount = 0;
        REQUIRE(animation->frame(5) == Result::Success);
        REQUIRE(callCount == 0);

        //Test Audio Resolver firing
        REQUIRE(animation->frame(20) == Result::Success);
        REQUIRE(callCount == 1);
        REQUIRE(received.active == false);
        REQUIRE(received.src != nullptr);   //source must remain identifiable on deactivation

        //Test Audio Resolver seeks again
        REQUIRE(animation->frame(6) == Result::Success);
        REQUIRE(received.active == true);
        REQUIRE(received.offset == Approx(6.0f / 60.0f).margin(0.01f));

        //Test Audio Resolver unregistration
        REQUIRE(animation->resolver(nullptr, nullptr) == Result::Success);
        callCount = 0;
        REQUIRE(animation->frame(25) == Result::Success);
        REQUIRE(callCount == 0);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Lottie Embedded Fonts Share", "[tvgLottie]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        ifstream file(TEST_DIR"/embedded_font.lot", ios::binary);
        REQUIRE(file.is_open());
        string json((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        file.close();

        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        uint32_t buffer[200*200] = {};
        canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888);

        auto first = unique_ptr<Animation>(Animation::gen());
        REQUIRE(first->picture()->load(json.c_str(), json.size(), "lottie", nullptr, true) == Result::Success);

        auto second = Animation::gen();
        REQUIRE(second->picture()->load(json.c_str(), json.size(), "lottie", nullptr, true) == Result::Success);

        //the first composition owns the font data the shared loader points at
        first.reset();

        REQUIRE(canvas->add(second->picture()) == Result::Success);
        REQUIRE(canvas->update() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        delete(second);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
