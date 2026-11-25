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

#include <limits.h>
#include "tvgCoverage.h"
#include "tvgSwCommon.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

namespace tvg
{

namespace
{

struct CoverageScratch
{
    SwMpool pool;
    SwShape shape;

    explicit CoverageScratch(bool raster) : pool(0, raster) {}
    ~CoverageScratch() { shapeFree(shape); }

    void reset()
    {
        shapeReset(shape);
        rleReset(shape.strokeRle);
    }
};

}

void CoverageMap::reset()
{
    rleFree(rle);
    rle = nullptr;
    bounds.reset();
}

static void _compact(SwRle& rle)
{
    uint32_t count = 0;
    for (const auto& span : rle.spans) {
        if (!span.coverage || span.len <= 0) continue;
        if (count) {
            auto& previous = rle.spans[count - 1];
            if (previous.y == span.y && previous.x + previous.len == span.x && previous.coverage == span.coverage) {
                previous.len += span.len;
                continue;
            }
        }
        rle.spans[count++] = span;
    }
    rle.spans.count = count;
}

static CoverageStatus _generate(const RenderShape& shape, const Matrix& transform, const RenderRegion& viewport, bool stroke, CoverageMap& map)
{
    map.reset();
    if (viewport.invalid() || (stroke && shape.strokeWidth() <= 0.0f)) return CoverageStatus::Empty;

    static thread_local CoverageScratch scratch(true);
    scratch.reset();
    auto& pool = scratch.pool;
    auto& sw = scratch.shape;
    if (!pool.cell(0)->buffer) return CoverageStatus::Error;
    RenderRegion renderBox{};
    // composite=true bypasses the rectangle shortcut; antiAlias=true preserves edge coverage.
    auto ready = stroke ? shapeGenStrokeRle(sw, &shape, transform, viewport, renderBox, &pool, 0, true) :
                          shapeGenRle(sw, &shape, transform, viewport, renderBox, &pool, 0, true, true);
    if (!ready) return renderBox.invalid() ? CoverageStatus::Empty : CoverageStatus::Error;
    auto& rle = stroke ? sw.strokeRle : sw.rle;
    if (!rle || rle->invalid()) return CoverageStatus::Empty;
    _compact(*rle);

    RenderRegion crop = {renderBox.xMax, renderBox.yMax, renderBox.xMin, renderBox.yMin};
    for (const auto& span : rle->spans) {
        if (!span.coverage) continue;
        crop.xMin = crop.xMin < span.x ? crop.xMin : span.x;
        crop.yMin = crop.yMin < span.y ? crop.yMin : span.y;
        crop.xMax = crop.xMax > span.x + span.len ? crop.xMax : span.x + span.len;
        crop.yMax = crop.yMax > span.y + 1 ? crop.yMax : span.y + 1;
    }
    if (crop.invalid()) return CoverageStatus::Empty;

    map.rle = rle;
    rle = nullptr;
    map.bounds = crop;
    return CoverageStatus::Ready;
}

CoverageStatus generateFillCoverage(const RenderShape& shape, const Matrix& transform, const RenderRegion& viewport, CoverageMap& map)
{
    return _generate(shape, transform, viewport, false, map);
}

CoverageStatus generateStrokeCoverage(const RenderShape& shape, const Matrix& transform, const RenderRegion& viewport, CoverageMap& map)
{
    return _generate(shape, transform, viewport, true, map);
}

static void _appendEdge(Point a, Point b, const RenderRegion& bounds, Array<Point>& edges)
{
    if (a.y == b.y || std::max(a.y, b.y) <= bounds.yMin || std::min(a.y, b.y) >= bounds.yMax) return;
    // Vertical clipping preserves the directed integral. Do not clamp x: an edge
    // left of the viewport still contributes winding throughout the visible row.
    auto d = b - a;
    auto t0 = tvg::clamp((float(bounds.yMin) - a.y) / d.y, 0.0f, 1.0f);
    auto t1 = tvg::clamp((float(bounds.yMax) - a.y) / d.y, 0.0f, 1.0f);
    edges.push(a + d * std::min(t0, t1));
    edges.push(a + d * std::max(t0, t1));
}

namespace
{

constexpr auto RLE_SUBPIXELS = 256;

struct FixedPoint
{
    int64_t x, y;

