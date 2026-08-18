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

#ifndef THORVG_TOOLS_AA_POC_FLAT_MASK_SCENE_H
#define THORVG_TOOLS_AA_POC_FLAT_MASK_SCENE_H

#include <cstdint>

#include <thorvg.h>

namespace aa_poc
{

constexpr uint32_t FLAT_MASK_WIDTH = 640;
constexpr uint32_t FLAT_MASK_HEIGHT = 360;

bool checkThorvg(tvg::Result result, const char* operation, const char* diagnosticName);

bool populateFlatMaskScene(tvg::GlCanvas& canvas, float offsetX, float offsetY,
                           float scale, const char* diagnosticName);

} // namespace aa_poc

#endif // THORVG_TOOLS_AA_POC_FLAT_MASK_SCENE_H
