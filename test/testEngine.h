/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_TEST_ENGINE_H_
#define _TVG_TEST_ENGINE_H_

#include <thorvg.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>
#include "catch.hpp"
#include "engine.h"

using namespace tvg;

struct RawImage
{
    uint32_t* data = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;

    ~RawImage()
    {
        unload();
    }

    void unload()
    {
        free(data);
        data = nullptr;
        width = 0;
        height = 0;
    }

    bool load(const char* path, uint32_t w, uint32_t h)
    {
        unload();

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        auto bytes = sizeof(uint32_t) * size_t(w) * h;
        data = static_cast<uint32_t*>(malloc(bytes));

        if (!data) return false;
        file.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(bytes));

        if (file.gcount() != static_cast<std::streamsize>(bytes)) {
            unload();
            return false;
        }

        width = w;
        height = h;

        return true;
    }
};

inline std::vector<MaskMethod> maskMethods()
{
    return {
        MaskMethod::None,
        MaskMethod::Alpha,
        MaskMethod::InvAlpha,
        MaskMethod::Luma,
        MaskMethod::InvLuma,
        MaskMethod::Add,
        MaskMethod::Subtract,
        MaskMethod::Intersect,
        MaskMethod::Difference,
        MaskMethod::Lighten,
        MaskMethod::Darken,
    };
}

inline std::vector<BlendMethod> blendMethods()
{
    return {
        BlendMethod::Normal,
        BlendMethod::Multiply,
        BlendMethod::Screen,
        BlendMethod::Overlay,
        BlendMethod::Darken,
        BlendMethod::Lighten,
        BlendMethod::ColorDodge,
        BlendMethod::ColorBurn,
        BlendMethod::HardLight,
        BlendMethod::SoftLight,
        BlendMethod::Difference,
        BlendMethod::Hue,
        BlendMethod::Saturation,
        BlendMethod::Color,
        BlendMethod::Luminosity,
        BlendMethod::Add,
        BlendMethod::Composition,
    };
}

struct TestEngine
{
    static constexpr uint32_t RenderSize = 100;
    static constexpr uint32_t RotationSize = 960;

