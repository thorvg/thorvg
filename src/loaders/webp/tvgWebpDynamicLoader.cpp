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

#include "webp/decode.h"

#if defined _WIN32 || defined __CYGWIN__
  #define DYNAMIC_API __declspec(dllexport)
#else
  #define DYNAMIC_API __attribute__ ((visibility ("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * exported function wrapper from the library
 */

DYNAMIC_API uint8_t* webp_decode_bgra(const uint8_t* data, size_t data_size, int* width, int* height)
{
    return WebPDecodeBGRA(data, data_size, width, height);
}

DYNAMIC_API int webp_get_info(const uint8_t* data, size_t data_size, int* width, int* height)
{
    return WebPGetInfo(data, data_size, width, height);
}

DYNAMIC_API void webp_free(void* ptr)
{
    // WebPFree(ptr);
}

#ifdef __cplusplus
}
#endif
