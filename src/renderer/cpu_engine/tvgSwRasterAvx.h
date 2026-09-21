/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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

#ifdef THORVG_AVX_SUPPORT

#include <immintrin.h>

#define N_32BITS_IN_256REG 8

static bool avxRasterABGRtoARGB(RenderSurface* surface)
{
    const auto shuffle = _mm256_setr_epi8(2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15,
                                        2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);
    const auto end = surface->w & ~7u;
    #pragma omp parallel for
    for (int32_t y = 0; y < (int32_t)surface->h; ++y) {
        auto dst = surface->buf32 + uint32_t(y) * surface->stride;
        uint32_t x = 0;
        for (; x < end; x += 8) {
            auto pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + x));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + x), _mm256_shuffle_epi8(pixels, shuffle));
        }
        for (; x < surface->w; ++x) {
            auto c = dst[x];
            dst[x] = (c & 0xff00ff00) | ((c & 0x00ff0000) >> 16) | ((c & 0x000000ff) << 16);
        }
    }
    return true;
}

static uint32_t avxInterpDownScaler(const uint32_t* img, uint32_t stride, uint32_t w, TVG_UNUSED uint32_t h, float sx, TVG_UNUSED float sy, int32_t miny, int32_t maxy, int32_t n)
{
    // At most 4*4 pixels contribute, so each channel sum fits in uint16_t.
    auto sum = _mm_setzero_si128();

    auto minx = static_cast<int32_t>(sx) - n;
    if (minx < 0) minx = 0;

    auto maxx = static_cast<int32_t>(sx) + n;
    if (maxx >= static_cast<int32_t>(w)) maxx = w;

    auto inc = (n / 2) + 1;
    n = 0;

    auto src = img + minx + miny * stride;

    for (auto y = miny; y < maxy; y += inc) {
        auto p = src;
        for (auto x = minx; x < maxx; x += inc, p += inc) {
            sum = _mm_add_epi16(sum, _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*p)));
            ++n;
        }
        src += (stride * inc);
    }

    auto c0 = _mm_extract_epi16(sum, 0) / n;
    auto c1 = _mm_extract_epi16(sum, 1) / n;
    auto c2 = _mm_extract_epi16(sum, 2) / n;
    auto c3 = static_cast<uint32_t>(_mm_extract_epi16(sum, 3)) / n;

    return (c3 << 24) | (c2 << 16) | (c1 << 8) | c0;
}

static __m256i avxScalePixels(__m256i pixels, __m256i factors)
{
    // Each 32-bit lane contains one pixel and a factor in [0, 256].
    // Scale alternating channels in 16-bit lanes without unpacking the pixels.
    const auto mask = _mm256_set1_epi32(0x00ff00ff);
    factors = _mm256_or_si256(factors, _mm256_slli_epi32(factors, 16));
    auto even = _mm256_mullo_epi16(_mm256_and_si256(pixels, mask), factors);
    auto odd = _mm256_mullo_epi16(_mm256_srli_epi16(pixels, 8), factors);
    return _mm256_or_si256(_mm256_srli_epi16(even, 8), _mm256_andnot_si256(mask, odd));
}

static void avxRasterTranslucentPixels(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity)
{
    const auto opacityVec = _mm256_set1_epi32(opacity + 1);
    const auto full = _mm256_set1_epi32(256);
    uint32_t i = 0;
    for (; len - i >= 8; i += 8) {
        auto source = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        if (opacity != 255) source = avxScalePixels(source, opacityVec);
        auto inverse = _mm256_sub_epi32(full, _mm256_srli_epi32(source, 24));
        auto target = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i),
                           _mm256_add_epi32(source, avxScalePixels(target, inverse)));
    }
    cRasterTranslucentPixels(dst + i, src + i, len - i, opacity);
}

static void avxRasterPixels(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity)
{
    if (opacity != 255) {
        avxRasterTranslucentPixels(dst, src, len, opacity);
        return;
    }

    uint32_t i = 0;
    for (; len - i >= 8; i += 8) {
        auto pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), pixels);
    }
    for (; i < len; ++i) dst[i] = src[i];
}

static void avxRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len) 
{
    dst += offset; 

    __m256i vecVal = _mm256_set1_epi8(val);

    int32_t i = 0;
    for (; i <= len - 32; i += 32) {
        _mm256_storeu_si256((__m256i*)(dst + i), vecVal);
    }

    for (; i < len; ++i) {
        dst[i] = val;
    }
}

static void avxRasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    if (len <= 0) return;
    //1. calculate how many iterations we need to cover the length
    uint32_t iterations = len / N_32BITS_IN_256REG;
    uint32_t avxFilled = iterations * N_32BITS_IN_256REG;

    //2. set the beginning of the array
    dst += offset;

    //3. fill the octets
    for (uint32_t i = 0; i < iterations; ++i, dst += N_32BITS_IN_256REG) {
        _mm256_storeu_si256((__m256i*)dst, _mm256_set1_epi32(val));
    }

    //4. fill leftovers (in the first step we have to set the pointer to the place where the avx job is done)
    int32_t leftovers = len - avxFilled;
    while (leftovers--) *dst++ = val;
}

