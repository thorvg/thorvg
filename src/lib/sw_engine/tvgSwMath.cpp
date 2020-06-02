/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef _TVG_SW_MATH_H_
#define _TVG_SW_MATH_H_

#include "tvgSwCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

constexpr SwCoord CORDIC_FACTOR = 0xDBD95B16UL;            //the Cordic shrink factor 0.858785336480436 * 2^32

//this table was generated for SW_FT_PI = 180L << 16, i.e. degrees
constexpr static auto ATAN_MAX = 23;
constexpr static SwFixed ATAN_TBL[] = {
    1740967L, 919879L, 466945L, 234379L, 117304L, 58666L, 29335L,
    14668L, 7334L, 3667L, 1833L, 917L, 458L, 229L, 115L,
    57L, 29L, 14L, 7L, 4L, 2L, 1L};

static inline SwCoord SATURATE(const SwCoord x)
{
    return (x >> (sizeof(SwCoord) * 8 - 1));
}


static inline SwFixed PAD_ROUND(const SwFixed x, int32_t n)
{
    return (((x) + ((n)/2)) & ~((n)-1));
}


static SwCoord _downscale(SwFixed x)
{
    //multiply a give value by the CORDIC shrink factor
    auto s = abs(x);
    int64_t t = (s * static_cast<int64_t>(CORDIC_FACTOR)) + 0x100000000UL;
    s = static_cast<SwFixed>(t >> 32);
    if (x < 0) s = -s;
    return s;
}


static int32_t _normalize(SwPoint& pt)
{
    /* the highest bit in overflow-safe vector components
       MSB of 0.858785336480436 * sqrt(0.5) * 2^30 */
    constexpr auto SAFE_MSB = 29;

    auto v = pt;

    //High order bit(MSB)
    //clz: count leading zero’s
    auto shift = 31 - __builtin_clz(abs(v.x) | abs(v.y));

    if (shift <= SAFE_MSB) {
        shift = SAFE_MSB - shift;
        pt.x = static_cast<SwCoord>((unsigned long)v.x << shift);
        pt.y = static_cast<SwCoord>((unsigned long)v.y << shift);
    } else {
        shift -= SAFE_MSB;
        pt.x = v.x >> shift;
        pt.y = v.y >> shift;
        shift = -shift;
    }
    return shift;
}


static void _polarize(SwPoint& pt)
{
    auto v = pt;
    SwFixed theta;

    //Get the vector into [-PI/4, PI/4] sector
    if (v.y > v.x) {
        if (v.y > -v.x) {
            auto tmp = v.y;
            v.y = -v.x;
            v.x = tmp;
            theta = ANGLE_PI2;
        } else {
            theta = v.y > 0 ? ANGLE_PI : -ANGLE_PI;
            v.x = -v.x;
            v.y = -v.y;
        }
    } else {
        if (v.y < -v.x) {
            theta = -ANGLE_PI2;
            auto tmp = -v.y;
            v.y = v.x;
            v.x = tmp;
        } else {
            theta = 0;
        }
    }

    auto atan = ATAN_TBL;
    uint32_t i;
    SwFixed j;

    //Pseudorotations. with right shifts
    for (i = 1, j = 1; i < ATAN_MAX; j <<= 1, ++i) {
        if (v.y > 0) {
            auto tmp = v.x + ((v.y + j) >> i);
            v.y = v.y - ((v.x + j) >> i);
            v.x = tmp;
            theta += *atan++;
        } else {
            auto tmp = v.x - ((v.y + j) >> i);
            v.y = v.y + ((v.x + j) >> i);
            v.x = tmp;
            theta -= *atan++;
        }
    }

    //round theta
    if (theta >= 0) theta =  PAD_ROUND(theta, 32);
    else theta = -PAD_ROUND(-theta, 32);

    pt.x = v.x;
    pt.y = theta;
}


