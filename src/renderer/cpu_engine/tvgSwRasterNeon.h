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

#ifdef THORVG_NEON_SUPPORT

#include <arm_neon.h>

//TODO : need to support windows ARM
#if defined(__ARM_64BIT_STATE) || defined(_M_ARM64)
    #define TVG_AARCH64 1
#else
    #define TVG_AARCH64 0
#endif

static bool neonRasterABGRtoARGB(RenderSurface* surface)
{
    const auto mask = vdupq_n_u32(0xff00ff00);
    const auto end = surface->w & ~3u;
    #pragma omp parallel for
    for (int32_t y = 0; y < (int32_t)surface->h; ++y) {
        auto dst = surface->buf32 + uint32_t(y) * surface->stride;
        uint32_t x = 0;
        for (; x < end; x += 4) {
            auto pixels = vld1q_u32(dst + x);
            auto swapped = vreinterpretq_u32_u16(vrev32q_u16(vreinterpretq_u16_u32(pixels)));
            vst1q_u32(dst + x, vbslq_u32(mask, pixels, swapped));
        }
        for (; x < surface->w; ++x) {
            auto c = dst[x];
            dst[x] = (c & 0xff00ff00) | ((c & 0x00ff0000) >> 16) | ((c & 0x000000ff) << 16);
        }
    }
    return true;
}

static void neonRasterUnpremultiply(uint32_t* buffer, uint32_t width)
{
    const auto mask = vdupq_n_u32(255);
    const auto one = vdupq_n_u32(1);
    uint32_t x = 0;
    for (; x + 4 <= width; x += 4) {
        auto pixels = vld1q_u32(buffer + x);
        auto alpha = vshrq_n_u32(pixels, 24);
        auto divisor = vmaxq_u32(alpha, one);
        auto divisorFloat = vcvtq_f32_u32(divisor);
        auto reciprocal = vrecpeq_f32(divisorFloat);
        reciprocal = vmulq_f32(reciprocal, vrecpsq_f32(divisorFloat, reciprocal));
        reciprocal = vmulq_f32(reciprocal, vrecpsq_f32(divisorFloat, reciprocal));
        auto result = vshlq_n_u32(alpha, 24);
        for (int shift = 0; shift < 24; shift += 8) {
            auto bits = vdupq_n_s32(shift);
            auto channel = vandq_u32(vshlq_u32(pixels, vnegq_s32(bits)), mask);
            auto numerator = vmulq_u32(channel, mask);
            auto quotient = vcvtq_u32_f32(vmulq_f32(vcvtq_f32_u32(numerator), reciprocal));
            auto product = vmulq_u32(quotient, divisor);
            quotient = vsubq_u32(quotient, vandq_u32(vcgtq_u32(product, numerator), one));
            product = vmulq_u32(quotient, divisor);
            quotient = vaddq_u32(quotient, vandq_u32(vcgeq_u32(numerator, vaddq_u32(product, divisor)), one));
            result = vorrq_u32(result, vshlq_u32(vminq_u32(quotient, mask), bits));
        }
        auto unchanged = vorrq_u32(vceqq_u32(alpha, mask), vceqq_u32(alpha, vdupq_n_u32(0)));
        vst1q_u32(buffer + x, vbslq_u32(unchanged, pixels, result));
    }
    for (; x < width; ++x) buffer[x] = rasterUnpremultiply(buffer[x]);
}

static void neonRasterPremultiply(uint32_t* buffer, uint32_t width)
{
    const auto mask = vdupq_n_u32(255);
    uint32_t x = 0;
    for (; x + 4 <= width; x += 4) {
        auto pixels = vld1q_u32(buffer + x);
        auto alpha = vshrq_n_u32(pixels, 24);
        auto result = vshlq_n_u32(alpha, 24);
        for (int shift = 0; shift < 24; shift += 8) {
            auto bits = vdupq_n_s32(shift);
            auto channel = vandq_u32(vshlq_u32(pixels, vnegq_s32(bits)), mask);
            auto scaled = vshrq_n_u32(vmulq_u32(channel, alpha), 8);
            result = vorrq_u32(result, vshlq_u32(scaled, bits));
        }
        auto opaque = vceqq_u32(alpha, mask);
        vst1q_u32(buffer + x, vbslq_u32(opaque, pixels, result));
    }
    cRasterPremultiply(buffer + x, width - x);
}

static uint32_t neonInterpDownScaler(const uint32_t* img, uint32_t stride, uint32_t w, TVG_UNUSED uint32_t h, float sx, TVG_UNUSED float sy, int32_t miny, int32_t maxy, int32_t n)
{
    // Each clipped span is at most 2*n; stepping by floor(n/2)+1 takes at most 4 samples per axis.
    // At most 4*4 pixels contribute, so each channel sum is <= 16*255=4080 and fits in uint16_t.
    // vaddw_u8 widens and accumulates 8-bit channels into uint16x8_t in one instruction.
    uint16x8_t sum = vdupq_n_u16(0);

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
            sum = vaddw_u8(sum, vreinterpret_u8_u32(vdup_n_u32(*p)));
            ++n;
        }
        src += (stride * inc);
    }

    uint32_t c[4];
    vst1q_u32(c, vmovl_u16(vrev64_u16(vget_low_u16(sum))));
    c[0] /= n;
    c[1] /= n;
    c[2] /= n;
    c[3] /= n;

    return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}

static uint8x8_t ALPHA_BLEND(uint8x8_t c, uint8x8_t a)
{
    return vshrn_n_u16(vmull_u8(c, a), 8);
}

