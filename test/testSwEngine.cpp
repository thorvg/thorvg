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
#include <cmath>
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

TEST_CASE("Intersection", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());

        uint32_t buffer[200 * 200] = {};
        canvas->target(buffer, 200, 200, 200, ColorSpace::ARGB8888);

        auto shape = Shape::gen();
        REQUIRE(shape);
        REQUIRE(shape->appendRect(50, 50, 100, 100) == Result::Success);
        REQUIRE(shape->fill(255, 0, 0, 255) == Result::Success);

        REQUIRE(canvas->add(shape) == Result::Success);
        REQUIRE(canvas->draw() == Result::Success);

        // Case1. Fully contained
        REQUIRE(shape->intersects(0, 0, 200, 200, true) == true);

        // Case2. Partially overlapping
        REQUIRE(shape->intersects(25, 25, 50, 50, false) == true);
        REQUIRE(shape->intersects(125, 125, 50, 50, false) == true);

        shape->visible(false);

        // Case3. Edge-touching
        REQUIRE(shape->intersects(49, 49, 2, 2, true) == false);
        REQUIRE(shape->intersects(149, 149, 2, 2, false) == true);

        // Case4. Fully separated
        REQUIRE(shape->intersects(0, 0, 25, 25, true) == false);
        REQUIRE(shape->intersects(175, 175, 25, 25, true) == false);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

// Compares the buffers per pixel: a whole-vector REQUIRE would expand
// hundreds of thousands of values under --success and slow the test.
static void requireBuffersEqual(const vector<uint32_t>& a, const vector<uint32_t>& b, uint32_t w)
{
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            FAIL("first mismatch at (" << i % w << ", " << i / w << "): "
                                       << "a=0x" << hex << a[i] << " b=0x" << b[i]);
        }
    }
    REQUIRE(true);
}