    static void basic(TvgTestEngine& engine, std::vector<BlendMethod> methods = {}, std::vector<MaskMethod> masks = {}, bool syncPerCombination = false)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));
        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);
            if (methods.empty()) methods = blendMethods();
            if (masks.empty()) masks = maskMethods();

            auto mask = []() {
                auto shape = Shape::gen();
                shape->appendRect(0, 10, 20, 30, 5, 5);
                shape->opacity(127);
                shape->fill(255, 255, 255);
                return shape;
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

                    if (syncPerCombination) {
                        REQUIRE(canvas->update() == Result::Success);
                        REQUIRE(canvas->draw(true) == Result::Success);
                        REQUIRE(canvas->sync() == Result::Success);
                        REQUIRE(engine.rendered());
                        REQUIRE(canvas->remove() == Result::Success);
                    }
                }
            }

            if (!syncPerCombination) {
                REQUIRE(canvas->draw(true) == Result::Success);
                REQUIRE(canvas->sync() == Result::Success);
                REQUIRE(engine.rendered());
            }
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static bool image(TvgTestEngine& engine, std::vector<BlendMethod> methods = {}, std::vector<MaskMethod> masks = {}, bool syncPerCombination = false)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));
        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);
            if (methods.empty()) methods = blendMethods();
            if (masks.empty()) masks = maskMethods();

            RawImage raw;
            if (!raw.load(TEST_DIR "/rawimage_200x300.raw", 200, 300)) {
                REQUIRE(Initializer::term() == Result::Success);
                return false;
            }

            auto mask = []() {
                auto shape = Shape::gen();
                shape->appendRect(0, 10, 20, 30, 5, 5);
                shape->fill(255, 255, 255);
                return shape;
            };

            for (auto method : methods) {
                for (auto maskOp : masks) {
                    //Non-transformed images
                    auto picture = Picture::gen();
                    REQUIRE(picture->load(raw.data, raw.width, raw.height, ColorSpace::ARGB8888, false) == Result::Success);
                    REQUIRE(picture->blend(method) == Result::Success);
                    if (maskOp != MaskMethod::None) REQUIRE(picture->mask(mask(), maskOp) == Result::Success);
                    REQUIRE(canvas->add(picture) == Result::Success);

                    //Clipped images
                    auto picture2 = picture->duplicate();
                    REQUIRE(picture2->clip(mask()) == Result::Success);
                    REQUIRE(canvas->add(picture2) == Result::Success);

                    //Transformed images
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
                    REQUIRE(picture6->load(raw.data, raw.width, raw.height, ColorSpace::ARGB8888, false) == Result::Success);
                    REQUIRE(picture6->clip(mask()) == Result::Success);
                    REQUIRE(picture6->blend(method) == Result::Success);
                    REQUIRE(canvas->add(picture6) == Result::Success);

                    //Scaled Clipped image
                    auto picture7 = picture6->duplicate();
                    REQUIRE(picture7->scale(2.0f) == Result::Success);
                    REQUIRE(canvas->add(picture7) == Result::Success);

                    if (syncPerCombination) {
                        REQUIRE(canvas->update() == Result::Success);
                        REQUIRE(canvas->draw() == Result::Success);
                        REQUIRE(canvas->sync() == Result::Success);
                        REQUIRE(engine.rendered());
                        REQUIRE(canvas->remove() == Result::Success);
                    }
                }
            }

            if (!syncPerCombination) {
                REQUIRE(canvas->draw() == Result::Success);
                REQUIRE(canvas->sync() == Result::Success);
                REQUIRE(engine.rendered());
            }
        }
        REQUIRE(Initializer::term() == Result::Success);
        return true;
    }

    static void filling(TvgTestEngine& engine, std::vector<BlendMethod> methods = {}, std::vector<MaskMethod> masks = {})
    {
        REQUIRE(engine.init(RenderSize, RenderSize));
        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);
            if (methods.empty()) methods = blendMethods();
            if (masks.empty()) masks = maskMethods();

            auto mask = []() {
                auto shape = Shape::gen();
                shape->appendRect(10, 10, 20, 30, 5, 5);
                shape->opacity(127);
                shape->fill(255, 255, 255);
                return shape;
            };

            Fill::ColorStop cs[4] = {
                {0.1f, 0, 0, 0, 0},
                {0.2f, 50, 25, 50, 25},
                {0.5f, 100, 100, 100, 125},
                {0.9f, 255, 255, 255, 255}};

            for (auto method : methods) {
                for (auto maskOp : masks) {
                    //Linear Gradient
                    auto linear = LinearGradient::gen();
                    REQUIRE(linear->colorStops(cs, 4) == Result::Success);
                    REQUIRE(linear->spread(FillSpread::Repeat) == Result::Success);
                    REQUIRE(linear->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);

                    auto shape1 = Shape::gen();
                    REQUIRE(shape1->appendRect(0, 0, 50, 50, 5, 5) == Result::Success);
                    REQUIRE(shape1->fill(linear) == Result::Success);
                    REQUIRE(shape1->blend(method) == Result::Success);
                    if (maskOp != MaskMethod::None) REQUIRE(shape1->mask(mask(), maskOp) == Result::Success);
                    REQUIRE(canvas->add(shape1) == Result::Success);

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
            REQUIRE(engine.rendered());
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static void sceneEffects(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));
        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);

            auto shape = Shape::gen();
            REQUIRE(shape);
            REQUIRE(shape->appendCircle(50, 50, 30, 30) == Result::Success);
            REQUIRE(shape->fill(0, 255, 0, 255) == Result::Success);

            auto scene = Scene::gen();
            REQUIRE(scene);
            REQUIRE(scene->add(shape) == Result::Success);

            auto picture = tvg::Picture::gen();
            picture->load(TEST_DIR"/tiger.svg");

            scene->add(picture);
            REQUIRE(canvas->add(scene) == Result::Success);

            // Gaussian Blur 
            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::GaussianBlur, 1.5, 0, 0, 75) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::GaussianBlur, 1.5, 1, 0, 75) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::GaussianBlur, 1.5, 2, 0, 75) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            // Drop Shadow
            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::DropShadow, 128, 128, 128, 200, 45.0, 5.0, 2.0, 60) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::DropShadow, 128, 128, 128, 200, 45.0, 5.0, 0.0, 60) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            // Fill
            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::Fill, 255, 0, 0, 128) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            // Tint
            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::Tint, 0, 0, 0, 255, 255, 255, 50.0) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            // Tritone
            REQUIRE(scene->add(SceneEffect::Clear) == Result::Success);
            REQUIRE(scene->add(SceneEffect::Tritone, 0, 0, 0, 128, 128, 128, 255, 255, 255, 128) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            // Tritone + Gaussian Blur + Drop Shadow
            REQUIRE(scene->add(SceneEffect::GaussianBlur, 1.5, 0, 0, 75) == Result::Success);
            REQUIRE(scene->add(SceneEffect::DropShadow, 128, 128, 128, 200, 45.0, 5.0, 2.0, 60) == Result::Success);

            REQUIRE(canvas->add(scene->duplicate()) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
            REQUIRE(engine.rendered());
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static void solidBatch(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RenderSize, RenderSize));
        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);

            for (uint32_t i = 0; i < 4; ++i) {
                auto shape = Shape::gen();
                REQUIRE(shape);
                REQUIRE(shape->appendRect(10, 10, 80, 80) == Result::Success);
                REQUIRE(shape->fill(255, 255, 255, 255) == Result::Success);
                REQUIRE(canvas->add(shape) == Result::Success);
            }

            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
            REQUIRE(engine.rendered());
        }
        REQUIRE(Initializer::term() == Result::Success);
    }

    static bool imageRotation(TvgTestEngine& engine)
    {
        REQUIRE(engine.init(RotationSize, RotationSize));
        REQUIRE(Initializer::init() == Result::Success);
        {
            auto canvas = engine.canvas();
            REQUIRE(canvas);
            REQUIRE(engine.target(canvas.get()) == Result::Success);
            RawImage raw;
            if (!raw.load(TEST_DIR "/rawimage_250x375.raw", 250, 375)) {
                REQUIRE(Initializer::term() == Result::Success);
                return false;
            }

            auto picture = Picture::gen();
            REQUIRE(picture);
            REQUIRE(picture->load(raw.data, raw.width, raw.height, ColorSpace::ARGB8888, false) == Result::Success);
            REQUIRE(picture->size(240, 240) == Result::Success);
            REQUIRE(picture->transform({0.572866f, -4.431353f, 336.605835f, 5.198910f, -0.386219f, 30.710693f, 0.0f, 0.0f, 1.0f}) == Result::Success);
            REQUIRE(canvas->add(picture) == Result::Success);

            REQUIRE(canvas->draw(true) == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
            REQUIRE(engine.rendered());
        }
        REQUIRE(Initializer::term() == Result::Success);
        return true;
    }
};

#endif
