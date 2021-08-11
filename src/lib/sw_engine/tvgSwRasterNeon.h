/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.

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

#ifdef THORVG_NEON_VECTOR_SUPPORT

#include <arm_neon.h>

static inline uint8x8_t ALPHA_BLEND_NEON(uint8x8_t c, uint8x8_t a)
{
	uint16x8_t t = vmull_u8(c, a);
	return vshrn_n_u16(t, 8);
}


static inline void neonRasterRGBA32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    uint32_t iterations = len / 4;
    uint32_t neonFilled = iterations * 4;

    dst += offset;
    uint32x4_t vectorVal = {val, val, val, val};

    for (uint32_t i = 0; i < iterations; ++i) {
        vst1q_u32(dst, vectorVal);
        dst += 4;
    }

    int32_t leftovers = len - neonFilled;
    while (leftovers--) *dst++ = val;
}


static inline bool neonRasterTranslucentRle(SwSurface* surface, const SwRleData* rle, uint32_t color)
{
    auto span = rle->spans;
    uint32_t src;
    uint8x8_t *vDst = NULL;

    for (uint32_t i = 0; i < rle->size; ++i) {
        auto dst = &surface->buffer[span->y * surface->stride + span->x];
        uint32_t align = 0;

        if ((((uint32_t) dst) & 0x7) != 0) {
            vDst = (uint8x8_t*)(dst+1);
            align = 1;
        } else {
            vDst = (uint8x8_t*) dst;
        }

        if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
        else src = color;
        auto ialpha = 255 - surface->blender.alpha(src);

        uint8x8_t vSrc = (uint8x8_t) vdup_n_u32(src);
        uint8x8_t vIalpha = vdup_n_u8((uint8_t) ialpha);

        uint32_t iterations = (span->len - align) / 2;
        uint32_t left = (span->len - align) % 2;

        //Fill not aligned byte
        if (align) *dst = src + ALPHA_BLEND(*dst, ialpha);

        for (uint32_t x = 0; x < iterations; ++x) vDst[x] = vadd_u8(vSrc, ALPHA_BLEND_NEON(vDst[x], vIalpha));

        if (left) dst[span->len - 1] = src + ALPHA_BLEND(dst[span->len - 1], ialpha);

        ++span;
    }
    return true;
}

#endif