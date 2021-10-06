/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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

#include <math.h>
#include <memory.h>
#include <ctype.h>
#include <errno.h>
#include "tvgSvgUtil.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static inline bool _floatExact(float a, float b)
{
    return memcmp(&a, &b, sizeof (float)) == 0;
}

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


/*
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/strtof-strtof-l-wcstof-wcstof-l?view=msvc-160
 *
 * src should be one of the following form :
 *
 * [whitespace] [sign] {digits [radix digits] | radix digits} [{e | E} [sign] digits]
 * [whitespace] [sign] {INF | INFINITY}
 * [whitespace] [sign] NAN [sequence]
 *
 * No hexadecimal form supported
 * no sequence supported after NAN
 */
float svgUtilStrtof(const char *nPtr, char **endPtr)
{
    const char *iter;
    const char *a;
    float val;
    unsigned long long integerPart;
    int minus;

    if (endPtr) *endPtr = (char*)nPtr;
    if (!nPtr) return 0.0f;

    a = iter = nPtr;

    //ignore leading whitespaces
    while (isspace(*iter)) iter++;

    //signed or not
    minus = 1;
    if (*iter == '-')
    {
        minus = -1;
        iter++;
    }
    else if (*iter == '+') iter++;

    if (tolower(*iter) == 'i')
    {
        if ((tolower(*(iter + 1)) == 'n') && (tolower(*(iter + 2)) == 'f'))
        {
            iter += 3;
        }
        else goto on_error;

        if (tolower(*(iter + 3)) == 'i')
        {
            if ((tolower(*(iter + 4)) == 'n') &&
                (tolower(*(iter + 5)) == 'i') &&
                (tolower(*(iter + 6)) == 't') &&
                (tolower(*(iter + 7)) == 'y'))
            {
               iter += 5;
            }
            else goto on_error;
         }
         if (endPtr) *endPtr = (char *)iter;
         return (minus == -1) ? -INFINITY : INFINITY;
    }

    if (tolower(*iter) == 'n')
    {
         if ((tolower(*(iter + 1)) == 'a') && (tolower(*(iter + 2)) == 'n')) iter += 3;
         else goto on_error;

         if (endPtr) *endPtr = (char *)iter;
         return (minus == -1) ? -NAN : NAN;
    }

    integerPart = 0;

    //(optional) integer part before dot
    if (isdigit(*iter))
    {
        for (; isdigit(*iter); iter++) integerPart = integerPart * 10ULL + (unsigned long long)(*iter - '0');

        a = iter;
    }
    else if (*iter != '.')
    {
        val = 0.0;
        goto on_success;
    }

    val = (float)integerPart;

    //(optional) decimal part after dot
    if (*iter == '.')
    {
        unsigned long long decimalPart;
        unsigned long long pow10;
        int count;

        iter++;

        decimalPart = 0;
        count = 0;
        pow10 = 1;

        if (isdigit(*iter))
        {
            for (; isdigit(*iter); iter++, count++)
            {
                if (count < 19)
                {
                    decimalPart = decimalPart * 10ULL +  + (unsigned long long)(*iter - '0');
                    pow10 *= 10ULL;
                }
            }
        }
        val += (float)decimalPart / (float)pow10;
        a = iter;
    }

    //(optional) exponent
    if ((*iter == 'e') || (*iter == 'E'))
    {
        float scale = 1.0f;
        unsigned int expo_part;
        int minus_e;

        ++iter;

        //Exception: svg may have 'em' unit for fonts. ex) 5em, 10.5em
        if ((*iter == 'm') || (*iter == 'M')) {
            //TODO: We don't support font em unit now, but has to multiply val * font size later...
            a = iter + 1;
            goto on_success;
        }

        //signed or not
        minus_e = 1;
        if (*iter == '-')
        {
            minus_e = -1;
            ++iter;
        }
        else if (*iter == '+') iter++;

        //exponential part
        expo_part = 0;
        if (isdigit(*iter))
        {
            while (*iter == 0) iter++;

            for (; isdigit(*iter); iter++)
            {
                expo_part = expo_part * 10U + (unsigned int)(*iter - '0');
            }
        }
        else if (!isdigit(*(a - 1)))
        {
            a = nPtr;
            goto on_success;
        }
        else if (*iter == 0) goto on_success;

        if ((_floatExact(val, 2.2250738585072011f)) && ((minus_e * (int)expo_part) == -308))
        {
            val *= 1.0e-308f;
            a = iter;
            errno = ERANGE;
            goto on_success;
        }

        if ((_floatExact(val, 2.2250738585072012f)) && ((minus_e * (int)expo_part) <= -308))
        {
            val *= 1.0e-308f;
            a = iter;
            goto on_success;
        }

        a = iter;

        while (expo_part >= 8U)
        {
            scale *= 1E8;
            expo_part -= 8U;
        }
        while (expo_part > 0U)
        {
            scale *= 10.0f;
            expo_part--;
        }

        val = (minus_e == -1) ? (val / scale) : (val * scale);
    }
    else if ((iter > nPtr) && !isdigit(*(iter - 1)))
    {
        a = nPtr;
        goto on_success;
    }

on_success:
    if (endPtr) *endPtr = (char *)a;
    return minus * val;

on_error:
    if (endPtr) *endPtr = (char *)nPtr;
    return 0.0f;
}

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

