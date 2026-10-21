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

//v / 255 rounded, exact while v fits in 16 bits
static uint32_t _div255(uint32_t v)
{
    v += 128;
    return (v + (v >> 8)) >> 8;
}

//W3C compositing in the premultiplied space: co = cs * (1 - ab) + cb * (1 - as) + as * ab * B, ao = as + ab - as * ab
//https://www.w3.org/TR/compositing-1/#blending
//x1, x2, x3 are as * ab * B of each channel in the 255 * 255 scale. Two channels share a word in 16-bit lanes,
//each rounded as in _div255()
static uint32_t _compose(uint32_t s, uint32_t d, uint32_t x1, uint32_t x2, uint32_t x3)
{
    uint32_t sa = A(s), da = A(d);
    auto ag = ((s >> 8) & 0x00ff00ff) * (255 - da) + ((d >> 8) & 0x00ff00ff) * (255 - sa) + (sa * da << 16) + x2 + 0x00800080;
    auto rb = (s & 0x00ff00ff) * (255 - da) + (d & 0x00ff00ff) * (255 - sa) + (x1 << 16) + x3 + 0x00800080;
    return ((ag + ((ag >> 8) & 0x00ff00ff)) & 0xff00ff00) | (((rb + ((rb >> 8) & 0x00ff00ff)) >> 8) & 0x00ff00ff);
}

//separable blending: x1, x2, x3 are as * ab * B of each channel in the 255 * 255 scale, the mode's f of the premultiplied
//channels s, d and the alphas sa, da. it composes as co = cs + cb - g with g = cs * ab + cb * as - as * ab * B, which folds
//into a few products per mode (exclusion: 2 * cs * cb), and co stays in [0, 255], so the four channels subtract in one word
//it needs B <= Cs + Cb per channel to keep g non-negative, which only the separable modes meet: the others use _compose()
static uint32_t _blend(uint32_t s, uint32_t d, uint32_t x1, uint32_t x2, uint32_t x3)
{
    //opaque source and destination: only B remains
    if ((s & d) >= 0xff000000) return JOIN(255, _div255(x1), _div255(x2), _div255(x3));
    uint32_t sa = A(s), da = A(d);
    if (!sa) return d;  //a transparent source leaves the destination
    if (!da) return s;  //a transparent destination takes the source
    //g can exceed 16 bits, so it takes the division instead of _div255()
    auto g = [&](uint32_t s, uint32_t d, uint32_t x) { return (s * da + d * sa - x + 127) / 255; };
    return s + d - (((sa * da + 127) / 255 << 24) + (g(C1(s), C1(d), x1) << 16) + (g(C2(s), C2(d), x2) << 8) + g(C3(s), C3(d), x3));
}

//W3C luminosity: 0.3 red, 0.59 green, 0.11 blue
//https://www.w3.org/TR/compositing-1/#blendingnonseparable
static int32_t _lum(const SwSurface* surface, int32_t c1, int32_t c2, int32_t c3)
{
    if (surface->cs == ColorSpace::ARGB8888 || surface->cs == ColorSpace::ARGB8888S) return (c1 * 30 + c2 * 59 + c3 * 11 + 50) / 100;
    return (c1 * 11 + c2 * 59 + c3 * 30 + 50) / 100;
}

static int32_t _sat(uint8_t c1, uint8_t c2, uint8_t c3)
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
    l = std::min(l, a);  //an invalid premultiplied color (c > a) can push l over a, then x - l can be zero
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
    auto sa = A(s), da = A(d);
    auto f = [&](uint8_t s, uint8_t d) { return abs(s * da - d * sa); };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

uint32_t blendExclusion(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](uint8_t s, uint8_t d) { return s * da + d * sa - 2 * s * d; };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

