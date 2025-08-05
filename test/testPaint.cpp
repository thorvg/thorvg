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

TEST_CASE("Custom Transformation", "[tvgPaint]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
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
    auto shape = unique_ptr<Shape>(Shape::gen());
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
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    REQUIRE(shape->opacity() == 255);

    REQUIRE(shape->opacity(155) == Result::Success);
    REQUIRE(shape->opacity() == 155);

    REQUIRE(shape->opacity(-1) == Result::Success);
    REQUIRE(shape->opacity() == 255);

    REQUIRE(shape->opacity(0) == Result::Success);
    REQUIRE(shape->opacity() == 0);
}

TEST_CASE("Visibility", "[tvgPaint]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    REQUIRE(shape->visible() == true);

    REQUIRE(shape->visible(false) == Result::Success);
    REQUIRE(shape->visible() == false);

    REQUIRE(shape->visible(false) == Result::Success);
    REQUIRE(shape->visible() == false);

    REQUIRE(shape->visible(true) == Result::Success);
    REQUIRE(shape->visible() == true);
}

TEST_CASE("Bounding Box", "[tvgPaint]")
{
    Initializer::init();
    {
        auto buffer = unique_ptr<uint32_t[]>(new uint32_t[500*500]);
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        canvas->target(buffer.get(), 500, 500, 500, ColorSpace::ARGB8888);

        auto shape = Shape::gen();
        canvas->push(shape);

        //Negative
        float x = 0, y = 0, w = 0, h = 0;
        REQUIRE(shape->bounds(&x, &y, &w, &h) == Result::InsufficientCondition);

        //Case 1
        REQUIRE(shape->appendRect(0.0f, 10.0f, 20.0f, 100.0f, 50.0f, 50.0f) == Result::Success);
        REQUIRE(shape->translate(100.0f, 111.0f) == Result::Success);

        canvas->update();

        //Positive
        REQUIRE(shape->bounds(&x, &y, &w, &h) == Result::Success);
        REQUIRE(x == 100.0f);
        REQUIRE(y == 121.0f);
        REQUIRE(w == 20.0f);
        REQUIRE(h == 100.0f);

        Point pts[4];
        REQUIRE(shape->bounds(pts) == Result::Success);
        REQUIRE(pts[0].x == 100.0f);
        REQUIRE(pts[3].x == 100.0f);
        REQUIRE(pts[0].y == 121.0f);
        REQUIRE(pts[1].y == 121.0f);
        REQUIRE(pts[1].x == 120.0f);
        REQUIRE(pts[2].x == 120.0f);
        REQUIRE(pts[2].y == 221.0f);
        REQUIRE(pts[3].y == 221.0f);

        REQUIRE(canvas->sync() == Result::Success);

        //Case 2
        REQUIRE(shape->reset() == Result::Success);
        REQUIRE(shape->moveTo(0.0f, 10.0f) == Result::Success);
        REQUIRE(shape->lineTo(20.0f, 210.0f) == Result::Success);
        auto identity = Matrix{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        REQUIRE(shape->transform(identity) == Result::Success);

        REQUIRE(canvas->update() == Result::Success);

        REQUIRE(shape->bounds(&x, &y, &w, &h) == Result::Success);
        REQUIRE(x == 0.0f);
        REQUIRE(y == 10.0f);
        REQUIRE(w == 20.0f);
        REQUIRE(h == 200.0f);

        REQUIRE(shape->bounds(pts) == Result::Success);
        REQUIRE(pts[0].x == 0.0f);
        REQUIRE(pts[3].x == 0.0f);
        REQUIRE(pts[0].y == 10.0f);
        REQUIRE(pts[1].y == 10.0f);
        REQUIRE(pts[1].x == 20.0f);
        REQUIRE(pts[2].x == 20.0f);
        REQUIRE(pts[2].y == 210.0f);
        REQUIRE(pts[3].y == 210.0f);
    }
    Initializer::term();
}

TEST_CASE("Intersection", "[tvgPaint]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());

        uint32_t buffer[200 * 200];
        canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->appendRect(50, 50, 100, 100) == Result::Success);
        REQUIRE(shape->fill(255, 0, 0, 255) == Result::Success);

        REQUIRE(canvas->push(shape) == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);

        // Case1. Fully contained
        REQUIRE(shape->intersects(0, 0, 200, 200) == true);

        // Case2. Partially overlapping
        REQUIRE(shape->intersects(25, 25, 50, 50) == true);
        REQUIRE(shape->intersects(125, 125, 50, 50) == true);

        // Case3. Edge-touching
        REQUIRE(shape->intersects(49, 49, 2, 2) == true);
        REQUIRE(shape->intersects(149, 149, 2, 2) == true);

        // Case4. Fully separated
        REQUIRE(shape->intersects(0, 0, 25, 25) == false);
        REQUIRE(shape->intersects(175, 175, 25, 25) == false);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Duplication", "[tvgPaint]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    //Setup paint properties
    REQUIRE(shape->opacity(0) == Result::Success);
    REQUIRE(shape->translate(200.0f, 100.0f) == Result::Success);
    REQUIRE(shape->scale(2.2f) == Result::Success);
    REQUIRE(shape->rotate(90.0f) == Result::Success);

    auto comp = Shape::gen();
    REQUIRE(comp);
    REQUIRE(shape->clip(comp) == Result::Success);

    //Duplication
    auto dup = unique_ptr<Paint>(shape->duplicate());
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
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    //Negative
    REQUIRE(shape->mask(nullptr) == MaskMethod::None);

    //Clipping
    auto comp = Shape::gen();
    REQUIRE(shape->clip() == nullptr);
    REQUIRE(shape->clip(comp) == Result::Success);
    REQUIRE(shape->clip() == comp);

    //AlphaMask
    comp = Shape::gen();
    REQUIRE(shape->mask(comp, MaskMethod::Alpha) == Result::Success);

    const Paint* comp2 = nullptr;
    REQUIRE(shape->mask(&comp2) == MaskMethod::Alpha);
    REQUIRE(comp == comp2);

    //InvAlphaMask
    comp = Shape::gen();
    REQUIRE(shape->mask(comp, MaskMethod::InvAlpha) == Result::Success);

    REQUIRE(shape->mask(&comp2) == MaskMethod::InvAlpha);
    REQUIRE(comp == comp2);

    //LumaMask
    comp = Shape::gen();
    REQUIRE(shape->mask(comp, MaskMethod::Luma) == Result::Success);

    REQUIRE(shape->mask(&comp2) == MaskMethod::Luma);
    REQUIRE(comp == comp2);

    //InvLumaMask
    comp = Shape::gen();
    REQUIRE(shape->mask(comp, MaskMethod::InvLuma) == Result::Success);

    REQUIRE(shape->mask(&comp2) == MaskMethod::InvLuma);
    REQUIRE(comp == comp2);
}

