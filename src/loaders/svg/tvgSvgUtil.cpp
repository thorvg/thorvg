/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include <cstring>
#include "tvgSvgUtil.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static uint8_t _hexCharToDec(const char c)
{
    if (c >= 'a') return c - 'a' + 10;
    else if (c >= 'A') return c - 'A' + 10;
    else return c - '0';
}

static uint8_t _base64Value(const char chr)
{
    if (chr >= 'A' && chr <= 'Z') return chr - 'A';
    else if (chr >= 'a' && chr <= 'z') return chr - 'a' + ('Z' - 'A') + 1;
    else if (chr >= '0' && chr <= '9') return chr - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
    else if (chr == '+' || chr == '-') return 62;
    else return 63;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

string svgUtilURLDecode(const char *src)
{
    if (!src) return nullptr;

    auto length = strlen(src);
    if (length == 0) return nullptr;

    string decoded;
    decoded.reserve(length);

    char a, b;
    while (*src) {
        if (*src == '%' &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            decoded += (_hexCharToDec(a) << 4) + _hexCharToDec(b);
            src+=3;
        } else if (*src == '+') {
            decoded += ' ';
            src++;
        } else {
            decoded += *src++;
        }
    }
    return decoded;
}


string svgUtilBase64Decode(const char *src)
{
    if (!src) return nullptr;

    auto length = strlen(src);
    if (length == 0) return nullptr;

    string decoded;
    decoded.reserve(3*(1+(length >> 2)));

    while (*src && *(src+1)) {
        if (*src <= 0x20) {
            ++src;
            continue;
        }

        auto value1 = _base64Value(src[0]);
        auto value2 = _base64Value(src[1]);
        decoded += (value1 << 2) + ((value2 & 0x30) >> 4);

        if (!src[2] || src[2] == '=' || src[2] == '.') break;
        auto value3 = _base64Value(src[2]);
        decoded += ((value2 & 0x0f) << 4) + ((value3 & 0x3c) >> 2);

        if (!src[3] || src[3] == '=' || src[3] == '.') break;
        auto value4 = _base64Value(src[3]);
        decoded += ((value3 & 0x03) << 6) + value4;
        src += 4;
    }
    return decoded;
}