//plus-lighter is a compositing operator: min(1, s + d) for the colors and the alpha alike
//https://drafts.fxtf.org/compositing-2/#porterduffcompositingoperators_plus_lighter
uint32_t blendAdd(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint8_t s, uint8_t d) { return std::min(s + d, 255); };
    return JOIN(f(A(s), A(d)), f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

//screen composes to s + d - s * d for the colors and the alpha alike
uint32_t blendScreen(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return s + d - JOIN(_div255(A(s) * A(d)), _div255(C1(s) * C1(d)), _div255(C2(s) * C2(d)), _div255(C3(s) * C3(d)));
}

uint32_t blendMultiply(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    return _blend(s, d, C1(s) * C1(d), C2(s) * C2(d), C3(s) * C3(d));
}

//multiply 2 * s * d while 2 * cb <= 1, screen above it: max() adds the screen part without a branch
uint32_t blendOverlay(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](uint8_t s, uint8_t d) { return 2 * s * d + std::max(0, 2 * d - da) * (sa - 2 * s); };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

uint32_t blendDarken(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](uint8_t s, uint8_t d) { return std::min(s * da, d * sa); };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

uint32_t blendLighten(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](uint8_t s, uint8_t d) { return std::max(s * da, d * sa); };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

//dodge, burn and soft-light take int32_t channels: with uint8_t, the compare and the subtraction before each division
//no longer fuse into one instruction (dodge 6% slower)
uint32_t blendColorDodge(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](int32_t s, int32_t d) {
        if (d == 0) return 0;
        if (s >= sa) return sa * da;
        return std::min(sa * da, sa * sa * d / (sa - s));
    };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

uint32_t blendColorBurn(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](int32_t s, int32_t d) {
        if (d >= da) return sa * da;
        if (s == 0) return 0;
        return sa * da - std::min(sa * da, sa * sa * (da - d) / s);
    };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

//the overlay with the source and the destination swapped
uint32_t blendHardLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](uint8_t s, uint8_t d) { return 2 * s * d + std::max(0, 2 * s - sa) * (da - 2 * d); };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

//int32_t channels as in dodge and burn
uint32_t blendSoftLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    auto f = [&](int32_t s, int32_t d) {
        if (d >= da) return sa * da;
        if (2 * s <= sa) return sa * d - (sa - 2 * s) * d * (da - d) / da;
        //the product stays below 2^31 for any 8-bit input
        if (4 * d <= da) return sa * d + (2 * s - sa) * d * ((16 * d - 12 * da) * d + 3 * da * da) / (da * da);

        // use Look up table for skip sqrt per every pixels.
        static constexpr uint8_t SQRT_LUT[256] = {
            0, 15, 22, 27, 31, 35, 39, 42, 45, 47, 50, 52, 55, 57, 59, 61,
            63, 65, 67, 69, 71, 73, 74, 76, 78, 79, 81, 82, 84, 85, 87, 88,
            90, 91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 104, 105, 107, 108, 109,
            110, 111, 112, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
            127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 141,
            142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151, 152, 153, 153, 154, 155,
            156, 157, 158, 158, 159, 160, 161, 162, 162, 163, 164, 165, 165, 166, 167, 168,
            168, 169, 170, 171, 171, 172, 173, 174, 174, 175, 176, 177, 177, 178, 179, 179,
            180, 181, 182, 182, 183, 184, 184, 185, 186, 186, 187, 188, 188, 189, 190, 190,
            191, 192, 192, 193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 200, 200, 201,
            201, 202, 203, 203, 204, 205, 205, 206, 206, 207, 208, 208, 209, 210, 210, 211,
            211, 212, 213, 213, 214, 214, 215, 216, 216, 217, 217, 218, 218, 219, 220, 220,
            221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227, 228, 228, 229, 229,
            230, 230, 231, 231, 232, 233, 233, 234, 234, 235, 235, 236, 236, 237, 237, 238,
            238, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246,
            247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 253, 254, 255};
        return sa * d + (2 * s - sa) * (SQRT_LUT[d * 255 / da] * da - 255 * d) / 255;
    };
    return _blend(s, d, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

uint32_t blendHue(const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s);
    int32_t c1 = C1(s), c2 = C2(s), c3 = C3(s);
    _saturate(c1, c2, c3, _sat(C1(d), C2(d), C3(d)) * sa);
    return _luminate(surface, s, d, c1, c2, c3, _lum(surface, C1(d) * sa, C2(d) * sa, C3(d) * sa));
}

uint32_t blendSaturation(const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s);
    int32_t c1 = C1(d), c2 = C2(d), c3 = C3(d);
    auto l = _lum(surface, c1 * sa, c2 * sa, c3 * sa);
    _saturate(c1, c2, c3, _sat(C1(s), C2(s), C3(s)) * A(d));
    return _luminate(surface, s, d, c1, c2, c3, l);
}

uint32_t blendColor(const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    return _luminate(surface, s, d, C1(s) * da, C2(s) * da, C3(s) * da, _lum(surface, C1(d) * sa, C2(d) * sa, C3(d) * sa));
}

uint32_t blendLuminosity(const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto sa = A(s), da = A(d);
    return _luminate(surface, s, d, C1(d) * sa, C2(d) * sa, C3(d) * sa, _lum(surface, C1(s) * da, C2(s) * da, C3(s) * da));
}