static bool avxRasterTranslucentRect(SwSurface* surface, const RenderRegion& bbox, const RenderColor& c)
{
    auto h = bbox.h();
    auto w = bbox.w();

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        auto buffer = surface->buf32 + (bbox.min.y * surface->stride) + bbox.min.x;

        uint32_t ialpha = 255 - c.a;

        auto avxColor = _mm256_set1_epi32(color);
        auto avxIalpha = _mm256_set1_epi32(ialpha + 1);

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            uint32_t x = 0;
            for (; w - x >= 8; x += 8) {
                auto pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + x));
                auto result = _mm256_add_epi32(avxColor, avxScalePixels(pixels, avxIalpha));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + x), result);
            }
            for (; x < w; ++x) dst[x] = color + ALPHA_BLEND(dst[x], ialpha);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require AVX Optimization, Channel Size = %d", surface->channelSize);
        auto buffer = surface->buf8 + (bbox.min.y * surface->stride) + bbox.min.x;
        auto ialpha = ~c.a;
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            for (uint32_t x = 0; x < w; ++x, ++dst) {
                *dst = c.a + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}

static bool avxRasterTranslucentRle(SwSurface* surface, const SwRle* rle, const RenderRegion& bbox, const RenderColor& c)
{
    const SwSpan* end;
    int32_t x, len;

    //32bit channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        uint32_t src;

        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;

            auto dst = &surface->buf32[span->y * surface->stride + x];
            auto ialpha = IA(src);

            auto avxSrc = _mm256_set1_epi32(src);
            auto avxIalpha = _mm256_set1_epi32(ialpha + 1);
            int32_t i = 0;
            for (; i <= len - 8; i += 8) {
                auto pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
                auto result = _mm256_add_epi32(avxSrc, avxScalePixels(pixels, avxIalpha));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), result);
            }
            for (; i < len; ++i) dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require AVX Optimization, Channel Size = %d", surface->channelSize);
        uint8_t src;
        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            auto dst = &surface->buf8[span->y * surface->stride + x];
            if (span->coverage < 255) src = MULTIPLY(span->coverage, c.a);
            else src = c.a;
            auto ialpha = ~c.a;
            for (auto x = 0; x < len; ++x, ++dst) {
                *dst = src + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}

static void avxRasterUnpremultiply(uint32_t* buffer, uint32_t width)
{
    const auto mask = _mm256_set1_epi32(0xff);
    const auto full = _mm256_set1_epi32(255);
    const auto one = _mm256_set1_epi32(1);
    uint32_t x = 0;
    for (; width - x >= 8; x += 8) {
        auto pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buffer + x));
        auto alpha = _mm256_srli_epi32(pixels, 24);
        auto unchanged = _mm256_or_si256(_mm256_cmpeq_epi32(alpha, full), _mm256_cmpeq_epi32(alpha, _mm256_setzero_si256()));
        auto divisor = _mm256_cvtepi32_ps(_mm256_max_epi32(alpha, one));
        auto result = _mm256_slli_epi32(alpha, 24);
        for (int shift = 0; shift < 24; shift += 8) {
            auto bits = _mm_cvtsi32_si128(shift);
            auto channel = _mm256_and_si256(_mm256_srl_epi32(pixels, bits), mask);
            auto scaled = _mm256_mullo_epi32(channel, full);
            auto quotient = _mm256_cvttps_epi32(_mm256_div_ps(_mm256_cvtepi32_ps(scaled), divisor));
            result = _mm256_or_si256(result, _mm256_sll_epi32(_mm256_min_epi32(quotient, full), bits));
        }
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(buffer + x), _mm256_blendv_epi8(result, pixels, unchanged));
    }
    for (; x < width; ++x) buffer[x] = rasterUnpremultiply(buffer[x]);
}

static void avxRasterPremultiply(uint32_t* buffer, uint32_t width)
{
    const auto full = _mm256_set1_epi32(255);
    const auto alphaMask = _mm256_set1_epi32(0xff000000);
    uint32_t x = 0;
    for (; width - x >= 8; x += 8) {
        auto pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(buffer + x));
        auto alpha = _mm256_srli_epi32(pixels, 24);
        // A factor of 256 preserves opaque pixels; other pixels use alpha / 256.
        auto factors = _mm256_sub_epi32(alpha, _mm256_cmpeq_epi32(alpha, full));
        auto result = avxScalePixels(pixels, factors);
        result = _mm256_blendv_epi8(result, pixels, alphaMask);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(buffer + x), result);
    }
    cRasterPremultiply(buffer + x, width - x);
}

#endif
