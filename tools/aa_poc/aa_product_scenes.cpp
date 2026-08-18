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

#include "aa_product_scenes.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace aa_poc
{
namespace
{

constexpr float KAPPA = 0.5522847498307936f;

struct Color
{
    uint8_t r, g, b, a;
};

struct PaintReleaser
{
    void operator()(tvg::Paint* paint) const { tvg::Paint::rel(paint); }
};

using ShapePtr = std::unique_ptr<tvg::Shape, PaintReleaser>;

bool check(tvg::Result result, const char* operation, const char* diagnosticName)
{
    if (result == tvg::Result::Success) return true;
    std::fprintf(stderr, "%s: %s failed (Result=%u)\n", diagnosticName, operation,
                 static_cast<unsigned>(result));
    return false;
}

struct Coordinates
{
    float scale;
    float offsetX;
    float offsetY;

    float x(float value) const { return value * scale + offsetX; }
    float y(float value) const { return value * scale + offsetY; }
};

class PathBuilder
{
public:
    PathBuilder(tvg::Shape& shape, const Coordinates& coordinates,
                const char* diagnosticName) :
        shape(shape), coordinates(coordinates), diagnosticName(diagnosticName)
    {
    }

    bool moveTo(float x, float y)
    {
        return check(shape.moveTo(coordinates.x(x), coordinates.y(y)),
                     "Shape::moveTo", diagnosticName);
    }

    bool lineTo(float x, float y)
    {
        return check(shape.lineTo(coordinates.x(x), coordinates.y(y)),
                     "Shape::lineTo", diagnosticName);
    }

    bool cubicTo(float x1, float y1, float x2, float y2, float x, float y)
    {
        return check(shape.cubicTo(coordinates.x(x1), coordinates.y(y1),
                                   coordinates.x(x2), coordinates.y(y2),
                                   coordinates.x(x), coordinates.y(y)),
                     "Shape::cubicTo", diagnosticName);
    }

    bool close()
    {
        return check(shape.close(), "Shape::close", diagnosticName);
    }

private:
    tvg::Shape& shape;
    const Coordinates& coordinates;
    const char* diagnosticName;
};

template<typename Builder>
bool addShape(tvg::GlCanvas& canvas, const Coordinates& coordinates,
              const Color& color, tvg::FillRule fillRule,
              const char* diagnosticName, const Builder& builder)
{
    ShapePtr shape(tvg::Shape::gen());
    if (!shape) {
        std::fprintf(stderr, "%s: Shape::gen() failed\n", diagnosticName);
        return false;
    }

    PathBuilder path(*shape, coordinates, diagnosticName);
    if (!builder(path) || !check(shape->fill(color.r, color.g, color.b, color.a), "Shape::fill", diagnosticName) || !check(shape->fillRule(fillRule), "Shape::fillRule", diagnosticName) || !check(canvas.add(shape.get()), "Canvas::add", diagnosticName)) {
        return false;
    }
    shape.release();
    return true;
}

bool addCircle(tvg::GlCanvas& canvas, const Coordinates& coordinates,
               float cx, float cy, float radius, const Color& color,
               const char* diagnosticName)
{
    auto k = radius * KAPPA;
    return addShape(canvas, coordinates, color, tvg::FillRule::NonZero,
                    diagnosticName, [=](PathBuilder& path) {
                        return path.moveTo(cx + radius, cy) && path.cubicTo(cx + radius, cy + k, cx + k, cy + radius, cx, cy + radius) && path.cubicTo(cx - k, cy + radius, cx - radius, cy + k, cx - radius, cy) && path.cubicTo(cx - radius, cy - k, cx - k, cy - radius, cx, cy - radius) && path.cubicTo(cx + k, cy - radius, cx + radius, cy - k, cx + radius, cy) && path.close();
                    });
}

bool populateFlatCore(tvg::GlCanvas& canvas, const Coordinates& coordinates,
                      const char* diagnosticName)
{
    if (!addShape(canvas, coordinates, {20, 107, 224, 255},
                  tvg::FillRule::NonZero, diagnosticName,
                  [](PathBuilder& path) {
                      return path.moveTo(18.375f, 30.125f) && path.lineTo(99.125f, 18.625f) && path.lineTo(106.625f, 60.375f) && path.lineTo(25.875f, 75.625f) && path.close();
                  })) return false;

    if (!addShape(canvas, coordinates, {224, 31, 41, 255},
                  tvg::FillRule::NonZero, diagnosticName,
                  [](PathBuilder& path) {
                      return path.moveTo(142.25f, 25.375f) && path.lineTo(231.75f, 37.625f) && path.lineTo(227.25f, 69.875f) && path.lineTo(160.5f, 62.375f) && path.lineTo(137.75f, 49.125f) && path.close();
                  })) return false;

    if (!addShape(canvas, coordinates, {20, 161, 94, 255},
                  tvg::FillRule::NonZero, diagnosticName,
                  [](PathBuilder& path) {
                      return path.moveTo(20.625f, 143.25f) && path.lineTo(59.875f, 116.75f) && path.lineTo(106.375f, 148.125f) && path.lineTo(83.125f, 171.875f) && path.lineTo(105.625f, 211.25f) && path.lineTo(61.125f, 225.5f) && path.lineTo(25.375f, 188.75f) && path.lineTo(47.625f, 166.0f) && path.close();
                  })) return false;

    return addShape(canvas, coordinates, {178, 51, 178, 255},
                    tvg::FillRule::NonZero, diagnosticName,
                    [](PathBuilder& path) {
                        return path.moveTo(145.25f, 132.625f) && path.lineTo(222.625f, 118.375f) && path.lineTo(236.125f, 165.25f) && path.lineTo(218.875f, 220.625f) && path.lineTo(158.375f, 229.125f) && path.lineTo(132.625f, 180.375f) && path.close();
                    });
}

bool populateCurveCore(tvg::GlCanvas& canvas, const Coordinates& coordinates,
                       const char* diagnosticName)
{
    if (!addCircle(canvas, coordinates, 63.375f, 61.625f, 38.25f,
                   {242, 122, 15, 255}, diagnosticName)) return false;

    if (!addShape(canvas, coordinates, {178, 51, 178, 255},
                  tvg::FillRule::NonZero, diagnosticName,
                  [](PathBuilder& path) {
                      return path.moveTo(184.0f, 19.0f) && path.cubicTo(216.0f, 18.0f, 237.0f, 39.0f, 233.0f, 65.0f) && path.cubicTo(230.0f, 91.0f, 208.0f, 108.0f, 180.0f, 102.0f) && path.cubicTo(150.0f, 108.0f, 127.0f, 86.0f, 132.0f, 59.0f) && path.cubicTo(135.0f, 31.0f, 157.0f, 20.0f, 184.0f, 19.0f) && path.close();
                  })) return false;

    if (!addShape(canvas, coordinates, {20, 161, 94, 255},
                  tvg::FillRule::NonZero, diagnosticName,
                  [](PathBuilder& path) {
                      return path.moveTo(20.0f, 166.0f) && path.cubicTo(45.0f, 144.0f, 84.0f, 139.0f, 112.0f, 159.0f) && path.cubicTo(116.0f, 181.0f, 89.0f, 211.0f, 55.0f, 216.0f) && path.cubicTo(31.0f, 216.0f, 17.0f, 193.0f, 20.0f, 166.0f) && path.close();
                  })) return false;

    return addShape(canvas, coordinates, {17, 128, 178, 255},
                    tvg::FillRule::NonZero, diagnosticName,
                    [](PathBuilder& path) {
                        return path.moveTo(143.0f, 141.0f) && path.lineTo(211.0f, 128.0f) && path.cubicTo(237.0f, 143.0f, 241.0f, 179.0f, 219.0f, 198.0f) && path.lineTo(173.0f, 229.0f) && path.cubicTo(145.0f, 218.0f, 130.0f, 179.0f, 143.0f, 141.0f) && path.close();
                    });
}

bool addFlatTile(tvg::GlCanvas& canvas, const Coordinates& coordinates,
                 float cx, float cy, float skew, const Color& color,
                 const char* diagnosticName)
{
    return addShape(canvas, coordinates, color, tvg::FillRule::NonZero,
                    diagnosticName, [=](PathBuilder& path) {
                        return path.moveTo(cx - 27.0f, cy - 13.0f + skew) && path.lineTo(cx + 20.0f, cy - 17.0f - skew) && path.lineTo(cx + 28.0f, cy + 9.0f) && path.lineTo(cx + 7.0f, cy + 17.0f + skew) && path.lineTo(cx - 25.0f, cy + 12.0f - skew) && path.close();
                    });
}

bool addCurvedTile(tvg::GlCanvas& canvas, const Coordinates& coordinates,
                   float cx, float cy, const Color& color,
                   const char* diagnosticName)
{
    return addShape(canvas, coordinates, color, tvg::FillRule::NonZero,
                    diagnosticName, [=](PathBuilder& path) {
                        return path.moveTo(cx - 28.0f, cy) && path.cubicTo(cx - 27.0f, cy - 13.0f, cx - 15.0f, cy - 18.0f, cx, cy - 16.0f) && path.cubicTo(cx + 17.0f, cy - 18.0f, cx + 28.0f, cy - 10.0f, cx + 27.0f, cy + 2.0f) && path.cubicTo(cx + 27.0f, cy + 13.0f, cx + 14.0f, cy + 18.0f, cx - 2.0f, cy + 16.0f) && path.cubicTo(cx - 17.0f, cy + 18.0f, cx - 29.0f, cy + 11.0f, cx - 28.0f, cy) && path.close();
                    });
}

bool populateMixedProductTile(tvg::GlCanvas& canvas,
                              const Coordinates& coordinates,
                              const char* diagnosticName)
{
    constexpr Color colors[] = {
        {20, 107, 224, 255},
        {242, 122, 15, 255},
        {178, 51, 178, 255},
        {20, 161, 94, 255},
        {17, 128, 178, 255},
        {218, 56, 63, 255},
    };
    constexpr float centersX[] = {43.375f, 128.125f, 212.875f};
    constexpr float centersY[] = {31.625f, 96.125f, 160.875f, 225.375f};

    uint32_t index = 0;
    for (auto cy : centersY) {
        for (auto cx : centersX) {
            auto success = (index % 2 == 0) ? addFlatTile(canvas, coordinates, cx, cy,
                                                          static_cast<float>(static_cast<int32_t>(index % 3) - 1) * 1.5f,
                                                          colors[index % 6], diagnosticName)
                                            : addCurvedTile(canvas, coordinates, cx, cy,
                                                            colors[index % 6], diagnosticName);
            if (!success) return false;
            ++index;
        }
    }
    return true;
}

bool populateTransparencyCore(tvg::GlCanvas& canvas,
                              const Coordinates& coordinates,
                              const char* diagnosticName)
{
    // Both contours belong to one translucent paint. EvenOdd removes the inner
    // contour without ever blending two independent translucent boundaries.
    return addShape(canvas, coordinates, {115, 46, 199, 128},
                    tvg::FillRule::EvenOdd, diagnosticName,
                    [](PathBuilder& path) {
                        return path.moveTo(128.375f, 25.625f) && path.cubicTo(188.625f, 20.375f, 232.125f, 58.875f, 226.625f, 118.375f) && path.cubicTo(233.375f, 177.625f, 192.875f, 225.125f, 132.625f, 229.375f) && path.cubicTo(73.125f, 235.625f, 27.875f, 195.375f, 30.625f, 134.125f) && path.cubicTo(23.875f, 74.625f, 67.375f, 29.125f, 128.375f, 25.625f) && path.close() && path.moveTo(169.625f, 128.375f) && path.cubicTo(169.625f, 150.125f, 151.25f, 166.625f, 128.125f, 165.875f) && path.cubicTo(104.875f, 166.625f, 87.625f, 150.0f, 88.375f, 127.625f) && path.cubicTo(87.625f, 105.875f, 105.375f, 89.625f, 128.875f, 90.375f) && path.cubicTo(151.875f, 89.625f, 170.375f, 106.125f, 169.625f, 128.375f) && path.close();
                    });
}

}  // namespace

const char* sceneName(SceneKind scene)
{
    switch (scene) {
        case SceneKind::FlatCore: return "flat-core";
        case SceneKind::CurveCore: return "curve-core";
        case SceneKind::MixedProductTile: return "mixed-product-tile";
        case SceneKind::TransparencyCore: return "transparency-core";
    }
    return "unknown";
}

bool parseScene(const char* name, SceneKind& scene)
{
    if (!name) return false;
    if (std::strcmp(name, "flat-core") == 0) scene = SceneKind::FlatCore;
    else if (std::strcmp(name, "curve-core") == 0) scene = SceneKind::CurveCore;
    else if (std::strcmp(name, "mixed-product-tile") == 0) scene = SceneKind::MixedProductTile;
    else if (std::strcmp(name, "transparency-core") == 0) scene = SceneKind::TransparencyCore;
    else return false;
    return true;
}

uint32_t expectedShapeCount(SceneKind scene)
{
    switch (scene) {
        case SceneKind::FlatCore: return 4;
        case SceneKind::CurveCore: return 4;
        case SceneKind::MixedProductTile: return 12;
        case SceneKind::TransparencyCore: return 1;
    }
    return 0;
}

bool populateProductScene(tvg::GlCanvas& canvas, SceneKind scene,
                          float coordinateScale, float offsetX, float offsetY,
                          const char* diagnosticName)
{
    if (!std::isfinite(coordinateScale) || coordinateScale <= 0.0f || !std::isfinite(offsetX) || !std::isfinite(offsetY)) {
        std::fprintf(stderr, "%s: product scene scale must be positive and all coordinates finite\n",
                     diagnosticName);
        return false;
    }

    Coordinates coordinates{coordinateScale, offsetX, offsetY};
    switch (scene) {
        case SceneKind::FlatCore:
            return populateFlatCore(canvas, coordinates, diagnosticName);
        case SceneKind::CurveCore:
            return populateCurveCore(canvas, coordinates, diagnosticName);
        case SceneKind::MixedProductTile:
            return populateMixedProductTile(canvas, coordinates, diagnosticName);
        case SceneKind::TransparencyCore:
            return populateTransparencyCore(canvas, coordinates, diagnosticName);
    }
    return false;
}

}  // namespace aa_poc
