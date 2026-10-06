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

//W3C compositing in the premultiplied space: s * (1 - Da) + d * (1 - Sa) + Sa * Da * B(Cs, Cb)
//x1, x2, x3: Sa * Da * B(Cs, Cb) per channel in the 255 * 255 scale
static uint32_t _compose(uint32_t s, uint32_t d, int32_t x1, int32_t x2, int32_t x3)
{
    int32_t sa = A(s), da = A(d);
    auto c = [&](int32_t s, int32_t d, int32_t x) { return (s * (255 - da) + d * (255 - sa) + x + 127) / 255; };
    return JOIN(sa + da - (sa * da + 127) / 255, c(C1(s), C1(d), x1), c(C2(s), C2(d), x2), c(C3(s), C3(d), x3));
}

template<typename F>
static uint32_t _separate(uint32_t s, uint32_t d, F f)
{
    int32_t sa = A(s), da = A(d);
    return _compose(s, d, f(C1(s), C1(d), sa, da), f(C2(s), C2(d), sa, da), f(C3(s), C3(d), sa, da));
}

//W3C luminosity: 0.3 * R + 0.59 * G + 0.11 * B
static int32_t _lum(const SwSurface* surface, int32_t c1, int32_t c2, int32_t c3)
{
    auto w = surface->join(30, 59, 11, 0);
    return (c1 * C1(w) + c2 * C2(w) + c3 * C3(w) + 50) / 100;
}

static int32_t _sat(int32_t c1, int32_t c2, int32_t c3)
{
    return std::max(c1, std::max(c2, c3)) - std::min(c1, std::min(c2, c3));
}

static void _setSat(int32_t& c1, int32_t& c2, int32_t& c3, int32_t s)
{
    auto n = std::min(c1, std::min(c2, c3));
    auto x = std::max(c1, std::max(c2, c3));
    auto f = [&](int32_t& c) { c = (x > n) ? int32_t(int64_t(c - n) * s / (x - n)) : 0; };
    f(c1); f(c2); f(c3);
}

//set the luminosity, then clip the color into [0, a]
static void _setLum(const SwSurface* surface, int32_t& c1, int32_t& c2, int32_t& c3, int32_t l, int32_t a)
{
    auto t = l - _lum(surface, c1, c2, c3);
    c1 += t; c2 += t; c3 += t;
    auto n = std::min(c1, std::min(c2, c3));
    auto x = std::max(c1, std::max(c2, c3));
    auto f = [&](int32_t& c) {
        if (n < 0) c = l + int32_t(int64_t(c - l) * l / (l - n));
        if (x > a) c = l + int32_t(int64_t(c - l) * (a - l) / (x - l));
    };
    f(c1); f(c2); f(c3);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

uint32_t opBlendMethod(const SwSurface* surface, uint32_t s, uint32_t d, uint8_t a)
{
    //colors are clamped by the alpha to tolerate invalid premultiplied pixels
    auto valid = [](uint32_t c) { auto a = A(c); return JOIN(a, std::min(C1(c), a), std::min(C2(c), a), std::min(C3(c), a)); };
    auto t = surface->blender(surface, valid(s), valid(d));
    return (a == 255) ? t : INTERPOLATE(t, d, a);
}

uint32_t blendDifference(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return abs(s * da - d * sa); });
}

uint32_t blendExclusion(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return s * da + d * sa - 2 * s * d; });
}

uint32_t blendAdd(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return std::min(s * da + d * sa, sa * da); });
}

uint32_t blendScreen(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return s * da + d * sa - s * d; });
}

uint32_t blendMultiply(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return s * d; });
}

uint32_t blendOverlay(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return (2 * d <= da) ? 2 * s * d : sa * da - 2 * (sa - s) * (da - d); });
}

uint32_t blendDarken(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return std::min(s * da, d * sa); });
}

uint32_t blendLighten(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return std::max(s * da, d * sa); });
}

uint32_t blendColorDodge(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return (d == 0) ? 0 : ((s >= sa) ? sa * da : std::min(sa * sa * d / (sa - s), sa * da)); });
}

uint32_t blendColorBurn(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return (d >= da) ? sa * da : ((s == 0) ? 0 : sa * da - std::min(sa * sa * (da - d) / s, sa * da)); });
}

uint32_t blendHardLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) { return (2 * s <= sa) ? 2 * s * d : sa * da - 2 * (sa - s) * (da - d); });
}

uint32_t blendSoftLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _separate(s, d, [](int32_t s, int32_t d, int32_t sa, int32_t da) {
        if (da == 0) return 0;
        if (2 * s <= sa) return sa * d - (sa - 2 * s) * d * (da - d) / da;
        if (4 * d <= da) return sa * d + int32_t(int64_t(2 * s - sa) * d * ((16 * d - 12 * da) * d + 3 * da * da) / (da * da));
        return sa * d + (2 * s - sa) * (int32_t(sqrtf(float(d * da)) * 256.0f) - 256 * d) / 256;
    });
}

uint32_t blendHue(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), da = A(d), c1 = C1(s) * da, c2 = C2(s) * da, c3 = C3(s) * da;
    _setSat(c1, c2, c3, _sat(C1(d), C2(d), C3(d)) * sa);
    _setLum(surface, c1, c2, c3, _lum(surface, C1(d) * sa, C2(d) * sa, C3(d) * sa), sa * da);
    return _compose(s, d, c1, c2, c3);
}

uint32_t blendSaturation(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), da = A(d), c1 = C1(d) * sa, c2 = C2(d) * sa, c3 = C3(d) * sa;
    auto l = _lum(surface, c1, c2, c3);
    _setSat(c1, c2, c3, _sat(C1(s), C2(s), C3(s)) * da);
    _setLum(surface, c1, c2, c3, l, sa * da);
    return _compose(s, d, c1, c2, c3);
}

uint32_t blendColor(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), da = A(d), c1 = C1(s) * da, c2 = C2(s) * da, c3 = C3(s) * da;
    _setLum(surface, c1, c2, c3, _lum(surface, C1(d) * sa, C2(d) * sa, C3(d) * sa), sa * da);
    return _compose(s, d, c1, c2, c3);
}

uint32_t blendLuminosity(const SwSurface* surface, uint32_t s, uint32_t d)
{
    int32_t sa = A(s), da = A(d), c1 = C1(d) * sa, c2 = C2(d) * sa, c3 = C3(d) * sa;
    _setLum(surface, c1, c2, c3, _lum(surface, C1(s) * da, C2(s) * da, C3(s) * da), sa * da);
    return _compose(s, d, c1, c2, c3);
}
