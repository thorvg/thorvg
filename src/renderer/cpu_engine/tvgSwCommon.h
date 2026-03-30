/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#ifndef _TVG_SW_COMMON_H_
#define _TVG_SW_COMMON_H_

#include <algorithm>
#include "tvgCommon.h"
#include "tvgMath.h"
#include "tvgRender.h"

#define SW_CURVE_TYPE_POINT 0
#define SW_CURVE_TYPE_CUBIC 1
#define SW_ANGLE_PI (180L << 16)
#define SW_ANGLE_2PI (SW_ANGLE_PI << 1)
#define SW_ANGLE_PI2 (SW_ANGLE_PI >> 1)
#define SW_COLOR_TABLE 1024

static inline float TO_FLOAT(int32_t val)
{
    return static_cast<float>(val) / 64.0f;
}

struct SwPoint
{
    int32_t x, y;

    SwPoint& operator-=(const SwPoint& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    SwPoint& operator+=(const SwPoint& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    SwPoint operator+(const SwPoint& rhs) const
    {
        return {x + rhs.x, y + rhs.y};
    }

    SwPoint operator-(const SwPoint& rhs) const
    {
        return {x - rhs.x, y - rhs.y};
    }

    bool operator==(const SwPoint& rhs) const
    {
        return (x == rhs.x && y == rhs.y);
    }

    bool operator!=(const SwPoint& rhs) const
    {
        return (x != rhs.x || y != rhs.y);
    }

    bool zero() const
    {
        if (x == 0 && y == 0) return true;
        else return false;
    }

    bool tiny() const
    {
        //2 is epsilon...
        if (abs(x) < 2 && abs(y) < 2) return true;
        else return false;
    }

    Point toPoint() const
    {
        return {TO_FLOAT(x),  TO_FLOAT(y)};
    }
};

struct SwSize
{
    int32_t w, h;
};

struct SwOutline
{
    Array<SwPoint> pts;             //the outline's points
    Array<uint32_t> cntrs;          //the contour end points
    Array<uint8_t> types;           //curve type
    Array<bool> closed;             //opened or closed path?
    FillRule fillRule;
};

struct SwSpan
{
    int32_t x, y;
    int32_t len;
    uint8_t coverage;

    bool fetch(const RenderRegion& bbox, int32_t& x, int32_t& len) const
    {
        x = std::max((int32_t)this->x, bbox.min.x);
        len = std::min((int32_t)(this->x + this->len), bbox.max.x) - x;
        return (len > 0) ? true : false;
    }
};

struct SwRle
{
    Array<SwSpan> spans;

    const SwSpan* fetch(const RenderRegion& bbox, const SwSpan** end) const
    {
        return fetch(bbox.min.y, bbox.max.y - 1, end);
    }

    const SwSpan* fetch(int32_t min, int32_t max, const SwSpan** end) const
    {
        const SwSpan* begin;

        if (min <= spans.first().y) {
            begin = spans.begin();
        } else {
            auto comp = [](const SwSpan& span, int y) { return span.y < y; };
            begin = lower_bound(spans.begin(), spans.end(), min, comp);
        }
        if (end) {
            if (max >= spans.last().y) {
                *end = spans.end();
            } else {
                auto comp = [](int y, const SwSpan& span) { return y < span.y; };
                *end = upper_bound(spans.begin(), spans.end(), max, comp);
            }
        }
        return begin;
    }

    bool invalid() const { return spans.empty(); }
    bool valid() const { return !invalid(); }
    uint32_t size() const { return spans.count; }
    SwSpan* data() const { return spans.data; }
};

using Area = long;

struct SwCell
{
    int32_t x;
    int32_t cover;
    Area area;
    SwCell *next;
};

struct SwFill
{
    struct SwLinear {
        float dx, dy;
        float offset;
    };

    struct SwRadial {
        float a11, a12, a13;
        float a21, a22, a23;
        float fx, fy, fr;
        float dx, dy, dr;
        float invA, a;
    };

    union {
        SwLinear linear;
        SwRadial radial;
    };

    uint32_t ctable[SW_COLOR_TABLE];
    FillSpread spread;

    bool solid = false; //solid color fill with the last color from colorStops
    bool translucent;
};

struct SwStrokeBorder
{
    Array<SwPoint> pts;
    uint8_t* tags = nullptr;
    int32_t start = 0;        //index of current sub-path start point
    bool movable = false;      //true: for ends of lineto borders

    ~SwStrokeBorder()
    {
        tvg::free(tags);
    }
};

struct SwStroke
{
    int64_t angleIn;
    int64_t angleOut;
    SwPoint center;
    int64_t lineLength;
    int64_t subPathAngle;
    SwPoint ptStartSubPath;
    int64_t subPathLineLength;
    int64_t width;
    int64_t miterlimit;
    SwFill* fill = nullptr;
    SwStrokeBorder* borders[2];
    float sx, sy;
    StrokeCap cap;
    StrokeJoin join;
    StrokeJoin joinSaved;
    bool firstPt;
    bool closedSubPath;
    bool handleWideStrokes;
};

struct SwDashStroke
{
    SwOutline* outline = nullptr;
    float curLen = 0;
    int32_t curIdx = 0;
    Point ptStart = {0, 0};
    Point ptCur = {0, 0};
    float* pattern = nullptr;
    uint32_t cnt = 0;
    bool curOpGap = false;
    bool move = true;
};

struct SwShape
{
    SwOutline* outline = nullptr;
    SwStroke* stroke = nullptr;
    SwFill* fill = nullptr;
    SwRle* rle = nullptr;
    SwRle* strokeRle = nullptr;
    RenderRegion bbox;        //Keep it boundary without stroke region. Using for optimal filling.
    bool fastTrack = false;   //Fast Track: axis-aligned rectangle without any clips?
};

struct SwImage
{
    SwOutline*   outline = nullptr;
    SwRle*   rle = nullptr;
    union {
        pixel_t*  data;      //system based data pointer
        uint32_t* buf32;     //for explicit 32bits channels
        uint8_t*  buf8;      //for explicit 8bits grayscale
    };
    uint32_t     w, h, stride;
    int32_t      ox = 0;         //offset x
    int32_t      oy = 0;         //offset y
    float        scale;

    uint8_t      channelSize;
    FilterMethod filter;
    bool         direct = false;  //draw image directly (with offset)
    bool         scaled = false;  //draw scaled image
};

struct SwSurface;
struct SwCompositor;

typedef uint8_t(*SwMask)(uint8_t s, uint8_t d, uint8_t a);                            //src, dst, alpha
typedef uint32_t (*SwBlender)(const SwSurface* surface, uint32_t s, uint32_t d);      // surface, src, dst
typedef uint32_t(*SwBlenderA)(uint32_t s, uint32_t d, uint8_t a);                     //src, dst, alpha
typedef uint32_t(*SwJoin)(uint8_t r, uint8_t g, uint8_t b, uint8_t a);                //color channel join
typedef void (*SwSplit)(uint32_t c, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);  // color channel split
typedef uint8_t(*SwAlpha)(uint8_t*);                                                  //blending alpha

struct SwSurface : RenderSurface
{
    SwJoin  join;
    SwSplit split;
    SwAlpha alphas[4];                    //Alpha:2, InvAlpha:3, Luma:4, InvLuma:5
    SwBlender blender = nullptr;          //blender (optional)
    SwCompositor* compositor = nullptr;   //compositor (optional)
    BlendMethod blendMethod = BlendMethod::Normal;

    SwAlpha alpha(MaskMethod method)
    {
        auto idx = (int)(method) - 1;       //-1 for None
        return alphas[idx > 3 ? 0 : idx];   //CompositeMethod has only four Matting methods.
    }

    SwSurface()
    {
    }

    SwSurface(const SwSurface* rhs) : RenderSurface(rhs)
    {
        join = rhs->join;
        split = rhs->split;
        memcpy(alphas, rhs->alphas, sizeof(alphas));
        blender = rhs->blender;
        compositor = rhs->compositor;
        blendMethod = rhs->blendMethod;
    }
};

struct SwCompositor : RenderCompositor
{
    SwSurface* recoverSfc;                  //Recover surface when composition is started
    SwCompositor* recoverCmp;               //Recover compositor when composition is done
    SwImage image;
    RenderRegion bbox;
    bool valid;
};

struct SwCellPool
{
    #define DEFAULT_POOL_SIZE 16368

    uint32_t size;
    SwCell* buffer;

    SwCellPool() : size(DEFAULT_POOL_SIZE), buffer(tvg::malloc<SwCell>(DEFAULT_POOL_SIZE)) {}
    ~SwCellPool() { tvg::free(buffer); }
};

struct SwMpool
{
    SwOutline* outlines;
    SwStrokeBorder* lBorders;
    SwStrokeBorder* rBorders;
    SwCellPool* cellPools;

    SwMpool(uint32_t threads)
    {
        auto allocSize = threads + 1;
        outlines = new SwOutline[allocSize];
        lBorders = new SwStrokeBorder[allocSize];
        rBorders = new SwStrokeBorder[allocSize];
        cellPools = new SwCellPool[allocSize];
    }

    ~SwMpool()
    {
        delete[] (outlines);
        delete[] (lBorders);
        delete[] (rBorders);
        delete[] (cellPools);
    }

    SwCellPool* cell(unsigned idx)
    {
        return &cellPools[idx];
    }

    SwOutline* outline(unsigned idx)
    {
        outlines[idx].pts.clear();
        outlines[idx].cntrs.clear();
        outlines[idx].types.clear();
        outlines[idx].closed.clear();

        return &outlines[idx];
    }

    SwStrokeBorder* strokeLBorder(unsigned idx)
    {
        lBorders[idx].pts.clear();
        lBorders[idx].start = -1;
        return &lBorders[idx];
    }

    SwStrokeBorder* strokeRBorder(unsigned idx)
    {
        rBorders[idx].pts.clear();
        rBorders[idx].start = -1;
        return &rBorders[idx];
    }
};

static inline int32_t TO_SWCOORD(float val)
{
    return int32_t(val * 64.0f);
}

static inline uint32_t JOIN(uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3)
{
    return (c0 << 24 | c1 << 16 | c2 << 8 | c3);
}

static inline uint32_t ALPHA_BLEND(uint32_t c, uint32_t a)
{
    ++a;
    return (((((c >> 8) & 0x00ff00ff) * a) & 0xff00ff00) + ((((c & 0x00ff00ff) * a) >> 8) & 0x00ff00ff));
}

static inline uint32_t INTERPOLATE(uint32_t s, uint32_t d, uint8_t a)
{
    return (((((((s >> 8) & 0xff00ff) - ((d >> 8) & 0xff00ff)) * a) + (d & 0xff00ff00)) & 0xff00ff00) + ((((((s & 0xff00ff) - (d & 0xff00ff)) * a) >> 8) + (d & 0xff00ff)) & 0xff00ff));
}

static inline uint8_t INTERPOLATE8(uint8_t s, uint8_t d, uint8_t a)
{
    return (((s) * (a) + 0xff) >> 8) + (((d) * ~(a) + 0xff) >> 8);
}

static inline int32_t HALF_STROKE(float width)
{
    return TO_SWCOORD(width * 0.5f);
}

static inline uint8_t A(uint32_t c)
{
    return ((c) >> 24);
}

static inline uint8_t IA(uint32_t c)
{
    return (~(c) >> 24);
}

static inline uint8_t C1(uint32_t c)
{
    return ((c) >> 16);
}

static inline uint8_t C2(uint32_t c)
{
    return ((c) >> 8);
}

static inline uint8_t C3(uint32_t c)
{
    return (c);
}

static inline uint32_t PREMULTIPLY(uint32_t c, uint8_t a)
{
    return (c & 0xff000000) + ((((c >> 8) & 0xff) * a) & 0xff00) + ((((c & 0x00ff00ff) * a) >> 8) & 0x00ff00ff);
}

static inline RenderColor BLEND_UPRE(uint32_t c)
{
    RenderColor o = {C1(c), C2(c), C3(c), A(c)};
    if (o.a > 0 && o.a < 255) {
        o.r = std::min(o.r * 255u / o.a, 255u);
        o.g = std::min(o.g * 255u / o.a, 255u);
        o.b = std::min(o.b * 255u / o.a, 255u);
    }
    return o;
}

static inline uint32_t BLEND_PRE(uint32_t c1, uint32_t c2, uint8_t a)
{
    if (a == 255) return c1;
    else if (a == 0) return c2;
    return ALPHA_BLEND(c1, a) + ALPHA_BLEND(c2, 255 - a);
}

static inline int32_t LUM(int32_t r, int32_t g, int32_t b)
{
    return ((77 * r) + (151 * g) + (28 * b)) / 256;
}

static inline int32_t SAT(int32_t r, int32_t g, int32_t b)
{
    return (std::max(r, std::max(g, b)) - std::min(r, std::min(g, b)));
}

static inline uint8_t CLAMP8(int32_t c)
{
    return static_cast<uint8_t>(tvg::clamp(c, 0, 255));
}

static inline void UNPREMULTIPLY(int32_t& r, int32_t& g, int32_t& b, uint8_t a)
{
    if (a > 0 && a < 255) {
        auto ia = ((255u << 16) + a - 1) / a;
        auto rb = (((uint64_t)b << 32) | uint32_t(r)) * ia;
        r = std::min<uint32_t>(uint32_t((rb >> 16) & 0xffff), 255u);
        g = std::min<uint32_t>((uint32_t(g) * ia) >> 16, 255u);
        b = std::min<uint32_t>(uint32_t(rb >> 48), 255u);
    }
}

static inline void CLIP_COLOR(int32_t& r, int32_t& g, int32_t& b)
{
    auto n = std::min(r, std::min(g, b));
    auto x = std::max(r, std::max(g, b));

    if (n >= 0 && x <= 255) return;

    auto l = LUM(r, g, b);

    if (n < 0) {
        auto denom = l - n;
        r = l + (((r - l) * l) / denom);
        g = l + (((g - l) * l) / denom);
        b = l + (((b - l) * l) / denom);
    }

    if (x > 255) {
        auto scale = 255 - l;
        auto denom = x - l;
        r = l + (((r - l) * scale) / denom);
        g = l + (((g - l) * scale) / denom);
        b = l + (((b - l) * scale) / denom);
    }
}

static inline void SET_LUM(int32_t& r, int32_t& g, int32_t& b, int32_t l)
{
    auto d = l - LUM(r, g, b);
    r += d;
    g += d;
    b += d;
    CLIP_COLOR(r, g, b);
}

static inline void SET_SAT(int32_t& r, int32_t& g, int32_t& b, int32_t s)
{
    int32_t *min, *mid, *max;

    if (r <= g) {
        if (g <= b) min = &r, mid = &g, max = &b;
        else if (r <= b) min = &r, mid = &b, max = &g;
        else min = &b, mid = &r, max = &g;
    } else {
        if (r <= b) min = &g, mid = &r, max = &b;
        else if (g <= b) min = &g, mid = &b, max = &r;
        else min = &b, mid = &g, max = &r;
    }

    if (*max > *min) {
        *mid = ((*mid - *min) * s) / (*max - *min);
        *max = s;
    } else *mid = *max = 0;
    *min = 0;
}

static inline uint32_t opBlendInterp(uint32_t s, uint32_t d, uint8_t a)
{
    return INTERPOLATE(s, d, a);
}

static inline uint32_t opBlendNormal(uint32_t s, uint32_t d, uint8_t a)
{
    auto t = ALPHA_BLEND(s, a);
    return t + ALPHA_BLEND(d, IA(t));
}

static inline uint32_t opBlendPreNormal(uint32_t s, uint32_t d, TVG_UNUSED uint8_t a)
{
    return s + ALPHA_BLEND(d, IA(s));
}

static inline uint32_t opBlendSrcOver(uint32_t s, TVG_UNUSED uint32_t d, TVG_UNUSED uint8_t a)
{
    return s;
}

static inline uint32_t opBlendDifference(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint8_t s, uint8_t d) {
        return (s > d) ? (s - d) : (d - s);
    };

    return JOIN(255, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

static inline uint32_t opBlendExclusion(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint8_t s, uint8_t d) {
        return tvg::clamp(s + d - 2 * MULTIPLY(s, d), 0, 255);
    };

    return JOIN(255, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

static inline uint32_t opBlendAdd(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint8_t s, uint8_t d) {
        return std::min(s + d, 255);
    };

    return JOIN(255, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

static inline uint32_t opBlendScreen(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint8_t s, uint8_t d) {
        return s + d - MULTIPLY(s, d);
    };

    return JOIN(255, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

static inline uint32_t opBlendMultiply(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto o = BLEND_UPRE(d);

    auto f = [](uint8_t s, uint8_t d) {
        return MULTIPLY(s, d);
    };

    return BLEND_PRE(JOIN(255, f(C1(s), o.r), f(C2(s), o.g), f(C3(s), o.b)), s, o.a);
}

static inline uint32_t opBlendOverlay(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto o = BLEND_UPRE(d);

    auto f = [](uint8_t s, uint8_t d) {
        return (d < 128) ? std::min(255, 2 * MULTIPLY(s, d)) : (255 - std::min(255, 2 * MULTIPLY(255 - s, 255 - d)));
    };

    return BLEND_PRE(JOIN(255, f(C1(s), o.r), f(C2(s), o.g), f(C3(s), o.b)), s, o.a);
}

static inline uint32_t opBlendDarken(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto o = BLEND_UPRE(d);

    auto f = [](uint8_t s, uint8_t d) {
        return std::min(s, d);
    };

    return BLEND_PRE(JOIN(255, f(C1(s), o.r), f(C2(s), o.g), f(C3(s), o.b)), s, o.a);
}

static inline uint32_t opBlendLighten(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto f = [](uint8_t s, uint8_t d) {
        return std::max(s, d);
    };

    return JOIN(255, f(C1(s), C1(d)), f(C2(s), C2(d)), f(C3(s), C3(d)));
}

static inline uint32_t opBlendColorDodge(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto o = BLEND_UPRE(d);

    auto f = [](uint8_t s, uint8_t d) {
        return d == 0 ? 0 : (s == 255 ? 255 : std::min(d * 255 / (255 - s), 255));
    };

    return BLEND_PRE(JOIN(255, f(C1(s), o.r), f(C2(s), o.g), f(C3(s), o.b)), s, o.a);
}

static inline uint32_t opBlendColorBurn(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto o = BLEND_UPRE(d);

    auto f = [](uint8_t s, uint8_t d) {
        return d == 255 ? 255 : (s == 0 ? 0 : 255 - std::min((255 - d) * 255 / s, 255));
    };

    return BLEND_PRE(JOIN(255, f(C1(s), o.r), f(C2(s), o.g), f(C3(s), o.b)), s, o.a);
}

static inline uint32_t opBlendHardLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto o = BLEND_UPRE(d);

    auto f = [](uint8_t s, uint8_t d) {
        return (s < 128) ? std::min(255, 2 * MULTIPLY(s, d)) : (255 - std::min(255, 2 * MULTIPLY(255 - s, 255 - d)));
    };

    return BLEND_PRE(JOIN(255, f(C1(s), o.r), f(C2(s), o.g), f(C3(s), o.b)), s, o.a);
}

static inline uint32_t opBlendSoftLight(TVG_UNUSED const SwSurface* surface, uint32_t s, uint32_t d)
{
    auto o = BLEND_UPRE(d);
    // LUT optimization for soft-light blend is also used by jQuery Canvas Effects:
    // https://0.s3.envato.com/files/26317668/index.html
    static constexpr uint16_t dc[256] = {
        0, 1012, 2000, 2965, 3907, 4827, 5724, 6599, 7453, 8286, 9098, 9890, 10662, 11414, 12148, 12862,
        13558, 14236, 14896, 15539, 16165, 16775, 17368, 17946, 18508, 19055, 19587, 20106, 20610, 21101, 21578, 22043,
        22496, 22936, 23365, 23783, 24190, 24586, 24972, 25349, 25716, 26074, 26424, 26765, 27099, 27425, 27744, 28056,
        28362, 28662, 28956, 29245, 29530, 29810, 30086, 30358, 30627, 30893, 31156, 31417, 31677, 31935, 32192, 32448,
        32704, 32958, 33211, 33462, 33710, 33957, 34203, 34446, 34688, 34928, 35166, 35403, 35638, 35872, 36104, 36335,
        36564, 36792, 37018, 37243, 37467, 37689, 37910, 38130, 38349, 38566, 38782, 38997, 39211, 39423, 39635, 39845,
        40054, 40262, 40469, 40675, 40880, 41084, 41287, 41489, 41690, 41889, 42088, 42287, 42484, 42680, 42875, 43070,
        43263, 43456, 43648, 43839, 44029, 44218, 44407, 44595, 44782, 44968, 45153, 45338, 45522, 45705, 45888, 46069,
        46250, 46431, 46610, 46789, 46967, 47145, 47322, 47498, 47674, 47849, 48023, 48197, 48370, 48542, 48714, 48885,
        49056, 49226, 49395, 49564, 49733, 49900, 50067, 50234, 50400, 50566, 50731, 50895, 51059, 51222, 51385, 51548,
        51709, 51871, 52032, 52192, 52352, 52511, 52670, 52829, 52986, 53144, 53301, 53457, 53614, 53769, 53924, 54079,
        54233, 54387, 54541, 54694, 54846, 54998, 55150, 55301, 55452, 55603, 55753, 55902, 56052, 56201, 56349, 56497,
        56645, 56792, 56939, 57086, 57232, 57378, 57523, 57668, 57813, 57957, 58101, 58245, 58388, 58531, 58674, 58816,
        58958, 59099, 59241, 59382, 59522, 59662, 59802, 59942, 60081, 60220, 60358, 60497, 60635, 60772, 60910, 61047,
        61183, 61320, 61456, 61592, 61727, 61863, 61997, 62132, 62266, 62400, 62534, 62668, 62801, 62934, 63066, 63199,
        63331, 63463, 63594, 63725, 63856, 63987, 64118, 64248, 64378, 64507, 64637, 64766, 64895, 65023, 65152, 65280
    };

    auto f = [&](uint8_t s, uint8_t d) {
        if (s < 128) return d - (((255 - 2 * s) * d * (255 - d) + 32512) / 65025);
        return d + (((2 * s - 255) * (dc[d] - d * 256) + 32640) / 65280);
    };

    return BLEND_PRE(JOIN(255, f(C1(s), o.r), f(C2(s), o.g), f(C3(s), o.b)), s, o.a);
}

static inline uint32_t opBlendHue(const SwSurface* surface, uint32_t s, uint32_t d)
{
    uint8_t sr, sg, sb, dr, dg, db, a;
    surface->split(s, sr, sg, sb, a);
    surface->split(d, dr, dg, db, a);

    int32_t r = sr, g = sg, b = sb;
    int32_t lr = dr, lg = dg, lb = db;
    UNPREMULTIPLY(lr, lg, lb, a);
    SET_SAT(r, g, b, SAT(lr, lg, lb));
    SET_LUM(r, g, b, LUM(lr, lg, lb));

    return BLEND_PRE(surface->join(CLAMP8(r), CLAMP8(g), CLAMP8(b), 255), s, a);
}

static inline uint32_t opBlendSaturation(const SwSurface* surface, uint32_t s, uint32_t d)
{
    uint8_t sr, sg, sb, dr, dg, db, a;
    surface->split(s, sr, sg, sb, a);
    surface->split(d, dr, dg, db, a);

    int32_t r = dr, g = dg, b = db;
    UNPREMULTIPLY(r, g, b, a);
    auto l = LUM(r, g, b);
    SET_SAT(r, g, b, SAT(sr, sg, sb));
    SET_LUM(r, g, b, l);

    return BLEND_PRE(surface->join(CLAMP8(r), CLAMP8(g), CLAMP8(b), 255), s, a);
}

static inline uint32_t opBlendColor(const SwSurface* surface, uint32_t s, uint32_t d)
{
    uint8_t sr, sg, sb, dr, dg, db, a;
    surface->split(s, sr, sg, sb, a);
    surface->split(d, dr, dg, db, a);

    int32_t r = sr, g = sg, b = sb;
    int32_t lr = dr, lg = dg, lb = db;
    UNPREMULTIPLY(lr, lg, lb, a);
    SET_LUM(r, g, b, LUM(lr, lg, lb));

    return BLEND_PRE(surface->join(CLAMP8(r), CLAMP8(g), CLAMP8(b), 255), s, a);
}

static inline uint32_t opBlendLuminosity(const SwSurface* surface, uint32_t s, uint32_t d)
{
    uint8_t sr, sg, sb, dr, dg, db, a;
    surface->split(s, sr, sg, sb, a);
    surface->split(d, dr, dg, db, a);

    int32_t r = dr, g = dg, b = db;
    UNPREMULTIPLY(r, g, b, a);
    SET_LUM(r, g, b, LUM(sr, sg, sb));

    return BLEND_PRE(surface->join(CLAMP8(r), CLAMP8(g), CLAMP8(b), 255), s, a);
}

int64_t mathMultiply(int64_t a, int64_t b);
int64_t mathDivide(int64_t a, int64_t b);
int64_t mathMulDiv(int64_t a, int64_t b, int64_t c);
void mathRotate(SwPoint& pt, int64_t angle);
int64_t mathTan(int64_t angle);
int64_t mathAtan(const SwPoint& pt);
int64_t mathCos(int64_t angle);
int64_t mathSin(int64_t angle);
void mathSplitCubic(SwPoint* base);
void mathSplitLine(SwPoint* base);
int64_t mathDiff(int64_t angle1, int64_t angle2);
int64_t mathLength(const SwPoint& pt);
int mathCubicAngle(const SwPoint* base, int64_t& angleIn, int64_t& angleMid, int64_t& angleOut);
int64_t mathMean(int64_t angle1, int64_t angle2);
SwPoint mathTransform(const Point* to, const Matrix& transform);
bool mathUpdateOutlineBBox(const SwOutline* outline, const RenderRegion& clipBox, RenderRegion& renderBox, bool fastTrack);

void shapeReset(SwShape& shape);
bool shapePrepare(SwShape& shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid, bool hasComposite);
bool shapeGenRle(SwShape& shape, const RenderRegion& bbox, SwMpool* mpool, unsigned tid, bool antiAlias);
void shapeDelOutline(SwShape& shape, SwMpool* mpool, uint32_t tid);
void shapeResetStroke(SwShape& shape, const RenderShape* rshape, const Matrix& transform, SwMpool* mpool, unsigned tid);
bool shapeGenStrokeRle(SwShape& shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid);
void shapeFree(SwShape& shape);
void shapeDelStroke(SwShape& shape);
bool shapeGenFillColors(SwShape& shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
bool shapeGenStrokeFillColors(SwShape& shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
void shapeResetFill(SwShape& shape);
void shapeResetStrokeFill(SwShape& shape);
bool shapeStrokeBBox(SwShape& shape, const RenderShape* rshape, Point* pt4, const Matrix& m, SwMpool* mpool);
void shapeDelFill(SwShape& shape);

void strokeReset(SwStroke* stroke, const RenderShape* shape, const Matrix& transform, SwMpool* mpool, unsigned tid);
bool strokeParseOutline(SwStroke* stroke, const SwOutline& outline, SwMpool* mpool, unsigned tid);
SwOutline* strokeExportOutline(SwStroke* stroke, SwMpool* mpool, unsigned tid);
void strokeFree(SwStroke* stroke);

bool imagePrepare(SwImage& image, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid);
bool imageGenRle(SwImage& image, const RenderRegion& bbox, SwMpool* mpool, unsigned tid, bool antiAlias);
void imageDelOutline(SwImage& image, SwMpool* mpool, uint32_t tid);
void imageReset(SwImage& image);
void imageFree(SwImage& image);

bool fillGenColorTable(SwFill* fill, const Fill* fdata, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable);
const Fill::ColorStop* fillFetchSolid(const SwFill* fill, const Fill* fdata);
void fillReset(SwFill* fill);
void fillFree(SwFill* fill);

//OPTIMIZE_ME: Skip the function pointer access
void fillLinear(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, SwMask maskOp, uint8_t opacity);                                   //composite masking ver.
void fillLinear(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwMask maskOp, uint8_t opacity);                     //direct masking ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlenderA op, uint8_t a);                                        //blending ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlenderA op, SwBlender op2, const SwSurface* surface, uint8_t a);  // blending + BlendingMethod(op2) ver.
void fillLinear(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwAlpha alpha, uint8_t csize, uint8_t opacity);     //matting ver.

void fillRadial(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, SwMask op, uint8_t a);                                             //composite masking ver.
void fillRadial(const SwFill* fill, uint8_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwMask op, uint8_t a) ;                              //direct masking ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlenderA op, uint8_t a);                                        //blending ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, SwBlenderA op, SwBlender op2, const SwSurface* surface, uint8_t a);  // blending + BlendingMethod(op2) ver.
void fillRadial(const SwFill* fill, uint32_t* dst, uint32_t y, uint32_t x, uint32_t len, uint8_t* cmp, SwAlpha alpha, uint8_t csize, uint8_t opacity);     //matting ver.

SwRle* rleRender(SwRle* rle, const SwOutline* outline, const RenderRegion& bbox, SwMpool* mpool, unsigned tid, bool antiAlias);
SwRle* rleRender(const RenderRegion* bbox);
void rleFree(SwRle* rle);
void rleReset(SwRle* rle);
void rleMerge(SwRle* rle, SwRle* clip1, SwRle* clip2);
bool rleClip(SwRle* rle, const SwRle* clip);
bool rleClip(SwRle* rle, const RenderRegion* clip);
bool rleIntersect(const SwRle* rle, const RenderRegion& region);

void mpoolInit(uint32_t threads);
void mpoolTerm();
SwMpool* mpoolReq();

bool rasterCompositor(SwSurface* surface);
bool rasterShape(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, RenderColor& c);
bool rasterTexmapPolygon(SwSurface* surface, const SwImage& image, const Matrix& transform, const RenderRegion& bbox, uint8_t opacity);
bool rasterScaledImage(SwSurface* surface, const SwImage& image, const Matrix& transform, const RenderRegion& bbox, uint8_t opacity);
bool rasterDirectImage(SwSurface* surface, const SwImage& image, const RenderRegion& bbox, uint8_t opacity);
bool rasterScaledRleImage(SwSurface* surface, const SwImage& image, const Matrix& transform, const RenderRegion& bbox, uint8_t opacity);
bool rasterDirectRleImage(SwSurface* surface, const SwImage& image, const RenderRegion& bbox, uint8_t opacity);
bool rasterStroke(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, RenderColor& c);
bool rasterGradientShape(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, const Fill* fdata, uint8_t opacity);
bool rasterGradientStroke(SwSurface* surface, SwShape* shape, const RenderRegion& bbox, const Fill* fdata, uint8_t opacity);
bool rasterClear(SwSurface* surface, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void rasterPixel32(uint32_t *dst, uint32_t val, uint32_t offset, int32_t len);
void rasterTranslucentPixel32(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity);
void rasterPixel32(uint32_t* dst, uint32_t* src, uint32_t len, uint8_t opacity);
void rasterGrayscale8(uint8_t *dst, uint8_t val, uint32_t offset, int32_t len);
void rasterXYFlip(uint32_t* src, uint32_t* dst, int32_t stride, int32_t w, int32_t h, const RenderRegion& bbox, bool flipped);
void rasterUnpremultiply(RenderSurface* surface);
void rasterPremultiply(RenderSurface* surface);
bool rasterConvertCS(RenderSurface* surface, ColorSpace to);
uint32_t rasterUnpremultiply(uint32_t data);

bool effectGaussianBlur(SwCompositor* cmp, SwSurface* surface, const RenderEffectGaussianBlur* params);
bool effectGaussianBlurRegion(RenderEffectGaussianBlur* effect);
void effectGaussianBlurUpdate(RenderEffectGaussianBlur* effect, const Matrix& transform);
bool effectDropShadow(SwCompositor* cmp, SwSurface* surfaces[2], const RenderEffectDropShadow* params, bool direct);
bool effectDropShadowRegion(RenderEffectDropShadow* effect);
void effectDropShadowUpdate(RenderEffectDropShadow* effect, const Matrix& transform);
void effectFillUpdate(RenderEffectFill* effect);
bool effectFill(SwCompositor* cmp, const RenderEffectFill* params, bool direct);
void effectTintUpdate(RenderEffectTint* effect);
bool effectTint(SwCompositor* cmp, const RenderEffectTint* params, bool direct);
void effectTritoneUpdate(RenderEffectTritone* effect);
bool effectTritone(SwCompositor* cmp, const RenderEffectTritone* params, bool direct);

#endif /* _TVG_SW_COMMON_H_ */
