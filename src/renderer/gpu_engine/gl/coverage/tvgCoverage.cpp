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
    auto rle = stroke ? sw.strokeRle : sw.rle;
    if (!rle || rle->invalid()) return CoverageStatus::Empty;

    RenderRegion crop = {renderBox.max, renderBox.min};
    for (const auto& span : rle->spans) {
        if (!span.coverage) continue;
        crop.min.x = std::min(crop.min.x, span.x);
        crop.min.y = std::min(crop.min.y, span.y);
        crop.max.x = std::max(crop.max.x, span.x + span.len);
        crop.max.y = std::max(crop.max.y, span.y + 1);
    }
    if (crop.invalid()) return CoverageStatus::Empty;

    auto width = crop.w();
    auto data = tvg::calloc<uint8_t>(crop.h(), width);
    if (!data) return CoverageStatus::Error;

    for (const auto& span : rle->spans) {
        if (!span.coverage) continue;
        auto offset = size_t(span.y - crop.min.y) * size_t(width) + size_t(span.x - crop.min.x);
        memset(data + offset, span.coverage, size_t(span.len));
    }
    map.data = data;
    map.bounds = crop;
    map.stride = width;
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

}

#endif //THORVG_ANALYTIC_AA_SUPPORT
