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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "tvgCoverage.h"

using namespace tvg;

namespace
{

constexpr uint32_t Width = 160, Height = 128;
constexpr RenderRegion Viewport = {{0, 0}, {Width, Height}};

CoverageStatus fillCoverage(const RenderPath& path, FillRule rule, const Matrix& transform, const RenderRegion& viewport, CoverageMap& map)
{
    RenderShape shape;
    shape.path = path;
    shape.rule = rule;
    return generateFillCoverage(shape, transform, viewport, map);
}

void rectangle(RenderPath& path, float x, float y, float w, float h, bool reverse = false)
{
    path.moveTo({x, y});
    path.lineTo({reverse ? x : x + w, reverse ? y + h : y});
    path.lineTo({x + w, y + h});
    path.lineTo({reverse ? x + w : x, reverse ? y : y + h});
    path.close();
}

RenderPath notch()
{
    RenderPath path;
    path.moveTo({20.25f, 18.5f});
    path.lineTo({132.6f, 18.5f});
    path.lineTo({132.6f, 110.2f});
    path.lineTo({20.25f, 110.2f});
    path.lineTo({20.25f, 83.4f});
    path.cubicTo({71.0f, 82.4f}, {82.0f, 50.0f}, {20.25f, 43.0f});
    path.close();
    return path;
}

RenderPath star()
{
    RenderPath path;
    path.moveTo({80.0f, 14.0f});
    path.lineTo({120.0f, 112.0f});
    path.lineTo({18.0f, 50.0f});
    path.lineTo({142.0f, 50.0f});
    path.lineTo({40.0f, 112.0f});
    path.close();
    return path;
}

uint8_t pixel(const CoverageMap& map, int32_t x, int32_t y)
{
    if (!map.data || x < map.bounds.min.x || y < map.bounds.min.y || x >= map.bounds.max.x || y >= map.bounds.max.y) return 0;
    return map.data[(y - map.bounds.min.y) * map.stride + x - map.bounds.min.x];
}

bool cleared(const CoverageMap& map)
{
    return !map.data && map.stride == 0 && map.bounds.min.x == 0 && map.bounds.min.y == 0 && map.bounds.max.x == 0 && map.bounds.max.y == 0;
}

bool same(const CoverageMap& a, const CoverageMap& b)
{
    if (a.bounds.min.x != b.bounds.min.x || a.bounds.min.y != b.bounds.min.y ||
        a.bounds.max.x != b.bounds.max.x || a.bounds.max.y != b.bounds.max.y || a.stride != b.stride) return false;
    if (!a.data || !b.data) return cleared(a) && cleared(b);
    return std::equal(a.data, a.data + a.stride * a.bounds.h(), b.data);
}

struct Validation
{
    unsigned checks = 0, failures = 0, comparisons = 0;
    std::string dumpDir;
    std::vector<uint8_t> frame;

    bool check(bool value, const std::string& message)
    {
        ++checks;
        if (!value) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
        return value;
    }

    bool reference(const RenderPath& path, FillRule rule, const Matrix& transform, const RenderRegion& viewport, std::vector<uint8_t>& alpha,
                   const RenderStroke* stroke = nullptr, bool strokeOnly = false)
    {
        std::vector<uint32_t> buffer(Width * Height, 0);
        auto canvas = std::unique_ptr<SwCanvas>(SwCanvas::gen(EngineOption::None));
        if (!check(bool(canvas), "SW canvas creation")) return false;
        if (!check(canvas->target(buffer.data(), Width, Width, Height, ColorSpace::ARGB8888) == Result::Success, "SW target")) return false;
        if (!check(canvas->viewport(viewport.min.x, viewport.min.y, viewport.w(), viewport.h()) == Result::Success, "SW viewport")) return false;
        if (!path.empty()) {
            auto shape = Shape::gen();
            if (!check(shape->appendPath(path.cmds.data, path.cmds.count, path.pts.data, path.pts.count) == Result::Success, "SW path")) {
                delete shape;
                return false;
            }
            // An empty contour disables the snapped rectangle shortcut without changing fill coverage.
            if (!strokeOnly) shape->moveTo(path.pts.first().x, path.pts.first().y);
            shape->fill(255, 255, 255, strokeOnly ? 0 : 255);
            shape->fillRule(rule);
            if (stroke) {
                shape->trimpath(stroke->trim.begin, stroke->trim.end, stroke->trim.simultaneous);
                if (strokeOnly) {
                    shape->strokeFill(255, 255, 255);
                    shape->strokeWidth(stroke->width);
                    shape->strokeCap(stroke->cap);
                    shape->strokeJoin(stroke->join);
                    shape->strokeMiterlimit(stroke->miterlimit);
                    shape->strokeDash(stroke->dash.pattern, stroke->dash.count, stroke->dash.offset);
                }
            }
            shape->transform(transform);
            if (!check(canvas->add(shape) == Result::Success, "SW add")) return false;
            if (!check(canvas->draw(true) == Result::Success && canvas->sync() == Result::Success, "SW draw/sync")) return false;
        }
        alpha.resize(buffer.size());
        std::transform(buffer.begin(), buffer.end(), alpha.begin(), [](uint32_t value) { return uint8_t(value >> 24); });
        return true;
    }

