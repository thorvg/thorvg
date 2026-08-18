/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "aa_poc_cli.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace aa_poc
{

bool parseFloat(const char* text, float& value)
{
    char* end = nullptr;
    errno = 0;
    auto parsed = std::strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

ParseOptionResult parseCommonOption(int& index, int argc, char** argv,
                                    RunOptions& options)
{
    if (std::strcmp(argv[index], "--output-dir") == 0) {
        if (index + 1 >= argc) return ParseOptionResult::Error;
        options.outputDir = argv[++index];
    } else if (std::strcmp(argv[index], "--offset-x") == 0) {
        if (index + 1 >= argc || !parseFloat(argv[++index], options.offsetX)) {
            return ParseOptionResult::Error;
        }
    } else if (std::strcmp(argv[index], "--offset-y") == 0) {
        if (index + 1 >= argc || !parseFloat(argv[++index], options.offsetY)) {
            return ParseOptionResult::Error;
        }
    } else if (std::strcmp(argv[index], "--offset-grid") == 0) {
        options.offsetGrid = true;
    } else if (std::strcmp(argv[index], "--motion") == 0) {
        options.motion = true;
    } else {
        return ParseOptionResult::NotMatched;
    }
    return ParseOptionResult::Matched;
}

bool parseOptions(int argc, char** argv, RunOptions& options)
{
    for (int i = 1; i < argc; ++i) {
        if (parseCommonOption(i, argc, argv, options) != ParseOptionResult::Matched) {
            return false;
        }
    }
    return validOptions(options);
}

bool validOptions(const RunOptions& options)
{
    return !options.outputDir.empty() && !(options.offsetGrid && options.motion);
}

void printUsage(const char* executable, const char* leadingExtraSyntax)
{
    std::printf("usage: %s ", executable);
    if (leadingExtraSyntax && *leadingExtraSyntax) std::printf("%s ", leadingExtraSyntax);
    std::printf("[--output-dir DIR] [--offset-x PX] [--offset-y PX] "
                "[--offset-grid | --motion]\n");
}

bool makeOutputDirectory(const std::string& path)
{
#if defined(_WIN32)
    auto result = _mkdir(path.c_str());
#else
    auto result = mkdir(path.c_str(), 0755);
#endif
    return result == 0 || errno == EEXIST;
}

bool renderOffsets(const RunOptions& options, const RenderCallback& render)
{
    if (!validOptions(options) || !makeOutputDirectory(options.outputDir)) return false;

    auto invoke = [&](float offsetX, float offsetY, const std::string& outputDir) {
        if (!makeOutputDirectory(outputDir)) return false;
        return render(offsetX, offsetY, outputDir);
    };

    auto success = true;
    if (options.offsetGrid) {
        for (unsigned int y = 0; y < 8; ++y) {
            for (unsigned int x = 0; x < 8; ++x) {
                char name[32];
                std::snprintf(name, sizeof(name), "offset-%u-%u", x, y);
                auto outputDir = options.outputDir + "/" + name;
                success = invoke(options.offsetX + x / 8.0f,
                                 options.offsetY + y / 8.0f, outputDir) && success;
            }
        }
    } else if (options.motion) {
        for (unsigned int frame = 0; frame <= 64; ++frame) {
            char name[32];
            std::snprintf(name, sizeof(name), "frame-%03u", frame);
            auto outputDir = options.outputDir + "/" + name;
            success = invoke(options.offsetX + frame / 64.0f,
                             options.offsetY, outputDir) && success;
        }
    } else {
        success = invoke(options.offsetX, options.offsetY, options.outputDir);
    }
    return success;
}

} // namespace aa_poc
