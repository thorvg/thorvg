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
#ifndef _TVG_SW_STROKER_H_
#define _TVG_SW_STROKER_H_

#include "tvgSwCommon.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
constexpr auto CORDIC_FACTOR = 0xDBD95B16UL;            //the Cordic shrink factor 0.858785336480436 * 2^32
constexpr static SwFixed ANGLE_PI = (180L << 16);
constexpr static SwFixed ANGLE_2PI = (ANGLE_PI << 1);
constexpr static SwFixed ANGLE_PI2 = (ANGLE_PI >> 1);
constexpr static SwFixed ANGLE_PI4 = (ANGLE_PI >> 2);

//this table was generated for SW_FT_PI = 180L << 16, i.e. degrees
constexpr static auto ATAN_MAX = 23;
constexpr static SwFixed ATAN_TBL[] = {
    1740967L, 919879L, 466945L, 234379L, 117304L, 58666L, 29335L,
    14668L, 7334L, 3667L, 1833L, 917L, 458L, 229L, 115L,
    57L, 29L, 14L, 7L, 4L, 2L, 1L};


static inline SwCoord SATURATE(const SwCoord x)
{
    return (x >> (sizeof(long) * 8 - 1));
}


static inline SwFixed SIDE_TO_ROTATE(int32_t s)
{
    return (ANGLE_PI2 - (s) * ANGLE_PI);
}


static int64_t _multiply(int64_t a, int64_t b)
{
    int32_t s = 1;

    //move sign
    if (a < 0) {
        a = -a;
        s = -s;
    }
    //move sign
    if (b < 0) {
        b = -b;
        s = -s;
    }
    int64_t c = (a * b + 0x8000L ) >> 16;
    return (s > 0) ? c : -c;
}


static int64_t _divide(int64_t a, int64_t b)
{
    int32_t s = 1;

    //move sign
    if (a < 0) {
        a = -a;
        s = -s;
    }
    //move sign
    if (b < 0) {
        b = -b;
        s = -s;
    }
    int64_t q = b > 0 ? ((a << 16) + (b >> 1)) / b : 0x7FFFFFFFL;
    return (s < 0 ? -q : q);
}


static SwFixed _angleDiff(SwFixed angle1, SwFixed angle2)
{
    auto delta = angle2 - angle1;

    delta %= ANGLE_2PI;
    if (delta < 0) delta += ANGLE_2PI;
    if (delta > ANGLE_PI) delta -= ANGLE_2PI;

    return delta;
}


static void _trigDownscale(SwPoint& pt)
{
    //multiply a give value by the CORDIC shrink factor

    auto s = pt;

    //abs
    if (pt.x < 0) pt.x = -pt.x;
    if (pt.y < 0) pt.y = -pt.y;

    int64_t vx = (pt.x * static_cast<int64_t>(CORDIC_FACTOR)) + 0x100000000UL;
    int64_t vy = (pt.y * static_cast<int64_t>(CORDIC_FACTOR)) + 0x100000000UL;

    pt.x = static_cast<SwFixed>(vx >> 32);
    pt.y = static_cast<SwFixed>(vy >> 32);

    if (s.x < 0) pt.x = -pt.x;
    if (s.y < 0) pt.y = -pt.y;
}


static int32_t _trigPrenorm(SwPoint& pt)
{
    /* the highest bit in overflow-safe vector components
       MSB of 0.858785336480436 * sqrt(0.5) * 2^30 */
    constexpr auto TRIG_SAFE_MSB = 29;

    auto v = pt;

    //High order bit(MSB)
    //clz: count leading zero’s
    auto shift = 31 - __builtin_clz(abs(v.x) | abs(v.y));

    if (shift <= TRIG_SAFE_MSB) {
        shift = TRIG_SAFE_MSB - shift;
        pt.x = static_cast<SwCoord>((unsigned long)v.x << shift);
        pt.y = static_cast<SwCoord>((unsigned long)v.y << shift);
    } else {
        shift -= TRIG_SAFE_MSB;
        pt.x = v.x >> shift;
        pt.y = v.y >> shift;
        shift = -shift;
    }
    return shift;
}


