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

#include <memory>
#include "tvgCoverage.h"
#include "tvgSwCommon.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

namespace tvg
{

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

    SwMpool pool(0);
    if (!pool.cell(0)->buffer) return CoverageStatus::Error;
    SwShape sw;
    auto release = [](SwShape* shape) { shapeFree(*shape); };
    std::unique_ptr<SwShape, decltype(release)> cleanup(&sw, release);
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