static uint8x8_t neonScalePixels(uint8x8_t pixels, uint32x2_t factors)
{
    auto expanded = vcombine_u16(vdup_n_u16(static_cast<uint16_t>(vget_lane_u32(factors, 0))),
                                 vdup_n_u16(static_cast<uint16_t>(vget_lane_u32(factors, 1))));
    return vmovn_u16(vshrq_n_u16(vmulq_u16(vmovl_u8(pixels), expanded), 8));
}

static void neonRasterTranslucentPixels(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity)
{
    const auto opacityVec = vdup_n_u32(opacity + 1);
    uint32_t i = 0;
    for (; len - i >= 2; i += 2) {
        auto source = vld1_u8(reinterpret_cast<const uint8_t*>(src + i));
        if (opacity != 255) source = neonScalePixels(source, opacityVec);
        auto inverse = vsub_u32(vdup_n_u32(256), vshr_n_u32(vreinterpret_u32_u8(source), 24));
        auto target = vld1_u8(reinterpret_cast<const uint8_t*>(dst + i));
        vst1_u8(reinterpret_cast<uint8_t*>(dst + i), vadd_u8(source, neonScalePixels(target, inverse)));
    }
    cRasterTranslucentPixels(dst + i, src + i, len - i, opacity);
}

static void neonRasterPixels(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity)
{
    if (opacity != 255) {
        neonRasterTranslucentPixels(dst, src, len, opacity);
        return;
    }

    uint32_t i = 0;
    for (; len - i >= 4; i += 4) {
        vst1q_u32(dst + i, vld1q_u32(src + i));
    }
    for (; i < len; ++i) dst[i] = src[i];
}

static void neonRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    int32_t i = 0;
    const uint8x16_t valVec = vdupq_n_u8(val);
#if TVG_AARCH64
    uint8x16x4_t valQuad = {valVec, valVec, valVec, valVec};
    for (; i <= len - 16 * 4; i += 16 * 4) {
        vst1q_u8_x4(dst + i, valQuad);
    }
#else
    for (; i <= len - 16; i += 16) {
        vst1q_u8(dst + i, valVec);
    }
#endif
    for (; i < len; i++) {
        dst[i] = val;
    }
}

static void neonRasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    uint32x4_t vecVal = vdupq_n_u32(val);

#if TVG_AARCH64
    uint32_t iterations = len / 16;
    uint32_t neonFilled = iterations * 16;
    uint32x4x4_t valQuad = {vecVal, vecVal, vecVal, vecVal};
    for (uint32_t i = 0; i < iterations; ++i, dst += 16) {
        vst4q_u32(dst, valQuad);
    }
#else
    uint32_t iterations = len / 4;
    uint32_t neonFilled = iterations * 4;
    for (uint32_t i = 0; i < iterations; ++i, dst +=4) {
        vst1q_u32(dst, vecVal);
    }
#endif
    auto leftovers = len - neonFilled;
    while (leftovers--) *dst++ = val;
}

static bool neonRasterTranslucentRle(SwSurface* surface, const SwRle* rle, const RenderRegion& bbox, const RenderColor& c)
{
    const SwSpan* end;
    int32_t x, len;

    //32bit channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        uint32_t src;
        uint8x8_t *vDst = nullptr;
        int32_t align;

        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;

            auto dst = &surface->buf32[span->y * surface->stride + x];
            auto ialpha = IA(src);

            if ((((uintptr_t) dst) & 0x7) != 0) {
                //fill not aligned byte
                *dst = src + ALPHA_BLEND(*dst, ialpha);
                vDst = (uint8x8_t*)(dst + 1);
                align = 1;
            } else {
                vDst = (uint8x8_t*) dst;
                align = 0;
            }

            uint8x8_t vSrc = (uint8x8_t) vdup_n_u32(src);
            uint8x8_t vIalpha = vdup_n_u8((uint8_t) ialpha);

            for (int32_t x = 0; x < (len - align) / 2; ++x)
                vDst[x] = vadd_u8(vSrc, ALPHA_BLEND(vDst[x], vIalpha));

            auto leftovers = (len - align) % 2;
            if (leftovers > 0) dst[len - 1] = src + ALPHA_BLEND(dst[len - 1], ialpha);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Neon Optimization, Channel Size = %d", surface->channelSize);
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

static bool neonRasterTranslucentRect(SwSurface* surface, const RenderRegion& bbox, const RenderColor& c)
{
    auto h = bbox.h();
    auto w = bbox.w();

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        auto buffer = surface->buf32 + (bbox.min.y * surface->stride) + bbox.min.x;
        auto ialpha = 255 - c.a;

        auto vColor = vdup_n_u32(color);
        auto vIalpha = vdup_n_u8((uint8_t) ialpha);

        uint8x8_t* vDst = nullptr;
        uint32_t align;

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];

            if ((((uintptr_t) dst) & 0x7) != 0) {
                //fill not aligned byte
                *dst = color + ALPHA_BLEND(*dst, ialpha);
                vDst = (uint8x8_t*) (dst + 1);
                align = 1;
            } else {
                vDst = (uint8x8_t*) dst;
                align = 0;
            }

            for (uint32_t x = 0; x <  (w - align) / 2; ++x)
                vDst[x] = vadd_u8((uint8x8_t)vColor, ALPHA_BLEND(vDst[x], vIalpha));

            auto leftovers = (w - align) % 2;
            if (leftovers > 0) dst[w - 1] = color + ALPHA_BLEND(dst[w - 1], ialpha);
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Neon Optimization, Channel Size = %d", surface->channelSize);
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

#endif
