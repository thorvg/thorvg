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

TEST_CASE("Shape Creation", "[tvgShape]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    REQUIRE(shape->identifier() == Shape::identifier());
    REQUIRE(shape->identifier() != Picture::identifier());
    REQUIRE(shape->identifier() != Scene::identifier());
}

TEST_CASE("Appending Commands", "[tvgShape]")
{
    auto shape = Shape::gen();
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
    auto shape = Shape::gen();
    REQUIRE(shape);

    REQUIRE(shape->moveTo(100, 100) == Result::Success);
    REQUIRE(shape->lineTo(120, 140) == Result::Success);

    REQUIRE(shape->appendRect(0, 0, 0, 0, 0, 0) == Result::Success);
    REQUIRE(shape->appendRect(0, 0,99999999.0f, -99999999.0f, 0, 0) == Result::Success);
    REQUIRE(shape->appendRect(0, 0, 0, 0, -99999999.0f, 99999999.0f) == Result::Success);
    REQUIRE(shape->appendRect(99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, -99999999.0f) == Result::Success);

    REQUIRE(shape->appendCircle(0, 0, 0, 0) == Result::Success);
    REQUIRE(shape->appendCircle(-99999999.0f, 99999999.0f, 0, 0) == Result::Success);
    REQUIRE(shape->appendCircle(-99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f) == Result::Success);

    REQUIRE(shape->appendArc(0, 0, 0, 0, 0, false) == Result::Success);
    REQUIRE(shape->appendArc(0, 0, 0, 0, 0, true) == Result::Success);
    REQUIRE(shape->appendArc(-99999999.0f, 99999999.0f, 0, 0, 0, false) == Result::Success);
    REQUIRE(shape->appendArc(-99999999.0f, 99999999.0f, 0, 0, 0, true) == Result::Success);
    REQUIRE(shape->appendArc(-99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, 0, false) == Result::Success);
    REQUIRE(shape->appendArc(-99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, 0, true) == Result::Success);
    REQUIRE(shape->appendArc(-99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, -400, false) == Result::Success);
    REQUIRE(shape->appendArc(-99999999.0f, 99999999.0f, -99999999.0f, 99999999.0f, 400, true) == Result::Success);
}

TEST_CASE("Appending Pathes", "[tvgShape]")
{
    auto shape = Shape::gen();
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
    REQUIRE(shape->pathCommands(&cmds2) == 5);

    const Point* pts2;
    REQUIRE(shape->pathCoords(&pts2) == 5);

    for (int i = 0; i < 5; ++i) {
        REQUIRE(cmds2[i] == cmds[i]);
        REQUIRE(pts[i].x == pts2[i].x);
        REQUIRE(pts[i].y == pts2[i].y);
    }

    shape->reset();
    REQUIRE(shape->pathCommands(nullptr) == 0);
    REQUIRE(shape->pathCoords(nullptr) == 0);
}

TEST_CASE("Stroking", "[tvgShape]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    //Stroke Order Before Stroke Setting
    REQUIRE(shape->order(true) == Result::Success);
    REQUIRE(shape->order(false) == Result::Success);

    //Stroke Width
    REQUIRE(shape->stroke(0) == Result::Success);
    REQUIRE(shape->strokeWidth() == 0);
    REQUIRE(shape->stroke(300) == Result::Success);
    REQUIRE(shape->strokeWidth() == 300);

    //Stroke Color
    uint8_t r, g, b, a;
    REQUIRE(shape->stroke(0, 50, 100, 200) == Result::Success);
    REQUIRE(shape->strokeColor(nullptr, nullptr, &b, nullptr) == Result::Success);
    REQUIRE(b == 100);
    REQUIRE(shape->strokeColor(&r, &g, &b, &a) == Result::Success);
    REQUIRE(r == 0);
    REQUIRE(g == 50);
    REQUIRE(b == 100);
    REQUIRE(a == 200);
    REQUIRE(shape->strokeColor(nullptr, nullptr, nullptr, nullptr) == Result::Success);

    //Stroke Dash
    float dashPattern[3] = {0, 1.5f, 2.22f};
    REQUIRE(shape->stroke(dashPattern, 3) == Result::InvalidArguments);

    float dashPattern2[3] = {1.0f, 1.5f, 2.22f};
    REQUIRE(shape->stroke(dashPattern2, 3) == Result::Success);

    const float* dashPattern3;
    REQUIRE(shape->strokeDash(nullptr) == 3);
    REQUIRE(shape->strokeDash(&dashPattern3) == 3);
    REQUIRE(dashPattern3[0] == 1.0f);
    REQUIRE(dashPattern3[1] == 1.5f);
    REQUIRE(dashPattern3[2] == 2.22f);

    REQUIRE(shape->stroke(nullptr, 0) == Result::Success);

    //Stroke Cap
    REQUIRE(shape->strokeCap() == StrokeCap::Square);
    REQUIRE(shape->stroke(StrokeCap::Round) == Result::Success);
    REQUIRE(shape->stroke(StrokeCap::Butt) == Result::Success);
    REQUIRE(shape->strokeCap() == StrokeCap::Butt);

    //Stroke Join
    REQUIRE(shape->strokeJoin() == StrokeJoin::Bevel);
    REQUIRE(shape->stroke(StrokeJoin::Miter) == Result::Success);
    REQUIRE(shape->stroke(StrokeJoin::Round) == Result::Success);
    REQUIRE(shape->strokeJoin() == StrokeJoin::Round);

    //Stroke Miterlimit
    REQUIRE(shape->strokeMiterlimit() == 4.0f);
    REQUIRE(shape->strokeMiterlimit(0.00001f) == Result::Success);
    REQUIRE(shape->strokeMiterlimit() == 0.00001f);
    REQUIRE(shape->strokeMiterlimit(1000.0f) == Result::Success);
    REQUIRE(shape->strokeMiterlimit() == 1000.0f);
    REQUIRE(shape->strokeMiterlimit(-0.001f) == Result::InvalidArguments);

    //Stroke Trim
    float begin, end;
    REQUIRE(shape->strokeTrim(&begin, &end) == true);
    REQUIRE(begin == Approx(0.0).margin(0.000001));
    REQUIRE(end == Approx(1.0).margin(0.000001));

    REQUIRE(shape->strokeTrim(0.3f, 0.88f, false) == Result::Success);
    REQUIRE(shape->strokeTrim(&begin, &end) == false);
    REQUIRE(begin == Approx(0.3).margin(0.000001));
    REQUIRE(end == Approx(0.88).margin(0.000001));

    //Stroke Order After Stroke Setting
    REQUIRE(shape->order(true) == Result::Success);
    REQUIRE(shape->order(false) == Result::Success);
}

TEST_CASE("Shape Filling", "[tvgShape]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    //Fill Color
    uint8_t r, g, b, a;
    REQUIRE(shape->fill(255, 100, 50, 5) == Result::Success);
    REQUIRE(shape->fillColor(&r, nullptr, &b, nullptr) == Result::Success);
    REQUIRE(r == 255);
    REQUIRE(b == 50);
    REQUIRE(shape->fillColor(&r, &g, &b, &a) == Result::Success);
    REQUIRE(g == 100);
    REQUIRE(a == 5);

    //Fill Rule
    REQUIRE(shape->fillRule() == FillRule::Winding);
    REQUIRE(shape->fill(FillRule::EvenOdd) == Result::Success);
    REQUIRE(shape->fillRule() == FillRule::EvenOdd);
}
