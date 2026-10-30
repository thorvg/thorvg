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

#include <limits>
#include "tvgGlCoverage.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

static inline uint32_t _countRuns(const tvg::CoverageMap& map, uint32_t limit)
{
    uint32_t count = 0;
    auto width = map.bounds.w();
    for (uint32_t y = 0; y < map.bounds.h(); ++y) {
        auto row = map.data + size_t(y) * map.stride;
        for (uint32_t x = 0; x < width;) {
            auto coverage = row[x++];
            if (!coverage) continue;
            while (x < width && row[x] == coverage) ++x;
            if (++count > limit) return count;
        }
    }
    return count;
}

static inline void _writeRuns(const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride, uint16_t* out)
{
    for (uint32_t y = 0; y < height; ++y) {
        auto row = data + size_t(y) * stride;
        for (uint32_t x = 0; x < width;) {
            auto start = x;
            auto coverage = row[x++];
            if (!coverage) continue;
            while (x < width && row[x] == coverage) ++x;
            *out++ = uint16_t(start);
            *out++ = uint16_t(y);
            *out++ = uint16_t(x - start);
            *out++ = coverage;
        }
    }
}

bool packGlCoverage(const tvg::CoverageMap& map, uint32_t maxTextureSize, GlCoverageData& packed)
{
    packed.reset();
    if (!map.data) return true;
    if (!maxTextureSize) return false;

    auto width = map.bounds.w();
    auto height = map.bounds.h();
    auto denseSize = size_t(width) * height;
    auto denseFits = width <= maxTextureSize && height <= maxTextureSize;
    if (denseFits) {
        packed.data = map.data;
        packed.width = width;
        packed.height = height;
    }

    if (width > std::numeric_limits<uint16_t>::max() || height > std::numeric_limits<uint16_t>::max()) return denseFits;
    // Prefer runs only when their padded RGBA16UI storage is smaller than dense R8.
    auto maxRuns = uint32_t((denseSize - 1) / 8);
    if (!maxRuns) return denseFits;

    auto runCount = _countRuns(map, maxRuns);
    if (!runCount || runCount > maxRuns) return denseFits;

    auto textureWidth = std::min(runCount, maxTextureSize);
    auto textureHeight = runCount / textureWidth + (runCount % textureWidth != 0);
    auto texels = textureWidth * textureHeight;
    if (textureHeight > maxTextureSize || texels > maxRuns) return denseFits;

    auto records = tvg::calloc<uint16_t>(size_t(texels), 4 * sizeof(uint16_t));
    if (!records) return denseFits;
    _writeRuns(map.data, width, height, map.stride, records);

    packed.data = packed.storage = records;
    packed.width = textureWidth;
    packed.height = textureHeight;
    packed.runCount = runCount;
    packed.format = GlCoverageFormat::Runs;
    return true;
}

#endif //THORVG_ANALYTIC_AA_SUPPORT