    void dump(const std::string& name, const char* suffix, const CoverageMap& map, const std::vector<uint8_t>& bytes)
    {
        if (dumpDir.empty() || !map.data) return;
        auto filename = dumpDir + "/" + name + suffix;
        std::ofstream file(filename, std::ios::binary);
        file << "P5\n# origin " << map.bounds.min.x << ' ' << map.bounds.min.y << "\n" << map.bounds.w() << ' ' << map.bounds.h() << "\n255\n";
        for (auto y = map.bounds.min.y; y < map.bounds.max.y; ++y) {
            file.write(reinterpret_cast<const char*>(bytes.data() + y * Width + map.bounds.min.x), map.bounds.w());
        }
        file.flush();
        check(bool(file), "write " + filename);
    }

    bool compare(const std::string& name, const RenderPath& path, CoverageMap& map, FillRule rule = FillRule::NonZero,
                 const Matrix& transform = identity(), const RenderRegion& viewport = Viewport, bool partial = true)
    {
        std::vector<uint8_t> alpha;
        if (!reference(path, rule, transform, viewport, alpha)) return false;
        auto status = fillCoverage(path, rule, transform, viewport, map);
        return compareBytes(name, status, map, alpha, viewport, partial);
    }

    bool compareShape(const std::string& name, const RenderShape& shape, CoverageMap& map, bool strokeOnly = true,
                      const Matrix& transform = identity(), const RenderRegion& viewport = Viewport, bool partial = true)
    {
        std::vector<uint8_t> alpha;
        if (!reference(shape.path, shape.rule, transform, viewport, alpha, shape.stroke, strokeOnly)) return false;
        auto status = strokeOnly ? generateStrokeCoverage(shape, transform, viewport, map) : generateFillCoverage(shape, transform, viewport, map);
        return compareBytes(name, status, map, alpha, viewport, partial);
    }

    bool compareBytes(const std::string& name, CoverageStatus status, CoverageMap& map, const std::vector<uint8_t>& alpha,
                      const RenderRegion& viewport, bool partial)
    {
        auto any = std::any_of(alpha.begin(), alpha.end(), [](uint8_t value) { return value != 0; });
        if (!any) return check(status == CoverageStatus::Empty && cleared(map), name + ": empty coverage");
        if (!check(status == CoverageStatus::Ready && map.data, name + ": ready coverage")) return false;
        if (!check(map.bounds.min.x >= viewport.min.x && map.bounds.min.y >= viewport.min.y &&
                   map.bounds.max.x <= viewport.max.x && map.bounds.max.y <= viewport.max.y &&
                   map.bounds.valid() && map.stride == map.bounds.w(), name + ": crop/stride")) return false;

        RenderRegion crop = {{Width, Height}, {0, 0}};
        frame.assign(Width * Height, 0);
        unsigned partialCount = 0;
        size_t mismatch = alpha.size();
        for (uint32_t y = 0; y < Height; ++y) {
            for (uint32_t x = 0; x < Width; ++x) {
                auto index = y * Width + x;
                auto value = frame[index] = pixel(map, x, y);
                if (value > 0 && value < 255) ++partialCount;
                if (value != alpha[index] && mismatch == alpha.size()) mismatch = index;
                if (alpha[index]) {
                    crop.min.x = std::min(crop.min.x, int32_t(x));
                    crop.min.y = std::min(crop.min.y, int32_t(y));
                    crop.max.x = std::max(crop.max.x, int32_t(x + 1));
                    crop.max.y = std::max(crop.max.y, int32_t(y + 1));
                }
            }
        }
        ++comparisons;
        if (mismatch != alpha.size()) {
            std::cerr << name << ": first byte difference at " << mismatch % Width << ',' << mismatch / Width
                      << ", coverage=" << unsigned(frame[mismatch]) << ", SW=" << unsigned(alpha[mismatch]) << '\n';
        }
        check(mismatch == alpha.size(), name + ": exact SW bytes including zero area outside crop");
        check(map.bounds.min.x == crop.min.x && map.bounds.min.y == crop.min.y && map.bounds.max.x == crop.max.x && map.bounds.max.y == crop.max.y,
              name + ": tight nonzero crop");
        if (partial) check(partialCount > 0, name + ": fractional AA bytes");
        dump(name, ".coverage.pgm", map, frame);
        dump(name, ".sw.pgm", map, alpha);
        return mismatch == alpha.size();
    }
};

}

