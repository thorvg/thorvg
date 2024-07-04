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

#include <thorvg.h>
#include "config.h"
#include "catch.hpp"

using namespace tvg;
using namespace std;


TEST_CASE("Filling Creation", "[tvgFill]")
{
    auto linear = LinearGradient::gen();
    REQUIRE(linear);

    REQUIRE(linear->type() == Type::LinearGradient);
    
    auto radial = RadialGradient::gen();
    REQUIRE(radial);

    REQUIRE(radial->type() == Type::RadialGradient);
}

TEST_CASE("Common Filling", "[tvgFill]")
{
    auto fill = LinearGradient::gen();
    REQUIRE(fill);

    //Options
    REQUIRE(fill->spread() == FillSpread::Pad);
    REQUIRE(fill->spread(FillSpread::Pad) == Result::Success);
    REQUIRE(fill->spread(FillSpread::Reflect) == Result::Success);
    REQUIRE(fill->spread(FillSpread::Repeat) == Result::Success);
    REQUIRE(fill->spread() == FillSpread::Repeat);

    //ColorStops
    const Fill::ColorStop* cs = nullptr;
    REQUIRE(fill->colorStops(nullptr) == 0);
    REQUIRE(fill->colorStops(&cs) == 0);
    REQUIRE(cs == nullptr);

    Fill::ColorStop cs2[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };

    REQUIRE(fill->colorStops(nullptr, 4) == Result::InvalidArguments);
    REQUIRE(fill->colorStops(cs2, 0) == Result::InvalidArguments);
    REQUIRE(fill->colorStops(cs2, 4) == Result::Success);
    REQUIRE(fill->colorStops(&cs) == 4);

    for (int i = 0; i < 4; ++i) {
        REQUIRE(cs[i].offset == cs2[i].offset);
        REQUIRE(cs[i].r == cs2[i].r);
        REQUIRE(cs[i].g == cs2[i].g);
        REQUIRE(cs[i].b == cs2[i].b);
    };

    //Reset ColorStop
    REQUIRE(fill->colorStops(nullptr, 0) == Result::Success);
    REQUIRE(fill->colorStops(&cs) == 0);

    //Set to Shape
    auto shape = Shape::gen();
    REQUIRE(shape);

    auto pFill = fill.get();
    REQUIRE(shape->fill(std::move(fill)) == Result::Success);
    REQUIRE(shape->fill() == pFill);
}

TEST_CASE("Fill Transformation", "[tvgFill]")
{
    auto fill = LinearGradient::gen();
    REQUIRE(fill);

    //no transformation
    auto mGet = fill->transform();
    REQUIRE(mGet.e11 == Approx(1.0f).margin(0.000001));
    REQUIRE(mGet.e12 == Approx(0.0f).margin(0.000001));
    REQUIRE(mGet.e13 == Approx(0.0f).margin(0.000001));
    REQUIRE(mGet.e21 == Approx(0.0f).margin(0.000001));
    REQUIRE(mGet.e22 == Approx(1.0f).margin(0.000001));
    REQUIRE(mGet.e23 == Approx(0.0f).margin(0.000001));
    REQUIRE(mGet.e31 == Approx(0.0f).margin(0.000001));
    REQUIRE(mGet.e32 == Approx(0.0f).margin(0.000001));
    REQUIRE(mGet.e33 == Approx(1.0f).margin(0.000001));

    auto mSet = Matrix{1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, -7.7f, -8.8f, -9.9f};
    REQUIRE(fill->transform(mSet) == Result::Success);

    //transformation was set
    mGet = fill->transform();
    REQUIRE(mGet.e11 == Approx(mSet.e11).margin(0.000001));
    REQUIRE(mGet.e12 == Approx(mSet.e12).margin(0.000001));
    REQUIRE(mGet.e13 == Approx(mSet.e13).margin(0.000001));
    REQUIRE(mGet.e21 == Approx(mSet.e21).margin(0.000001));
    REQUIRE(mGet.e22 == Approx(mSet.e22).margin(0.000001));
    REQUIRE(mGet.e23 == Approx(mSet.e23).margin(0.000001));
    REQUIRE(mGet.e31 == Approx(mSet.e31).margin(0.000001));
    REQUIRE(mGet.e32 == Approx(mSet.e32).margin(0.000001));
    REQUIRE(mGet.e33 == Approx(mSet.e33).margin(0.000001));
}

TEST_CASE("Linear Filling", "[tvgFill]")
{
    auto fill = LinearGradient::gen();
    REQUIRE(fill);

    float x1, y1, x2, y2;

    REQUIRE(fill->linear(nullptr, nullptr, nullptr, nullptr) == Result::Success);
    REQUIRE(fill->linear(0, 0, 0, 0) == Result::Success);

    REQUIRE(fill->linear(&x1, nullptr, &x2, nullptr) == Result::Success);
    REQUIRE(x1 == 0.0f);
    REQUIRE(x2 == 0.0f);

    REQUIRE(fill->linear(-1.0f, -1.0f, 100.0f, 100.0f) == Result::Success);
    REQUIRE(fill->linear(&x1, &y1, &x2, &y2) == Result::Success);
    REQUIRE(x1 == -1.0f);
    REQUIRE(y1 == -1.0f);
    REQUIRE(x2 == 100.0f);
    REQUIRE(y2 == 100.0f);
}

