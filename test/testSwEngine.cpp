/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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

#ifdef THORVG_CPU_ENGINE_SUPPORT

TEST_CASE("Basic draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        uint32_t buffer[100*100] = {};
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
                REQUIRE(canvas->add(shape1) == Result::Success);

                //Cubic
                auto shape2 = Shape::gen();
                REQUIRE(shape2->moveTo(50, 25) == Result::Success);
                REQUIRE(shape2->cubicTo(62, 25, 75, 38, 75, 50) == Result::Success);
                REQUIRE(shape2->close() == Result::Success);
                REQUIRE(shape2->strokeFill(255, 0, 0, 125) == Result::Success);
                REQUIRE(shape2->strokeWidth(1) == Result::Success);
                REQUIRE(shape2->blend(method) == Result::Success);
                if (maskOp != MaskMethod::None) REQUIRE(shape2->mask(mask(), maskOp) == Result::Success);
                REQUIRE(canvas->add(shape2) == Result::Success);

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
                REQUIRE(canvas->add(shape3) == Result::Success);

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
                REQUIRE(canvas->add(shape4) == Result::Success);
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

        uint32_t buffer[100*100] = {};
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
                REQUIRE(canvas->add(picture) == Result::Success);

                //Clipped images
                auto picture2 = picture->duplicate();
                REQUIRE(picture2->clip(mask()) == Result::Success);
                REQUIRE(canvas->add(picture2) == Result::Success);

                // Transformed images
                auto picture3 = picture->duplicate();
                REQUIRE(picture3->rotate(45) == Result::Success);
                REQUIRE(canvas->add(picture3) == Result::Success);

                //Up-scaled Image
                auto picture4 = picture->duplicate();
                REQUIRE(picture4->scale(2.0f) == Result::Success);
                REQUIRE(canvas->add(picture4) == Result::Success);

                //Down-scaled Image
                auto picture5 = picture->duplicate();
                REQUIRE(picture5->scale(0.25f) == Result::Success);
                REQUIRE(canvas->add(picture5) == Result::Success);

                //Direct Clipped image
                auto picture6 = Picture::gen();
                REQUIRE(picture6->load(data, 200, 300, ColorSpace::ARGB8888, false) == Result::Success);
                REQUIRE(picture6->clip(mask()) == Result::Success);
                REQUIRE(picture6->blend(method) == Result::Success);
                REQUIRE(canvas->add(picture6) == Result::Success);

                //Scaled Clipped image
                auto picture7 = picture6->duplicate();
                REQUIRE(picture7->scale(2.0f) == Result::Success);
                REQUIRE(canvas->add(picture7) == Result::Success);
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

        uint32_t buffer[100*100] = {};
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
                REQUIRE(canvas->add(shape) == Result::Success);

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
                REQUIRE(canvas->add(shape2) == Result::Success);
            }
        }

        REQUIRE(canvas->draw() == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Image Rotation", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
        REQUIRE(canvas);

        const uint32_t cw = 960;
        const uint32_t ch = 960;
        vector<uint32_t> buffer(static_cast<size_t>(cw) * ch);
        REQUIRE(canvas->target(buffer.data(), cw, ch, cw, ColorSpace::ARGB8888) == Result::Success);

        auto picture = Picture::gen();
        REQUIRE(picture);
 
        ifstream file(TEST_DIR"/rawimage_250x375.raw");
        if (!file.is_open()) return;
        auto data = (uint32_t*)malloc(sizeof(uint32_t) * (250*375));
        file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 250 * 375);
        file.close();

        REQUIRE(picture->load(data, 250, 375, ColorSpace::ARGB8888, false) == Result::Success);

        REQUIRE(picture->size(240, 240) == Result::Success);
        REQUIRE(picture->transform({0.572866f, -4.431353f, 336.605835f, 5.198910f, -0.386219f, 30.710693f, 0.0f, 0.0f, 1.0f}) == Result::Success);
        REQUIRE(canvas->add(picture) == Result::Success);

        REQUIRE(canvas->draw(true) == Result::Success);
        REQUIRE(canvas->sync() == Result::Success);

        free(data);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

