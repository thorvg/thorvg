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

TEST_CASE("Custom Transformation", "[tvgPaint]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    //Verify default transform
    Matrix m1 = shape->transform();
    REQUIRE(m1.e11 == Approx(1.0f).margin(0.000001));
    REQUIRE(m1.e12 == Approx(0).margin(0.000001));
    REQUIRE(m1.e13 == Approx(0).margin(0.000001));
    REQUIRE(m1.e21 == Approx(0).margin(0.000001));
    REQUIRE(m1.e22 == Approx(1.0f).margin(0.000001));
    REQUIRE(m1.e23 == Approx(0).margin(0.000001));
    REQUIRE(m1.e31 == Approx(0).margin(0.000001));
    REQUIRE(m1.e32 == Approx(0).margin(0.000001));
    REQUIRE(m1.e33 == Approx(1.0f).margin(0.000001));

    //Custom transform
    auto m2 = Matrix{1.0f, 2.0f, 3.0f, 4.0f, 0.0f, -4.0f, -3.0f, -2.0f, -1.0f};
    REQUIRE(shape->transform(m2) == Result::Success);

    auto m3 = shape->transform();
    REQUIRE(m2.e11 == Approx(m3.e11).margin(0.000001));
    REQUIRE(m2.e12 == Approx(m3.e12).margin(0.000001));
    REQUIRE(m2.e13 == Approx(m3.e13).margin(0.000001));
    REQUIRE(m2.e21 == Approx(m3.e21).margin(0.000001));
    REQUIRE(m2.e22 == Approx(m3.e22).margin(0.000001));
    REQUIRE(m2.e23 == Approx(m3.e23).margin(0.000001));
    REQUIRE(m2.e31 == Approx(m3.e31).margin(0.000001));
    REQUIRE(m2.e32 == Approx(m3.e32).margin(0.000001));
    REQUIRE(m2.e33 == Approx(m3.e33).margin(0.000001));

    //It's not allowed if the custom transform is applied.
    REQUIRE(shape->translate(155.0f, -155.0f) == Result::InsufficientCondition);
    REQUIRE(shape->scale(4.7f) == Result::InsufficientCondition);
    REQUIRE(shape->rotate(45.0f) == Result::InsufficientCondition);

    //Verify Transform is not modified
    auto m4 = shape->transform();
    REQUIRE(m2.e11 == Approx(m4.e11).margin(0.000001));
    REQUIRE(m2.e12 == Approx(m4.e12).margin(0.000001));
    REQUIRE(m2.e13 == Approx(m4.e13).margin(0.000001));
    REQUIRE(m2.e21 == Approx(m4.e21).margin(0.000001));
    REQUIRE(m2.e22 == Approx(m4.e22).margin(0.000001));
    REQUIRE(m2.e23 == Approx(m4.e23).margin(0.000001));
    REQUIRE(m2.e31 == Approx(m4.e31).margin(0.000001));
    REQUIRE(m2.e32 == Approx(m4.e32).margin(0.000001));
    REQUIRE(m2.e33 == Approx(m4.e33).margin(0.000001));
}


TEST_CASE("Basic Transformation", "[tvgPaint]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    REQUIRE(shape->translate(155.0f, -155.0f) == Result::Success);
    REQUIRE(shape->rotate(45.0f) == Result::Success);
    REQUIRE(shape->scale(4.7f) == Result::Success);

    auto m = shape->transform();
    REQUIRE(m.e11 == Approx(3.323402f).margin(0.000001));
    REQUIRE(m.e12 == Approx(-3.323401f).margin(0.000001));
    REQUIRE(m.e13 == Approx(155.0f).margin(0.000001));
    REQUIRE(m.e21 == Approx(3.323401f).margin(0.000001));
    REQUIRE(m.e22 == Approx(3.323402f).margin(0.000001));
    REQUIRE(m.e23 == Approx(-155.0f).margin(0.000001));
    REQUIRE(m.e31 == Approx(0).margin(0.000001));
    REQUIRE(m.e32 == Approx(0).margin(0.000001));
    REQUIRE(m.e33 == Approx(1).margin(0.000001));
}

