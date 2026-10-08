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

#ifndef _TVG_COVERAGE_H_
#define _TVG_COVERAGE_H_

#include "tvgRender.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

struct SwRle;

namespace tvg
{

enum class CoverageStatus : uint8_t {Ready, Empty, Error};

struct CoverageMap
{
    SwRle* rle = nullptr;  // Owns ordered nonzero runs, with adjacent equal coverage merged.
    RenderRegion bounds{};

    CoverageMap() = default;
    CoverageMap(const CoverageMap&) = delete;
    CoverageMap& operator=(const CoverageMap&) = delete;
    ~CoverageMap() { reset(); }

    void reset();
};

// Internal renderer inputs follow SW contracts. Each call replaces map with cropped coverage, independent of paint.
CoverageStatus generateFillCoverage(const RenderShape& shape, const Matrix& transform, const RenderRegion& viewport, CoverageMap& map);
CoverageStatus generateStrokeCoverage(const RenderShape& shape, const Matrix& transform, const RenderRegion& viewport, CoverageMap& map);
CoverageStatus clipCoverage(CoverageMap& map, const CoverageMap& clip);
bool intersectsCoverage(const CoverageMap& map, const RenderRegion& region);

}

#endif //THORVG_ANALYTIC_AA_SUPPORT
#endif //_TVG_COVERAGE_H_