static void _trigPseudoRotate(SwPoint& pt, SwFixed theta)
{
    auto v = pt;

    //Rotate inside [-PI/4, PI/4] sector
    while (theta < -ANGLE_PI4) {
        auto temp = v.y;
        v.y = -v.x;
        v.x = temp;
        theta += ANGLE_PI2;
    }

    while (theta > ANGLE_PI4) {
        auto temp = -v.y;
        v.y = v.x;
        v.x = temp;
        theta -= ANGLE_PI2;
    }

    auto atan = ATAN_TBL;
    uint32_t i;
    SwFixed j;

    for (i = 1, j = 1; i < ATAN_MAX; j <<= 1, ++i) {
        if (theta < 0) {
            auto temp = v.x + ((v.y + j) >> i);
            v.y = v.y - ((v.x + j) >> i);
            v.x = temp;
            theta += *atan++;
        }else {
            auto temp = v.x - ((v.y + j) >> i);
            v.y = v.y + ((v.x + j) >> i);
            v.x = temp;
            theta -= *atan++;
        }
    }

    pt = v;
}


static void _rotate(SwPoint& pt, SwFixed angle)
{
    if (angle == 0 || (pt.x == 0 && pt.y == 0)) return;

    auto v  = pt;
    auto shift = _trigPrenorm(v);
    _trigPseudoRotate(v, angle);
    _trigDownscale(v);

    if (shift > 0) {
        auto half = static_cast<int32_t>(1L << (shift - 1));
        v.x = (v.x + half + SATURATE(v.x)) >> shift;
        v.y = (v.y + half + SATURATE(v.y)) >> shift;
    } else {
        shift = -shift;
        v.x = static_cast<SwCoord>((unsigned long)v.x << shift);
        v.y = static_cast<SwCoord>((unsigned long)v.y << shift);
    }
}


static SwFixed _tan(SwFixed angle)
{
    SwPoint v = {CORDIC_FACTOR >> 8, 0};
    _rotate(v, angle);
    return _divide(v.y, v.x);
}


static SwFixed _cos(SwFixed angle)
{
    SwPoint v = {CORDIC_FACTOR >> 8, 0};
    _rotate(v, angle);
    return (v.x + 0x80L) >> 8;
}


static void _lineTo(SwStroke& stroke, const SwPoint& to)
{

}


static void _cubicTo(SwStroke& stroke, const SwPoint& ctrl1, const SwPoint& ctrl2, const SwPoint& to)
{

}


static void _arcTo(SwStroke& stroke, int32_t side)
{

}


static void _growBorder(SwStrokeBorder* border, uint32_t newPts)
{
    auto maxOld = border->maxPts;
    auto maxNew = border->ptsCnt + newPts;

    if (maxNew <= maxOld) return;

    auto maxCur = maxOld;

    while (maxCur < maxNew)
        maxCur += (maxCur >> 1) + 16;

    border->pts = static_cast<SwPoint*>(realloc(border->pts, maxCur * sizeof(SwPoint)));
    assert(border->pts);

    border->tags = static_cast<uint8_t*>(realloc(border->tags, maxCur * sizeof(uint8_t)));
    assert(border->tags);

    border->maxPts = maxCur;

    printf("realloc border!!! (%u => %u)\n", maxOld, maxCur);
}



static void _borderLineTo(SwStrokeBorder* border, SwPoint& to, bool movable)
{
    constexpr SwCoord EPSILON = 2;

    assert(border && border->start >= 0);

    if (border->movable) {
        //move last point
        border->pts[border->ptsCnt - 1] = to;
    } else {
        //don't add zero-length line_to
        auto diff = border->pts[border->ptsCnt - 1] - to;
        if (border->ptsCnt > 0 && abs(diff.x) < EPSILON && abs(diff.y) < EPSILON) return;

        _growBorder(border, 1);
        border->pts[border->ptsCnt] = to;
        border->tags[border->ptsCnt] = SW_STROKE_TAG_ON;
        border->ptsCnt += 1;
    }

    border->movable = movable;
}


static void _addCap(SwStroke& stroke, SwFixed angle, int32_t side)
{
     if (stroke.cap == StrokeCap::Square) {
        auto rotate = SIDE_TO_ROTATE(side);
        auto border = stroke.borders + side;

        SwPoint delta = {stroke.width, 0};
        _rotate(delta, angle);

        SwPoint delta2 = {stroke.width, 0};
        _rotate(delta2, angle + rotate);

        delta += stroke.center + delta2;

        _borderLineTo(border, delta, false);

        delta = {stroke.width, 0};
        _rotate(delta, angle);

        delta2 = {stroke.width, 0};
        _rotate(delta2, angle - rotate);

        delta += delta2 + stroke.center;

        _borderLineTo(border, delta, false);

    } else if (stroke.cap == StrokeCap::Round) {

        stroke.angleIn = angle;
        stroke.angleOut = angle + ANGLE_PI;
        _arcTo(stroke, side);
        return;

    } else {  //Butt
        auto rotate = SIDE_TO_ROTATE(side);
        auto border = stroke.borders + side;

        SwPoint delta = {stroke.width, 0};
        _rotate(delta, angle + rotate);

        delta += stroke.center;

        _borderLineTo(border, delta, false);

        delta = {stroke.width, 0};
        _rotate(delta, angle - rotate);

        delta += stroke.center;

        _borderLineTo(border, delta, false);
    }
}


