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

#include <thorvg.h>
#include <fstream>
#include <cstring>
#include "config.h"
#include "catch.hpp"

using namespace tvg;
using namespace std;


TEST_CASE("Animation Basic", "[tvgAnimation]")
{
    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    //Negative cases
    REQUIRE(animation->frame(0.0f) == Result::InsufficientCondition);
    REQUIRE(animation->curFrame() == 0.0f);
    REQUIRE(animation->totalFrame() == 0.0f);
    REQUIRE(animation->duration() == 0.0f);
}

#ifdef THORVG_LOTTIE_LOADER_SUPPORT

TEST_CASE("Animation Lottie", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 1) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/invalid.json") == Result::InvalidArguments);
    REQUIRE(picture->load(TEST_DIR"/test.json") == Result::Success);

    REQUIRE(animation->totalFrame() == Approx(120).margin(0.001f));
    REQUIRE(animation->curFrame() == 0);
    REQUIRE(animation->duration() == Approx(4.004).margin(0.001f)); //120/29.97
    REQUIRE(animation->frame(20) == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie2", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test2.json") == Result::Success);

    REQUIRE(animation->frame(20) == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie3", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test3.json") == Result::Success);
    
    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie4", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test4.json") == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie5", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test5.json") == Result::Success);
    
    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie6", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test6.json") == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie7", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test7.json") == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie8", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test8.json") == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Lottie9", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    REQUIRE(picture->load(TEST_DIR"/test9.json") == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

TEST_CASE("Animation Segment", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier() == Picture::identifier());

    float begin, end;

    //Segment by range before loaded
    REQUIRE(animation->segment(0, 0.5) == Result::InsufficientCondition);

    //Get current segment before loaded
    REQUIRE(animation->segment(&begin, &end) == Result::InsufficientCondition);

    //Animation load
    REQUIRE(picture->load(TEST_DIR"/lottiemarker.json") == Result::Success);

    //Get current segment before segment
    REQUIRE(animation->segment(&begin, &end) == Result::Success);
    REQUIRE(begin == 0.0f);
    REQUIRE(end == 1.0f);

    //Segment by range
    REQUIRE(animation->segment(0.25, 0.5) == Result::Success);

    //Get current segment
    REQUIRE(animation->segment(&begin, &end) == Result::Success);
    REQUIRE(begin == 0.25);
    REQUIRE(end == 0.5);

    //Get only segment begin
    REQUIRE(animation->segment(&begin) == Result::Success);
    REQUIRE(begin == 0.25);

    //Get only segment end
    REQUIRE(animation->segment(nullptr, &end) == Result::Success);
    REQUIRE(end == 0.5);

    //Segment by invalid range
    REQUIRE(animation->segment(-0.5, 1.5) == Result::InvalidArguments);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

#endif