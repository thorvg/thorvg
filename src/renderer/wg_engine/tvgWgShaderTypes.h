/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#ifndef _TVG_WG_SHADER_TYPES_H_
#define _TVG_WG_SHADER_TYPES_H_

#include "tvgRender.h"

///////////////////////////////////////////////////////////////////////////////
// shader types
///////////////////////////////////////////////////////////////////////////////

// mat4x4f
struct WgShaderTypeMat4x4f
{
    float mat[16]{};

    WgShaderTypeMat4x4f();
    WgShaderTypeMat4x4f(const Matrix& transform);
    WgShaderTypeMat4x4f(size_t w, size_t h);
    void identity();
    void update(const Matrix& transform);
    void update(size_t w, size_t h);
};

// vec4f
struct WgShaderTypeVec4f
{
    float vec[4]{};

    WgShaderTypeVec4f() {};
    WgShaderTypeVec4f(const ColorSpace colorSpace, uint8_t o);
    WgShaderTypeVec4f(const RenderColor& c);
    void update(const ColorSpace colorSpace, uint8_t o);
    void update(const RenderColor& c);
};

// sampler, texture, vec4f
#define WG_TEXTURE_GRADIENT_SIZE 512
struct WgShaderTypeGradient
{
    float settings[4+4]{}; // WGSL: struct GradSettings { settings: vec4f, focal: vec4f; transform: mat4f };
    uint8_t texData[WG_TEXTURE_GRADIENT_SIZE * 4];

    void update(const LinearGradient* linearGradient);
    void update(const RadialGradient* radialGradient);
    void updateTexData(const Fill::ColorStop* stops, uint32_t stopCnt);
};

#endif // _TVG_WG_SHADER_TYPES_H_
