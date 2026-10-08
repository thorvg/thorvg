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

#ifndef _TVG_GL_COVERAGE_H_
#define _TVG_GL_COVERAGE_H_

#include "tvgCoverage.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

enum class GlCoverageFormat : uint8_t {Dense, Runs};

struct GlCoverageData
{
    const void* data = nullptr;  // Borrows the CoverageMap for Dense, points into storage for Runs.
    void* storage = nullptr;     // Owns run records until reset() or destruction.
    uint32_t width = 0, height = 0, runCount = 0;
    GlCoverageFormat format = GlCoverageFormat::Dense;

    GlCoverageData() = default;
    GlCoverageData(const GlCoverageData&) = delete;
    GlCoverageData& operator=(const GlCoverageData&) = delete;
    ~GlCoverageData() { reset(); }

    void reset()
    {
        tvg::free(storage);
        data = storage = nullptr;
        width = height = runCount = 0;
        format = GlCoverageFormat::Dense;
    }
};

// Dense borrows map.data. Runs owns RGBA16UI records {x, y, length, coverage}
// relative to map.bounds, with absent pixels and trailing records equal to zero.
bool packGlCoverage(const tvg::CoverageMap& map, uint32_t maxTextureSize, GlCoverageData& packed);

#endif //THORVG_ANALYTIC_AA_SUPPORT
#endif //_TVG_GL_COVERAGE_H_