static void _rotate(SwPoint& pt, SwFixed theta)
{
    auto v = pt;

    //Rotate inside [-PI/4, PI/4] sector
    while (theta < -ANGLE_PI4) {
        auto tmp = v.y;
        v.y = -v.x;
        v.x = tmp;
        theta += ANGLE_PI2;
    }

    while (theta > ANGLE_PI4) {
        auto tmp = -v.y;
        v.y = v.x;
        v.x = tmp;
        theta -= ANGLE_PI2;
    }

    auto atan = ATAN_TBL;
    uint32_t i;
    SwFixed j;

    for (i = 1, j = 1; i < ATAN_MAX; j <<= 1, ++i) {
        if (theta < 0) {
            auto tmp = v.x + ((v.y + j) >> i);
            v.y = v.y - ((v.x + j) >> i);
            v.x = tmp;
            theta += *atan++;
        }else {
            auto tmp = v.x - ((v.y + j) >> i);
            v.y = v.y + ((v.x + j) >> i);
            v.x = tmp;
            theta -= *atan++;
        }
    }

    pt = v;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwFixed mathMean(SwFixed angle1, SwFixed angle2)
{
    return angle1 + mathDiff(angle1, angle2) / 2;
}


bool mathSmallCubic(SwPoint* base, SwFixed& angleIn, SwFixed& angleMid, SwFixed& angleOut)
{
    auto d1 = base[2] - base[3];
    auto d2 = base[1] - base[2];
    auto d3 = base[0] - base[1];

    if (d1.small()) {
        if (d2.small()) {
            if (d3.small()) {
                //basically a point.
                //do nothing to retain original direction
            } else {
                angleIn = angleMid = angleOut = mathAtan(d3);
            }
        } else {
            if (d3.small()) {
                angleIn = angleMid = angleOut = mathAtan(d2);
            } else {
                angleIn = angleMid = mathAtan(d2);
                angleOut = mathAtan(d3);
            }
        }
    } else {
        if (d2.small()) {
            if (d3.small()) {
                angleIn = angleMid = angleOut = mathAtan(d1);
            } else {
                angleIn = mathAtan(d1);
                angleOut = mathAtan(d3);
                angleMid = mathMean(angleIn, angleOut);
            }
        } else {
            if (d3.small()) {
                angleIn = mathAtan(d1);
                angleMid = angleOut = mathAtan(d2);
            } else {
                angleIn = mathAtan(d1);
                angleMid = mathAtan(d2);
                angleOut = mathAtan(d3);
            }
        }
    }

    auto theta1 = abs(mathDiff(angleIn, angleMid));
    auto theta2 = abs(mathDiff(angleMid, angleOut));

    if ((theta1 < (ANGLE_PI / 8)) && (theta2 < (ANGLE_PI / 8))) return true;
    else return false;
}


int64_t mathMultiply(int64_t a, int64_t b)
{
    int32_t s = 1;

    //move sign
    if (a < 0) {
        a = -a;
        s = -s;
    }
    if (b < 0) {
        b = -b;
        s = -s;
    }
    int64_t c = (a * b + 0x8000L ) >> 16;
    return (s > 0) ? c : -c;
}


int64_t mathDivide(int64_t a, int64_t b)
{
    int32_t s = 1;

    //move sign
    if (a < 0) {
        a = -a;
        s = -s;
    }
    if (b < 0) {
        b = -b;
        s = -s;
    }
    int64_t q = b > 0 ? ((a << 16) + (b >> 1)) / b : 0x7FFFFFFFL;
    return (s < 0 ? -q : q);
}


int64_t mathMulDiv(int64_t a, int64_t b, int64_t c)
{
    int32_t s = 1;

    //move sign
    if (a < 0) {
        a = -a;
        s = -s;
    }
    if (b < 0) {
        b = -b;
        s = -s;
    }
    if (c < 0) {
        c = -c;
        s = -s;
    }
    int64_t d = c > 0 ? (a * b + (c >> 1)) / c : 0x7FFFFFFFL;

    return (s > 0 ? d : -d);
}


void mathRotate(SwPoint& pt, SwFixed angle)
{
    if (angle == 0 || (pt.x == 0 && pt.y == 0)) return;

    auto v  = pt;
    auto shift = _normalize(v);
    auto theta = angle;

    _rotate(v, theta);

    v.x = _downscale(v.x);
    v.y = _downscale(v.y);

    if (shift > 0) {
        auto half = static_cast<int32_t>(1L << (shift - 1));
        pt.x = (v.x + half + SATURATE(v.x)) >> shift;
        pt.y = (v.y + half + SATURATE(v.y)) >> shift;
    } else {
        shift = -shift;
        pt.x = static_cast<SwCoord>((unsigned long)v.x << shift);
        pt.y = static_cast<SwCoord>((unsigned long)v.y << shift);
    }
}

SwFixed mathTan(SwFixed angle)
{
    SwPoint v = {CORDIC_FACTOR >> 8, 0};
    _rotate(v, angle);
    return mathDivide(v.y, v.x);
}


SwFixed mathAtan(const SwPoint& pt)
{
    if (pt.x == 0 && pt.y == 0) return 0;

    auto v = pt;
    _normalize(v);
    _polarize(v);

    return v.y;
}


SwFixed mathSin(SwFixed angle)
{
    return mathCos(ANGLE_PI2 - angle);
}


SwFixed mathCos(SwFixed angle)
{
    SwPoint v = {CORDIC_FACTOR >> 8, 0};
    _rotate(v, angle);
    return (v.x + 0x80L) >> 8;
}


SwFixed mathLength(SwPoint& pt)
{
    auto v = pt;

    //trivial case
    if (v.x == 0) return abs(v.y);
    if (v.y == 0) return abs(v.x);

    //general case
    auto shift = _normalize(v);
    _polarize(v);
    v.x = _downscale(v.x);

    if (shift > 0) return (v.x + (1 << (shift -1))) >> shift;
    return static_cast<SwFixed>((uint32_t)v.x << -shift);
}


void mathSplitCubic(SwPoint* base)
{
    assert(base);

    SwCoord a, b, c, d;

    base[6].x = base[3].x;
    c = base[1].x;
    d = base[2].x;
    base[1].x = a = (base[0].x + c) / 2;
    base[5].x = b = (base[3].x + d) / 2;
    c = (c + d) / 2;
    base[2].x = a = (a + c) / 2;
    base[4].x = b = (b + c) / 2;
    base[3].x = (a + b) / 2;

    base[6].y = base[3].y;
    c = base[1].y;
    d = base[2].y;
    base[1].y = a = (base[0].y + c) / 2;
    base[5].y = b = (base[3].y + d) / 2;
    c = (c + d) / 2;
    base[2].y = a = (a + c) / 2;
    base[4].y = b = (b + c) / 2;
    base[3].y = (a + b) / 2;
}


SwFixed mathDiff(SwFixed angle1, SwFixed angle2)
{
    auto delta = angle2 - angle1;

    delta %= ANGLE_2PI;
    if (delta < 0) delta += ANGLE_2PI;
    if (delta > ANGLE_PI) delta -= ANGLE_2PI;

    return delta;
}
#endif /* _TVG_SW_MATH_H_ */