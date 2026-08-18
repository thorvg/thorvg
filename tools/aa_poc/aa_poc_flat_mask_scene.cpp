/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "aa_poc_flat_mask_scene.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

namespace aa_poc
{
namespace
{

struct ByteColor
{
    uint8_t r, g, b, a;
};

struct PaintReleaser
{
    void operator()(tvg::Paint* paint) const
    {
        tvg::Paint::rel(paint);
    }
};

using ShapePtr = std::unique_ptr<tvg::Shape, PaintReleaser>;

tvg::Matrix makeTransform(float x, float y, float degrees, float scale)
{
    constexpr float DEG_TO_RAD = 0.01745329251994329577f;
    auto radians = degrees * DEG_TO_RAD;
    auto cosine = std::cos(radians) * scale;
    auto sine = std::sin(radians) * scale;
    return {cosine, -sine, x * scale,
            sine, cosine, y * scale,
            0.0f, 0.0f, 1.0f};
}

bool submitShape(tvg::GlCanvas& canvas, ShapePtr shape, const ByteColor& color,
                 uint8_t opacity, tvg::FillRule fillRule, const tvg::Matrix& transform,
                 const char* diagnosticName)
{
    if (!shape) {
        std::fprintf(stderr, "%s: Shape::gen() failed\n", diagnosticName);
        return false;
    }
    if (!checkThorvg(shape->fill(color.r, color.g, color.b, color.a),
                     "Shape::fill", diagnosticName)) return false;
    if (!checkThorvg(shape->fillRule(fillRule), "Shape::fillRule", diagnosticName)) return false;
    if (!checkThorvg(shape->opacity(opacity), "Paint::opacity", diagnosticName)) return false;
    if (!checkThorvg(shape->transform(transform), "Paint::transform", diagnosticName)) return false;
    if (!checkThorvg(canvas.add(shape.get()), "Canvas::add", diagnosticName)) return false;
    shape.release();
    return true;
}

} // namespace

bool checkThorvg(tvg::Result result, const char* operation, const char* diagnosticName)
{
    if (result == tvg::Result::Success) return true;
    std::fprintf(stderr, "%s: %s failed (Result=%u)\n", diagnosticName, operation,
                 static_cast<unsigned>(result));
    return false;
}

bool populateFlatMaskScene(tvg::GlCanvas& canvas, float offsetX, float offsetY,
                           float scale, const char* diagnosticName)
{
    auto transform = [=](float x, float y, float degrees = 0.0f) {
        return makeTransform(x + offsetX, y + offsetY, degrees, scale);
    };

    // Transformed rectangle. Its left edge crosses the canvas viewport, making
    // clipping part of every candidate/reference render.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape || !checkThorvg(shape->appendRect(-45.0f, -23.0f, 90.0f, 46.0f),
                                   "Shape::appendRect", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {20, 107, 224, 255}, 255,
                         tvg::FillRule::NonZero, transform(28.0f, 72.0f, 18.0f),
                         diagnosticName)) return false;
    }

    // Four cubic Beziers rather than appendCircle keep curve flattening visible
    // in the POC input path.
    {
        constexpr float RADIUS = 31.0f;
        constexpr float KAPPA = 0.5522847498307936f;
        constexpr float K = RADIUS * KAPPA;
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkThorvg(shape->moveTo(RADIUS, 0.0f), "Shape::moveTo", diagnosticName) ||
            !checkThorvg(shape->cubicTo(RADIUS, K, K, RADIUS, 0.0f, RADIUS),
                         "Shape::cubicTo", diagnosticName) ||
            !checkThorvg(shape->cubicTo(-K, RADIUS, -RADIUS, K, -RADIUS, 0.0f),
                         "Shape::cubicTo", diagnosticName) ||
            !checkThorvg(shape->cubicTo(-RADIUS, -K, -K, -RADIUS, 0.0f, -RADIUS),
                         "Shape::cubicTo", diagnosticName) ||
            !checkThorvg(shape->cubicTo(K, -RADIUS, RADIUS, -K, RADIUS, 0.0f),
                         "Shape::cubicTo", diagnosticName) ||
            !checkThorvg(shape->close(), "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {242, 122, 15, 255}, 255,
                         tvg::FillRule::NonZero, transform(145.0f, 72.0f),
                         diagnosticName)) return false;
    }

    // Closed, inflected cubic contour.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkThorvg(shape->moveTo(-48.0f, -22.0f), "Shape::moveTo", diagnosticName) ||
            !checkThorvg(shape->cubicTo(-15.0f, -65.0f, 15.0f, 25.0f, 48.0f, -22.0f),
                         "Shape::cubicTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(48.0f, 22.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->cubicTo(15.0f, 65.0f, -15.0f, -25.0f, -48.0f, 22.0f),
                         "Shape::cubicTo", diagnosticName) ||
            !checkThorvg(shape->close(), "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {178, 51, 178, 255}, 255,
                         tvg::FillRule::NonZero, transform(272.0f, 72.0f, -4.0f),
                         diagnosticName)) return false;
    }

    // Simple polygon control case.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkThorvg(shape->moveTo(0.0f, -37.0f), "Shape::moveTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(42.0f, 32.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(-42.0f, 32.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->close(), "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {224, 31, 41, 255}, 255,
                         tvg::FillRule::NonZero, transform(402.0f, 72.0f, 7.0f),
                         diagnosticName)) return false;
    }

    // Two partly overlapping paints separate fill alpha from effective Paint
    // opacity. Cross-paint composition remains intentional; each paint must
    // still apply its own color and opacity only once.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape || !checkThorvg(shape->appendRect(-44.0f, -25.0f, 88.0f, 50.0f),
                                   "Shape::appendRect", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {115, 46, 199, 128}, 255,
                         tvg::FillRule::NonZero, transform(548.0f, 69.0f, -12.0f),
                         diagnosticName)) return false;
    }
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape || !checkThorvg(shape->appendRect(-31.0f, -18.0f, 62.0f, 36.0f),
                                   "Shape::appendRect", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {12, 151, 102, 255}, 128,
                         tvg::FillRule::NonZero, transform(572.0f, 84.0f, 9.0f),
                         diagnosticName)) return false;
    }

    // Filled open contour. ThorVG's normal geometry preparation supplies the
    // implicit closing edge; the POC intentionally does not call close().
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkThorvg(shape->moveTo(-43.0f, -29.0f), "Shape::moveTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(37.0f, -24.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->cubicTo(53.0f, -5.0f, 24.0f, 33.0f, -4.0f, 26.0f),
                         "Shape::cubicTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(-34.0f, 32.0f), "Shape::lineTo", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {20, 161, 94, 255}, 255,
                         tvg::FillRule::NonZero, transform(72.0f, 220.0f, -5.0f),
                         diagnosticName)) return false;
    }

    // Multi-contour NonZero hole: inner contour runs opposite the outer one.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkThorvg(shape->appendRect(-48.0f, -37.0f, 96.0f, 74.0f,
                                           0.0f, 0.0f, true),
                         "Shape::appendRect", diagnosticName) ||
            !checkThorvg(shape->appendRect(-22.0f, -16.0f, 44.0f, 32.0f,
                                           0.0f, 0.0f, false),
                         "Shape::appendRect", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {17, 128, 178, 255}, 255,
                         tvg::FillRule::NonZero, transform(215.0f, 220.0f, 4.0f),
                         diagnosticName)) return false;
    }

    // Multi-contour EvenOdd hole: contour direction intentionally matches.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkThorvg(shape->appendRect(-48.0f, -37.0f, 96.0f, 74.0f,
                                           0.0f, 0.0f, true),
                         "Shape::appendRect", diagnosticName) ||
            !checkThorvg(shape->appendRect(-22.0f, -16.0f, 44.0f, 32.0f,
                                           0.0f, 0.0f, true),
                         "Shape::appendRect", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {232, 146, 12, 255}, 255,
                         tvg::FillRule::EvenOdd, transform(365.0f, 220.0f, -4.0f),
                         diagnosticName)) return false;
    }

    // Five-point self-intersection exercises stencil fill classification.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkThorvg(shape->moveTo(0.0f, -43.0f), "Shape::moveTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(25.0f, 35.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(-41.0f, -13.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(41.0f, -13.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->lineTo(-25.0f, 35.0f), "Shape::lineTo", diagnosticName) ||
            !checkThorvg(shape->close(), "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {218, 56, 63, 255}, 255,
                         tvg::FillRule::NonZero, transform(520.0f, 220.0f, 2.0f),
                         diagnosticName)) return false;
    }

    return true;
}

} // namespace aa_poc
