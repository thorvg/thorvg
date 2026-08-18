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

#ifndef THORVG_TOOLS_AA_POC_CLI_H
#define THORVG_TOOLS_AA_POC_CLI_H

#include <functional>
#include <string>

namespace aa_poc
{

struct RunOptions
{
    std::string outputDir;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    bool offsetGrid = false;
    bool motion = false;
};

enum class ParseOptionResult
{
    Matched,
    NotMatched,
    Error,
};

using RenderCallback = std::function<bool(float offsetX, float offsetY,
                                          const std::string& outputDir)>;

bool parseFloat(const char* text, float& value);
ParseOptionResult parseCommonOption(int& index, int argc, char** argv,
                                    RunOptions& options);
bool parseOptions(int argc, char** argv, RunOptions& options);
bool validOptions(const RunOptions& options);
void printUsage(const char* executable, const char* leadingExtraSyntax = nullptr);
bool makeOutputDirectory(const std::string& path);
bool renderOffsets(const RunOptions& options, const RenderCallback& render);

} // namespace aa_poc

#endif // THORVG_TOOLS_AA_POC_CLI_H
