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

#ifdef THORVG_AVX_VECTOR_SUPPORT

#include <immintrin.h>


static inline void avxRasterRGBA32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len)
{
    //1. calculate how many iterations we need to cover length
    uint32_t iterations = len / 8;
    uint32_t avxFilled = iterations * 8;

    //2. set beginning of the array
    dst += offset;
    __m256i_u* avxDst = (__m256i_u*) dst;

    //3. fill octets
    for (uint32_t i = 0; i < iterations; ++i) {
        *avxDst = _mm256_set1_epi32(val);
        avxDst++;
    }

    //4. fill leftovers (in first step we have to set pointer to place where avx job is done)
    int32_t leftovers = len - avxFilled;
    dst += avxFilled;

    while (leftovers--) *dst++ = val;
}

#endif