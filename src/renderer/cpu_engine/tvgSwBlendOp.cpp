/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

//W3C compositing in the premultiplied space: co = cs * (1 - ab) + cb * (1 - as) + as * ab * B, ao = as + ab - as * ab
//https://www.w3.org/TR/compositing-1/#blending
//x1, x2, x3 are as * ab * B of each channel in the 255 * 255 scale. Two channels share a word in 16-bit lanes,
//then each lane is rounded with v / 255 = (v + 128 + ((v + 128) >> 8)) >> 8
static uint32_t _compose(uint32_t s, uint32_t d, uint32_t x1, uint32_t x2, uint32_t x3)
{
    uint32_t sa = A(s), da = A(d);
    auto ag = ((s >> 8) & 0x00ff00ff) * (255 - da) + ((d >> 8) & 0x00ff00ff) * (255 - sa) + (sa * da << 16) + x2 + 0x00800080;
    auto rb = (s & 0x00ff00ff) * (255 - da) + (d & 0x00ff00ff) * (255 - sa) + (x1 << 16) + x3 + 0x00800080;
    return ((ag + ((ag >> 8) & 0x00ff00ff)) & 0xff00ff00) | (((rb + ((rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff);
}

//separable blending: f(s, d, sa, da) returns as * ab * B in the 255 * 255 scale from the premultiplied channels
template<typename F>
static uint32_t _blend(uint32_t s, uint32_t d, F f)
{
    //opaque source and destination: only B remains
    if ((s & d) >= 0xff000000) {
        auto b = [&](int32_t s, int32_t d) { uint32_t v = f(s, d, 255, 255) + 128; return (v + (v >> 8)) >> 8; };
        return JOIN(255, b(C1(s), C1(d)), b(C2(s), C2(d)), b(C3(s), C3(d)));
    }
    int32_t sa = A(s), da = A(d);
    return _compose(s, d, f(C1(s), C1(d), sa, da), f(C2(s), C2(d), sa, da), f(C3(s), C3(d), sa, da));
}

//W3C luminosity: 0.3 red, 0.59 green, 0.11 blue
//https://www.w3.org/TR/compositing-1/#blendingnonseparable
static int32_t _lum(const SwSurface* surface, int32_t c1, int32_t c2, int32_t c3)
{
    if (surface->cs == ColorSpace::ARGB8888 || surface->cs == ColorSpace::ARGB8888S) return (c1 * 30 + c2 * 59 + c3 * 11 + 50) / 100;
    return (c1 * 11 + c2 * 59 + c3 * 30 + 50) / 100;
}

static int32_t _sat(int32_t c1, int32_t c2, int32_t c3)
{
    return std::max(c1, std::max(c2, c3)) - std::min(c1, std::min(c2, c3));
}

//set the saturation s, only the channel ratios matter so the color needs no alpha scaling
static void _saturate(int32_t& c1, int32_t& c2, int32_t& c3, int32_t s)
{
    auto n = std::min(c1, std::min(c2, c3));
    auto x = std::max(c1, std::max(c2, c3));
    auto m = (x > n) ? (c1 + c2 + c3 - x - 2 * n) * s / (x - n) : 0;  //the middle channel
    auto f = [&](int32_t& c) { c = (c == n) ? 0 : ((c == x) ? s : m); };
    f(c1); f(c2); f(c3);
}

//set the luminosity l, clip the color into [0, as * ab], then composite it
static uint32_t _luminate(const SwSurface* surface, uint32_t s, uint32_t d, int32_t c1, int32_t c2, int32_t c3, int32_t l)
{
    int32_t a = A(s) * A(d);
    auto t = l - _lum(surface, c1, c2, c3);
    c1 += t; c2 += t; c3 += t;
    auto n = std::min(c1, std::min(c2, c3));
    auto x = std::max(c1, std::max(c2, c3));
    auto f = [&](int32_t& c) {
        if (n < 0) c = l + int64_t(c - l) * l / (l - n);
        if (x > a) c = l + int64_t(c - l) * (a - l) / (x - l);
    };
    f(c1); f(c2); f(c3);
    return _compose(s, d, c1, c2, c3);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

//the blenders composite the premultiplied source over the premultiplied destination

uint32_t blendDifference(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return abs(s * da - d * sa); });
}

uint32_t blendExclusion(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return s * da + d * sa - 2 * s * d; });
}

//plus-lighter is a compositing operator: min(1, s + d) for the colors and the alpha alike
//https://drafts.fxtf.org/compositing-2/#porterduffcompositingoperators_plus_lighter
uint32_t blendAdd(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint32_t s, uint32_t d) { return std::min(s + d, 255u); };
    return JOIN(f(A(s), A(d)), f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

//screen composes to s + d - s * d for the colors and the alpha alike
uint32_t blendScreen(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint32_t s, uint32_t d) { auto v = s * d + 128; return (v + (v >> 8)) >> 8; };
    return s + d - JOIN(f(A(s), A(d)), f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

uint32_t blendMultiply(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return s * d; });
}

uint32_t blendOverlay(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return (2 * d <= da) ? 2 * s * d : sa * da - 2 * (sa - s) * (da - d); });
}

uint32_t blendDarken(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return std::min(s * da, d * sa); });
}

uint32_t blendLighten(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return std::max(s * da, d * sa); });
}

uint32_t blendColorDodge(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) {
        if (d == 0) return 0;
        if (s >= sa) return sa * da;
        return std::min(sa * da, sa * sa * d / (sa - s));
    });
}

uint32_t blendColorBurn(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) {
        if (d >= da) return sa * da;
        if (s == 0) return 0;
        return sa * da - std::min(sa * da, sa * sa * (da - d) / s);
    });
}

uint32_t blendHardLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return (2 * s <= sa) ? 2 * s * d : sa * da - 2 * (sa - s) * (da - d); });
}

uint32_t blendSoftLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) {
        if (d == 0) return 0;
        if (2 * s <= sa) return sa * d - (sa - 2 * s) * d * (da - d) / da;
        if (4 * d <= da) return sa * d + int32_t(int64_t(2 * s - sa) * d * ((16 * d - 12 * da) * d + 3 * da * da) / (da * da));
        return sa * d + int32_t((2 * s - sa) * (sqrtf(float(d * da)) - d));
    });
}

uint32_t blendHue(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), c1 = C1(s), c2 = C2(s), c3 = C3(s);
    _saturate(c1, c2, c3, _sat(C1(d), C2(d), C3(d)) * sa);
    return _luminate(surface, s, d, c1, c2, c3, _lum(surface, C1(d) * sa, C2(d) * sa, C3(d) * sa));
}

uint32_t blendSaturation(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), c1 = C1(d), c2 = C2(d), c3 = C3(d);
    auto l = _lum(surface, c1 * sa, c2 * sa, c3 * sa);
    _saturate(c1, c2, c3, _sat(C1(s), C2(s), C3(s)) * A(d));
    return _luminate(surface, s, d, c1, c2, c3, l);
}

uint32_t blendColor(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), da = A(d);
    return _luminate(surface, s, d, C1(s) * da, C2(s) * da, C3(s) * da, _lum(surface, C1(d) * sa, C2(d) * sa, C3(d) * sa));
}

uint32_t blendLuminosity(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), da = A(d);
    return _luminate(surface, s, d, C1(d) * sa, C2(d) * sa, C3(d) * sa, _lum(surface, C1(s) * da, C2(s) * da, C3(s) * da));
}
