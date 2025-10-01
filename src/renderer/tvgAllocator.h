/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY  KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_ALLOCATOR_H_
#define _TVG_ALLOCATOR_H_

#include "config.h"

#ifdef TVG_CUSTOM_ALLOCATOR_HEADER
#  include TVG_CUSTOM_ALLOCATOR_HEADER
#endif

#if !defined(TVG_MALLOC) || !defined(TVG_CALLOC) || !defined(TVG_REALLOC) || !defined(TVG_FREE)
#  include <cstdlib>
#endif

#ifndef TVG_MALLOC
#  define TVG_MALLOC(sz)       std::malloc(sz)
#endif

#ifndef TVG_CALLOC
#  define TVG_CALLOC(n,sz)     std::calloc(n,sz)
#endif

#ifndef TVG_REALLOC
#  define TVG_REALLOC(p,sz)    std::realloc(p,sz)
#endif

#ifndef TVG_FREE
#  define TVG_FREE(p)          std::free(p)
#endif

#endif //_TVG_ALLOCATOR_H_