TEST_CASE("Opacity", "[tvgPaint]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    REQUIRE(shape->opacity() == 255);

    REQUIRE(shape->opacity(155) == Result::Success);
    REQUIRE(shape->opacity() == 155);

    REQUIRE(shape->opacity(-1) == Result::Success);
    REQUIRE(shape->opacity() == 255);

    REQUIRE(shape->opacity(0) == Result::Success);
    REQUIRE(shape->opacity() == 0);
}

TEST_CASE("Bounding Box", "[tvgPaint]")
{
    Initializer::init(tvg::CanvasEngine::Sw, 0);

    auto canvas = SwCanvas::gen();
    auto shape = Shape::gen().release();
    canvas->push(tvg::cast(shape));
    canvas->sync();

    //Negative
    float x = 0, y = 0, w = 0, h = 0;
    REQUIRE(shape->bounds(&x, &y, &w, &h, false) == Result::InsufficientCondition);

    //Case 1
    REQUIRE(shape->appendRect(0.0f, 10.0f, 20.0f, 100.0f, 50.0f, 50.0f) == Result::Success);
    REQUIRE(shape->translate(100.0f, 111.0f) == Result::Success);
    REQUIRE(shape->bounds(&x, &y, &w, &h, false) == Result::Success);
    REQUIRE(x == 0.0f);
    REQUIRE(y == 10.0f);
    REQUIRE(w == 20.0f);
    REQUIRE(h == 100.0f);

    REQUIRE(canvas->update(shape) == Result::Success);
    REQUIRE(shape->bounds(&x, &y, &w, &h, true) == Result::Success);
    REQUIRE(x == 100.0f);
    REQUIRE(y == 121.0f);
    REQUIRE(w == 20.0f);
    REQUIRE(h == 100.0f);

    //Case 2
    REQUIRE(shape->reset() == Result::Success);
    REQUIRE(shape->moveTo(0.0f, 10.0f) == Result::Success);
    REQUIRE(shape->lineTo(20.0f, 210.0f) == Result::Success);
    auto identity = Matrix{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    REQUIRE(shape->transform(identity) == Result::Success);
    REQUIRE(shape->bounds(&x, &y, &w, &h, false) == Result::Success);
    REQUIRE(x == 0.0f);
    REQUIRE(y == 10.0f);
    REQUIRE(w == 20.0f);
    REQUIRE(h == 200.0f);

    REQUIRE(canvas->update(shape) == Result::Success);
    REQUIRE(shape->bounds(&x, &y, &w, &h, true) == Result::Success);
    REQUIRE(x == 0.0f);
    REQUIRE(y == 10.0f);
    REQUIRE(w == 20.0f);
    REQUIRE(h == 200.0f);

    Initializer::term(tvg::CanvasEngine::Sw);
}

TEST_CASE("Duplication", "[tvgPaint]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    //Setup paint properties
    REQUIRE(shape->opacity(0) == Result::Success);
    REQUIRE(shape->translate(200.0f, 100.0f) == Result::Success);
    REQUIRE(shape->scale(2.2f) == Result::Success);
    REQUIRE(shape->rotate(90.0f) == Result::Success);

    auto comp = Shape::gen();
    REQUIRE(comp);
    REQUIRE(shape->clip(std::move(comp)) == Result::Success);

    //Duplication
    auto dup = tvg::cast<Shape>(shape->duplicate());
    REQUIRE(dup);

    //Compare properties
    REQUIRE(dup->opacity() == 0);

    auto m = shape->transform();
    REQUIRE(m.e11 == Approx(0.0f).margin(0.000001));
    REQUIRE(m.e12 == Approx(-2.2f).margin(0.000001));
    REQUIRE(m.e13 == Approx(200.0f).margin(0.000001));
    REQUIRE(m.e21 == Approx(2.2f).margin(0.000001));
    REQUIRE(m.e22 == Approx(0.0f).margin(0.000001));
    REQUIRE(m.e23 == Approx(100.0f).margin(0.000001));
    REQUIRE(m.e31 == Approx(0.0f).margin(0.000001));
    REQUIRE(m.e32 == Approx(0.0f).margin(0.000001));
    REQUIRE(m.e33 == Approx(1.0f).margin(0.000001));
}

TEST_CASE("Composition", "[tvgPaint]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    //Negative
    REQUIRE(shape->composite(nullptr) == CompositeMethod::None);

    auto comp = Shape::gen();
    REQUIRE(shape->composite(std::move(comp), CompositeMethod::None) == Result::InvalidArguments);

    //Clipping
    comp = Shape::gen();
    auto pComp = comp.get();
    REQUIRE(shape->clip(std::move(comp)) == Result::Success);

    //AlphaMask
    comp = Shape::gen();
    pComp = comp.get();
    REQUIRE(shape->composite(std::move(comp), CompositeMethod::AlphaMask) == Result::Success);

    const Paint* pComp2 = nullptr;
    REQUIRE(shape->composite(&pComp2) == CompositeMethod::AlphaMask);
    REQUIRE(pComp == pComp2);

    //InvAlphaMask
    comp = Shape::gen();
    pComp = comp.get();
    REQUIRE(shape->composite(std::move(comp), CompositeMethod::InvAlphaMask) == Result::Success);

    REQUIRE(shape->composite(&pComp2) == CompositeMethod::InvAlphaMask);
    REQUIRE(pComp == pComp2);

    //LumaMask
    comp = Shape::gen();
    pComp = comp.get();
    REQUIRE(shape->composite(std::move(comp), CompositeMethod::LumaMask) == Result::Success);

    REQUIRE(shape->composite(&pComp2) == CompositeMethod::LumaMask);
    REQUIRE(pComp == pComp2);

    //InvLumaMask
    comp = Shape::gen();
    pComp = comp.get();
    REQUIRE(shape->composite(std::move(comp), CompositeMethod::InvLumaMask) == Result::Success);

    REQUIRE(shape->composite(&pComp2) == CompositeMethod::InvLumaMask);
    REQUIRE(pComp == pComp2);
}

TEST_CASE("Blending", "[tvgPaint]")
{
    auto shape = Shape::gen();
    REQUIRE(shape);

    //Default
    REQUIRE(shape->blend() == BlendMethod::Normal);

    //Add
    REQUIRE(shape->blend(BlendMethod::Add) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Add);

    //Screen
    REQUIRE(shape->blend(BlendMethod::Screen) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Screen);

    //Multiply
    REQUIRE(shape->blend(BlendMethod::Multiply) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Multiply);

    //Overlay
    REQUIRE(shape->blend(BlendMethod::Overlay) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Overlay);

    //Difference
    REQUIRE(shape->blend(BlendMethod::Difference) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Difference);

    //Exclusion
    REQUIRE(shape->blend(BlendMethod::Exclusion) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Exclusion);

    //Darken
    REQUIRE(shape->blend(BlendMethod::Darken) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Darken);

    //Lighten
    REQUIRE(shape->blend(BlendMethod::Lighten) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::Lighten);

    //ColorDodge
    REQUIRE(shape->blend(BlendMethod::ColorDodge) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::ColorDodge);

    //ColorBurn
    REQUIRE(shape->blend(BlendMethod::ColorBurn) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::ColorBurn);

    //HardLight
    REQUIRE(shape->blend(BlendMethod::HardLight) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::HardLight);

    //SoftLight
    REQUIRE(shape->blend(BlendMethod::SoftLight) == Result::Success);
    REQUIRE(shape->blend() == BlendMethod::SoftLight);
}