struct Pixel
{
    uint8_t r, g, b, a;
};

static Pixel _decode(uint32_t pixel, ColorSpace cs)
{
    if (cs == ColorSpace::ABGR8888 || cs == ColorSpace::ABGR8888S) {
        return {static_cast<uint8_t>(pixel), static_cast<uint8_t>(pixel >> 8), static_cast<uint8_t>(pixel >> 16), static_cast<uint8_t>(pixel >> 24)};
    }
    return {static_cast<uint8_t>(pixel >> 16), static_cast<uint8_t>(pixel >> 8), static_cast<uint8_t>(pixel), static_cast<uint8_t>(pixel >> 24)};
}

static int _diff(uint8_t a, uint8_t b)
{
    return a > b ? a - b : b - a;
}

static Pixel _renderBlendedPixel(BlendMethod method, ColorSpace cs)
{
    constexpr uint32_t Size = 8;
    uint32_t buffer[Size * Size] = {};

    auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
    REQUIRE(canvas);
    REQUIRE(canvas->target(buffer, Size, Size, Size, cs) == Result::Success);

    auto dst = Shape::gen();
    REQUIRE(dst);
    REQUIRE(dst->appendRect(0, 0, Size, Size) == Result::Success);
    REQUIRE(dst->fill(20, 80, 210, 255) == Result::Success);
    REQUIRE(canvas->add(dst) == Result::Success);

    auto src = Shape::gen();
    REQUIRE(src);
    REQUIRE(src->appendRect(0, 0, Size, Size) == Result::Success);
    REQUIRE(src->fill(230, 40, 20, 255) == Result::Success);
    REQUIRE(src->blend(method) == Result::Success);
    REQUIRE(canvas->add(src) == Result::Success);

    REQUIRE(canvas->draw(true) == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    return _decode(buffer[(Size / 2) * Size + (Size / 2)], cs);
}

static void _requirePixelNear(const Pixel& lhs, const Pixel& rhs)
{
    REQUIRE(_diff(lhs.r, rhs.r) <= 1);
    REQUIRE(_diff(lhs.g, rhs.g) <= 1);
    REQUIRE(_diff(lhs.b, rhs.b) <= 1);
    REQUIRE(_diff(lhs.a, rhs.a) <= 1);
}

TEST_CASE("Non-separable blend colorspace parity", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        const BlendMethod methods[] = {
            BlendMethod::Hue,
            BlendMethod::Saturation,
            BlendMethod::Color,
            BlendMethod::Luminosity};

        for (auto method : methods) {
            INFO("BlendMethod: " << static_cast<int>(method));
            auto native = _renderBlendedPixel(method, ColorSpace::ABGR8888);
            auto web = _renderBlendedPixel(method, ColorSpace::ARGB8888S);
            INFO("ABGR8888 rgba: " << static_cast<int>(native.r) << ", " << static_cast<int>(native.g) << ", " << static_cast<int>(native.b) << ", " << static_cast<int>(native.a));
            INFO("ARGB8888S rgba: " << static_cast<int>(web.r) << ", " << static_cast<int>(web.g) << ", " << static_cast<int>(web.b) << ", " << static_cast<int>(web.a));
            _requirePixelNear(native, web);
        }
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Soft-light blend matches compositing spec", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        const ColorSpace colorspaces[] = {
            ColorSpace::ABGR8888,
            ColorSpace::ARGB8888S};

        for (auto cs : colorspaces) {
            INFO("ColorSpace: " << static_cast<int>(cs));
            auto pixel = _renderBlendedPixel(BlendMethod::SoftLight, cs);
            _requirePixelNear(pixel, {55, 42, 179, 255});
        }
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#endif
