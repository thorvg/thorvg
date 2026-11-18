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

#include "tvgGlCoverage.h"
#include "tvgSwCommon.h"

#ifdef THORVG_ANALYTIC_AA_SUPPORT

static bool _packDense(const tvg::CoverageMap& map, uint32_t maxTextureSize, GlCoverageData& packed)
{
    auto width = map.bounds.w();
    auto height = map.bounds.h();
    if (width > maxTextureSize || height > maxTextureSize) return false;
    auto data = tvg::calloc<uint8_t>(height, width);
    if (!data) return false;
    for (const auto& span : map.rle->spans) {
        auto offset = size_t(span.y - map.bounds.yMin) * width + span.x - map.bounds.xMin;
        memset(data + offset, span.coverage, span.len);
    }
    packed.data = packed.storage = data;
    packed.width = width;
    packed.height = height;
    return true;
}

static void _writeRuns(const tvg::CoverageMap& map, uint16_t* out)
{
    for (const auto& span : map.rle->spans) {
        *out++ = uint16_t(span.x - map.bounds.xMin);
        *out++ = uint16_t(span.y - map.bounds.yMin);
        *out++ = uint16_t(span.len);
        *out++ = span.coverage;
    }
}

bool packGlCoverage(const tvg::CoverageMap& map, uint32_t maxTextureSize, GlCoverageData& packed)
{
    packed.reset();
    if (!map.rle || map.rle->invalid() || map.bounds.invalid()) return true;
    if (!maxTextureSize) return false;

    auto width = map.bounds.w();
    auto height = map.bounds.h();
    auto denseSize = size_t(width) * height;
    if (width > UINT16_MAX || height > UINT16_MAX) return _packDense(map, maxTextureSize, packed);
    // Prefer runs only when their padded RGBA16UI storage is smaller than dense R8.
    auto maxRuns = uint32_t((denseSize - 1) / 8);
    auto runCount = map.rle->size();
    if (runCount > maxRuns) return _packDense(map, maxTextureSize, packed);

    auto textureWidth = runCount < maxTextureSize ? runCount : maxTextureSize;
    auto textureHeight = runCount / textureWidth + (runCount % textureWidth != 0);
    auto texels = textureWidth * textureHeight;
    if (textureHeight > maxTextureSize || texels > maxRuns) return _packDense(map, maxTextureSize, packed);

    auto records = tvg::calloc<uint16_t>(size_t(texels), 4 * sizeof(uint16_t));
    if (!records) return _packDense(map, maxTextureSize, packed);
    _writeRuns(map, records);

    packed.data = packed.storage = records;
    packed.width = textureWidth;
    packed.height = textureHeight;
    packed.runCount = runCount;
    packed.format = GlCoverageFormat::Runs;
    return true;
}

#endif //THORVG_ANALYTIC_AA_SUPPORT
