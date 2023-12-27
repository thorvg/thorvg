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
    REQUIRE(picture->identifier == Picture::identifier);

    //Negative cases
    REQUIRE(animation->frame(0.0f) == Result::InsufficientCondition);
    REQUIRE(animation->curFrame() == 0.0f);
    REQUIRE(animation->totalFrame() == 0.0f);
    REQUIRE(animation->duration() == 0.0f);
}

#ifdef THORVG_LOTTIE_LOADER_SUPPORT

TEST_CASE("Animation Lottie", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(1) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/invalid.json") == Result::InvalidArguments);
    REQUIRE(picture->load(TEST_DIR"/test.json") == Result::Success);

    REQUIRE(animation->totalFrame() == Approx(120).margin(004004));
    REQUIRE(animation->curFrame() == 0);
    REQUIRE(animation->duration() == Approx(4).margin(004004));
    REQUIRE(animation->frame(20) == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie2", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test2.json") == Result::Success);

    REQUIRE(animation->frame(20) == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie3", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test3.json") == Result::Success);
    
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie4", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test4.json") == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie5", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test5.json") == Result::Success);
    
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie6", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test6.json") == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie7", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test7.json") == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie8", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test8.json") == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Animation Lottie9", "[tvgAnimation]")
{
    REQUIRE(Initializer::init(0) == Result::Success);

    auto animation = Animation::gen();
    REQUIRE(animation);

    auto picture = animation->picture();
    REQUIRE(picture->identifier == Picture::identifier);

    REQUIRE(picture->load(TEST_DIR"/test9.json") == Result::Success);

    REQUIRE(Initializer::term() == Result::Success);
}

#endif