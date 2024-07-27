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

#include "tvgWgShaderTypes.h"

///////////////////////////////////////////////////////////////////////////////
// shader types
///////////////////////////////////////////////////////////////////////////////

WgShaderTypeMat4x4f::WgShaderTypeMat4x4f()
{
    identity();
}


WgShaderTypeMat4x4f::WgShaderTypeMat4x4f(const Matrix* transform)
{
    update(transform);
}


void WgShaderTypeMat4x4f::identity()
{
    mat[0]  = 1.0f; mat[1]  = 0.0f; mat[2]  = 0.0f; mat[3]  = 0.0f;
    mat[4]  = 0.0f; mat[5]  = 1.0f; mat[6]  = 0.0f; mat[7]  = 0.0f;
    mat[8]  = 0.0f; mat[9]  = 0.0f; mat[10] = 1.0f; mat[11] = 0.0f;
    mat[12] = 0.0f; mat[13] = 0.0f; mat[14] = 0.0f; mat[15] = 1.0f;
}


WgShaderTypeMat4x4f::WgShaderTypeMat4x4f(size_t w, size_t h)
{
    update(w, h);
}


void WgShaderTypeMat4x4f::update(const Matrix* transform)
{
    identity();
    if (transform) {
        mat[0]  = transform->e11;
        mat[1]  = transform->e21;
        mat[2]  = 0.0f;
        mat[3]  = transform->e31;
        mat[4]  = transform->e12;
        mat[5]  = transform->e22;
        mat[6]  = 0.0f;
        mat[7]  = transform->e32;
        mat[8]  = 0.0f;
        mat[9]  = 0.0f;
        mat[10] = 1.0f;
        mat[11] = 0.0f;
        mat[12] = transform->e13;
        mat[13] = transform->e23;
        mat[14] = 0.0f;
        mat[15] = transform->e33;
    };
}


void WgShaderTypeMat4x4f::update(size_t w, size_t h)
{
    mat[0]  = +2.0f / w; mat[1]  = +0.0f;     mat[2]  = +0.0f; mat[3]  = +0.0f;
    mat[4]  = +0.0f;     mat[5]  = -2.0f / h; mat[6]  = +0.0f; mat[7]  = +0.0f;
    mat[8]  = +0.0f;     mat[9]  = +0.0f;     mat[10] = -1.0f; mat[11] = +0.0f;
    mat[12] = -1.0f;     mat[13] = +1.0f;     mat[14] = +0.0f; mat[15] = +1.0f;
}


WgShaderTypeBlendSettings::WgShaderTypeBlendSettings(const ColorSpace colorSpace, uint8_t o)
{
    update(colorSpace, o);
}


void WgShaderTypeBlendSettings::update(const ColorSpace colorSpace, uint8_t o)
{
    format = (uint32_t)colorSpace;
    dummy0 = 0.0f;
    dummy1 = 0.0f;
    opacity = o / 255.0f;
}


WgShaderTypeSolidColor::WgShaderTypeSolidColor(const uint8_t* c)
{
    update(c);
}


void WgShaderTypeSolidColor::update(const uint8_t* c)
{
    color[0] = c[0] / 255.0f; // red
    color[1] = c[1] / 255.0f; // green
    color[2] = c[2] / 255.0f; // blue
    color[3] = c[3] / 255.0f; // alpha
}


WgShaderTypeLinearGradient::WgShaderTypeLinearGradient(const LinearGradient* linearGradient)
{
    update(linearGradient);
}


void WgShaderTypeLinearGradient::update(const LinearGradient* linearGradient)
{
    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = linearGradient->colorStops(&stops);
    
    nStops = stopCnt;
    spread = uint32_t(linearGradient->spread());
    
    for (uint32_t i = 0; i < stopCnt; ++i) {
        stopPoints[i] = stops[i].offset;
        stopColors[i * 4 + 0] = stops[i].r / 255.f;
        stopColors[i * 4 + 1] = stops[i].g / 255.f;
        stopColors[i * 4 + 2] = stops[i].b / 255.f;
        stopColors[i * 4 + 3] = stops[i].a / 255.f;
    }
    
    linearGradient->linear(&startPos[0], &startPos[1], &endPos[0], &endPos[1]);
}


WgShaderTypeRadialGradient::WgShaderTypeRadialGradient(const RadialGradient* radialGradient)
{
    update(radialGradient);
}


void WgShaderTypeRadialGradient::update(const RadialGradient* radialGradient)
{
    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = radialGradient->colorStops(&stops);

    nStops = stopCnt;
    spread = uint32_t(radialGradient->spread());

    for (uint32_t i = 0; i < stopCnt; ++i) {
        stopPoints[i] = stops[i].offset;
        stopColors[i * 4 + 0] = stops[i].r / 255.f;
        stopColors[i * 4 + 1] = stops[i].g / 255.f;
        stopColors[i * 4 + 2] = stops[i].b / 255.f;
        stopColors[i * 4 + 3] = stops[i].a / 255.f;
    }

    radialGradient->radial(&centerPos[0], &centerPos[1], &radius[0]);
}
