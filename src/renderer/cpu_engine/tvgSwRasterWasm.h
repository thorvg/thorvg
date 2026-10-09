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

#ifdef THORVG_WASM_SIMD_SUPPORT

#include <wasm_simd128.h>

// WebAssembly SIMD (128-bit) rasterizer.
// Only instructions that lower to a single instruction on both x86 and ARM are used:
// no per-lane variable shifts and no float-to-integer truncation (i32x4.trunc_sat_*).

static bool wasmRasterABGRtoARGB(RenderSurface* surface)
{
    const auto end = surface->w & ~3u;
    #pragma omp parallel for
    for (int32_t y = 0; y < (int32_t)surface->h; ++y) {
        auto dst = surface->buf32 + uint32_t(y) * surface->stride;
        uint32_t x = 0;
        for (; x < end; x += 4) {
            auto pixels = wasm_v128_load(dst + x);
            wasm_v128_store(dst + x, wasm_i8x16_shuffle(pixels, pixels, 2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15));
        }
        for (; x < surface->w; ++x) {
            auto c = dst[x];
            dst[x] = (c & 0xff00ff00) | ((c & 0x00ff0000) >> 16) | ((c & 0x000000ff) << 16);
        }
    }
    return true;
}

static uint32_t wasmInterpDownScaler(const uint32_t* img, uint32_t stride, uint32_t w, TVG_UNUSED uint32_t h, float sx, TVG_UNUSED float sy, int32_t miny, int32_t maxy, int32_t n)
{
    // At most 4*4 pixels contribute, so each channel sum fits in uint16_t.
    auto sum = wasm_i16x8_const_splat(0);

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
            sum = wasm_i16x8_add(sum, wasm_u16x8_extend_low_u8x16(wasm_v128_load32_zero(p)));
            ++n;
        }
        src += (stride * inc);
    }

    auto c0 = wasm_u16x8_extract_lane(sum, 0) / n;
    auto c1 = wasm_u16x8_extract_lane(sum, 1) / n;
    auto c2 = wasm_u16x8_extract_lane(sum, 2) / n;
    auto c3 = static_cast<uint32_t>(wasm_u16x8_extract_lane(sum, 3)) / n;

    return (c3 << 24) | (c2 << 16) | (c1 << 8) | c0;
}

static inline v128_t wasmScalePixels16(v128_t pixels, v128_t factors)
{
    // Scale alternating channels in 16-bit lanes without unpacking the pixels.
    // Each 16-bit lane of factors holds the factor, in [0, 256], of the pixel it belongs to.
    const auto mask = wasm_i32x4_const_splat(0x00ff00ff);
    auto even = wasm_i16x8_mul(wasm_v128_and(pixels, mask), factors);
    auto odd = wasm_i16x8_mul(wasm_u16x8_shr(pixels, 8), factors);
    return wasm_v128_or(wasm_u16x8_shr(even, 8), wasm_v128_andnot(odd, mask));
}

static inline v128_t wasmScalePixels(v128_t pixels, v128_t factors)
{
    // Each 32-bit lane contains one pixel and a factor in [0, 256].
    return wasmScalePixels16(pixels, wasm_v128_or(factors, wasm_i32x4_shl(factors, 16)));
}

static inline v128_t wasmMultiply8(v128_t pixels, v128_t factors)
{
    // MULTIPLY() on sixteen 8-bit lanes, the factor is replicated in 16-bit lanes.
    const auto mask = wasm_i16x8_const_splat(0x00ff);
    auto even = wasm_i16x8_mul(wasm_v128_and(pixels, mask), factors);
    auto odd = wasm_i16x8_mul(wasm_u16x8_shr(pixels, 8), factors);
    even = wasm_u16x8_shr(wasm_i16x8_add(even, mask), 8);
    odd = wasm_v128_andnot(wasm_i16x8_add(odd, mask), mask);
    return wasm_v128_or(even, odd);
}

