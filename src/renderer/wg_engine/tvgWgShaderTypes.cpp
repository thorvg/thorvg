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
#include <cassert>

///////////////////////////////////////////////////////////////////////////////
// shader types
///////////////////////////////////////////////////////////////////////////////

WgShaderTypeMat4x4f::WgShaderTypeMat4x4f()
{
    identity();
}


WgShaderTypeMat4x4f::WgShaderTypeMat4x4f(const Matrix& transform)
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


void WgShaderTypeMat4x4f::update(const Matrix& transform)
{

    mat[0]  = transform.e11;
    mat[1]  = transform.e21;
    mat[2]  = 0.0f;
    mat[3]  = transform.e31;
    mat[4]  = transform.e12;
    mat[5]  = transform.e22;
    mat[6]  = 0.0f;
    mat[7]  = transform.e32;
    mat[8]  = 0.0f;
    mat[9]  = 0.0f;
    mat[10] = 1.0f;
    mat[11] = 0.0f;
    mat[12] = transform.e13;
    mat[13] = transform.e23;
    mat[14] = 0.0f;
    mat[15] = transform.e33;
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
    settings[0] = (uint32_t)colorSpace;
    settings[3] = o / 255.0f;
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


void WgShaderTypeGradient::update(const LinearGradient* linearGradient)
{
    // update gradient data
    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = linearGradient->colorStops(&stops);
    updateTexData(stops, stopCnt);
    // update base points
    linearGradient->linear(&settings[0], &settings[1], &settings[2], &settings[3]);
};


void WgShaderTypeGradient::update(const RadialGradient* radialGradient)
{
    // update gradient data
    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = radialGradient->colorStops(&stops);
    updateTexData(stops, stopCnt);
    // update base points
    radialGradient->radial(&settings[2], &settings[3], &settings[0]);
};


void WgShaderTypeGradient::updateTexData(const Fill::ColorStop* stops, uint32_t stopCnt)
{
    assert(stops);
    // head
    uint32_t range_s = 0;
    uint32_t range_e = uint32_t(stops[0].offset * (WG_TEXTURE_GRADIENT_SIZE-1));
    for (uint32_t ti = range_s; (ti < range_e) && (ti < WG_TEXTURE_GRADIENT_SIZE); ti++) {
        texData[ti * 4 + 0] = stops[0].r;
        texData[ti * 4 + 1] = stops[0].g;
        texData[ti * 4 + 2] = stops[0].b;
        texData[ti * 4 + 3] = stops[0].a;
    }
    // body
    for (uint32_t di = 1; di < stopCnt; di++) {
        range_s = uint32_t(stops[di-1].offset * (WG_TEXTURE_GRADIENT_SIZE-1));
        range_e = uint32_t(stops[di-0].offset * (WG_TEXTURE_GRADIENT_SIZE-1));
        for (uint32_t ti = range_s; (ti < range_e) && (ti < WG_TEXTURE_GRADIENT_SIZE); ti++) {
            float t = float(ti - range_s) / (range_e - range_s);
            texData[ti * 4 + 0] = uint8_t((1.0f - t) * stops[di-1].r + t * stops[di].r);
            texData[ti * 4 + 1] = uint8_t((1.0f - t) * stops[di-1].g + t * stops[di].g);
            texData[ti * 4 + 2] = uint8_t((1.0f - t) * stops[di-1].b + t * stops[di].b);
            texData[ti * 4 + 3] = uint8_t((1.0f - t) * stops[di-1].a + t * stops[di].a);
        }
    }
    // tail
    range_s = uint32_t(stops[stopCnt-1].offset * (WG_TEXTURE_GRADIENT_SIZE-1));
    range_e = WG_TEXTURE_GRADIENT_SIZE;
    for (uint32_t ti = range_s; ti < range_e; ti++) {
        texData[ti * 4 + 0] = stops[stopCnt-1].r;
        texData[ti * 4 + 1] = stops[stopCnt-1].g;
        texData[ti * 4 + 2] = stops[stopCnt-1].b;
        texData[ti * 4 + 3] = stops[stopCnt-1].a;
    }
}