    Point toPoint() const
    {
        return {float(x) / RLE_SUBPIXELS, float(y) / RLE_SUBPIXELS};
    }
};

struct FixedCubic
{
    FixedPoint start, ctrl1, ctrl2, end;
};

}

static FixedPoint _midpoint(const FixedPoint& a, const FixedPoint& b)
{
    return {(a.x + b.x) >> 1, (a.y + b.y) >> 1};
}

// Match the chord-distance and acute-angle tests in the FreeType-derived
// software rasterizer's _cubicTo() (tvgSwRle.cpp).
static bool _cubicNeedsSplit(const FixedCubic& cubic)
{
    auto dx = cubic.start.x - cubic.end.x;
    auto dy = cubic.start.y - cubic.end.y;
    auto ax = std::abs(dx);
    auto ay = std::abs(dy);
    auto length = std::max(ax, ay) + ((3 * std::min(ax, ay)) >> 3);
    if (length > SHRT_MAX) return true;

    auto limit = length * (RLE_SUBPIXELS / 6);
    auto needsSplit = [&](const FixedPoint& control) {
        auto x = control.x - cubic.end.x;
        auto y = control.y - cubic.end.y;
        // An offset this large already fails the acute-angle test.
        if (std::abs(x) > 2 * SHRT_MAX || std::abs(y) > 2 * SHRT_MAX) return true;
        return std::abs(dy * x - dx * y) > limit || x * (x - dx) + y * (y - dy) > 0;
    };
    return needsSplit(cubic.ctrl2) || needsSplit(cubic.ctrl1);
}

static bool _curveEdges(const SwPoint& from, const SwPoint* points, const RenderRegion& bounds, Array<Point>& edges)
{
    auto upscale = [](const SwPoint& p) {
        return FixedPoint{int64_t(p.x) * (RLE_SUBPIXELS / 64), int64_t(p.y) * (RLE_SUBPIXELS / 64)};
    };
    FixedCubic stack[32];
    uint32_t pending = 1, segments = 0;
    stack[0] = {upscale(from), upscale(points[0]), upscale(points[1]), upscale(points[2])};
    while (pending) {
        auto cubic = stack[--pending];
        if (!_cubicNeedsSplit(cubic)) {
            _appendEdge(cubic.start.toPoint(), cubic.end.toPoint(), bounds, edges);
            ++segments;
            continue;
        }

        if (pending + 2 > 32 || segments + pending + 2 > 16384) return false;
        auto a = _midpoint(cubic.start, cubic.ctrl1);
        auto b = _midpoint(cubic.ctrl1, cubic.ctrl2);
        auto c = _midpoint(cubic.ctrl2, cubic.end);
        auto d = _midpoint(a, b);
        auto e = _midpoint(b, c);
        auto mid = _midpoint(d, e);
        // Push the right half first so the contour is emitted in order.
        stack[pending++] = {mid, e, c, cubic.end};
        stack[pending++] = {cubic.start, a, d, mid};
    }
    return true;
}

CoverageStatus generateCoverageEdges(const RenderShape& shape, const Matrix& transform, const RenderRegion& viewport, bool stroke, CoverageMap& map, Array<Point>& edges)
{
    map.reset();
    edges.clear();
    if (viewport.invalid() || (stroke && shape.strokeWidth() <= 0.0f)) return CoverageStatus::Empty;

    // Reuse trim/dash/stroke construction and the SW 26.6 coordinate export,
    // but allocate neither raster cells nor pixel spans.
    static thread_local CoverageScratch scratch(false);
    scratch.reset();
    auto& pool = scratch.pool;
    auto& sw = scratch.shape;
    SwOutline* outline;
    if (stroke) {
        outline = shapeGenStrokeOutline(sw, &shape, transform, viewport, map.bounds, &pool, 0);
    } else {
        outline = shapeGenOutline(sw, &shape, transform, viewport, map.bounds, &pool, 0, true) ? sw.outline : nullptr;
    }
    if (!outline || map.bounds.invalid()) return CoverageStatus::Empty;

    auto pts = outline->out.data;
    SwPoint start{}, previous{};
    bool open = false;
    for (auto cmd : outline->path->cmds) {
        switch (cmd) {
            case PathCommand::MoveTo:
                if (open) _appendEdge(previous.toPoint(), start.toPoint(), map.bounds, edges);
                start = previous = *pts++;
                open = true;
                break;
            case PathCommand::LineTo: {
                auto next = *pts++;
                _appendEdge(previous.toPoint(), next.toPoint(), map.bounds, edges);
                previous = next;
                open = true;
                break;
            }
            case PathCommand::CubicTo: {
                if (!_curveEdges(previous, pts, map.bounds, edges)) {
                    edges.clear();
                    map.reset();
                    return CoverageStatus::Error;
                }
                previous = pts[2];
                pts += 3;
                open = true;
                break;
            }
            case PathCommand::Close:
                if (open) _appendEdge(previous.toPoint(), start.toPoint(), map.bounds, edges);
                previous = start;
                open = false;
                break;
        }
    }
    if (open) _appendEdge(previous.toPoint(), start.toPoint(), map.bounds, edges);
    return edges.empty() ? CoverageStatus::Empty : CoverageStatus::Ready;
}

static bool _windingFilled(int winding, FillRule rule)
{
    return rule == FillRule::EvenOdd ? winding % 2 != 0 : winding != 0;
}

static bool _horizontalEndpoint(const Point& p, const Point& adjacent, const RenderRegion& b, double cx)
{
    return p.y > b.yMin && p.y < b.yMax && p.x <= cx && (p.y != adjacent.y || adjacent.x > cx);
}

static int _horizontalWindingChange(const Array<Point>& edges, float y, double cx)
{
    int change = 0;
    for (uint32_t i = 0; i < edges.count; i += 2) {
        const auto& a = edges[i];
        const auto& b = edges[i + 1];
        change += (a.y == y && a.x <= cx) - (b.y == y && b.x <= cx);
    }
    return change;
}

static bool _crossesFillBoundary(const Array<Point>& edges, const Point& a, const Point& b,
                               const RenderRegion& box, FillRule rule)
{
    auto lo = std::max(double(box.yMin), double(std::min(a.y, b.y)));
    auto hi = std::min(double(box.yMax), double(std::max(a.y, b.y)));
    if (lo >= hi || std::max(a.x, b.x) <= box.xMin || std::min(a.x, b.x) >= box.xMax) return false;
    auto dx = double(b.x) - a.x;
    auto dy = double(b.y) - a.y;
    if (dx != 0) {
        auto y0 = a.y + dy * (double(box.xMin) - a.x) / dx;
        auto y1 = a.y + dy * (double(box.xMax) - a.x) / dx;
        lo = std::max(lo, std::min(y0, y1));
        hi = std::min(hi, std::max(y0, y1));
        if (lo >= hi) return false;
    }
    auto middle = lo + (hi - lo) * 0.5;
    int jump = 0, startChange = 0, endChange = 0;
    for (uint32_t i = 0; i < edges.count; i += 2) {
        const auto& c = edges[i];
        const auto& d = edges[i + 1];
        if (dx * (double(c.y) - a.y) != dy * (double(c.x) - a.x) ||
            dx * (double(d.y) - a.y) != dy * (double(d.x) - a.x)) continue;
        jump += (c.y <= middle && middle < d.y) - (d.y <= middle && middle < c.y);
        startChange += (c.y == a.y) - (d.y == a.y);
        endChange += (c.y == b.y) - (d.y == b.y);
    }
    auto interior = [&](const Point& p) {
        return p.x > box.xMin && p.x < box.xMax && p.y > box.yMin && p.y < box.yMax;
    };
    return _windingFilled(jump, rule) || (interior(a) && _windingFilled(startChange, rule)) ||
           (interior(b) && _windingFilled(endChange, rule));
}

bool intersectsCoverageEdges(const Array<Point>& edges, const RenderRegion& box, FillRule rule)
{
    auto cx = (double(box.xMin) + box.xMax) * 0.5;
    auto cy = (double(box.yMin) + box.yMax) * 0.5;
    int winding = 0;
    bool boundary = false;
    for (uint32_t i = 0; i < edges.count; i += 2) {
        const auto& a = edges[i];
        const auto& b = edges[i + 1];
        auto top = std::min(a.y, b.y), bottom = std::max(a.y, b.y);
        if (bottom <= box.yMin || top >= box.yMax || std::min(a.x, b.x) >= box.xMax) continue;
        if (top <= cy && cy < bottom) {
            auto side = (double(b.x) - a.x) * (cy - a.y) - (double(b.y) - a.y) * (cx - a.x);
            if (b.y > a.y ? side <= 0 : side >= 0) winding += b.y > a.y ? 1 : -1;
        }
        if (!boundary) {
            boundary = std::max(a.x, b.x) > box.xMin ||
                _horizontalEndpoint(a, edges[i ? i - 1 : edges.count - 1], box, cx) ||
                _horizontalEndpoint(b, edges[i + 2 < edges.count ? i + 2 : 0], box, cx);
        }
    }
    if (_windingFilled(winding, rule)) return true;
    if (!boundary) return false;
    for (uint32_t i = 0; i < edges.count; i += 2) {
        const auto& a = edges[i];
        const auto& b = edges[i + 1];
        if (_crossesFillBoundary(edges, a, b, box, rule)) return true;
        if (_horizontalEndpoint(a, edges[i ? i - 1 : edges.count - 1], box, cx) &&
            _windingFilled(_horizontalWindingChange(edges, a.y, cx), rule)) return true;
        if (_horizontalEndpoint(b, edges[i + 2 < edges.count ? i + 2 : 0], box, cx) &&
            _windingFilled(_horizontalWindingChange(edges, b.y, cx), rule)) return true;
    }
    return false;
}

CoverageStatus clipCoverage(CoverageMap& map, const CoverageMap& clip)
{
    auto bounds = RenderRegion::intersect(map.bounds, clip.bounds);
    if (bounds.invalid() || !clip.rle || clip.rle->invalid()) {
        map.reset();
        return CoverageStatus::Empty;
    }

    // An image starts with an implicit fully covered rectangle.
    if (!map.rle) {
        map.rle = new SwRle;
        map.rle->spans = clip.rle->spans;
        rleClip(map.rle, &bounds);
    } else {
        rleClip(map.rle, clip.rle);
    }
    _compact(*map.rle);
    if (map.rle->invalid()) {
        map.reset();
        return CoverageStatus::Empty;
    }
    map.bounds = bounds;
    return CoverageStatus::Ready;
}

bool intersectsCoverage(const CoverageMap& map, const RenderRegion& region)
{
    auto bounds = RenderRegion::intersect(map.bounds, region);
    return !bounds.invalid() && rleIntersect(map.rle, bounds);
}

}

#endif //THORVG_ANALYTIC_AA_SUPPORT