static void wasmRasterTranslucentPixels(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity)
{
    const auto opacityVec = wasm_i16x8_splat(opacity + 1);
    const auto full = wasm_i32x4_const_splat(256);
    uint32_t i = 0;
    for (; len - i >= 4; i += 4) {
        auto source = wasm_v128_load(src + i);
        if (opacity != 255) source = wasmScalePixels16(source, opacityVec);
        auto inverse = wasm_i32x4_sub(full, wasm_u32x4_shr(source, 24));
        auto target = wasm_v128_load(dst + i);
        wasm_v128_store(dst + i, wasm_i32x4_add(source, wasmScalePixels(target, inverse)));
    }
    cRasterTranslucentPixels(dst + i, src + i, len - i, opacity);
}

static void wasmRasterPixels(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity)
{
    if (opacity == 255) memcpy(dst, src, size_t(len) * sizeof(uint32_t));
    else wasmRasterTranslucentPixels(dst, src, len, opacity);
}

static void wasmRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len)
{
    if (len <= 0) return;
    memset(dst + offset, val, size_t(len));
}

static void wasmRasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    if (len <= 0) return;
    dst += offset;

    const auto vecVal = wasm_i32x4_splat(val);
    int32_t i = 0;
    for (; i <= len - 16; i += 16) {
        wasm_v128_store(dst + i, vecVal);
        wasm_v128_store(dst + i + 4, vecVal);
        wasm_v128_store(dst + i + 8, vecVal);
        wasm_v128_store(dst + i + 12, vecVal);
    }
    for (; i <= len - 4; i += 4) {
        wasm_v128_store(dst + i, vecVal);
    }
    for (; i < len; ++i) dst[i] = val;
}

static bool wasmRasterTranslucentRect(SwSurface* surface, const RenderRegion& bbox, const RenderColor& c)
{
    auto h = bbox.h();
    auto w = bbox.w();

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        auto buffer = surface->buf32 + (bbox.min.y * surface->stride) + bbox.min.x;
        uint32_t ialpha = 255 - c.a;
        const auto vColor = wasm_i32x4_splat(color);
        const auto vIalpha = wasm_i16x8_splat(ialpha + 1);
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            uint32_t x = 0;
            for (; w - x >= 4; x += 4) {
                auto pixels = wasm_v128_load(dst + x);
                wasm_v128_store(dst + x, wasm_i32x4_add(vColor, wasmScalePixels16(pixels, vIalpha)));
            }
            for (; x < w; ++x) {
                dst[x] = color + ALPHA_BLEND(dst[x], ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        auto buffer = surface->buf8 + (bbox.min.y * surface->stride) + bbox.min.x;
        const auto ialpha = static_cast<uint8_t>(~c.a);
        const auto vIalpha = wasm_i16x8_splat(ialpha);
        const auto vAlpha = wasm_i8x16_splat(c.a);
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            uint32_t x = 0;
            for (; w - x >= 16; x += 16) {
                auto pixels = wasm_v128_load(dst + x);
                wasm_v128_store(dst + x, wasm_i8x16_add(vAlpha, wasmMultiply8(pixels, vIalpha)));
            }
            for (; x < w; ++x) {
                dst[x] = c.a + MULTIPLY(dst[x], ialpha);
            }
        }
    }
    return true;
}

static bool wasmRasterTranslucentRle(SwSurface* surface, const SwRle* rle, const RenderRegion& bbox, const RenderColor& c)
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
            const auto vSrc = wasm_i32x4_splat(src);
            const auto vIalpha = wasm_i16x8_splat(ialpha + 1);
            int32_t i = 0;
            for (; i <= len - 4; i += 4) {
                auto pixels = wasm_v128_load(dst + i);
                wasm_v128_store(dst + i, wasm_i32x4_add(vSrc, wasmScalePixels16(pixels, vIalpha)));
            }
            for (; i < len; ++i) {
                dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        const auto ialpha = static_cast<uint8_t>(~c.a);
        const auto vIalpha = wasm_i16x8_splat(ialpha);
        uint8_t src;
        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            auto dst = &surface->buf8[span->y * surface->stride + x];
            if (span->coverage < 255) src = MULTIPLY(span->coverage, c.a);
            else src = c.a;
            const auto vSrc = wasm_i8x16_splat(src);
            int32_t i = 0;
            for (; i <= len - 16; i += 16) {
                auto pixels = wasm_v128_load(dst + i);
                wasm_v128_store(dst + i, wasm_i8x16_add(vSrc, wasmMultiply8(pixels, vIalpha)));
            }
            for (; i < len; ++i) {
                dst[i] = src + MULTIPLY(dst[i], ialpha);
            }
        }
    }
    return true;
}