static void _addReverseLeft(SwStroke& stroke, bool opened)
{
    auto right = stroke.borders + 0;
    auto left = stroke.borders + 1;
    assert(left->start >= 0);

    auto newPts = left->ptsCnt - left->start;

    if (newPts <= 0) return;

    _growBorder(right, newPts);

    auto dstPt = right->pts + right->ptsCnt;
    auto dstTag = right->tags + right->ptsCnt;
    auto srcPt = left->pts + left->ptsCnt - 1;
    auto srcTag = left->tags + left->ptsCnt - 1;

    while (srcPt >= left->pts + left->start) {
        *dstPt = *srcPt;
        *dstTag = *srcTag;

        if (opened) {
             dstTag[0] &= ~(SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
        } else {
            //switch begin/end tags if necessary
            auto ttag = dstTag[0] & (SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
            if (ttag == (SW_STROKE_TAG_BEGIN || SW_STROKE_TAG_END)) {
                dstTag[0] ^= (SW_STROKE_TAG_BEGIN || SW_STROKE_TAG_END);
            }
        }

        --srcPt;
        --srcTag;
        --dstPt;
        --dstTag;
    }

    left->ptsCnt = left->start;
    right->ptsCnt += newPts;
    right->movable = false;
    left->movable = false;
}


static void _closeBorder(SwStrokeBorder* border, bool reverse)
{
    assert(border && border->start >= 0);

    uint32_t start = border->start;
    uint32_t count = border->ptsCnt;

    //Don't record empty paths!
    if (count <= start + 1U) {
        border->ptsCnt = start;
    } else {
        /* Copy the last point to the start of this sub-path,
           since it contains the adjusted starting coordinates */
        border->ptsCnt = --count;
        border->pts[start] = border->pts[count];

        if (reverse) {
            //reverse the points
            auto pt1 = border->pts + start + 1;
            auto pt2 = border->pts + count - 1;

            for (; pt1 < pt2; pt1++, pt2--) {
                auto tmp = *pt1;
                *pt1 = *pt2;
                *pt2 = tmp;
            }

            //reverse the tags
            auto tag1 = border->tags + start + 1;
            auto tag2 = border->tags + count - 1;

            for (; tag1 < tag2; tag1++, tag2++) {
                auto tmp = *tag1;
                *tag1 = *tag2;
                *tag2 = tmp;
            }
        }

        border->tags[start] |= SW_STROKE_TAG_BEGIN;
        border->tags[count - 1] |=  SW_STROKE_TAG_END;
    }

    border->start = -1;
    border->movable = false;
}


static void _inside(SwStroke& stroke, int32_t side, SwFixed lineLength)
{
    auto border = stroke.borders + side;
    auto theta = _angleDiff(stroke.angleIn, stroke.angleOut) / 2;
    SwPoint delta;
    bool intersect;

    /* Only intersect borders if between two line_to's and both
       lines are long enough (line length is zero fur curves). */
    if (!border->movable || lineLength == 0) {
        intersect = false;
    } else {
        //compute minimum required length of lines
        SwFixed minLength = abs(_multiply(stroke.width, _tan(theta)));
        if (stroke.lineLength >= minLength && lineLength >= minLength) intersect = true;
    }

    auto rotate = SIDE_TO_ROTATE(side);

    if (!intersect) {
        delta = {stroke.width, 0};
        _rotate(delta, stroke.angleOut + rotate);
        delta += stroke.center;
        border->movable = false;
    } else {
        //compute median angle
        delta = {_divide(stroke.width, _cos(theta)), 0};
        _rotate(delta, stroke.angleIn + theta + rotate);
        delta += stroke.center;
    }

    _borderLineTo(border, delta, false);
}


static void _beginSubPath(SwStroke& stroke, SwPoint& to, bool opened)
{
    cout << "stroke opened? = " << opened << endl;

    /* We cannot process the first point because there is not enought
       information regarding its corner/cap. Later, it will be processed
       in the _strokeEndSubPath() */

    stroke.firstPt = true;
    stroke.center = to;
    stroke.subPathOpen = opened;

    /* Determine if we need to check whether the border radius is greater
       than the radius of curvature of a curve, to handle this case specially.
       This is only required if bevel joins or butt caps may be created because
       round & miter joins and round & square caps cover the nagative sector
       created with wide strokes. */
    if ((stroke.join != StrokeJoin::Round) || (stroke.subPathOpen && stroke.cap == StrokeCap::Butt))
        stroke.handleWideStrokes = true;
    else
        stroke.handleWideStrokes = false;

    stroke.subPathStart = to;
    stroke.angleIn = 0;
}


static void _endSubPath(SwStroke& stroke)
{
    if (stroke.subPathOpen) {
        auto right = stroke.borders;
        assert(right);

        /* all right, this is an opened path, we need to add a cap between
           right & left, add the reverse of left, then add a final cap
           between left & right */
        _addCap(stroke, stroke.angleIn, 0);

        //add reversed points from 'left' to 'right'
        _addReverseLeft(stroke, true);

        //now add the final cap
        stroke.center = stroke.subPathStart;
        _addCap(stroke, stroke.subPathAngle + ANGLE_PI, 0);

        /* now end the right subpath accordingly. The left one is rewind
           and deosn't need further processing */
        _closeBorder(right, false);
    } else {

        //close the path if needed
        if (stroke.center != stroke.subPathStart)
            _lineTo(stroke, stroke.subPathStart);

        //process the corner
        stroke.angleOut = stroke.subPathAngle;
        auto turn = _angleDiff(stroke.angleIn, stroke.angleOut);

        //No specific corner processing is required if the turn is 0
        if (turn != 0) {

            //when we turn to the right, the inside is 0
            auto inside = 0;

            //otherwise, the inside is 1
            if (turn < 0) inside = 1;

            _inside(stroke, inside, stroke.subPathLineLength);        //inside
            _inside(stroke, 1 - inside, stroke.subPathLineLength);    //outside
        }

        _closeBorder(stroke.borders + 0, false);
        _closeBorder(stroke.borders + 1, true);
    }
}


static void _deleteRle(SwStroke& stroke)
{
    if (!stroke.rle) return;
    if (stroke.rle->spans) free(stroke.rle->spans);
    free(stroke.rle);
    stroke.rle = nullptr;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void strokeFree(SwStroke* stroke)
{
    if (!stroke) return;
    _deleteRle(*stroke);
    free(stroke);
}


void strokeReset(SwStroke& stroke, float width, StrokeCap cap, StrokeJoin join)
{
    _deleteRle(stroke);

#if 0
    miterLimit = 4 * (1 >> 16);

    /* ensure miter limit has sensible value */
    if ( stroker->miter_limit < 0x10000 )
      stroker->miter_limit = 0x10000;
#endif

    stroke.width = TO_SWCOORD(width * 0.5f);
    stroke.cap = cap;

    //Save line join: it can be temporarily changed when stroking curves...
    stroke.joinSaved = stroke.join = join;

    stroke.borders[0].ptsCnt = 0;
    stroke.borders[0].start = -1;
    stroke.borders[0].valid = false;

    stroke.borders[1].ptsCnt = 0;
    stroke.borders[1].start = -1;
    stroke.borders[1].valid = false;
}

bool strokeParseOutline(SwStroke& stroke, SwOutline& outline)
{
    uint32_t first = 0;

    for (uint32_t i = 0; i < outline.cntrsCnt; ++i) {
        auto last = outline.cntrs[i];  //index of last point in contour
        auto limit = outline.pts + last;
        assert(limit);

        //Skip empty points
        if (last <= first) {
            first = last + 1;
            continue;
        }

        auto start = outline.pts[first];

        auto pt = outline.pts + first;
        assert(pt);
        auto types = outline.types + first;
        assert(types);

        auto type = types[0];

        //A contour cannot start with a cubic control point
        if (type == SW_CURVE_TYPE_CUBIC) return false;

        _beginSubPath(stroke, start, outline.opened);

        while (pt < limit) {
            assert(++pt);
            assert(++types);

            //emit a signel line_to
            if (types[0] == SW_CURVE_TYPE_POINT) {
                _lineTo(stroke, *pt);
            //types cubic
            } else {
                if (pt + 1 > limit || types[1] != SW_CURVE_TYPE_CUBIC) return false;

                pt += 2;
                types += 2;

                if (pt <= limit) {
                    _cubicTo(stroke, pt[-2], pt[-1], pt[0]);
                    continue;
                }
                _cubicTo(stroke, pt[-2], pt[-1], start);
                goto close;
            }
        }

    close:
        if (!stroke.firstPt) _endSubPath(stroke);
        first = last + 1;
    }
    return true;
}


#endif /* _TVG_SW_STROKER_H_ */