int main(int argc, char** argv)
{
    Validation validation;
    bool strokeOnly = false;
    for (int i = 1; i < argc; ++i) {
        auto argument = std::string(argv[i]);
        if (argument == "--stroke-only") strokeOnly = true;
        else if (argument == "--dump-dir" && i + 1 < argc) validation.dumpDir = argv[++i];
        else {
            std::cerr << "Usage: tvgCoverageTests [--stroke-only] [--dump-dir existing-directory]\n";
            return 2;
        }
    }
    if (!validation.check(Initializer::init(0) == Result::Success, "initialize")) return 1;
    if (!strokeOnly) {
        CoverageMap map;
        validation.check(cleared(map), "new map is empty");
        auto curved = notch();
        validation.compare("curved-notch", curved, map);
        validation.check(pixel(map, 35, 62) == 0 && pixel(map, 105, 62) == 255, "curved inward notch is preserved");
        auto initial = validation.frame;
        for (unsigned phase = 1; phase <= 3; ++phase) {
            auto transform = identity();
            transform.e13 = phase * 0.25f;
            transform.e23 = phase * 0.125f;
            validation.compare("fractional-notch-" + std::to_string(phase), curved, map, FillRule::NonZero, transform);
            validation.check(validation.frame != initial, "fractional movement changes coverage");
        }

        RenderPath holes;
        rectangle(holes, 12.25f, 10.5f, 132.0f, 105.0f);
        rectangle(holes, 51.5f, 36.25f, 49.0f, 46.0f);
        validation.compare("same-winding-nonzero", holes, map);
        validation.check(pixel(map, 76, 58) == 255, "NonZero retains same-winding interior");
        validation.compare("same-winding-evenodd", holes, map, FillRule::EvenOdd);
        validation.check(pixel(map, 76, 58) == 0, "EvenOdd opens same-winding hole");
        holes.clear();
        rectangle(holes, 12.25f, 10.5f, 132.0f, 105.0f);
        rectangle(holes, 51.5f, 36.25f, 49.0f, 46.0f, true);
        validation.compare("opposite-winding-nonzero", holes, map);
        validation.check(pixel(map, 76, 58) == 0, "NonZero opens opposite-winding hole");

        auto overlap = star();
        validation.compare("self-overlap-nonzero", overlap, map);
        validation.check(pixel(map, 80, 66) == 255, "NonZero retains multiply wound star center");
        validation.compare("self-overlap-evenodd", overlap, map, FillRule::EvenOdd);
        validation.check(pixel(map, 80, 66) == 0, "EvenOdd removes multiply wound star center");

        RenderPath rect;
        rectangle(rect, 14.0f, 18.0f, 72.0f, 64.0f);
        validation.compare("integer-rectangle", rect, map, FillRule::NonZero, identity(), Viewport, false);
        validation.check(map.data && map.bounds.min.x == 14 && map.bounds.min.y == 18 && map.bounds.w() == 72 && map.bounds.h() == 64 &&
                         std::all_of(map.data, map.data + map.stride * map.bounds.h(), [](uint8_t value) { return value == 255; }), "integer rectangle coverage");
        auto fractional = identity();
        fractional.e13 = 0.25f;
        fractional.e23 = 0.5f;
        validation.compare("fractional-rectangle", rect, map, FillRule::NonZero, fractional);
        rect.clear();
        rectangle(rect, 21.25f, 17.25f, 0.5f, 0.5f);
        validation.compare("one-pixel-crop", rect, map);
        validation.check(map.bounds.w() == 1 && map.bounds.h() == 1, "tiny shape occupies one pixel");

        auto offset = identity();
        offset.e13 = -59.125f;
        offset.e23 = -35.375f;
        validation.compare("offscreen-notch", curved, map, FillRule::NonZero, offset);
        validation.compare("offset-viewport", curved, map, FillRule::NonZero, identity(), {{31, 23}, {117, 93}});
        auto affine = identity();
        affine.e11 = 0.85f;
        affine.e12 = -0.18f;
        affine.e21 = 0.12f;
        affine.e22 = 0.9f;
        affine.e13 = 13.125f;
        affine.e23 = -2.375f;
        validation.compare("affine-notch", curved, map, FillRule::NonZero, affine);

        RenderPath empty;
        validation.compare("empty-path", empty, map, FillRule::NonZero, identity(), Viewport, false);
        empty.moveTo({20.25f, 10.5f});
        empty.lineTo({100.25f, 10.5f});
        validation.compare("zero-area-path", empty, map, FillRule::NonZero, identity(), Viewport, false);
        offset.e13 = 200.0f;
        validation.compare("fully-offscreen", curved, map, FillRule::NonZero, offset, Viewport, false);
        validation.compare("reuse-after-empty", curved, map);
        validation.check(fillCoverage(curved, FillRule::NonZero, identity(), {{0, 0}, {0, 0}}, map) == CoverageStatus::Empty && cleared(map), "empty viewport clears previous coverage");
        validation.compare("reuse-before-reset", holes, map, FillRule::EvenOdd);
        map.reset();
        validation.check(cleared(map), "reset clears pixels and dimensions");
        map.reset();
        validation.check(cleared(map), "reset is repeatable");
        validation.compare("reuse-after-reset", curved, map);
    }
    {
        CoverageMap map;
        RenderShape shape;
        shape.stroke = new RenderStroke();
        auto stroke = shape.stroke;
        stroke->width = 10.5f;
        shape.path.moveTo({30.25f, 44.5f});
        shape.path.lineTo({118.75f, 44.5f});
        stroke->cap = StrokeCap::Butt;
        validation.compareShape("stroke-butt-cap", shape, map);
        validation.check(pixel(map, 26, 44) == 0, "butt cap stops at the endpoint");
        auto butt = validation.frame;
        stroke->cap = StrokeCap::Round;
        validation.compareShape("stroke-round-cap", shape, map);
        validation.check(pixel(map, 26, 44) > 0 && validation.frame != butt, "round cap extends past the endpoint");
        auto round = validation.frame;
        stroke->cap = StrokeCap::Square;
        validation.compareShape("stroke-square-cap", shape, map);
        validation.check(pixel(map, 25, 39) > 0 && validation.frame != round, "square cap retains its outer corner");

        shape.path.clear();
        shape.path.moveTo({30.25f, 90.5f});
        shape.path.lineTo({70.5f, 25.25f});
        shape.path.lineTo({110.75f, 90.5f});
        stroke->width = 14.25f;
        stroke->cap = StrokeCap::Butt;
        stroke->join = StrokeJoin::Bevel;
        validation.compareShape("stroke-bevel-join", shape, map);
        auto bevel = validation.frame;
        stroke->join = StrokeJoin::Round;
        validation.compareShape("stroke-round-join", shape, map);
        validation.check(validation.frame != bevel, "round and bevel joins have distinct coverage");
        stroke->join = StrokeJoin::Miter;
        validation.compareShape("stroke-miter-join", shape, map);
        auto miter = validation.frame;
        stroke->miterlimit = 1.0f;
        validation.compareShape("stroke-miter-limit", shape, map);
        validation.check(validation.frame != miter, "miter limit clips the acute join");

        shape.path.clear();
        shape.path.moveTo({13.25f, 35.5f});
        shape.path.cubicTo({49.5f, 112.25f}, {101.75f, -13.5f}, {145.5f, 87.25f});
        shape.path.moveTo({22.5f, 103.25f});
        shape.path.lineTo({116.25f, 110.75f});
        stroke->width = 5.75f;
        stroke->cap = StrokeCap::Round;
        stroke->join = StrokeJoin::Round;
        stroke->miterlimit = 4.0f;
        stroke->dash.pattern = tvg::malloc<float>(3 * sizeof(float));
        stroke->dash.pattern[0] = 11.5f;
        stroke->dash.pattern[1] = 5.25f;
        stroke->dash.pattern[2] = 3.75f;
        stroke->dash.count = 3;
        stroke->dash.length = 20.5f;
        stroke->dash.offset = -9.5f;
        validation.compareShape("stroke-odd-dash-offset", shape, map);
        stroke->trim = {0.2f, 0.8f, true};
        validation.compareShape("stroke-dashed-trim", shape, map);
        auto simultaneous = validation.frame;
        stroke->trim.simultaneous = false;
        validation.compareShape("stroke-individual-trim", shape, map);
        validation.check(validation.frame != simultaneous, "trim modes differ across contours");
        stroke->trim = {0.8f, 0.2f, true};
        validation.compareShape("stroke-wrapped-trim", shape, map);
        stroke->trim = {};

        auto affine = identity();
        affine.e11 = 0.85f;
        affine.e12 = -0.18f;
        affine.e21 = 0.12f;
        affine.e22 = 0.9f;
        affine.e13 = 13.125f;
        affine.e23 = -2.375f;
        validation.compareShape("stroke-fractional-affine", shape, map, true, affine);
        auto offset = identity();
        offset.e13 = -59.125f;
        offset.e23 = -35.375f;
        validation.compareShape("stroke-offscreen", shape, map, true, offset);
        validation.compareShape("stroke-offset-viewport", shape, map, true, identity(), {{31, 23}, {117, 93}});

        tvg::free(stroke->dash.pattern);
        stroke->dash = {};
        shape.path = notch();
        shape.color = {31, 73, 149, 255};
        stroke->color = {255, 255, 255, 255};
        stroke->width = 11.25f;
        CoverageMap fillMap, strokeMap;
        validation.compareShape("shape-fill-beneath-stroke", shape, fillMap, false);
        validation.compareShape("shape-closed-stroke", shape, strokeMap);
        validation.check(pixel(fillMap, 105, 62) == 255 && pixel(strokeMap, 105, 62) == 0,
                         "fill interior and stroke coverage remain separate");
        shape.color = {};
        stroke->color = {};
        stroke->first = true;
        shape.rule = FillRule::EvenOdd;
        validation.check(generateStrokeCoverage(shape, identity(), Viewport, map) == CoverageStatus::Ready && same(map, strokeMap),
                         "stroke geometry ignores fill rule, colors and paint order");
        stroke->width = 19.0f;
        stroke->join = StrokeJoin::Miter;
        stroke->cap = StrokeCap::Square;
        validation.check(generateFillCoverage(shape, identity(), Viewport, map) == CoverageStatus::Ready && same(map, fillMap),
                         "fill geometry and AA do not depend on stroke width, join, cap or paint order");
        stroke->trim = {0.15f, 0.85f, true};
        validation.compareShape("shape-trimmed-fill", shape, fillMap, false);

        stroke->trim = {};
        stroke->width = 0.0f;
        validation.compareShape("stroke-zero-width", shape, map, true, identity(), Viewport, false);
        stroke->width = 5.75f;
        offset.e13 = 200.0f;
        validation.compareShape("stroke-fully-offscreen", shape, map, true, offset, Viewport, false);
        stroke->trim = {0.4f, 0.4f, true};
        validation.compareShape("stroke-empty-trim", shape, map, true, identity(), Viewport, false);
        stroke->trim = {};
        validation.check(generateStrokeCoverage(shape, identity(), {{0, 0}, {0, 0}}, map) == CoverageStatus::Empty && cleared(map), "stroke empty viewport");
        validation.compareShape("stroke-reuse-after-empty", shape, map);
        shape.path.clear();
        validation.compareShape("stroke-empty-path", shape, map, true, identity(), Viewport, false);
        RenderShape noStroke;
        noStroke.path = notch();
        validation.compareShape("stroke-absent", noStroke, map, true, identity(), Viewport, false);
    }
    validation.check(Initializer::term() == Result::Success, "terminate");
    std::cout << validation.comparisons << " exact SW comparisons; " << validation.checks << " checks; " << validation.failures << " failures\n";
    return validation.failures ? 1 : 0;
}
