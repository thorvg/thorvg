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

#ifndef _TVG_ENV_H_
#define _TVG_ENV_H_

#include "tvgCommon.h"

#ifdef _WIN32
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
#else
    #include <limits.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif


namespace tvg
{
    inline bool cachedir(char* output, size_t maxLen)
    {
#if !defined(THORVG_FILE_IO_SUPPORT) || defined(__EMSCRIPTEN__)
        return false;
#endif

#ifdef _WIN32
        // Windows: %LOCALAPPDATA%\thorvg
        auto appData = getenv("LOCALAPPDATA");
        if (!appData) {
            appData = getenv("APPDATA");
            if (!appData) return false;
        }
        snprintf(output, maxLen, "%s\\thorvg", appData);

        auto attribs = GetFileAttributesA(output);
        if (attribs == INVALID_FILE_ATTRIBUTES) {
            char thorvgDir[PATH_MAX];
            snprintf(thorvgDir, PATH_MAX, "%s\\thorvg", appData);

            if ((!CreateDirectoryA(thorvgDir, NULL) || !CreateDirectoryA(output, NULL)) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
        }
#elif defined(__linux__)
        // Linux: $XDG_CACHE_HOME/thorvg or ~/.cache/thorvg
        auto xdgCache = getenv("XDG_CACHE_HOME");
        if (xdgCache) {
            snprintf(output, maxLen, "%s/thorvg", xdgCache);
        } else {
            auto home = getenv("HOME");
            if (!home) return false;
            snprintf(output, maxLen, "%s/.cache/thorvg", home);
        }

        struct stat st = {};
        if (stat(output, &st) == -1) {
            if (xdgCache) {
                char thorvgDir[PATH_MAX];
                snprintf(thorvgDir, PATH_MAX, "%s/thorvg", xdgCache);
                mkdir(thorvgDir, 0755);
                mkdir(output, 0755);
            } else {
                auto home = getenv("HOME");
                if (!home) return false;
                char cacheBaseDir[PATH_MAX];
                snprintf(cacheBaseDir, PATH_MAX, "%s/.cache", home);
                mkdir(cacheBaseDir, 0755);

                char thorvgDir[PATH_MAX];
                snprintf(thorvgDir, PATH_MAX, "%s/.cache/thorvg", home);
                mkdir(thorvgDir, 0755);
                mkdir(output, 0755);
            }
        }
#elif defined(__APPLE__)
        // macOS: ~/Library/Caches/thorvg
        auto home = getenv("HOME");
        if (!home) return false;
        snprintf(output, maxLen, "%s/Library/Caches/thorvg", home);
        struct stat st = {0};
        if (stat(output, &st) == -1) {
            // Create thorvg directory first
            char thorvgDir[PATH_MAX];
            snprintf(thorvgDir, PATH_MAX, "%s/Library/Caches/thorvg", home);
            mkdir(thorvgDir, 0755);
            mkdir(output, 0755);
        }
#else
        return false;
#endif

        return true;
    }
}

#endif //_TVG_ENV_H_
