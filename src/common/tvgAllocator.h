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

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_ALLOCATOR_H_
#define _TVG_ALLOCATOR_H_

#include <cstdlib>
#include <cstddef>
#include <thorvg.h>

//separate memory allocators for clean customization
namespace tvg
{
    thread_local static Initializer::HeapAllocator _heapAllocator = {};

    template<typename T = void*>
    static inline T malloc(size_t size)
    {
        if (_heapAllocator.alloc) {
            return static_cast<T>(_heapAllocator.alloc(size));
        }

        return static_cast<T>(std::malloc(size));
    }

    template<typename T = void*>
    static inline T calloc(size_t nmem, size_t size)
    {
        if (_heapAllocator.calloc) {
            return static_cast<T>(_heapAllocator.calloc(nmem, size));
        }

        return static_cast<T>(std::calloc(nmem, size));
    }

    template<typename T = void*>
    static inline T realloc(void* ptr, size_t size)
    {
        if (_heapAllocator.realloc) {
            return static_cast<T>(_heapAllocator.realloc(ptr, size));
        }

        return static_cast<T>(std::realloc(ptr, size));
    }

    static inline void free(void* ptr)
    {
        if (_heapAllocator.free) {
            return _heapAllocator.free(ptr);
        }

        return std::free(ptr);
    }
}

#endif //_TVG_ALLOCATOR_H_