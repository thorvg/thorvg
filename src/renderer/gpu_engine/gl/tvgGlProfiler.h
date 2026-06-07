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

#ifndef _TVG_GL_PROFILER_H_
#define _TVG_GL_PROFILER_H_

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static inline bool tvgGlStencilAtlasProfileEnabled()
{
    static int enabled = -1;
    if (enabled >= 0) return enabled == 1;

    auto value = std::getenv("TVG_GL_STENCIL_ATLAS_PROFILE");
    enabled = (value && value[0] != '\0' && std::strcmp(value, "0") != 0 &&
               std::strcmp(value, "false") != 0 && std::strcmp(value, "FALSE") != 0) ? 1 : 0;
    return enabled == 1;
}

static inline bool tvgGlStencilAtlasProfileVerbose()
{
    static int verbose = -1;
    if (verbose >= 0) return verbose == 1;

    auto value = std::getenv("TVG_GL_STENCIL_ATLAS_PROFILE");
    verbose = (value && (std::strcmp(value, "2") == 0 || std::strcmp(value, "verbose") == 0 ||
                         std::strcmp(value, "VERBOSE") == 0)) ? 1 : 0;
    return verbose == 1;
}

static inline uint64_t tvgGlStencilAtlasProfileNowUs()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

static inline void tvgGlStencilAtlasProfileLog(const char* fmt, ...)
{
    if (!tvgGlStencilAtlasProfileEnabled()) return;

    std::fprintf(stderr, "[tvg-gl-stencil-atlas] ");

    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);

    std::fprintf(stderr, "\n");
}

#endif //_TVG_GL_PROFILER_H_
