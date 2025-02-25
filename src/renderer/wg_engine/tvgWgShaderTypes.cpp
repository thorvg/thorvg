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

#include "tvgWgShaderTypes.h"
#include <cassert>
#include "tvgMath.h"

//************************************************************************
// WgShaderTypeMat4x4f
//************************************************************************

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

//************************************************************************
// WgShaderTypeVec4f
//************************************************************************

WgShaderTypeVec4f::WgShaderTypeVec4f(const ColorSpace colorSpace, uint8_t o)
{
    update(colorSpace, o);
}


WgShaderTypeVec4f::WgShaderTypeVec4f(const RenderColor& c)
{
    update(c);
}


void WgShaderTypeVec4f::update(const ColorSpace colorSpace, uint8_t o)
{
    vec[0] = (uint32_t)colorSpace;
    vec[3] = o / 255.0f;
}


void WgShaderTypeVec4f::update(const RenderColor& c)
{
    vec[0] = c.r / 255.0f; // red
    vec[1] = c.g / 255.0f; // green
    vec[2] = c.b / 255.0f; // blue
    vec[3] = c.a / 255.0f; // alpha
}

void WgShaderTypeVec4f::update(const RenderRegion& r)
{
    vec[0] = r.x; // left
    vec[1] = r.y; // top
    vec[2] = r.x + r.w - 1; // right
    vec[3] = r.y + r.h - 1; // bottom
}

//************************************************************************
// WgShaderTypeGradient
//************************************************************************

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
    radialGradient->radial(&settings[0], &settings[1], &settings[2], &settings[4], &settings[5], &settings[6]);
};


void WgShaderTypeGradient::updateTexData(const Fill::ColorStop* stops, uint32_t stopCnt)
{
    assert(stops);
    static Array<Fill::ColorStop> sstops(stopCnt);
    sstops.clear();
    sstops.push(stops[0]);
    // filter by increasing offset
    for (uint32_t i = 1; i < stopCnt; i++)
        if (sstops.last().offset < stops[i].offset)
            sstops.push(stops[i]);
        else if (sstops.last().offset == stops[i].offset)
            sstops.last() = stops[i];
    // head
    uint32_t range_s = 0;
    uint32_t range_e = uint32_t(sstops[0].offset * (WG_TEXTURE_GRADIENT_SIZE-1));
    for (uint32_t ti = range_s; (ti < range_e) && (ti < WG_TEXTURE_GRADIENT_SIZE); ti++) {
        texData[ti * 4 + 0] = sstops[0].r;
        texData[ti * 4 + 1] = sstops[0].g;
        texData[ti * 4 + 2] = sstops[0].b;
        texData[ti * 4 + 3] = sstops[0].a;
    }
    // body
    for (uint32_t di = 1; di < sstops.count; di++) {
        range_s = uint32_t(sstops[di-1].offset * (WG_TEXTURE_GRADIENT_SIZE-1));
        range_e = uint32_t(sstops[di-0].offset * (WG_TEXTURE_GRADIENT_SIZE-1));
        float delta = 1.0f/(range_e - range_s);
        for (uint32_t ti = range_s; (ti < range_e) && (ti < WG_TEXTURE_GRADIENT_SIZE); ti++) {
            float t = (ti - range_s) * delta;
            texData[ti * 4 + 0] = lerp(sstops[di-1].r, sstops[di].r, t);
            texData[ti * 4 + 1] = lerp(sstops[di-1].g, sstops[di].g, t);
            texData[ti * 4 + 2] = lerp(sstops[di-1].b, sstops[di].b, t);
            texData[ti * 4 + 3] = lerp(sstops[di-1].a, sstops[di].a, t);
        }
    }
    // tail
    const tvg::Fill::ColorStop& colorStopLast = sstops.last();
    range_s = uint32_t(colorStopLast.offset * (WG_TEXTURE_GRADIENT_SIZE-1));
    range_e = WG_TEXTURE_GRADIENT_SIZE;
    for (uint32_t ti = range_s; ti < range_e; ti++) {
        texData[ti * 4 + 0] = colorStopLast.r;
        texData[ti * 4 + 1] = colorStopLast.g;
        texData[ti * 4 + 2] = colorStopLast.b;
        texData[ti * 4 + 3] = colorStopLast.a;
    }
}

//************************************************************************
// WgShaderTypeEffectParams
//************************************************************************

void WgShaderTypeEffectParams::update(const RenderEffectGaussianBlur* gaussian, const Matrix& transform)
{
    assert(gaussian);
    const float sigma = gaussian->sigma;
    const float scale = std::sqrt(transform.e11 * transform.e11 + transform.e12 * transform.e12);
    const float kernel = std::min(WG_GAUSSIAN_KERNEL_SIZE_MAX, 2 * sigma * scale); // kernel size
    params[0] = sigma;
    params[1] = std::min(WG_GAUSSIAN_KERNEL_SIZE_MAX / kernel, scale);
    params[2] = kernel;
    params[3] = 0.0f;
    extend = params[2] * 2; // kernel
}


void WgShaderTypeEffectParams::update(const RenderEffectDropShadow* dropShadow, const Matrix& transform)
{
    assert(dropShadow);
    const float radian = tvg::deg2rad(90.0f - dropShadow->angle);
    const float sigma = dropShadow->sigma;
    const float scale = std::sqrt(transform.e11 * transform.e11 + transform.e12 * transform.e12);
    const float kernel = std::min(WG_GAUSSIAN_KERNEL_SIZE_MAX, 2 * sigma * scale); // kernel size
    offset = {0, 0};
    if (dropShadow->distance > 0.0f) offset = {
        +1.0f * dropShadow->distance * cosf(radian) * scale,
        -1.0f * dropShadow->distance * sinf(radian) * scale
    };
    params[0] = sigma;
    params[1] = std::min(WG_GAUSSIAN_KERNEL_SIZE_MAX / kernel, scale);
    params[2] = kernel;
    params[3] = 0.0f;
    params[4] = dropShadow->color[0] / 255.0f; // red
    params[5] = dropShadow->color[1] / 255.0f; // green
    params[6] = dropShadow->color[2] / 255.0f; // blue
    params[7] = dropShadow->color[3] / 255.0f; // alpha
    params[8] = offset.x;
    params[9] = offset.y;
    extend = params[2] * 2; // kernel
}


void WgShaderTypeEffectParams::update(const RenderEffectFill* fill)
{
    params[0] = fill->color[0] / 255.0f;
    params[1] = fill->color[1] / 255.0f;
    params[2] = fill->color[2] / 255.0f;
    params[3] = fill->color[3] / 255.0f;
}
