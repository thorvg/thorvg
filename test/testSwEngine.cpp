/*
 * Copyright (c) 2021 - 2023 the ThorVG project. All rights reserved.

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
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888_STRAIGHT) == Result::Success);

    //Arc Line
    auto shape1 = tvg::Shape::gen();
    REQUIRE(shape1);

    REQUIRE(shape1->appendArc(150, 150, 80, 10, 180, false) == Result::Success);
    REQUIRE(shape1->stroke(255, 255, 255, 255) == Result::Success);
    REQUIRE(shape1->stroke(2) == Result::Success);
    REQUIRE(canvas->push(move(shape1)) == Result::Success);

    //Cubic
    auto shape2 = tvg::Shape::gen();
    REQUIRE(shape2);

    REQUIRE(shape2->moveTo(50, 25) == Result::Success);
    REQUIRE(shape2->cubicTo(62, 25, 75, 38, 75, 50) == Result::Success);
    REQUIRE(shape2->close() == Result::Success);
    REQUIRE(shape2->stroke(1) == Result::Success);
    REQUIRE(canvas->push(move(shape2)) == Result::Success);

    //Line
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);

    REQUIRE(shape3->moveTo(0, 0) == Result::Success);
    REQUIRE(shape3->lineTo(20, 0) == Result::Success);
    REQUIRE(shape3->lineTo(20, 20) == Result::Success);
    REQUIRE(shape3->lineTo(0, 20) == Result::Success);
    REQUIRE(shape3->close() == Result::Success);
    REQUIRE(shape3->fill(255, 255, 255, 255) == Result::Success);
    REQUIRE(canvas->push(move(shape3)) == Result::Success);

    //Dashed shape
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    float dashPattern[2] = {2.5f, 5.0f};

    REQUIRE(shape4->moveTo(0, 0) == Result::Success);
    REQUIRE(shape4->lineTo(25, 25) == Result::Success);
    REQUIRE(shape4->cubicTo(50, 50, 75, -75, 50, 100) == Result::Success);
    REQUIRE(shape4->close() == Result::Success);
    REQUIRE(shape4->stroke(255, 0, 0, 255) == Result::Success);
    REQUIRE(shape4->stroke(2) == Result::Success);
    REQUIRE(shape4->stroke(dashPattern, 2) == Result::Success);
    REQUIRE(shape4->stroke(StrokeCap::Round) == Result::Success);
    REQUIRE(canvas->push(move(shape4)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("Image Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //raw image
    ifstream file(TEST_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    //Not transformed images
    auto basicPicture = Picture::gen();
    REQUIRE(basicPicture);
    REQUIRE(basicPicture->load(data, 200, 300, false) == Result::Success);
    auto rectMask = tvg::Shape::gen();
    REQUIRE(rectMask);
    REQUIRE(rectMask->appendRect(10, 10, 30, 30, 0, 0) == Result::Success);
    REQUIRE(rectMask->fill(100, 100, 100, 255) == Result::Success);
    auto rleMask = tvg::Shape::gen();
    REQUIRE(rleMask);
    REQUIRE(rleMask->appendRect(0, 10, 20, 30, 5, 5) == Result::Success);
    REQUIRE(rleMask->fill(100, 100, 100, 255) == Result::Success);

    // Rect images
    auto basicPicture2 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture2);
    auto rectMask2 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask2);

    auto basicPicture3 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture3);
    auto rectMask3 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask3);

    auto basicPicture4 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture4);
    auto rectMask4 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask4);

    auto basicPicture5 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture5);

    // Rle images
    auto basicPicture6 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture6);

    auto basicPicture7 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture7);
    auto rleMask7 = tvg::cast<Shape>(rleMask->duplicate());
    REQUIRE(rleMask7);

    // Rect
    REQUIRE(basicPicture->composite(move(rectMask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture)) == Result::Success);

    REQUIRE(basicPicture2->composite(move(rectMask2), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture2)) == Result::Success);

    REQUIRE(basicPicture3->composite(move(rectMask3), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture3)) == Result::Success);

    REQUIRE(basicPicture4->composite(move(rectMask4), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture4)) == Result::Success);

    REQUIRE(basicPicture5->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture5)) == Result::Success);

    // Rle
    REQUIRE(basicPicture6->composite(move(rleMask), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture6)) == Result::Success);

    REQUIRE(basicPicture7->composite(move(rleMask7), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(basicPicture7->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture7)) == Result::Success);


    // Transformed images
    basicPicture = Picture::gen();
    REQUIRE(basicPicture);
    REQUIRE(basicPicture->load(data, 200, 300, false) == Result::Success);

    REQUIRE(basicPicture->rotate(45) == Result::Success);
    rectMask = tvg::Shape::gen();
    REQUIRE(rectMask);
    REQUIRE(rectMask->appendRect(10, 10, 30, 30, 0, 0) == Result::Success);
    rleMask = tvg::Shape::gen();
    REQUIRE(rleMask);
    REQUIRE(rleMask->appendRect(0, 10, 20, 30, 5, 5) == Result::Success);

    // Rect images
    basicPicture2 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture2);
    rectMask2 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask2);

    basicPicture3 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture3);
    rectMask3 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask3);

    basicPicture4 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture4);
    rectMask4 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask4);

    basicPicture5 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture5);

    // Rle images
    basicPicture6 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture6);

    basicPicture7 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture7);
    rleMask7 = tvg::cast<Shape>(rleMask->duplicate());
    REQUIRE(rleMask7);

    // Rect
    REQUIRE(basicPicture->composite(move(rectMask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture)) == Result::Success);

    REQUIRE(basicPicture2->composite(move(rectMask2), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture2)) == Result::Success);

    REQUIRE(basicPicture3->composite(move(rectMask3), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture3)) == Result::Success);

    REQUIRE(basicPicture4->composite(move(rectMask4), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture4)) == Result::Success);

    REQUIRE(basicPicture5->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture5)) == Result::Success);

    // Rle
    REQUIRE(basicPicture6->composite(move(rleMask), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture6)) == Result::Success);

    REQUIRE(basicPicture7->composite(move(rleMask7), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(basicPicture7->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture7)) == Result::Success);


    // Upscaled images
    basicPicture = Picture::gen();
    REQUIRE(basicPicture);
    REQUIRE(basicPicture->load(data, 200, 300, false) == Result::Success);
    REQUIRE(basicPicture->scale(2) == Result::Success);
    rectMask = tvg::Shape::gen();
    REQUIRE(rectMask);
    REQUIRE(rectMask->appendRect(10, 10, 30, 30, 0, 0) == Result::Success);
    rleMask = tvg::Shape::gen();
    REQUIRE(rleMask);
    REQUIRE(rleMask->appendRect(0, 10, 20, 30, 5, 5) == Result::Success);

    // Rect images
    basicPicture2 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture2);
    rectMask2 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask2);

    basicPicture3 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture3);
    rectMask3 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask3);

    basicPicture4 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture4);
    rectMask4 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask4);

    basicPicture5 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture5);

    // Rle images
    basicPicture6 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture6);

    basicPicture7 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture7);
    rleMask7 = tvg::cast<Shape>(rleMask->duplicate());
    REQUIRE(rleMask7);

    // Rect
    REQUIRE(basicPicture->composite(move(rectMask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture)) == Result::Success);

    REQUIRE(basicPicture2->composite(move(rectMask2), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture2)) == Result::Success);

    REQUIRE(basicPicture3->composite(move(rectMask3), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture3)) == Result::Success);

    REQUIRE(basicPicture4->composite(move(rectMask4), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture4)) == Result::Success);

    REQUIRE(basicPicture5->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture5)) == Result::Success);

    // Rle
    REQUIRE(basicPicture6->composite(move(rleMask), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture6)) == Result::Success);

    REQUIRE(basicPicture7->composite(move(rleMask7), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(basicPicture7->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture7)) == Result::Success);


    // Downscaled images
    basicPicture = Picture::gen();
    REQUIRE(basicPicture);
    REQUIRE(basicPicture->load(data, 200, 300, false) == Result::Success);
    REQUIRE(basicPicture->scale(0.2f) == Result::Success);
    rectMask = tvg::Shape::gen();
    REQUIRE(rectMask);
    REQUIRE(rectMask->appendRect(10, 10, 30, 30, 0, 0) == Result::Success);
    rleMask = tvg::Shape::gen();
    REQUIRE(rleMask);
    REQUIRE(rleMask->appendRect(0, 10, 20, 30, 5, 5) == Result::Success);

    // Rect images
    basicPicture2 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture2);
    rectMask2 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask2);

    basicPicture3 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture3);
    rectMask3 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask3);

    basicPicture4 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture4);
    rectMask4 = tvg::cast<Shape>(rectMask->duplicate());
    REQUIRE(rectMask4);

    basicPicture5 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture5);

    // Rle images
    basicPicture6 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture6);

    basicPicture7 = tvg::cast<Picture>(basicPicture->duplicate());
    REQUIRE(basicPicture7);
    rleMask7 = tvg::cast<Shape>(rleMask->duplicate());
    REQUIRE(rleMask7);

    // Rect
    REQUIRE(basicPicture->composite(move(rectMask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture)) == Result::Success);

    REQUIRE(basicPicture2->composite(move(rectMask2), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture2)) == Result::Success);

    REQUIRE(basicPicture3->composite(move(rectMask3), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture3)) == Result::Success);

    REQUIRE(basicPicture4->composite(move(rectMask4), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture4)) == Result::Success);

    REQUIRE(basicPicture5->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture5)) == Result::Success);

    // Rle
    REQUIRE(basicPicture6->composite(move(rleMask), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture6)) == Result::Success);

    REQUIRE(basicPicture7->composite(move(rleMask7), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(basicPicture7->opacity(100) == Result::Success);
    REQUIRE(canvas->push(move(basicPicture7)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);

    free(data);
}


TEST_CASE("Rect Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Basic shapes and masking
    auto basicShape = tvg::Shape::gen();
    REQUIRE(basicShape);
    auto basicMask = tvg::Shape::gen();
    REQUIRE(basicMask);
    REQUIRE(basicShape->appendRect(0, 0, 50, 50, 0, 0) == Result::Success);
    REQUIRE(basicMask->appendRect(10, 10, 30, 30, 0, 0) == Result::Success);
    REQUIRE(basicShape->fill(255, 255, 255, 155) == Result::Success);

    auto basicShape2 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape2);
    auto basicMask2 = tvg::cast<Shape>(basicMask->duplicate());
    REQUIRE(basicMask2);

    auto basicShape3 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape3);
    auto basicMask3 = tvg::cast<Shape>(basicMask->duplicate());
    REQUIRE(basicMask3);

    auto basicShape4 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape4);
    auto basicMask4 = tvg::cast<Shape>(basicMask->duplicate());
    REQUIRE(basicMask4);

    auto basicShape5 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape5);

    REQUIRE(basicShape->composite(move(basicMask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicShape)) == Result::Success);

    REQUIRE(basicShape2->composite(move(basicMask2), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicShape2)) == Result::Success);

    REQUIRE(basicShape3->composite(move(basicMask3), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicShape3)) == Result::Success);

    REQUIRE(basicShape4->composite(move(basicMask4), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicShape4)) == Result::Success);

    REQUIRE(canvas->push(move(basicShape5)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Basic shapes and masking
    auto basicShape = tvg::Shape::gen();
    REQUIRE(basicShape);
    auto basicMask = tvg::Shape::gen();
    REQUIRE(basicMask);
    REQUIRE(basicShape->appendRect(0, 0, 50, 50, 50, 50) == Result::Success);
    REQUIRE(basicMask->appendRect(10, 10, 30, 30, 0, 0) == Result::Success);
    REQUIRE(basicShape->fill(255, 255, 255, 100) == Result::Success);

    auto basicShape2 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape2);
    auto basicMask2 = tvg::cast<Shape>(basicMask->duplicate());
    REQUIRE(basicMask2);

    auto basicShape3 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape3);
    auto basicMask3 = tvg::cast<Shape>(basicMask->duplicate());
    REQUIRE(basicMask3);

    auto basicShape4 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape4);
    auto basicMask4 = tvg::cast<Shape>(basicMask->duplicate());
    REQUIRE(basicMask4);

    auto basicShape5 = tvg::cast<Shape>(basicShape->duplicate());
    REQUIRE(basicShape5);

    REQUIRE(basicShape->composite(move(basicMask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicShape)) == Result::Success);

    REQUIRE(basicShape2->composite(move(basicMask2), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicShape2)) == Result::Success);

    REQUIRE(basicShape3->composite(move(basicMask3), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(basicShape3)) == Result::Success);

    REQUIRE(basicShape4->composite(move(basicMask4), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(basicShape4)) == Result::Success);

    REQUIRE(canvas->push(move(basicShape5)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("Filling Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 0, 0) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 0, 0) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    REQUIRE(canvas->push(move(shape3)) == Result::Success);
    REQUIRE(canvas->push(move(shape4)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("Filling Opaque Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 255},
        {0.2f, 50, 25, 50, 255},
        {0.5f, 100, 100, 100, 255},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 0, 0) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 0, 0) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    REQUIRE(canvas->push(move(shape3)) == Result::Success);
    REQUIRE(canvas->push(move(shape4)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("Filling AlphaMask", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 0, 0) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 0, 0) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("Filling InvAlphaMask", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 0, 0) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 0, 0) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("Filling LumaMask", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 0, 0) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 0, 0) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("Filling ClipPath", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 0, 0) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 0, 0) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Filling Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 10, 10) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 10, 10) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    REQUIRE(canvas->push(move(shape3)) == Result::Success);
    REQUIRE(canvas->push(move(shape4)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Filling Opaque Draw", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 255},
        {0.2f, 50, 25, 50, 255},
        {0.5f, 100, 100, 100, 255},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 10, 10) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 10, 10) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    REQUIRE(canvas->push(move(shape3)) == Result::Success);
    REQUIRE(canvas->push(move(shape4)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Filling AlphaMask", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 10, 10) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 10, 10) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::AlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Filling InvAlphaMask", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 10, 10) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 10, 10) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::InvAlphaMask) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Filling LumaMask", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 10, 10) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 10, 10) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::LumaMask) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Filling InvLumaMask", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 10, 10) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 10, 10) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::InvLumaMask) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}


TEST_CASE("RLE Filling ClipPath", "[tvgSwEngine]")
{
    REQUIRE(Initializer::init(CanvasEngine::Sw, 0) == Result::Success);

    auto canvas = SwCanvas::gen();
    REQUIRE(canvas);

    uint32_t buffer[100*100];
    REQUIRE(canvas->target(buffer, 100, 100, 100, SwCanvas::Colorspace::ABGR8888) == Result::Success);

    //Fill
    auto linearFill = LinearGradient::gen();
    REQUIRE(linearFill);

    auto radialFill = RadialGradient::gen();
    REQUIRE(radialFill);

    Fill::ColorStop cs[4] = {
        {0.0f, 0, 0, 0, 0},
        {0.2f, 50, 25, 50, 25},
        {0.5f, 100, 100, 100, 125},
        {1.0f, 255, 255, 255, 255}
    };
    REQUIRE(linearFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(radialFill->colorStops(cs, 4) == Result::Success);
    REQUIRE(linearFill->linear(0.0f, 0.0f, 100.0f, 120.0f) == Result::Success);
    REQUIRE(radialFill->radial(50.0f, 50.0f, 50.0f) == Result::Success);

    //Mask
    auto mask = tvg::Shape::gen();
    REQUIRE(mask);
    REQUIRE(mask->appendCircle(50, 50, 50, 50) == Result::Success);

    //Filled Shapes
    auto shape3 = tvg::Shape::gen();
    REQUIRE(shape3);
    auto shape4 = tvg::Shape::gen();
    REQUIRE(shape4);
    REQUIRE(shape3->appendRect(0, 0, 50, 50, 10, 10) == Result::Success);
    REQUIRE(shape4->appendRect(50, 0, 50, 50, 10, 10) == Result::Success);

    REQUIRE(shape3->fill(move(linearFill)) == Result::Success);
    REQUIRE(shape4->fill(move(radialFill)) == Result::Success);

    //Scene
    auto scene = tvg::Scene::gen();
    REQUIRE(scene);
    REQUIRE(scene->push(move(shape3)) == Result::Success);
    REQUIRE(scene->push(move(shape4)) == Result::Success);
    REQUIRE(scene->composite(move(mask), tvg::CompositeMethod::ClipPath) == Result::Success);
    REQUIRE(canvas->push(move(scene)) == Result::Success);

    //Draw
    REQUIRE(canvas->draw() == Result::Success);
    REQUIRE(canvas->sync() == Result::Success);

    REQUIRE(Initializer::term(CanvasEngine::Sw) == Result::Success);
}

#endif