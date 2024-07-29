/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#include "tvgWgCommon.h"

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

// struct BlendSettings {
//     format  : u32, // ColorSpace
//     dummy0  : f32,
//     dummy1  : f32,
//     opacity : f32
// };
struct WgShaderTypeBlendSettings
{
    uint32_t format{}; // ColorSpace
    float dummy0{};
    float dummy1{};
    float opacity{};

    WgShaderTypeBlendSettings() {};
    WgShaderTypeBlendSettings(const ColorSpace colorSpace, uint8_t o);
    void update(const ColorSpace colorSpace, uint8_t o);
};

// struct SolidColor {
//     color: vec4f
// };
struct WgShaderTypeSolidColor
{
    float color[4]{};

    WgShaderTypeSolidColor(const uint8_t* c);
    void update(const uint8_t* c);
};

// const MAX_LINEAR_GRADIENT_STOPS = 4;
// struct LinearGradient {
//     nStops       : u32,
//     spread       : u32,
//     dummy0       : u32,
//     dummy1       : u32,
//     gradStartPos : vec2f,
//     gradEndPos   : vec2f,
//     stopPoints   : vec4f,
//     stopColors   : array<vec4f, MAX_LINEAR_GRADIENT_STOPS>
// };
#define MAX_LINEAR_GRADIENT_STOPS 32
struct WgShaderTypeLinearGradient
{
    uint32_t nStops{};
    uint32_t spread{};
    uint32_t dummy0{}; // align with WGSL struct
    uint32_t dummy1{}; // align with WGSL struct
    float startPos[2]{};
    float endPos[2]{};
    float stopPoints[MAX_LINEAR_GRADIENT_STOPS]{};
    float stopColors[4 * MAX_LINEAR_GRADIENT_STOPS]{};

    WgShaderTypeLinearGradient(const LinearGradient* linearGradient);
    void update(const LinearGradient* linearGradient);
};

// const MAX_RADIAL_GRADIENT_STOPS = 4;
// struct RadialGradient {
//     nStops     : u32,
//     spread     : u32,
//     dummy0     : u32,
//     dummy1     : u32,
//     centerPos  : vec2f,
//     radius     : vec2f,
//     stopPoints : vec4f,
//     stopColors : array<vec4f, MAX_RADIAL_GRADIENT_STOPS>
// };
#define MAX_RADIAL_GRADIENT_STOPS 32
struct WgShaderTypeRadialGradient
{
    uint32_t nStops{};
    uint32_t spread{};
    uint32_t dummy0{}; // align with WGSL struct
    uint32_t dummy1{}; // align with WGSL struct
    float centerPos[2]{};
    float radius[2]{};
    float stopPoints[MAX_RADIAL_GRADIENT_STOPS]{};
    float stopColors[4 * MAX_RADIAL_GRADIENT_STOPS]{};

    WgShaderTypeRadialGradient(const RadialGradient* radialGradient);
    void update(const RadialGradient* radialGradient);
};

#endif // _TVG_WG_SHADER_TYPES_H_