TEST_CASE("Radial Filling", "[tvgFill]")
{
    auto fill = RadialGradient::gen();
    REQUIRE(fill);

    float cx, cy, radius;

    REQUIRE(fill->radial(0, 0, -1) == Result::InvalidArguments);
    REQUIRE(fill->radial(nullptr, nullptr, nullptr) == Result::Success);
    REQUIRE(fill->radial(100, 120, 50) == Result::Success);

    REQUIRE(fill->radial(&cx, nullptr, &radius) == Result::Success);
    REQUIRE(cx == 100.0f);
    REQUIRE(radius == 50.0f);

    REQUIRE(fill->radial(nullptr, &cy, nullptr) == Result::Success);
    REQUIRE(cy == 120);

    REQUIRE(fill->radial(0, 0, 0) == Result::Success);
    REQUIRE(fill->radial(&cx, &cy, &radius) == Result::Success);
    REQUIRE(cx == 0.0f);
    REQUIRE(cy == 0.0f);
    REQUIRE(radius == 0.0f);
}

TEST_CASE("Linear Filling Duplication", "[tvgFill]")
{
    auto fill = LinearGradient::gen();
    REQUIRE(fill);

    //Setup
    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };

    REQUIRE(fill->colorStops(cs, 4) == Result::Success);
    REQUIRE(fill->spread(FillSpread::Reflect) == Result::Success);
    REQUIRE(fill->linear(-10.0f, 10.0f, 100.0f, 120.0f) == Result::Success);

    auto m = Matrix{1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, -7.7f, -8.8f, -9.9f};
    REQUIRE(fill->transform(m) == Result::Success);

    //Duplication
    auto dup = tvg::cast<LinearGradient>(fill->duplicate());
    REQUIRE(dup);

    REQUIRE(dup->spread() == FillSpread::Reflect);

    float x1, y1, x2, y2;
    REQUIRE(fill->linear(&x1, &y1, &x2, &y2) == Result::Success);
    REQUIRE(x1 == -10.0f);
    REQUIRE(y1 == 10.0f);
    REQUIRE(x2 == 100.0f);
    REQUIRE(y2 == 120.0f);

    const Fill::ColorStop* cs2 = nullptr;
    REQUIRE(fill->colorStops(&cs2) == 4);

    for (int i = 0; i < 4; ++i) {
        REQUIRE(cs[i].offset == cs2[i].offset);
        REQUIRE(cs[i].r == cs2[i].r);
        REQUIRE(cs[i].g == cs2[i].g);
        REQUIRE(cs[i].b == cs2[i].b);
    }

    auto mDup = dup->transform();
    REQUIRE(mDup.e11 == Approx(m.e11).margin(0.000001));
    REQUIRE(mDup.e12 == Approx(m.e12).margin(0.000001));
    REQUIRE(mDup.e13 == Approx(m.e13).margin(0.000001));
    REQUIRE(mDup.e21 == Approx(m.e21).margin(0.000001));
    REQUIRE(mDup.e22 == Approx(m.e22).margin(0.000001));
    REQUIRE(mDup.e23 == Approx(m.e23).margin(0.000001));
    REQUIRE(mDup.e31 == Approx(m.e31).margin(0.000001));
    REQUIRE(mDup.e32 == Approx(m.e32).margin(0.000001));
    REQUIRE(mDup.e33 == Approx(m.e33).margin(0.000001));
}

TEST_CASE("Radial Filling Duplication", "[tvgFill]")
{
    auto fill = RadialGradient::gen();
    REQUIRE(fill);

    //Setup
    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };

    REQUIRE(fill->colorStops(cs, 4) == Result::Success);
    REQUIRE(fill->spread(FillSpread::Reflect) == Result::Success);
    REQUIRE(fill->radial(100.0f, 120.0f, 50.0f) == Result::Success);

    auto m = Matrix{1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, -7.7f, -8.8f, -9.9f};
    REQUIRE(fill->transform(m) == Result::Success);

    //Duplication
    auto dup = tvg::cast<RadialGradient>(fill->duplicate());
    REQUIRE(dup);

    REQUIRE(dup->spread() == FillSpread::Reflect);

    float cx, cy, radius;
    REQUIRE(dup->radial(&cx, &cy, &radius) == Result::Success);
    REQUIRE(cx == 100.0f);
    REQUIRE(cy == 120.0f);
    REQUIRE(radius == 50.0f);

    const Fill::ColorStop* cs2 = nullptr;
    REQUIRE(fill->colorStops(&cs2) == 4);

    for (int i = 0; i < 4; ++i) {
        REQUIRE(cs[i].offset == cs2[i].offset);
        REQUIRE(cs[i].r == cs2[i].r);
        REQUIRE(cs[i].g == cs2[i].g);
        REQUIRE(cs[i].b == cs2[i].b);
    }

    auto mDup = dup->transform();
    REQUIRE(mDup.e11 == Approx(m.e11).margin(0.000001));
    REQUIRE(mDup.e12 == Approx(m.e12).margin(0.000001));
    REQUIRE(mDup.e13 == Approx(m.e13).margin(0.000001));
    REQUIRE(mDup.e21 == Approx(m.e21).margin(0.000001));
    REQUIRE(mDup.e22 == Approx(m.e22).margin(0.000001));
    REQUIRE(mDup.e23 == Approx(m.e23).margin(0.000001));
    REQUIRE(mDup.e31 == Approx(m.e31).margin(0.000001));
    REQUIRE(mDup.e32 == Approx(m.e32).margin(0.000001));
    REQUIRE(mDup.e33 == Approx(m.e33).margin(0.000001));
}
