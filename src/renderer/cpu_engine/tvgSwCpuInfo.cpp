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

#include <cstdint>
#include "tvgSwCpuInfo.h"

#ifdef THORVG_AVX_VECTOR_SUPPORT

#if defined(_MSC_VER)
    #include <intrin.h>
#else
    #include <cpuid.h>
#endif

static bool _cpuid(uint32_t leaf, uint32_t regs[4])
{
#if defined(_MSC_VER)
    int info[4];
    __cpuid(info, (int)leaf);
    regs[0] = (uint32_t)info[0];
    regs[1] = (uint32_t)info[1];
    regs[2] = (uint32_t)info[2];
    regs[3] = (uint32_t)info[3];
    return true;
#else
    return __get_cpuid(leaf, &regs[0], &regs[1], &regs[2], &regs[3]) != 0;
#endif
}

static uint64_t _xcr0()
{
#if defined(_MSC_VER)
    return _xgetbv(0);
#else
    uint32_t lo, hi;
    __asm__ __volatile__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((uint64_t)hi << 32) | lo;
#endif
}

bool avxUsable()
{
    uint32_t regs[4];
    if (!_cpuid(1, regs)) return false;

    // OS must enable XSAVE before XGETBV may be executed at all.
    if (!(regs[2] & (1u << 27))) return false;

    // cpu must implement AVX.
    if (!(regs[2] & (1u << 28))) return false;

    // OS must preserve the XMM and YMM state on a context switch.
    return (_xcr0() & 0x6) == 0x6;
}

#endif