TEST_CASE("RLE dense and tall fill consistency", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        // solid rect: interior is covered, a far corner stays clear (sweep / clip)
        {
            const uint32_t W = 64, H = 64;
            vector<uint32_t> buffer(W * H, 0);
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(buffer.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            auto rect = Shape::gen();
            REQUIRE(rect->appendRect(10, 10, 20, 20) == Result::Success);
            REQUIRE(rect->fill(200, 40, 80) == Result::Success);
            REQUIRE(canvas->add(rect) == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
            REQUIRE(buffer[20 + 20 * W] != 0);
            REQUIRE(buffer[0] == 0);
            REQUIRE(buffer[(W - 1) + (H - 1) * W] == 0);
        }

        // tall sawtooth plus a dense star: many rows / cells, including pool growth
        const uint32_t W = 64, H = 2048;
        auto makeScene = []() {
            const uint32_t TW = 64, TH = 2048;
            auto scene = Scene::gen();
            auto saw = Shape::gen();
            REQUIRE(saw->moveTo(0, 0) == Result::Success);
            for (int y = 0; y < int(TH); y += 8) {
                REQUIRE(saw->lineTo(40, float(y + 4)) == Result::Success);
                REQUIRE(saw->lineTo(8, float(y + 8)) == Result::Success);
            }
            REQUIRE(saw->lineTo(float(TW - 1), float(TH)) == Result::Success);
            REQUIRE(saw->lineTo(float(TW - 1), 0) == Result::Success);
            REQUIRE(saw->close() == Result::Success);
            REQUIRE(saw->fill(120, 40, 200) == Result::Success);
            REQUIRE(scene->add(saw) == Result::Success);

            auto star = Shape::gen();
            const int N = 200;
            const float cx = 32.0f, cy = 32.0f, r = 28.0f;
            REQUIRE(star->moveTo(cx + r, cy) == Result::Success);
            for (int i = 1; i <= N; ++i) {
                auto a = float(i) * 6.2831853f / float(N);
                auto rad = (i % 2) ? r * 0.35f : r;
                REQUIRE(star->lineTo(cx + rad * std::cos(a), cy + rad * std::sin(a)) == Result::Success);
            }
            REQUIRE(star->close() == Result::Success);
            REQUIRE(star->fill(30, 180, 60) == Result::Success);
            REQUIRE(scene->add(star) == Result::Success);
            return scene;
        };

        vector<uint32_t> first(W * H, 0), second(W * H, 0);
        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(first.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            REQUIRE(canvas->add(makeScene()) == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
        }
        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(second.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            REQUIRE(canvas->add(makeScene()) == Result::Success);
            REQUIRE(canvas->draw(true) == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
        }
        REQUIRE(first[32 + 32 * W] != 0);
        requireBuffersEqual(first, second, W);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

#ifdef THORVG_PARTIAL_RENDER_SUPPORT

TEST_CASE("Partial Rendering. Composited scene consistency", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        const uint32_t W = 400, H = 200;

        // a still half-transparent group and a recolored opaque shape aside
        auto makeScene = [](uint8_t moverFill, Shape** moverOut) {
            auto root = Scene::gen();
            auto group = Scene::gen();
            auto r1 = Shape::gen();
            REQUIRE(r1->appendRect(20, 20, 30, 40) == Result::Success);
            REQUIRE(r1->fill(120, 40, 200) == Result::Success);
            auto r2 = Shape::gen();
            REQUIRE(r2->appendRect(70, 20, 30, 40) == Result::Success);
            REQUIRE(r2->fill(120, 40, 200) == Result::Success);
            REQUIRE(group->add(r1) == Result::Success);
            REQUIRE(group->add(r2) == Result::Success);
            REQUIRE(group->opacity(128) == Result::Success);
            REQUIRE(root->add(group) == Result::Success);

            auto mover = Shape::gen();
            REQUIRE(mover->appendRect(300, 20, 20, 20) == Result::Success);
            REQUIRE(mover->fill(moverFill, 200, 40) == Result::Success);
            REQUIRE(root->add(mover) == Result::Success);
            if (moverOut) *moverOut = mover;
            return root;
        };

        vector<uint32_t> incremental(W * H, 0), fresh(W * H, 0);
        const int FRAMES = 4;

        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(incremental.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            Shape* mover = nullptr;
            REQUIRE(canvas->add(makeScene(20, &mover)) == Result::Success);
            for (int f = 0; f <= FRAMES; ++f) {
                if (f > 0) REQUIRE(mover->fill(uint8_t(20 + f), 200, 40) == Result::Success);
                REQUIRE(canvas->update() == Result::Success);
                REQUIRE(canvas->draw() == Result::Success);
                REQUIRE(canvas->sync() == Result::Success);
            }
        }
        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(fresh.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            REQUIRE(canvas->add(makeScene(20 + FRAMES, nullptr)) == Result::Success);
            REQUIRE(canvas->draw(true) == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
        }
        requireBuffersEqual(incremental, fresh, W);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Partial Rendering. Fragmented regions consistency", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        const uint32_t W = 800, H = 600;

        // a still translucent background with a large number of tiny updated shapes
        auto makeScene = [](uint8_t fill, vector<Shape*>* rectsOut) {
            auto scene = Scene::gen();
            auto bg = Shape::gen();
            REQUIRE(bg->appendRect(0, 0, W, H) == Result::Success);
            REQUIRE(bg->fill(120, 40, 200, 128) == Result::Success);
            REQUIRE(scene->add(bg) == Result::Success);
            for (int i = 0; i < 65; ++i) {
                auto x = 20 + (i % 13) * 10, y = 20 + (i / 13) * 10;
                auto rect = Shape::gen();
                REQUIRE(rect->appendRect(float(x), float(y), 2, 2) == Result::Success);
                REQUIRE(rect->fill(fill, 220, 40) == Result::Success);
                REQUIRE(scene->add(rect) == Result::Success);
                if (rectsOut) rectsOut->push_back(rect);
            }
            return scene;
        };

        vector<uint32_t> incremental(W * H, 0), fresh(W * H, 0);

        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(incremental.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            vector<Shape*> rects;
            REQUIRE(canvas->add(makeScene(20, &rects)) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            for (auto rect : rects)
                REQUIRE(rect->fill(21, 220, 40) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
        }
        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(fresh.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            REQUIRE(canvas->add(makeScene(21, nullptr)) == Result::Success);
            REQUIRE(canvas->draw(true) == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
        }
        requireBuffersEqual(incremental, fresh, W);
    }
    REQUIRE(Initializer::term() == Result::Success);
}

TEST_CASE("Partial Rendering. Heavy region fragmentation consistency", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init() == Result::Success);
    {
        const uint32_t W = 800, H = 600;

        // still composited bands crossed by recolored strips: the region union
        // fragments beyond the initial working list capacity
        auto makeScene = [](uint8_t stripFill, vector<Shape*>* stripsOut) {
            auto root = Scene::gen();
            for (int i = 0; i < 30; ++i) {
                auto group = Scene::gen();
                auto r1 = Shape::gen();
                REQUIRE(r1->appendRect(0, float(i * 4), 30, 2) == Result::Success);
                REQUIRE(r1->fill(30, 180, 60) == Result::Success);
                auto r2 = Shape::gen();
                REQUIRE(r2->appendRect(30, float(i * 4), 30, 2) == Result::Success);
                REQUIRE(r2->fill(30, 180, 60) == Result::Success);
                REQUIRE(group->add(r1) == Result::Success);
                REQUIRE(group->add(r2) == Result::Success);
                REQUIRE(group->opacity(128) == Result::Success);
                REQUIRE(root->add(group) == Result::Success);
            }
            for (int i = 0; i < 15; ++i) {
                auto strip = Shape::gen();
                REQUIRE(strip->appendRect(float(i * 4), 0, 2, 120) == Result::Success);
                REQUIRE(strip->fill(stripFill, 40, 200, 128) == Result::Success);
                REQUIRE(root->add(strip) == Result::Success);
                if (stripsOut) stripsOut->push_back(strip);
            }
            return root;
        };

        vector<uint32_t> incremental(W * H, 0), fresh(W * H, 0);

        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(incremental.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            vector<Shape*> strips;
            REQUIRE(canvas->add(makeScene(100, &strips)) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);

            for (auto strip : strips)
                REQUIRE(strip->fill(101, 40, 200, 128) == Result::Success);
            REQUIRE(canvas->update() == Result::Success);
            REQUIRE(canvas->draw() == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
        }
        {
            auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
            REQUIRE(canvas->target(fresh.data(), W, W, H, ColorSpace::ARGB8888S) == Result::Success);
            REQUIRE(canvas->add(makeScene(101, nullptr)) == Result::Success);
            REQUIRE(canvas->draw(true) == Result::Success);
            REQUIRE(canvas->sync() == Result::Success);
        }
        requireBuffersEqual(incremental, fresh, W);
    }
    REQUIRE(Initializer::term() == Result::Success);
}
#endif
#endif
