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

#ifndef _TVG_COMMON_H_
#define _TVG_COMMON_H_

#include "config.h"
#include "thorvg.h"

using namespace std;
using namespace tvg;

//for MSVC Compat
#ifdef _MSC_VER
    #define TVG_UNUSED
    #define strncasecmp _strnicmp
    #define strcasecmp _stricmp
#else
    #define TVG_UNUSED __attribute__ ((__unused__))
#endif

// Portable 'fallthrough' attribute
#if __has_cpp_attribute(fallthrough)
    #ifdef _MSC_VER
        #define TVG_FALLTHROUGH [[fallthrough]];
    #else
        #define TVG_FALLTHROUGH __attribute__ ((fallthrough));
    #endif
#else
    #define TVG_FALLTHROUGH
#endif

#if defined(_MSC_VER) && defined(__clang__)
    #define strncpy strncpy_s
    #define strdup _strdup
#endif

//TVG class identifier values
#define TVG_CLASS_ID_UNDEFINED 0
#define TVG_CLASS_ID_SHAPE     1
#define TVG_CLASS_ID_SCENE     2
#define TVG_CLASS_ID_PICTURE   3
#define TVG_CLASS_ID_LINEAR    4
#define TVG_CLASS_ID_RADIAL    5

enum class FileType { Tvg = 0, Svg, Raw, Png, Jpg, Unknown };

#ifdef THORVG_LOG_ENABLED
    constexpr auto ErrorColor = "\033[31m";  //red
    constexpr auto LogColor = "\033[32m";    //green
    constexpr auto GreyColor = "\033[90m";   //grey
    constexpr auto ResetColor = "\033[0m";   //default
    #define TVGERR(tag, fmt, ...) fprintf(stderr, "%s" tag "%s (%s l.%d): %s" fmt "\n", ErrorColor, GreyColor, __FILE__, __LINE__, ResetColor, ##__VA_ARGS__)
    #define TVGLOG(tag, fmt, ...) fprintf(stderr, "%s" tag "%s (%s l.%d): %s" fmt "\n", LogColor, GreyColor, __FILE__, __LINE__, ResetColor, ##__VA_ARGS__)
#else
    #define TVGERR(...)
    #define TVGLOG(...)
#endif

uint16_t THORVG_VERSION_NUMBER();

#endif //_TVG_COMMON_H_
