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

#include "aa_poc_comparison_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace aa_poc
{
namespace
{

constexpr float KAPPA = 0.5522847498307936f;

float channel(uint8_t value)
{
    return value / 255.0f;
}

void appendCircle(tvg::RenderPath& path, float cx, float cy, float radius)
{
    auto k = radius * KAPPA;
    path.moveTo({cx + radius, cy});
    path.cubicTo({cx + radius, cy + k}, {cx + k, cy + radius}, {cx, cy + radius});
    path.cubicTo({cx - k, cy + radius}, {cx - radius, cy + k}, {cx - radius, cy});
    path.cubicTo({cx - radius, cy - k}, {cx - k, cy - radius}, {cx, cy - radius});
    path.cubicTo({cx + k, cy - radius}, {cx + radius, cy - k}, {cx + radius, cy});
    path.close();
}

void appendRotatedRect(tvg::RenderPath& path, float cx, float cy,
                       float width, float height, float degrees)
{
    constexpr float DEG_TO_RAD = 0.01745329251994329577f;
    auto radians = degrees * DEG_TO_RAD;
    auto cosine = std::cos(radians);
    auto sine = std::sin(radians);
    auto transform = [&](float x, float y) {
        return tvg::Point{cx + x * cosine - y * sine,
                          cy + x * sine + y * cosine};
    };
    auto halfWidth = width * 0.5f;
    auto halfHeight = height * 0.5f;
    path.moveTo(transform(-halfWidth, -halfHeight));
    path.lineTo(transform(halfWidth, -halfHeight));
    path.lineTo(transform(halfWidth, halfHeight));
    path.lineTo(transform(-halfWidth, halfHeight));
    path.close();
}

ComparisonShape shape(tvg::RenderPath&& path, uint8_t r, uint8_t g,
                      uint8_t b, uint8_t a = 255)
{
    return {std::move(path), channel(r), channel(g), channel(b), channel(a)};
}

uint8_t byteChannel(float value)
{
    return static_cast<uint8_t>(std::lround(
        std::max(0.0f, std::min(1.0f, value)) * 255.0f));
}

bool check(tvg::Result result, const char* operation, const char* diagnosticName)
{
    if (result == tvg::Result::Success) return true;
    std::fprintf(stderr, "%s: %s failed (Result=%u)\n", diagnosticName, operation,
                 static_cast<unsigned>(result));
    return false;
}

struct PaintReleaser
{
    void operator()(tvg::Paint* paint) const { tvg::Paint::rel(paint); }
};

} // namespace

bool takeComparisonOption(int& argc, char** argv)
{
    auto comparison = false;
    for (int read = 1; read < argc;) {
        if (std::strcmp(argv[read], "--comparison") != 0) {
            ++read;
            continue;
        }
        comparison = true;
        for (int write = read; write + 1 < argc; ++write) argv[write] = argv[write + 1];
        --argc;
    }
    return comparison;
}

std::vector<ComparisonShape> makeComparisonScene(float offsetX, float offsetY,
                                                  float scale)
{
    auto point = [=](float x, float y) {
        return tvg::Point{(x + offsetX) * scale, (y + offsetY) * scale};
    };
    auto x = [=](float value) { return (value + offsetX) * scale; };
    auto y = [=](float value) { return (value + offsetY) * scale; };

    std::vector<ComparisonShape> scene;
    scene.reserve(8);

    // Long slanted line edges make one-pixel coverage ramps easy to inspect.
    {
        tvg::RenderPath path;
        appendRotatedRect(path, x(102.0f), y(105.0f),
                          148.0f * scale, 68.0f * scale, 17.0f);
        scene.push_back(shape(std::move(path), 20, 107, 224));
    }

    // Acute and obtuse joins reveal corner widening or clipping.
    {
        tvg::RenderPath path;
        path.moveTo(point(290.0f, 35.0f));
        path.lineTo(point(364.0f, 173.0f));
        path.lineTo(point(216.0f, 173.0f));
        path.close();
        scene.push_back(shape(std::move(path), 224, 31, 41));
    }

    // Four non-degenerate cubics provide a smooth, familiar silhouette.
    {
        tvg::RenderPath path;
        appendCircle(path, x(490.0f), y(105.0f), 72.0f * scale);
        scene.push_back(shape(std::move(path), 242, 122, 15));
    }

    // A smooth asymmetric cubic contour exercises changing curvature.
    {
        tvg::RenderPath path;
        path.moveTo(point(625.0f, 126.0f));
        path.cubicTo(point(612.0f, 45.0f), point(684.0f, 23.0f), point(731.0f, 72.0f));
        path.cubicTo(point(773.0f, 116.0f), point(733.0f, 183.0f), point(672.0f, 174.0f));
        path.cubicTo(point(642.0f, 170.0f), point(629.0f, 151.0f), point(625.0f, 126.0f));
        path.close();
        scene.push_back(shape(std::move(path), 178, 51, 178));
    }

    // A broad, shallow bar makes subpixel edge movement conspicuous without
    // entering any candidate's thin-fill fallback.
    {
        tvg::RenderPath path;
        appendRotatedRect(path, x(112.0f), y(342.0f),
                          174.0f * scale, 18.0f * scale, -11.0f);
        scene.push_back(shape(std::move(path), 20, 161, 94));
    }

    // Mixed straight/cubic boundary checks agreement at unlike endpoints.
    {
        tvg::RenderPath path;
        path.moveTo(point(215.0f, 390.0f));
        path.lineTo(point(235.0f, 274.0f));
        path.cubicTo(point(279.0f, 235.0f), point(356.0f, 273.0f), point(363.0f, 338.0f));
        path.cubicTo(point(368.0f, 393.0f), point(302.0f, 427.0f), point(215.0f, 390.0f));
        path.close();
        scene.push_back(shape(std::move(path), 17, 128, 178));
    }

    // Consecutive cubics expose join seams while remaining a simple contour.
    {
        tvg::RenderPath path;
        path.moveTo(point(435.0f, 355.0f));
        path.cubicTo(point(429.0f, 278.0f), point(482.0f, 247.0f), point(520.0f, 303.0f));
        path.cubicTo(point(559.0f, 251.0f), point(616.0f, 287.0f), point(605.0f, 355.0f));
        path.cubicTo(point(594.0f, 418.0f), point(526.0f, 435.0f), point(493.0f, 390.0f));
        path.cubicTo(point(468.0f, 422.0f), point(438.0f, 402.0f), point(435.0f, 355.0f));
        path.close();
        scene.push_back(shape(std::move(path), 12, 151, 102));
    }

    // Translucency makes accidental boundary overdraw visible as dark seams.
    {
        tvg::RenderPath path;
        path.moveTo(point(652.0f, 367.0f));
        path.cubicTo(point(643.0f, 293.0f), point(696.0f, 257.0f), point(738.0f, 306.0f));
        path.cubicTo(point(780.0f, 354.0f), point(746.0f, 430.0f), point(681.0f, 414.0f));
        path.cubicTo(point(663.0f, 408.0f), point(654.0f, 388.0f), point(652.0f, 367.0f));
        path.close();
        scene.push_back(shape(std::move(path), 115, 46, 199, 128));
    }

    return scene;
}

bool populateComparisonScene(tvg::Canvas& canvas, float offsetX, float offsetY,
                             float scale, const char* diagnosticName)
{
    auto scene = makeComparisonScene(offsetX, offsetY, scale);
    for (const auto& item : scene) {
        std::unique_ptr<tvg::Shape, PaintReleaser> paint(tvg::Shape::gen());
        if (!paint) {
            std::fprintf(stderr, "%s: Shape::gen() failed\n", diagnosticName);
            return false;
        }

        auto points = item.path.pts.data;
        ARRAY_FOREACH(command, item.path.cmds) {
            switch (*command) {
                case tvg::PathCommand::MoveTo:
                    if (!check(paint->moveTo(points->x, points->y),
                               "Shape::moveTo", diagnosticName)) return false;
                    ++points;
                    break;
                case tvg::PathCommand::LineTo:
                    if (!check(paint->lineTo(points->x, points->y),
                               "Shape::lineTo", diagnosticName)) return false;
                    ++points;
                    break;
                case tvg::PathCommand::CubicTo:
                    if (!check(paint->cubicTo(points[0].x, points[0].y,
                                              points[1].x, points[1].y,
                                              points[2].x, points[2].y),
                               "Shape::cubicTo", diagnosticName)) return false;
                    points += 3;
                    break;
                case tvg::PathCommand::Close:
                    if (!check(paint->close(), "Shape::close", diagnosticName)) return false;
                    break;
            }
        }

        if (!check(paint->fill(byteChannel(item.r), byteChannel(item.g),
                               byteChannel(item.b), byteChannel(item.a)),
                   "Shape::fill", diagnosticName) ||
            !check(canvas.add(paint.get()), "Canvas::add", diagnosticName)) return false;
        paint.release();
    }
    return true;
}

} // namespace aa_poc
