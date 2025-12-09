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
#include <fstream>
#include "config.h"
#include "catch.hpp"

using namespace tvg;
using namespace std;

#ifdef THORVG_SW_RASTER_SUPPORT

TEST_CASE("Basic draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100];
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888S) == Result::Success);

        std::vector<MaskMethod> masks;
        masks.push_back(MaskMethod::None);
        masks.push_back(MaskMethod::Alpha);
        masks.push_back(MaskMethod::InvAlpha);
        masks.push_back(MaskMethod::Luma);
        masks.push_back(MaskMethod::InvLuma);
        masks.push_back(MaskMethod::Add);
        masks.push_back(MaskMethod::Subtract);
        masks.push_back(MaskMethod::Intersect);
        masks.push_back(MaskMethod::Difference);
        masks.push_back(MaskMethod::Lighten);
        masks.push_back(MaskMethod::Darken);

        std::vector<BlendMethod> methods;
        methods.push_back(BlendMethod::Normal);
        methods.push_back(BlendMethod::Multiply);
        methods.push_back(BlendMethod::Screen);
        methods.push_back(BlendMethod::Overlay);
        methods.push_back(BlendMethod::Darken);
        methods.push_back(BlendMethod::Lighten);
        methods.push_back(BlendMethod::ColorDodge);
        methods.push_back(BlendMethod::ColorBurn);
        methods.push_back(BlendMethod::HardLight);
        methods.push_back(BlendMethod::SoftLight);
        methods.push_back(BlendMethod::Difference);
        methods.push_back(BlendMethod::Hue);
        methods.push_back(BlendMethod::Saturation);
        methods.push_back(BlendMethod::Color);
        methods.push_back(BlendMethod::Luminosity);
        methods.push_back(BlendMethod::Add);
        methods.push_back(BlendMethod::Composition);

        auto mask = []() {
            auto mask = Shape::gen();
            mask->appendRect(0, 10, 20, 30, 5, 5);
            mask->opacity(127);
            mask->fill(255, 255, 255);
            return mask;
        };

        for (auto method : methods) {
            for (auto maskOp : masks) {
                //Arc Line
                auto shape1 = Shape::gen();
                REQUIRE(shape1->strokeFill(255, 255, 255, 255) == Result::Success);
                REQUIRE(shape1->strokeWidth(2) == Result::Success);
                REQUIRE(shape1->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(shape1->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->push(shape1) == Result::Success);

                //Cubic
                auto shape2 = Shape::gen();
                REQUIRE(shape2->moveTo(50, 25) == Result::Success);
                REQUIRE(shape2->cubicTo(62, 25, 75, 38, 75, 50) == Result::Success);
                REQUIRE(shape2->close() == Result::Success);
                REQUIRE(shape2->strokeFill(255, 0, 0, 125) == Result::Success);
                REQUIRE(shape2->strokeWidth(1) == Result::Success);
                REQUIRE(shape2->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(shape2->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->push(shape2) == Result::Success);

                //Fill
                auto shape3 = Shape::gen();
                REQUIRE(shape3->moveTo(0, 0) == Result::Success);
                REQUIRE(shape3->lineTo(20, 0) == Result::Success);
                REQUIRE(shape3->lineTo(20, 20) == Result::Success);
                REQUIRE(shape3->lineTo(0, 20) == Result::Success);
                REQUIRE(shape3->close() == Result::Success);
                REQUIRE(shape3->fill(255, 255, 255) == Result::Success);
                REQUIRE(shape3->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(shape3->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->push(shape3) == Result::Success);

                //Dashed Line shape
                auto shape4 = Shape::gen();
                float dashPattern[2] = {2.5f, 5.0f};
                REQUIRE(shape4->moveTo(0, 0) == Result::Success);
                REQUIRE(shape4->lineTo(25, 25) == Result::Success);
                REQUIRE(shape4->cubicTo(50, 50, 75, -75, 50, 100) == Result::Success);
                REQUIRE(shape4->close() == Result::Success);
                REQUIRE(shape4->fill(255, 255, 255) == Result::Success);
                REQUIRE(shape4->strokeFill(255, 0, 0, 255) == Result::Success);
                REQUIRE(shape4->strokeWidth(2) == Result::Success);
                REQUIRE(shape4->strokeDash(dashPattern, 2) == Result::Success);
                REQUIRE(shape4->strokeCap(StrokeCap::Round) == Result::Success);
                REQUIRE(shape4->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(shape4->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->push(shape4) == Result::Success);
            }
        }
        REQUIRE(canvas->draw(true) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Image Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100];
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        //raw image
        ifstream file(TEST_DIR"/rawimage_200x300.raw");
        if (!file.is_open()) return;
        auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
        file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
        file.close();

        std::vector<MaskMethod> masks;
        masks.push_back(MaskMethod::None);
        masks.push_back(MaskMethod::Alpha);
        masks.push_back(MaskMethod::InvAlpha);
        masks.push_back(MaskMethod::Luma);
        masks.push_back(MaskMethod::InvLuma);
        masks.push_back(MaskMethod::Add);
        masks.push_back(MaskMethod::Subtract);
        masks.push_back(MaskMethod::Intersect);
        masks.push_back(MaskMethod::Difference);
        masks.push_back(MaskMethod::Lighten);
        masks.push_back(MaskMethod::Darken);

        std::vector<BlendMethod> methods;
        methods.push_back(BlendMethod::Normal);
        methods.push_back(BlendMethod::Multiply);
        methods.push_back(BlendMethod::Screen);
        methods.push_back(BlendMethod::Overlay);
        methods.push_back(BlendMethod::Darken);
        methods.push_back(BlendMethod::Lighten);
        methods.push_back(BlendMethod::ColorDodge);
        methods.push_back(BlendMethod::ColorBurn);
        methods.push_back(BlendMethod::HardLight);
        methods.push_back(BlendMethod::SoftLight);
        methods.push_back(BlendMethod::Difference);
        methods.push_back(BlendMethod::Hue);
        methods.push_back(BlendMethod::Saturation);
        methods.push_back(BlendMethod::Color);
        methods.push_back(BlendMethod::Luminosity);
        methods.push_back(BlendMethod::Add);
        methods.push_back(BlendMethod::Composition);

        auto mask = []() {
            auto mask = Shape::gen();
            mask->appendRect(0, 10, 20, 30, 5, 5);
            mask->fill(255, 255, 255);
            return mask;
        };

        for (auto method : methods) {
            for (auto maskOp : masks) {
                //Non-transformed images
                auto picture = Picture::gen();
                REQUIRE(picture->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);
                REQUIRE(picture->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(picture->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->push(picture) == Result::Success);

                //Clipped images
                auto picture2 = picture->duplicate();
                REQUIRE(picture2->clip(mask()) == Result::Success);
                REQUIRE(canvas->push(picture2) == Result::Success);

                // Transformed images
                auto picture3 = picture->duplicate();
                REQUIRE(picture3->rotate(45) == Result::Success);
                REQUIRE(canvas->push(picture3) == Result::Success);

                //Up-scaled Image
                auto picture4 = picture->duplicate();
                REQUIRE(picture4->scale(2.0f) == Result::Success);
                REQUIRE(canvas->push(picture4) == Result::Success);

                //Down-scaled Image
                auto picture5 = picture->duplicate();
                REQUIRE(picture5->scale(0.25f) == Result::Success);
                REQUIRE(canvas->push(picture5) == Result::Success);

                //Direct Clipped image
                auto picture6 = Picture::gen();
                REQUIRE(picture6->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);
                REQUIRE(picture6->clip(mask()) == Result::Success);
                REQUIRE(picture6->blend(method) == Result::Success);
                REQUIRE(canvas->push(picture6) == Result::Success);

                //Scaled Clipped image
                auto picture7 = picture6->duplicate();
                REQUIRE(picture7->scale(2.0f) == Result::Success);
                REQUIRE(canvas->push(picture7) == Result::Success);
            }
        }

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
        free(data);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Filling Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100];
        REQUIRE(canvas->target(buffer, 100, 100, 100, ColorSpace::ARGB8888) == Result::Success);

        std::vector<MaskMethod> masks;
        masks.push_back(MaskMethod::None);
        masks.push_back(MaskMethod::Alpha);
        masks.push_back(MaskMethod::InvAlpha);
        masks.push_back(MaskMethod::Luma);
        masks.push_back(MaskMethod::InvLuma);
        masks.push_back(MaskMethod::Add);
        masks.push_back(MaskMethod::Subtract);
        masks.push_back(MaskMethod::Intersect);
        masks.push_back(MaskMethod::Difference);
        masks.push_back(MaskMethod::Lighten);
        masks.push_back(MaskMethod::Darken);

        std::vector<BlendMethod> methods;
        methods.push_back(BlendMethod::Normal);
        methods.push_back(BlendMethod::Multiply);
        methods.push_back(BlendMethod::Screen);
        methods.push_back(BlendMethod::Overlay);
        methods.push_back(BlendMethod::Darken);
        methods.push_back(BlendMethod::Lighten);
        methods.push_back(BlendMethod::ColorDodge);
        methods.push_back(BlendMethod::ColorBurn);
        methods.push_back(BlendMethod::HardLight);
        methods.push_back(BlendMethod::SoftLight);
        methods.push_back(BlendMethod::Difference);
        methods.push_back(BlendMethod::Hue);
        methods.push_back(BlendMethod::Saturation);
        methods.push_back(BlendMethod::Color);
        methods.push_back(BlendMethod::Luminosity);
        methods.push_back(BlendMethod::Add);
        methods.push_back(BlendMethod::Composition);

        auto mask = []() {
            auto mask = Shape::gen();
            mask->appendRect(10, 10, 20, 30, 5, 5);
            mask->opacity(127);
            mask->fill(255, 255, 255);
            return mask;
        };

        Fill::ColorStop cs[4] = {
            {0.1f, 0, 0, 0, 0},
            {0.2f, 50, 25, 50, 25},
            {0.5f, 100, 100, 100, 125},
            {0.9f, 255, 255, 255, 255}
        };

        for (auto method : methods) {
            for (auto maskOp : masks) {
                //Linear Gradient
                auto linear = LinearGradient::gen();
                REQUIRE(linear->colorStops(cs, 4) == Result::Success);
                REQUIRE(linear->spread(FillSpread::Repeat) == Result::Success);
                REQUIRE(linear->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);

                auto shape = Shape::gen();
                REQUIRE(shape->appendRect(0, 0, 50, 50, 5, 5) == Result::Success);
                REQUIRE(shape->fill(linear) == Result::Success);
                REQUIRE(shape->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(shape->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->push(shape) == Result::Success);

                //Radial Gradient
                auto radial = RadialGradient::gen();
                REQUIRE(radial->colorStops(cs, 4) == Result::Success);
                REQUIRE(radial->spread(FillSpread::Pad) == Result::Success);
                REQUIRE(radial->radial(50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 0.0f) == Result::Success);

                auto shape2 = Shape::gen();
                REQUIRE(shape2->appendRect(50, 0, 50, 50) == Result::Success);
                REQUIRE(shape2->fill(radial) == Result::Success);
                REQUIRE(shape2->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(shape2->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->push(shape2) == Result::Success);
            }
        }

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Trimpath Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[200*200];
        REQUIRE(canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888) == Result::Success);

        //Line trimpath - exercises Line::split and length
        auto shape1 = Shape::gen();
        REQUIRE(shape1->moveTo(10, 10) == Result::Success);
        REQUIRE(shape1->lineTo(190, 10) == Result::Success);
        REQUIRE(shape1->strokeWidth(3) == Result::Success);
        REQUIRE(shape1->strokeFill(255, 0, 0) == Result::Success);
        REQUIRE(shape1->trimpath(0.25f, 0.75f) == Result::Success);
        REQUIRE(canvas->push(shape1) == Result::Success);

        //Cubic bezier trimpath - exercises Bezier::length, split, at
        auto shape2 = Shape::gen();
        REQUIRE(shape2->moveTo(10, 50) == Result::Success);
        REQUIRE(shape2->cubicTo(60, 10, 140, 90, 190, 50) == Result::Success);
        REQUIRE(shape2->strokeWidth(3) == Result::Success);
        REQUIRE(shape2->strokeFill(0, 255, 0) == Result::Success);
        REQUIRE(shape2->trimpath(0.1f, 0.9f) == Result::Success);
        REQUIRE(canvas->push(shape2) == Result::Success);

        //Circle trimpath - quarter arc
        auto shape3 = Shape::gen();
        REQUIRE(shape3->appendCircle(100, 120, 40, 40) == Result::Success);
        REQUIRE(shape3->strokeWidth(3) == Result::Success);
        REQUIRE(shape3->strokeFill(0, 0, 255) == Result::Success);
        REQUIRE(shape3->trimpath(0.0f, 0.25f) == Result::Success);
        REQUIRE(canvas->push(shape3) == Result::Success);

        //Multiple path trimpath with simultaneous=true
        auto shape4 = Shape::gen();
        REQUIRE(shape4->moveTo(10, 170) == Result::Success);
        REQUIRE(shape4->lineTo(60, 170) == Result::Success);
        REQUIRE(shape4->moveTo(80, 170) == Result::Success);
        REQUIRE(shape4->cubicTo(100, 150, 140, 190, 190, 170) == Result::Success);
        REQUIRE(shape4->strokeWidth(3) == Result::Success);
        REQUIRE(shape4->strokeFill(255, 255, 0) == Result::Success);
        REQUIRE(shape4->trimpath(0.2f, 0.8f, true) == Result::Success);
        REQUIRE(canvas->push(shape4) == Result::Success);

        //Ellipse trimpath
        auto shape5 = Shape::gen();
        REQUIRE(shape5->appendCircle(100, 100, 80, 40) == Result::Success);
        REQUIRE(shape5->strokeWidth(2) == Result::Success);
        REQUIRE(shape5->strokeFill(128, 0, 128) == Result::Success);
        REQUIRE(shape5->trimpath(0.33f, 0.67f) == Result::Success);
        REQUIRE(canvas->push(shape5) == Result::Success);

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Transformed Shapes Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[200*200];
        REQUIRE(canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888) == Result::Success);

        //Rotated rectangle - exercises matrix rotation
        auto shape1 = Shape::gen();
        REQUIRE(shape1->appendRect(50, 50, 60, 40) == Result::Success);
        REQUIRE(shape1->fill(255, 0, 0) == Result::Success);
        REQUIRE(shape1->rotate(45.0f) == Result::Success);
        REQUIRE(canvas->push(shape1) == Result::Success);

        //Scaled circle - exercises matrix scaling
        auto shape2 = Shape::gen();
        REQUIRE(shape2->appendCircle(100, 100, 20, 20) == Result::Success);
        REQUIRE(shape2->fill(0, 255, 0) == Result::Success);
        REQUIRE(shape2->scale(2.0f) == Result::Success);
        REQUIRE(canvas->push(shape2) == Result::Success);

        //Translated and rotated bezier
        auto shape3 = Shape::gen();
        REQUIRE(shape3->moveTo(0, 0) == Result::Success);
        REQUIRE(shape3->cubicTo(30, -20, 70, 20, 100, 0) == Result::Success);
        REQUIRE(shape3->strokeWidth(3) == Result::Success);
        REQUIRE(shape3->strokeFill(0, 0, 255) == Result::Success);
        REQUIRE(shape3->translate(50, 150) == Result::Success);
        REQUIRE(shape3->rotate(30.0f) == Result::Success);
        REQUIRE(canvas->push(shape3) == Result::Success);

        //Combined transformations
        auto shape4 = Shape::gen();
        REQUIRE(shape4->appendRect(0, 0, 30, 30, 5, 5) == Result::Success);
        REQUIRE(shape4->fill(255, 255, 0) == Result::Success);
        REQUIRE(shape4->translate(100, 50) == Result::Success);
        REQUIRE(shape4->scale(1.5f) == Result::Success);
        REQUIRE(shape4->rotate(60.0f) == Result::Success);
        REQUIRE(canvas->push(shape4) == Result::Success);

        //90 degree rotation
        auto shape5 = Shape::gen();
        REQUIRE(shape5->appendRect(10, 10, 40, 20) == Result::Success);
        REQUIRE(shape5->fill(0, 255, 255) == Result::Success);
        REQUIRE(shape5->rotate(90.0f) == Result::Success);
        REQUIRE(canvas->push(shape5) == Result::Success);

        //180 degree rotation
        auto shape6 = Shape::gen();
        REQUIRE(shape6->appendRect(100, 10, 40, 20) == Result::Success);
        REQUIRE(shape6->fill(255, 0, 255) == Result::Success);
        REQUIRE(shape6->rotate(180.0f) == Result::Success);
        REQUIRE(canvas->push(shape6) == Result::Success);

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Complex Bezier Paths Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[200*200];
        REQUIRE(canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888) == Result::Success);

        //S-curve with multiple beziers
        auto shape1 = Shape::gen();
        REQUIRE(shape1->moveTo(10, 100) == Result::Success);
        REQUIRE(shape1->cubicTo(40, 40, 80, 40, 100, 100) == Result::Success);
        REQUIRE(shape1->cubicTo(120, 160, 160, 160, 190, 100) == Result::Success);
        REQUIRE(shape1->strokeWidth(3) == Result::Success);
        REQUIRE(shape1->strokeFill(255, 0, 0) == Result::Success);
        REQUIRE(canvas->push(shape1) == Result::Success);

        //Closed bezier path - filled
        auto shape2 = Shape::gen();
        REQUIRE(shape2->moveTo(50, 150) == Result::Success);
        REQUIRE(shape2->cubicTo(80, 120, 120, 120, 150, 150) == Result::Success);
        REQUIRE(shape2->cubicTo(170, 180, 30, 180, 50, 150) == Result::Success);
        REQUIRE(shape2->close() == Result::Success);
        REQUIRE(shape2->fill(0, 255, 0, 128) == Result::Success);
        REQUIRE(canvas->push(shape2) == Result::Success);

        //Very small bezier curves
        auto shape3 = Shape::gen();
        REQUIRE(shape3->moveTo(10, 10) == Result::Success);
        REQUIRE(shape3->cubicTo(10.1f, 10.1f, 10.2f, 10.2f, 10.3f, 10.3f) == Result::Success);
        REQUIRE(shape3->strokeWidth(1) == Result::Success);
        REQUIRE(shape3->strokeFill(0, 0, 255) == Result::Success);
        REQUIRE(canvas->push(shape3) == Result::Success);

        //Bezier with collinear control points
        auto shape4 = Shape::gen();
        REQUIRE(shape4->moveTo(10, 50) == Result::Success);
        REQUIRE(shape4->cubicTo(40, 50, 60, 50, 90, 50) == Result::Success);
        REQUIRE(shape4->strokeWidth(2) == Result::Success);
        REQUIRE(shape4->strokeFill(128, 128, 0) == Result::Success);
        REQUIRE(canvas->push(shape4) == Result::Success);

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Trimpath Edge Cases Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[200*200];
        REQUIRE(canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888) == Result::Success);

        //Reversed trim (begin > end wraps around)
        auto shape1 = Shape::gen();
        REQUIRE(shape1->appendCircle(50, 50, 30, 30) == Result::Success);
        REQUIRE(shape1->strokeWidth(3) == Result::Success);
        REQUIRE(shape1->strokeFill(255, 0, 0) == Result::Success);
        REQUIRE(shape1->trimpath(0.75f, 0.25f) == Result::Success);
        REQUIRE(canvas->push(shape1) == Result::Success);

        //Very small trim range
        auto shape2 = Shape::gen();
        REQUIRE(shape2->appendCircle(150, 50, 30, 30) == Result::Success);
        REQUIRE(shape2->strokeWidth(3) == Result::Success);
        REQUIRE(shape2->strokeFill(0, 255, 0) == Result::Success);
        REQUIRE(shape2->trimpath(0.49f, 0.51f) == Result::Success);
        REQUIRE(canvas->push(shape2) == Result::Success);

        //Trimpath on rounded rect
        auto shape3 = Shape::gen();
        REQUIRE(shape3->appendRect(20, 100, 60, 40, 10, 10) == Result::Success);
        REQUIRE(shape3->strokeWidth(3) == Result::Success);
        REQUIRE(shape3->strokeFill(0, 0, 255) == Result::Success);
        REQUIRE(shape3->trimpath(0.1f, 0.6f) == Result::Success);
        REQUIRE(canvas->push(shape3) == Result::Success);

        //Full path trim (0 to 1)
        auto shape4 = Shape::gen();
        REQUIRE(shape4->moveTo(120, 100) == Result::Success);
        REQUIRE(shape4->cubicTo(150, 80, 170, 120, 180, 100) == Result::Success);
        REQUIRE(shape4->strokeWidth(3) == Result::Success);
        REQUIRE(shape4->strokeFill(255, 255, 0) == Result::Success);
        REQUIRE(shape4->trimpath(0.0f, 1.0f) == Result::Success);
        REQUIRE(canvas->push(shape4) == Result::Success);

        //Trimpath with dash pattern
        auto shape5 = Shape::gen();
        REQUIRE(shape5->appendCircle(100, 170, 25, 25) == Result::Success);
        float dashPattern[2] = {5, 3};
        REQUIRE(shape5->strokeDash(dashPattern, 2) == Result::Success);
        REQUIRE(shape5->strokeWidth(2) == Result::Success);
        REQUIRE(shape5->strokeFill(128, 0, 128) == Result::Success);
        REQUIRE(shape5->trimpath(0.2f, 0.8f) == Result::Success);
        REQUIRE(canvas->push(shape5) == Result::Success);

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Stroke Variations Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[200*200];
        REQUIRE(canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888) == Result::Success);

        //Round cap with bezier
        auto shape1 = Shape::gen();
        REQUIRE(shape1->moveTo(10, 30) == Result::Success);
        REQUIRE(shape1->cubicTo(50, 10, 80, 50, 120, 30) == Result::Success);
        REQUIRE(shape1->strokeWidth(10) == Result::Success);
        REQUIRE(shape1->strokeFill(255, 0, 0) == Result::Success);
        REQUIRE(shape1->strokeCap(StrokeCap::Round) == Result::Success);
        REQUIRE(canvas->push(shape1) == Result::Success);

        //Square cap with bezier
        auto shape2 = Shape::gen();
        REQUIRE(shape2->moveTo(10, 70) == Result::Success);
        REQUIRE(shape2->cubicTo(50, 50, 80, 90, 120, 70) == Result::Success);
        REQUIRE(shape2->strokeWidth(10) == Result::Success);
        REQUIRE(shape2->strokeFill(0, 255, 0) == Result::Success);
        REQUIRE(shape2->strokeCap(StrokeCap::Square) == Result::Success);
        REQUIRE(canvas->push(shape2) == Result::Success);

        //Round join with sharp angle
        auto shape3 = Shape::gen();
        REQUIRE(shape3->moveTo(10, 120) == Result::Success);
        REQUIRE(shape3->lineTo(60, 100) == Result::Success);
        REQUIRE(shape3->lineTo(110, 140) == Result::Success);
        REQUIRE(shape3->strokeWidth(8) == Result::Success);
        REQUIRE(shape3->strokeFill(0, 0, 255) == Result::Success);
        REQUIRE(shape3->strokeJoin(StrokeJoin::Round) == Result::Success);
        REQUIRE(canvas->push(shape3) == Result::Success);

        //Miter join
        auto shape4 = Shape::gen();
        REQUIRE(shape4->moveTo(130, 120) == Result::Success);
        REQUIRE(shape4->lineTo(160, 100) == Result::Success);
        REQUIRE(shape4->lineTo(190, 140) == Result::Success);
        REQUIRE(shape4->strokeWidth(8) == Result::Success);
        REQUIRE(shape4->strokeFill(255, 255, 0) == Result::Success);
        REQUIRE(shape4->strokeJoin(StrokeJoin::Miter) == Result::Success);
        REQUIRE(canvas->push(shape4) == Result::Success);

        //Bevel join
        auto shape5 = Shape::gen();
        REQUIRE(shape5->moveTo(10, 180) == Result::Success);
        REQUIRE(shape5->lineTo(50, 160) == Result::Success);
        REQUIRE(shape5->lineTo(90, 180) == Result::Success);
        REQUIRE(shape5->strokeWidth(8) == Result::Success);
        REQUIRE(shape5->strokeFill(0, 255, 255) == Result::Success);
        REQUIRE(shape5->strokeJoin(StrokeJoin::Bevel) == Result::Success);
        REQUIRE(canvas->push(shape5) == Result::Success);

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}
#endif