TEST_CASE("Blending", "[tvgPaint]")
{
    auto shape = unique_ptr<Shape>(Shape::gen());
    REQUIRE(shape);

    //Add
    REQUIRE(shape->blend(BlendMethod::Add) == Result::Success);

    //Screen
    REQUIRE(shape->blend(BlendMethod::Screen) == Result::Success);

    //Multiply
    REQUIRE(shape->blend(BlendMethod::Multiply) == Result::Success);

    //Overlay
    REQUIRE(shape->blend(BlendMethod::Overlay) == Result::Success);

    //Difference
    REQUIRE(shape->blend(BlendMethod::Difference) == Result::Success);

    //Exclusion
    REQUIRE(shape->blend(BlendMethod::Exclusion) == Result::Success);

    //Darken
    REQUIRE(shape->blend(BlendMethod::Darken) == Result::Success);

    //Lighten
    REQUIRE(shape->blend(BlendMethod::Lighten) == Result::Success);

    //ColorDodge
    REQUIRE(shape->blend(BlendMethod::ColorDodge) == Result::Success);

    //ColorBurn
    REQUIRE(shape->blend(BlendMethod::ColorBurn) == Result::Success);

    //HardLight
    REQUIRE(shape->blend(BlendMethod::HardLight) == Result::Success);

    //SoftLight
    REQUIRE(shape->blend(BlendMethod::SoftLight) == Result::Success);
}

TEST_CASE("Refernce Count", "[tvgPaint]")
{
    auto shape = Shape::gen();
    REQUIRE(shape->refCnt() == 0);
    REQUIRE(shape->unref(false) == 0);
    REQUIRE(shape->ref() == 1);
    REQUIRE(shape->ref() == 2);
    REQUIRE(shape->ref() == 3);
    REQUIRE(shape->unref() == 2);
    REQUIRE(shape->unref() == 1);
    REQUIRE(shape->unref() == 0);

    Initializer::init();
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());

        shape = Shape::gen();
        REQUIRE(shape->ref() == 1);
        canvas->push(shape);
        REQUIRE(shape->refCnt() == 2);
        REQUIRE(shape->unref() == 1);

        shape = Shape::gen();
        REQUIRE(shape->ref() == 1);
        auto scene = Scene::gen();
        scene->push(shape);
        canvas->push(scene);
        REQUIRE(shape->refCnt() == 2);
        REQUIRE(shape->unref() == 1);

        shape = Shape::gen();
        REQUIRE(shape->ref() == 1);
        scene = Scene::gen();
        scene->push(shape);
        scene->remove();
        canvas->push(scene);
        REQUIRE(shape->unref() == 0);
    }
    Initializer::term();
}