static inline v128_t wasmUnpremultiplyChannel(v128_t channel, v128_t alpha, v128_t divisor)
{
    // floor(channel * 255 / alpha) without a float-to-integer truncation:
    // adding 2^23 rounds the quotient to the nearest integer in the mantissa,
    // then one integer comparison brings a rounded-up result back down.
    const auto full = wasm_i32x4_const_splat(255);
    auto numerator = wasm_i32x4_mul(channel, full);
    auto rounded = wasm_f32x4_add(wasm_f32x4_div(wasm_f32x4_convert_i32x4(numerator), divisor), wasm_f32x4_const_splat(8388608.0f));
    auto quotient = wasm_v128_and(rounded, wasm_i32x4_const_splat(0x007fffff));
    quotient = wasm_i32x4_add(quotient, wasm_i32x4_gt(wasm_i32x4_mul(quotient, alpha), numerator));
    return wasm_i32x4_min(quotient, full);
}

static void wasmRasterUnpremultiply(uint32_t* buffer, uint32_t width)
{
    const auto mask = wasm_i32x4_const_splat(0xff);
    const auto zero = wasm_i32x4_const_splat(0);
    uint32_t x = 0;
    for (; width - x >= 4; x += 4) {
        auto pixels = wasm_v128_load(buffer + x);
        auto alpha = wasm_u32x4_shr(pixels, 24);
        //fully opaque or fully transparent pixels stay as they are, which is the common case
        auto unchanged = wasm_v128_or(wasm_i32x4_eq(alpha, mask), wasm_i32x4_eq(alpha, zero));
        if (wasm_i32x4_all_true(unchanged)) continue;
        alpha = wasm_i32x4_max(alpha, wasm_i32x4_const_splat(1));
        auto divisor = wasm_f32x4_convert_i32x4(alpha);
        auto c1 = wasmUnpremultiplyChannel(wasm_v128_and(pixels, mask), alpha, divisor);
        auto c2 = wasmUnpremultiplyChannel(wasm_v128_and(wasm_u32x4_shr(pixels, 8), mask), alpha, divisor);
        auto c3 = wasmUnpremultiplyChannel(wasm_v128_and(wasm_u32x4_shr(pixels, 16), mask), alpha, divisor);
        auto result = wasm_v128_or(wasm_v128_or(c1, wasm_i32x4_shl(c2, 8)), wasm_i32x4_shl(c3, 16));
        result = wasm_v128_or(result, wasm_v128_and(pixels, wasm_i32x4_const_splat(0xff000000)));
        wasm_v128_store(buffer + x, wasm_v128_bitselect(pixels, result, unchanged));
    }
    for (; x < width; ++x) buffer[x] = rasterUnpremultiply(buffer[x]);
}

static void wasmRasterPremultiply(uint32_t* buffer, uint32_t width)
{
    const auto full = wasm_i32x4_const_splat(255);
    const auto alphaMask = wasm_i32x4_const_splat(0xff000000);
    uint32_t x = 0;
    for (; width - x >= 4; x += 4) {
        auto pixels = wasm_v128_load(buffer + x);
        auto alpha = wasm_u32x4_shr(pixels, 24);
        auto opaque = wasm_i32x4_eq(alpha, full);
        if (wasm_i32x4_all_true(opaque)) continue;
        // A factor of 256 preserves opaque pixels; other pixels use alpha / 256.
        auto result = wasmScalePixels(pixels, wasm_i32x4_sub(alpha, opaque));
        wasm_v128_store(buffer + x, wasm_v128_bitselect(pixels, result, alphaMask));
    }
    cRasterPremultiply(buffer + x, width - x);
}

#endif
