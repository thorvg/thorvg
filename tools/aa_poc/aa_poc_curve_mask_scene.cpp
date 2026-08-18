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

#include "aa_poc_curve_mask_scene.h"

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
    if (!checkCurveMaskThorvg(shape->fill(color.r, color.g, color.b, color.a),
                              "Shape::fill", diagnosticName)) return false;
    if (!checkCurveMaskThorvg(shape->fillRule(fillRule),
                              "Shape::fillRule", diagnosticName)) return false;
    if (!checkCurveMaskThorvg(shape->opacity(opacity),
                              "Paint::opacity", diagnosticName)) return false;
    if (!checkCurveMaskThorvg(shape->transform(transform),
                              "Paint::transform", diagnosticName)) return false;
    if (!checkCurveMaskThorvg(canvas.add(shape.get()),
                              "Canvas::add", diagnosticName)) return false;
    shape.release();
    return true;
}

bool appendCircle(tvg::Shape& shape, float radius, bool reverse,
                  const char* diagnosticName)
{
    constexpr float KAPPA = 0.5522847498307936f;
    auto k = radius * KAPPA;
    if (!checkCurveMaskThorvg(shape.moveTo(radius, 0.0f),
                              "Shape::moveTo", diagnosticName)) return false;
    if (reverse) {
        if (!checkCurveMaskThorvg(shape.cubicTo(radius, -k, k, -radius, 0.0f, -radius),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape.cubicTo(-k, -radius, -radius, -k, -radius, 0.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape.cubicTo(-radius, k, -k, radius, 0.0f, radius),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape.cubicTo(k, radius, radius, k, radius, 0.0f),
                                  "Shape::cubicTo", diagnosticName)) return false;
    } else {
        if (!checkCurveMaskThorvg(shape.cubicTo(radius, k, k, radius, 0.0f, radius),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape.cubicTo(-k, radius, -radius, k, -radius, 0.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape.cubicTo(-radius, -k, -k, -radius, 0.0f, -radius),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape.cubicTo(k, -radius, radius, -k, radius, 0.0f),
                                  "Shape::cubicTo", diagnosticName)) return false;
    }
    return checkCurveMaskThorvg(shape.close(), "Shape::close", diagnosticName);
}

} // namespace

bool checkCurveMaskThorvg(tvg::Result result, const char* operation,
                          const char* diagnosticName)
{
    if (result == tvg::Result::Success) return true;
    std::fprintf(stderr, "%s: %s failed (Result=%u)\n", diagnosticName, operation,
                 static_cast<unsigned>(result));
    return false;
}

bool populateCurveMaskScene(tvg::GlCanvas& canvas, float offsetX, float offsetY,
                            float scale, const char* diagnosticName)
{
    auto transform = [=](float x, float y, float degrees = 0.0f) {
        return makeTransform(x + offsetX, y + offsetY, degrees, scale);
    };

    // Circle made from four original cubics.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape || !appendCircle(*shape, 58.0f, false, diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {20, 107, 224, 255}, 255,
                         tvg::FillRule::NonZero, transform(90.0f, 105.0f),
                         diagnosticName)) return false;
    }

    // Tight, high-curvature arch with a shallow curved return.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkCurveMaskThorvg(shape->moveTo(-65.0f, 43.0f),
                                  "Shape::moveTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-96.0f, -88.0f, 96.0f, -88.0f,
                                                 65.0f, 43.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(43.0f, 73.0f, -43.0f, 73.0f,
                                                 -65.0f, 43.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->close(),
                                  "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {242, 122, 15, 255}, 255,
                         tvg::FillRule::NonZero, transform(280.0f, 105.0f),
                         diagnosticName)) return false;
    }

    // Closed contour with two inflected S-shaped cubic sides.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkCurveMaskThorvg(shape->moveTo(-62.0f, -25.0f),
                                  "Shape::moveTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-28.0f, -82.0f, 25.0f, 56.0f,
                                                 62.0f, -18.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->lineTo(62.0f, 20.0f),
                                  "Shape::lineTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(25.0f, 82.0f, -28.0f, -56.0f,
                                                 -62.0f, 25.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->close(),
                                  "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {178, 51, 178, 255}, 255,
                         tvg::FillRule::NonZero, transform(500.0f, 105.0f, -3.0f),
                         diagnosticName)) return false;
    }

    // Narrow filled bar isolates the analytical LineTo path.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkCurveMaskThorvg(shape->moveTo(-70.0f, -8.0f),
                                  "Shape::moveTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->lineTo(68.0f, -19.0f),
                                  "Shape::lineTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->lineTo(70.0f, 8.0f),
                                  "Shape::lineTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->lineTo(-68.0f, 19.0f),
                                  "Shape::lineTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->close(),
                                  "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {25, 140, 173, 255}, 255,
                         tvg::FillRule::NonZero, transform(700.0f, 105.0f),
                         diagnosticName)) return false;
    }

    // Four connected cubics expose seams and overlapping conservative patches.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkCurveMaskThorvg(shape->moveTo(-64.0f, 8.0f),
                                  "Shape::moveTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-68.0f, -64.0f, -12.0f, -72.0f,
                                                 3.0f, -22.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(20.0f, -68.0f, 75.0f, -34.0f,
                                                 61.0f, 22.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(48.0f, 72.0f, -12.0f, 75.0f,
                                                 -31.0f, 31.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-53.0f, 68.0f, -69.0f, 52.0f,
                                                 -64.0f, 8.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->close(),
                                  "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {20, 161, 94, 255}, 255,
                         tvg::FillRule::NonZero, transform(90.0f, 315.0f),
                         diagnosticName)) return false;
    }

    // Curves meeting at an acute bottom join.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkCurveMaskThorvg(shape->moveTo(0.0f, 70.0f),
                                  "Shape::moveTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-14.0f, 36.0f, -72.0f, 13.0f,
                                                 -61.0f, -34.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-50.0f, -74.0f, -10.0f, -57.0f,
                                                 0.0f, -20.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(10.0f, -57.0f, 50.0f, -74.0f,
                                                 61.0f, -34.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(72.0f, 13.0f, 14.0f, 36.0f,
                                                 0.0f, 70.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->close(),
                                  "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {224, 31, 41, 255}, 255,
                         tvg::FillRule::NonZero, transform(280.0f, 315.0f),
                         diagnosticName)) return false;
    }

    // Curved crescent with a pronounced concave boundary.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkCurveMaskThorvg(shape->moveTo(-62.0f, -48.0f),
                                  "Shape::moveTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-5.0f, -75.0f, 60.0f, -60.0f,
                                                 68.0f, -12.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(76.0f, 35.0f, 28.0f, 68.0f,
                                                 -62.0f, 50.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-25.0f, 36.0f, -4.0f, 22.0f,
                                                 -2.0f, 9.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(0.0f, -8.0f, -25.0f, -30.0f,
                                                 -62.0f, -48.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->close(),
                                  "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {17, 128, 178, 255}, 255,
                         tvg::FillRule::NonZero, transform(500.0f, 315.0f),
                         diagnosticName)) return false;
    }

    // Paint opacity, rather than patch opacity, must be applied exactly once.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !checkCurveMaskThorvg(shape->moveTo(-66.0f, 8.0f),
                                  "Shape::moveTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-70.0f, -59.0f, -15.0f, -73.0f,
                                                 8.0f, -25.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(31.0f, -70.0f, 76.0f, -29.0f,
                                                 62.0f, 28.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(48.0f, 74.0f, -14.0f, 72.0f,
                                                 -35.0f, 29.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->cubicTo(-56.0f, 66.0f, -68.0f, 48.0f,
                                                 -66.0f, 8.0f),
                                  "Shape::cubicTo", diagnosticName) ||
            !checkCurveMaskThorvg(shape->close(),
                                  "Shape::close", diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {115, 46, 199, 255}, 128,
                         tvg::FillRule::NonZero, transform(700.0f, 315.0f),
                         diagnosticName)) return false;
    }

    // Curved NonZero hole: the inner cubic contour reverses winding.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !appendCircle(*shape, 66.0f, false, diagnosticName) ||
            !appendCircle(*shape, 31.0f, true, diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {18, 137, 92, 255}, 255,
                         tvg::FillRule::NonZero, transform(280.0f, 525.0f, 4.0f),
                         diagnosticName)) return false;
    }

    // Curved EvenOdd hole: both cubic contours deliberately share winding.
    {
        ShapePtr shape(tvg::Shape::gen());
        if (!shape ||
            !appendCircle(*shape, 66.0f, false, diagnosticName) ||
            !appendCircle(*shape, 31.0f, false, diagnosticName)) return false;
        if (!submitShape(canvas, std::move(shape), {232, 146, 12, 255}, 255,
                         tvg::FillRule::EvenOdd, transform(520.0f, 525.0f, -4.0f),
                         diagnosticName)) return false;
    }

    return true;
}

} // namespace aa_poc
