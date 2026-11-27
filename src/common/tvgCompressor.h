/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_COMPRESSOR_H_
#define _TVG_COMPRESSOR_H_

#include <cstddef>

namespace tvg
{
    std::size_t b64Decode(const char* encoded, const size_t len, char** decoded);

    inline std::size_t b64Encode(const char* data, const size_t len, char* encoded)
    {
        static constexpr const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        auto out = encoded;
        for (std::size_t i = 0; i < len; i += 3) {
            unsigned v = (unsigned char)data[i] << 16;
            if (i + 1 < len) v |= (unsigned char)data[i + 1] << 8;
            if (i + 2 < len) v |= (unsigned char)data[i + 2];
            *out++ = table[(v >> 18) & 63];
            *out++ = table[(v >> 12) & 63];
            *out++ = i + 1 < len ? table[(v >> 6) & 63] : '=';
            *out++ = i + 2 < len ? table[v & 63] : '=';
        }
        return out - encoded;
    }

    unsigned long djb2Encode(const char* str);
}

#endif  //_TVG_COMPRESSOR_H_
