/*
 * Copyright (c) 2021 - 2025 the ThorVG project. All rights reserved.

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

TEST_CASE("Shape Creation", "[tvgShape]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    REQUIRE(shape->type() == Type::Shape);
}

TEST_CASE("Appending Commands", "[tvgShape]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    REQUIRE(shape->close() == Result::Success);

    REQUIRE(shape->moveTo(100, 100) == Result::Success);
    REQUIRE(shape->moveTo(99999999.0f, -99999999.0f) == Result::Success);
    REQUIRE(shape->moveTo(0, 0) == Result::Success);

    REQUIRE(shape->lineTo(120, 140) == Result::Success);
    REQUIRE(shape->lineTo(99999999.0f, -99999999.0f) == Result::Success);
    REQUIRE(shape->lineTo(0, 0) == Result::Success);

    REQUIRE(shape->cubicTo(0, 0, 0, 0, 0, 0) == Result::Success);
    REQUIRE(shape->cubicTo(0, 0, 99999999.0f, -99999999.0f, 0, 0) == Result::Success);
    REQUIRE(shape->cubicTo(0, 0, 99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f) == Result::Success);
    REQUIRE(shape->cubicTo(99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f) == Result::Success);

    REQUIRE(shape->close() == Result::Success);

    REQUIRE(shape->reset() == Result::Success);
    REQUIRE(shape->reset() == Result::Success);
}

TEST_CASE("Appending Shapes", "[tvgShape]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    REQUIRE(shape->moveTo(100, 100) == Result::Success);
    REQUIRE(shape->lineTo(120, 140) == Result::Success);

    REQUIRE(shape->appendRect(0, 0, 0, 0, 0, 0) == Result::Success);
    REQUIRE(shape->appendRect(0, 0,99999999.0f, -99999999.0f, 0, 0, true) == Result::Success);
    REQUIRE(shape->appendRect(0, 0, 0, 0, -99999999.0f, 99999999.0f, false) == Result::Success);
    REQUIRE(shape->appendRect(99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f, true) == Result::Success);
    REQUIRE(shape->appendRect(99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f, false) == Result::Success);

    REQUIRE(shape->appendCircle(0, 0, 0, 0) == Result::Success);
    REQUIRE(shape->appendCircle(-99999999.0f, 99999999.0f, 0, 0, true) == Result::Success);
    REQUIRE(shape->appendCircle(-99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, false) == Result::Success);
}

TEST_CASE("Appending Paths", "[tvgShape]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    //Negative cases
    REQUIRE(shape->appendPath(nullptr, 0, nullptr, 0) == Result::InvalidArguments);
    REQUIRE(shape->appendPath(nullptr, 100, nullptr, 0) == Result::InvalidArguments);
    REQUIRE(shape->appendPath(nullptr, 0, nullptr, 100) == Result::InvalidArguments);

    PathCommand cmds[5] = {
        PathCommand::Close,
        PathCommand::MoveTo,
        PathCommand::LineTo,
        PathCommand::CubicTo,
        PathCommand::Close
    };

    Point pts[5] = {
        {100, 100},
        {200, 200},
        {10, 10},
        {20, 20},
        {30, 30}
    };

    REQUIRE(shape->appendPath(cmds, 0, pts, 5) == Result::InvalidArguments);
    REQUIRE(shape->appendPath(cmds, 5, pts, 0) == Result::InvalidArguments);
    REQUIRE(shape->appendPath(cmds, 5, pts, 5) == Result::Success);

    const PathCommand* cmds2;
    const Point* pts2;
    uint32_t cmds2Cnt, pts2Cnt;
    REQUIRE(shape->path(&cmds2, &cmds2Cnt, &pts2, &pts2Cnt) == Result::Success);
    REQUIRE(cmds2Cnt == 5);
    REQUIRE(pts2Cnt == 5);

    for (int i = 0; i < 5; ++i) {
        REQUIRE(cmds2[i] == cmds[i]);
        REQUIRE(pts[i].x == pts2[i].x);
        REQUIRE(pts[i].y == pts2[i].y);
    }

    shape->reset();
    REQUIRE(shape->path(nullptr, &cmds2Cnt, nullptr, &pts2Cnt) == Result::Success);
    REQUIRE(cmds2Cnt == 0);
    REQUIRE(pts2Cnt == 0);
}

TEST_CASE("Stroking", "[tvgShape]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    //Stroke Order Before Stroke Setting
    REQUIRE(shape->order(true) == Result::Success);
    REQUIRE(shape->order(false) == Result::Success);

    //Stroke Width
    REQUIRE(shape->strokeWidth(0) == Result::Success);
    REQUIRE(shape->strokeWidth() == 0);
    REQUIRE(shape->strokeWidth(300) == Result::Success);
    REQUIRE(shape->strokeWidth() == 300);

    //Stroke Color
    uint8_t r, g, b, a;
    REQUIRE(shape->strokeFill(0, 50, 100, 200) == Result::Success);
    REQUIRE(shape->strokeFill(nullptr, nullptr, &b, nullptr) == Result::Success);
    REQUIRE(b == 100);
    REQUIRE(shape->strokeFill(&r, &g, &b, &a) == Result::Success);
    REQUIRE(r == 0);
    REQUIRE(g == 50);
    REQUIRE(b == 100);
    REQUIRE(a == 200);
    REQUIRE(shape->strokeFill(nullptr, nullptr, nullptr, nullptr) == Result::Success);

    //Stroke Dash
    REQUIRE(shape->strokeDash(nullptr, 3) == Result::InvalidArguments);

    float dashPattern0[3] = {-10.0f, 1.5f, 2.22f};
    REQUIRE(shape->strokeDash(dashPattern0, 0) == Result::InvalidArguments);
    REQUIRE(shape->strokeDash(dashPattern0, 3) == Result::Success);

    float dashPattern1[2] = {0.0f, 0.0f};
    REQUIRE(shape->strokeDash(dashPattern1, 2) == Result::Success);

    float dashPattern2[1] = {10.0f};
    REQUIRE(shape->strokeDash(dashPattern2, 1) == Result::Success);

    float dashPattern3[3] = {1.0f, 1.5f, 2.22f};
    REQUIRE(shape->strokeDash(dashPattern3, 3) == Result::Success);
    REQUIRE(shape->strokeDash(dashPattern3, 3, 4.5) == Result::Success);

    const float* dashPattern4;
    float offset;
    REQUIRE(shape->strokeDash(nullptr) == 3);
    REQUIRE(shape->strokeDash(&dashPattern4) == 3);
    REQUIRE(shape->strokeDash(&dashPattern4, &offset) == 3);
    REQUIRE(dashPattern4[0] == 1.0f);
    REQUIRE(dashPattern4[1] == 1.5f);
    REQUIRE(dashPattern4[2] == 2.22f);
    REQUIRE(offset == 4.5f);

    REQUIRE(shape->strokeDash(nullptr, 0) == Result::Success);

    //Stroke Cap
    REQUIRE(shape->strokeCap() == StrokeCap::Square);
    REQUIRE(shape->strokeCap(StrokeCap::Round) == Result::Success);
    REQUIRE(shape->strokeCap(StrokeCap::Butt) == Result::Success);
    REQUIRE(shape->strokeCap() == StrokeCap::Butt);

    //Stroke Join
    REQUIRE(shape->strokeJoin() == StrokeJoin::Bevel);
    REQUIRE(shape->strokeJoin(StrokeJoin::Miter) == Result::Success);
    REQUIRE(shape->strokeJoin(StrokeJoin::Round) == Result::Success);
    REQUIRE(shape->strokeJoin() == StrokeJoin::Round);

    //Stroke Miterlimit
    REQUIRE(shape->strokeMiterlimit() == 4.0f);
    REQUIRE(shape->strokeMiterlimit(0.00001f) == Result::Success);
    REQUIRE(shape->strokeMiterlimit() == 0.00001f);
    REQUIRE(shape->strokeMiterlimit(1000.0f) == Result::Success);
    REQUIRE(shape->strokeMiterlimit() == 1000.0f);
    REQUIRE(shape->strokeMiterlimit(-0.001f) == Result::InvalidArguments);

    REQUIRE(shape->trimpath(0.3f, 0.88f, false) == Result::Success);

    //Stroke Order After Stroke Setting
    REQUIRE(shape->order(true) == Result::Success);
    REQUIRE(shape->order(false) == Result::Success);
}

TEST_CASE("Shape Filling", "[tvgShape]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    //Fill Color
    uint8_t r, g, b, a;
    REQUIRE(shape->fill(255, 100, 50, 5) == Result::Success);
    REQUIRE(shape->fill(&r, nullptr, &b, nullptr) == Result::Success);
    REQUIRE(r == 255);
    REQUIRE(b == 50);
    REQUIRE(shape->fill(&r, &g, &b, &a) == Result::Success);
    REQUIRE(g == 100);
    REQUIRE(a == 5);

    //Fill Rule
    REQUIRE(shape->fillRule() == FillRule::NonZero);
    REQUIRE(shape->fill(FillRule::EvenOdd) == Result::Success);
    REQUIRE(shape->fillRule() == FillRule::EvenOdd);
}
