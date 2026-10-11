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

#ifndef _TVG_ALLOCATOR_H_
#define _TVG_ALLOCATOR_H_

#include <cstdlib>
#include <cstddef>
#include <new>

//separate memory allocators for clean customization
namespace tvg
{
    template<typename T = void>
    static inline T* malloc(size_t size)
    {
        return static_cast<T*>(std::malloc(size));
    }

    template<typename T = void>
    static inline T* calloc(size_t nmem, size_t size)
    {
        return static_cast<T*>(std::calloc(nmem, size));
    }

    template<typename T = void>
    static inline T* realloc(T* ptr, size_t size)
    {
        return static_cast<T*>(std::realloc(ptr, size));
    }

    template<typename T = void>
    static inline void free(T* ptr)
    {
        std::free(ptr);
    }

    //Keep object allocation local to ThorVG, including in static builds.
    struct Allocator
    {
        static void* operator new(std::size_t size)
        {
            if (auto ptr = tvg::malloc(size ? size : 1)) return ptr;
            //ThorVG also builds without exceptions; never construct at nullptr.
            std::abort();
        }

        static void* operator new[](std::size_t size)
        {
            return Allocator::operator new(size);
        }

        static void* operator new(std::size_t size, const std::nothrow_t&) noexcept
        {
            return tvg::malloc(size ? size : 1);
        }

        static void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept
        {
            return Allocator::operator new(size, tag);
        }

        static void* operator new(std::size_t, void* ptr) noexcept
        {
            return ptr;
        }

        static void* operator new[](std::size_t, void* ptr) noexcept
        {
            return ptr;
        }

        static void operator delete(void* ptr) noexcept
        {
            tvg::free(ptr);
        }

        static void operator delete[](void* ptr) noexcept
        {
            tvg::free(ptr);
        }

        static void operator delete(void* ptr, const std::nothrow_t&) noexcept
        {
            tvg::free(ptr);
        }

        static void operator delete[](void* ptr, const std::nothrow_t&) noexcept
        {
            tvg::free(ptr);
        }

        static void operator delete(void*, void*) noexcept {}

        static void operator delete[](void*, void*) noexcept {}
    };
}

#endif //_TVG_ALLOCATOR_H_
