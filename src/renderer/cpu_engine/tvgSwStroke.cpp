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

#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define SW_STROKE_TAG_POINT 1
#define SW_STROKE_TAG_CUBIC 2
#define SW_STROKE_TAG_BEGIN 4
#define SW_STROKE_TAG_END 8

static inline float SIDE_TO_ROTATE(const int32_t s)
{
    return s ? -SW_PI2 : SW_PI2;
}

static inline void SCALE(const SwStroke& stroke, Point& pt)
{
    pt.x *= stroke.sx;
    pt.y *= stroke.sy;
}

static void _growBorder(SwStrokeBorder* border, uint32_t newPts)
{
    if (border->pts.count + newPts <= border->pts.reserved) return;
    border->pts.grow(newPts * 20);
    border->tags = tvg::realloc<uint8_t>(border->tags, border->pts.reserved);      //align the pts / tags memory size
}


static void _borderClose(SwStrokeBorder* border, bool reverse)
{
    auto start = border->start;
    auto count = border->pts.count;

    //Don't record empty paths!
    if (count <= start + 1U) {
        border->pts.count = start;
    } else {
        /* Copy the last point to the start of this sub-path,
           since it contains the adjusted starting coordinates */
        border->pts.count = --count;
        border->pts[start] = border->pts[count];

        if (reverse) {
            //reverse the points
            auto pt1 = border->pts.data + start + 1;
            auto pt2 = border->pts.data + count - 1;

            while (pt1 < pt2) {
                std::swap(*pt1, *pt2);
                ++pt1;
                --pt2;
            }

            //reverse the tags
            auto tag1 = border->tags + start + 1;
            auto tag2 = border->tags + count - 1;

            while (tag1 < tag2) {
                std::swap(*tag1, *tag2);
                ++tag1;
                --tag2;
            }
        }

        border->tags[start] |= SW_STROKE_TAG_BEGIN;
        border->tags[count - 1] |=  SW_STROKE_TAG_END;
    }

    border->start = -1;
    border->movable = false;
}


static void _borderCubicTo(SwStrokeBorder* border, const Point& ctrl1, const Point& ctrl2, const Point& to)
{
    _growBorder(border, 3);

    auto tag = border->tags + border->pts.count;

    border->pts.push(ctrl1);
    border->pts.push(ctrl2);
    border->pts.push(to);

    tag[0] = SW_STROKE_TAG_CUBIC;
    tag[1] = SW_STROKE_TAG_CUBIC;
    tag[2] = SW_STROKE_TAG_POINT;

    border->movable = false;
}


static void _borderArcTo(SwStrokeBorder* border, const Point& center, float radius, float angleStart, float angleDiff, SwStroke& stroke)
{
    Point a = {radius, 0.0f};
    mathRotate(a, angleStart);
    SCALE(stroke, a);
    a += center;

    auto total = angleDiff;
    auto angle = angleStart;
    auto rotate = (angleDiff >= 0) ? SW_PI2 : -SW_PI2;

    while (!tvg::zero(total)) {
        auto step = total;
        if (step > SW_PI2) step = SW_PI2;
        else if (step < -SW_PI2) step = -SW_PI2;

        auto next = angle + step;
        auto theta = fabsf(step) * 0.5f;

        //compute end point
        Point b = {radius, 0.0f};
        mathRotate(b, next);
        SCALE(stroke, b);
        b += center;

        //compute first and second control points
        auto length = radius * (sinf(theta) * 4.0f) / ((1.0f + cosf(theta)) * 3.0f);

        Point a2 = {length, 0.0f};
        mathRotate(a2, angle + rotate);
        SCALE(stroke, a2);
        a2 += a;

        Point b2 = {length, 0.0f};
        mathRotate(b2, next - rotate);
        SCALE(stroke, b2);
        b2 += b;

        //add cubic arc
        _borderCubicTo(border, a2, b2, b);

        //process the rest of the arc?
        a = b;
        total -= step;
        angle = next;
    }
}


static void _borderLineTo(SwStrokeBorder* border, const Point& to, bool movable)
{
    if (border->movable) {
        //move last point
        border->pts.last() = to;
    } else {
        //don't add zero-length line_to
        if (!border->pts.empty() && tvg::zero(border->pts.last() - to)) return;
        _growBorder(border, 1);
        border->tags[border->pts.count] = SW_STROKE_TAG_POINT;
        border->pts.push(to);
    }

    border->movable = movable;
}


static void _borderMoveTo(SwStrokeBorder* border, Point& to)
{
    //close current open path if any?
    if (border->start >= 0) _borderClose(border, false);

    border->start = border->pts.count;
    border->movable = false;

    _borderLineTo(border, to, false);
}


static void _arcTo(SwStroke& stroke, int32_t side)
{
    auto border = stroke.borders[side];
    auto rotate = SIDE_TO_ROTATE(side);
    auto total = mathDiff(stroke.angleIn, stroke.angleOut);
    if (total == SW_PI) total = -rotate * 2;

    _borderArcTo(border, stroke.center, stroke.width, stroke.angleIn + rotate, total, stroke);
    border->movable = false;
}


static void _outside(SwStroke& stroke, int32_t side, float lineLength)
{
    auto border = stroke.borders[side];

    if (stroke.join == StrokeJoin::Round) {
        _arcTo(stroke, side);
    } else {
        //this is a mitered (pointed) or beveled (truncated) corner
        auto rotate = SIDE_TO_ROTATE(side);
        auto bevel = stroke.join == StrokeJoin::Bevel;
        auto phi = 0.0f;
        auto thcos = 0.0f;

        if (!bevel) {
            auto theta = mathDiff(stroke.angleIn, stroke.angleOut);
            if (theta == SW_PI) {
                theta = rotate;
                phi = stroke.angleIn;
            } else {
                theta /= 2;
                phi = stroke.angleIn + theta + rotate;
            }

            thcos = mathCos(theta);
            auto sigma = stroke.miterlimit * thcos;
            if (sigma < 1.0f) bevel = true;      // is miter limit exceeded?
        }

        //this is a bevel (broken angle)
        if (bevel) {
            Point delta = {stroke.width, 0.0f};
            mathRotate(delta, stroke.angleOut + rotate);
            SCALE(stroke, delta);
            delta += stroke.center;
            border->movable = false;
            _borderLineTo(border, delta, false);
        //this is a miter (intersection)
        } else {
            auto length = stroke.width / thcos;
            Point delta = {length, 0.0f};
            mathRotate(delta, phi);
            SCALE(stroke, delta);
            delta += stroke.center;
            _borderLineTo(border, delta, false);

            /* Now add and end point
               Only needed if not lineto (lineLength is zero for curves) */
            if (tvg::zero(lineLength)) {
                delta = {stroke.width, 0.0f};
                mathRotate(delta, stroke.angleOut + rotate);
                SCALE(stroke, delta);
                delta += stroke.center;
                _borderLineTo(border, delta, false);
            }
        }
    }
}


static void _inside(SwStroke& stroke, int32_t side, float lineLength)
{
    auto border = stroke.borders[side];
    auto theta = mathDiff(stroke.angleIn, stroke.angleOut) * 0.5f;
    auto intersect = false;

    /* Only intersect borders if between two line_to's and both
       lines are long enough (line length is zero for curves). */
    if (border->movable && lineLength > 0.0f) {
        //compute minimum required length of lines
        auto minLength = fabsf(stroke.width * mathTan(theta));
        if (stroke.lineLength >= minLength && lineLength >= minLength) intersect = true;
    }

    auto rotate = SIDE_TO_ROTATE(side);
    Point delta;

    if (!intersect) {
        delta = {stroke.width, 0.0f};
        mathRotate(delta, stroke.angleOut + rotate);
        SCALE(stroke, delta);
        delta += stroke.center;
        border->movable = false;
    } else {
        //compute median angle
        auto phi = stroke.angleIn + theta;
        auto thcos = mathCos(theta);
        delta = {stroke.width / thcos, 0.0f};
        mathRotate(delta, phi + rotate);
        SCALE(stroke, delta);
        delta += stroke.center;
    }

    _borderLineTo(border, delta, false);
}


void _processCorner(SwStroke& stroke, int64_t lineLength)
{
    auto turn = mathDiff(stroke.angleIn, stroke.angleOut);

    //no specific corner processing is required if the turn is 0
    if (turn == 0) return;

    //when we turn to the right, the inside side is 0
    int32_t inside = 0;

    //otherwise, the inside is 1
    if (turn < 0) inside = 1;

    //process the inside
    _inside(stroke, inside, lineLength);

    //process the outside
    _outside(stroke, 1 - inside, lineLength);
}


void _firstSubPath(SwStroke& stroke, float startAngle, float lineLength)
{
    Point delta = {stroke.width, 0.0f};
    mathRotate(delta, startAngle + SW_PI2);
    SCALE(stroke, delta);

    auto pt = stroke.center + delta;
    _borderMoveTo(stroke.borders[0], pt);

    pt = stroke.center - delta;
    _borderMoveTo(stroke.borders[1], pt);

    /* Save angle, position and line length for last join
       lineLength is zero for curves */
    stroke.subPathAngle = startAngle;
    stroke.firstPt = false;
    stroke.subPathLineLength = lineLength;
}


static void _lineTo(SwStroke& stroke, const Point& to)
{
    auto delta = to - stroke.center;

    //a zero-length lineto is a no-op
    if (tvg::zero(delta)) {
        //round and square caps are expected to be drawn as a dot even for zero-length lines
        if (stroke.firstPt && stroke.cap != StrokeCap::Butt) _firstSubPath(stroke, 0, 0); 
        return; 
    }

    /* The lineLength is used to determine the intersection of strokes outlines.
       The scale needs to be reverted since the stroke width has not been scaled.
       An alternative option is to scale the width of the stroke properly by
       calculating the mixture of the sx/sy rating on the stroke direction. */
    delta.x /= stroke.sx;
    delta.y /= stroke.sy;
    auto lineLength = fsqrt(delta);
    auto angle = mathAtan(delta);

    delta = {stroke.width, 0.0f};
    mathRotate(delta, angle + SW_PI2);
    SCALE(stroke, delta);

    //process corner if necessary
    if (stroke.firstPt) {
        /* This is the first segment of a subpath. We need to add a point to each border
        at their respective starting point locations. */
        _firstSubPath(stroke, angle, lineLength);
    } else {
        //process the current corner
        stroke.angleOut = angle;
        _processCorner(stroke, lineLength);
    }

    //now add a line segment to both the inside and outside paths
    for (int side = 0; side < 2; ++side) {
        //the ends of lineto borders are movable
        _borderLineTo(stroke.borders[side], to + delta, true);
        delta.x = -delta.x;
        delta.y = -delta.y;
    }

    stroke.angleIn = angle;
    stroke.center = to;
    stroke.lineLength = lineLength;
}


static void _cubicTo(SwStroke& stroke, const Point& ctrl1, const Point& ctrl2, const Point& to)
{
    Point bezStack[37];   //TODO: static?
    auto limit = bezStack + 32;
    auto arc = bezStack;
    auto firstArc = true;
    arc[0] = to;
    arc[1] = ctrl2;
    arc[2] = ctrl1;
    arc[3] = stroke.center;

    while (arc >= bezStack) {
        float angleIn, angleOut, angleMid;

        //initialize with current direction
        angleIn = angleOut = angleMid = stroke.angleIn;

        auto valid = mathCubicAngle(arc, angleIn, angleMid, angleOut);

        //valid size
        if (valid > 0 && arc < limit) {
            if (stroke.firstPt) stroke.angleIn = angleIn;
            mathSplitCubic(arc);
            arc += 3;
            continue;
        }

        //ignoreable size
        if (valid < 0 && arc == bezStack) {
            stroke.center = to;

            //round and square caps are expected to be drawn as a dot even for zero-length lines
            if (stroke.firstPt && stroke.cap != StrokeCap::Butt) _firstSubPath(stroke, 0, 0);
            return;
        }

        //small size
        if (firstArc) {
            firstArc = false;
            //process corner if necessary
            if (stroke.firstPt) {
                _firstSubPath(stroke, angleIn, 0);
            } else {
                stroke.angleOut = angleIn;
                _processCorner(stroke, 0);
            }
        } else if (abs(mathDiff(stroke.angleIn, angleIn)) > (SW_PI / 8) / 4) {
            //if the deviation from one arc to the next is too great add a round corner
            stroke.center = arc[3];
            stroke.angleOut = angleIn;
            stroke.join = StrokeJoin::Round;

            _processCorner(stroke, 0);

            //reinstate line join style
            stroke.join = stroke.joinSaved;
        }

        //the arc's angle is small enough; we can add it directly to each border
        auto theta1 = mathDiff(angleIn, angleMid) / 2;
        auto theta2 = mathDiff(angleMid, angleOut) / 2;
        auto phi1 = mathMean(angleIn, angleMid);
        auto phi2 = mathMean(angleMid, angleOut);
        auto length1 = stroke.width / mathCos(theta1);
        auto length2 = stroke.width / mathCos(theta2);
        auto alpha0 = 0.0f;

        //compute direction of original arc
        if (stroke.handleWideStrokes) alpha0 = mathAtan(arc[0] - arc[3]);

        for (int side = 0; side < 2; ++side) {
            auto border = stroke.borders[side];
            auto rotate = SIDE_TO_ROTATE(side);

            //compute control points
            Point _ctrl1 = {length1, 0.0f};
            mathRotate(_ctrl1, phi1 + rotate);
            SCALE(stroke, _ctrl1);
            _ctrl1 += arc[2];

            Point _ctrl2 = {length2, 0.0f};
            mathRotate(_ctrl2, phi2 + rotate);
            SCALE(stroke, _ctrl2);
            _ctrl2 += arc[1];

            //compute end point
            Point end = {stroke.width, 0.0f};
            mathRotate(end, angleOut + rotate);
            SCALE(stroke, end);
            end += arc[0];

            if (stroke.handleWideStrokes) {
                /* determine whether the border radius is greater than the radius of
                   curvature of the original arc */
                auto start = border->pts.last();
                auto alpha1 = mathAtan(end - start);

                //is the direction of the border arc opposite to that of the original arc?
                if (abs(mathDiff(alpha0, alpha1)) > SW_PI / 2) {

                    //use the sine rule to find the intersection point
                    auto beta = mathAtan(arc[3] - start);
                    auto gamma = mathAtan(arc[0] - end);
                    auto bvec = end - start;
                    auto blen = fsqrt(bvec);
                    auto sinA = abs(mathSin(alpha1 - gamma));
                    auto sinB = abs(mathSin(beta - gamma));
                    auto alen = blen * sinA / sinB;

                    Point delta = {alen, 0.0f};
                    mathRotate(delta, beta);
                    delta += start;

                    //circumnavigate the negative sector backwards
                    border->movable = false;
                    _borderLineTo(border, delta, false);
                    _borderLineTo(border, end, false);
                    _borderCubicTo(border, _ctrl2, _ctrl1, start);

                    //and then move to the endpoint
                    _borderLineTo(border, end, false);
                    continue;
                }
            }
            _borderCubicTo(border, _ctrl1, _ctrl2, end);
        }
        arc -= 3;
        stroke.angleIn = angleOut;
    }
    stroke.center = to;
}


static void _addCap(SwStroke& stroke, float angle, int32_t side)
{
    if (stroke.cap == StrokeCap::Square) {
        auto rotate = SIDE_TO_ROTATE(side);
        auto border = stroke.borders[side];

        Point delta = {stroke.width, 0.0f};
        mathRotate(delta, angle);
        SCALE(stroke, delta);

        Point delta2 = {stroke.width, 0.0f};
        mathRotate(delta2, angle + rotate);
        SCALE(stroke, delta2);
        delta += stroke.center + delta2;

        _borderLineTo(border, delta, false);

        delta = {stroke.width, 0.0f};
        mathRotate(delta, angle);
        SCALE(stroke, delta);

        delta2 = {stroke.width, 0.0f};
        mathRotate(delta2, angle - rotate);
        SCALE(stroke, delta2);
        delta += delta2 + stroke.center;

        _borderLineTo(border, delta, false);
    } else if (stroke.cap == StrokeCap::Round) {
        stroke.angleIn = angle;
        stroke.angleOut = angle + SW_PI;
        _arcTo(stroke, side);
    } else {  //Butt
        auto rotate = SIDE_TO_ROTATE(side);
        auto border = stroke.borders[side];

        Point delta = {stroke.width, 0.0f};
        mathRotate(delta, angle + rotate);
        SCALE(stroke, delta);
        delta += stroke.center;

        _borderLineTo(border, delta, false);

        delta = {stroke.width, 0.0f};
        mathRotate(delta, angle - rotate);
        SCALE(stroke, delta);
        delta += stroke.center;

        _borderLineTo(border, delta, false);
    }
}


static void _addReverseLeft(SwStroke& stroke, bool opened)
{
    auto right = stroke.borders[0];
    auto left = stroke.borders[1];
    auto newPts = left->pts.count - left->start;

    if (newPts <= 0) return;

    _growBorder(right, newPts);

    auto dstTag = right->tags + right->pts.count;
    auto srcPt = left->pts.end() - 1;
    auto srcTag = left->tags + left->pts.count - 1;

    while (srcPt >= left->pts.data + left->start) {
        right->pts.push(*srcPt);
        *dstTag = *srcTag;

        if (opened) {
             dstTag[0] &= ~(SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
        } else {
            //switch begin/end tags if necessary
            auto ttag = dstTag[0] & (SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
            if (ttag == SW_STROKE_TAG_BEGIN || ttag == SW_STROKE_TAG_END)
              dstTag[0] ^= (SW_STROKE_TAG_BEGIN | SW_STROKE_TAG_END);
        }
        --srcPt;
        --srcTag;
        ++dstTag;
    }

    left->pts.count = left->start;
    right->movable = false;
    left->movable = false;
}


static void _beginSubPath(SwStroke& stroke, const Point& to, bool closed)
{
    /* We cannot process the first point because there is not enough
       information regarding its corner/cap. Later, it will be processed
       in the _endSubPath() */

    stroke.firstPt = true;
    stroke.center = to;
    stroke.closedSubPath = closed;

    /* Determine if we need to check whether the border radius is greater
       than the radius of curvature of a curve, to handle this case specially.
       This is only required if bevel joins or butt caps may be created because
       round & miter joins and round & square caps cover the negative sector
       created with wide strokes. */
    if ((stroke.join != StrokeJoin::Round) || (!stroke.closedSubPath && stroke.cap == StrokeCap::Butt))
        stroke.handleWideStrokes = true;
    else
        stroke.handleWideStrokes = false;

    stroke.ptStartSubPath = to;
    stroke.angleIn = 0;
}


static void _endSubPath(SwStroke& stroke)
{
    if (stroke.closedSubPath) {
        //close the path if needed
        if (stroke.center != stroke.ptStartSubPath)
            _lineTo(stroke, stroke.ptStartSubPath);

        //process the corner
        stroke.angleOut = stroke.subPathAngle;
        auto turn = mathDiff(stroke.angleIn, stroke.angleOut);

        //No specific corner processing is required if the turn is 0
        if (turn != 0) {
            //when we turn to the right, the inside is 0
            int32_t inside = 0;

            //otherwise, the inside is 1
            if (turn < 0) inside = 1;

            _inside(stroke, inside, stroke.subPathLineLength);        //inside
            _outside(stroke, 1 - inside, stroke.subPathLineLength);   //outside
        }

        _borderClose(stroke.borders[0], false);
        _borderClose(stroke.borders[1], true);
    } else {
        auto right = stroke.borders[0];

        /* all right, this is an opened path, we need to add a cap between
           right & left, add the reverse of left, then add a final cap
           between left & right */
        _addCap(stroke, stroke.angleIn, 0);

        //add reversed points from 'left' to 'right'
        _addReverseLeft(stroke, true);

        //now add the final cap
        stroke.center = stroke.ptStartSubPath;
        _addCap(stroke, stroke.subPathAngle + SW_PI, 0);

        /* now end the right subpath accordingly. The left one is rewind
           and doesn't need further processing */
        _borderClose(right, false);
    }
}


static void _exportBorderOutline(const SwStroke& stroke, SwOutline* outline, uint32_t side)
{
    auto border = stroke.borders[side];
    if (border->pts.empty()) return;

    auto src = border->tags;
    auto idx = outline->pts.count;

    ARRAY_FOREACH(pts, border->pts) {
        if (*src & SW_STROKE_TAG_POINT) outline->types.push(SW_CURVE_TYPE_POINT);
        else if (*src & SW_STROKE_TAG_CUBIC) outline->types.push(SW_CURVE_TYPE_CUBIC);
        if (*src & SW_STROKE_TAG_END) outline->cntrs.push(idx);
        ++src;
        ++idx;
    }
    outline->pts.push(border->pts);
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void strokeFree(SwStroke* stroke)
{
    if (!stroke) return;

    fillFree(stroke->fill);
    stroke->fill = nullptr;

    tvg::free(stroke);
}


void strokeReset(SwStroke* stroke, const RenderShape* rshape, const Matrix& transform, SwMpool* mpool, unsigned tid)
{
    stroke->sx = sqrtf(powf(transform.e11, 2.0f) + powf(transform.e21, 2.0f));
    stroke->sy = sqrtf(powf(transform.e12, 2.0f) + powf(transform.e22, 2.0f));
    stroke->width = HALF_STROKE(rshape->strokeWidth());
    stroke->cap = rshape->strokeCap();
    stroke->miterlimit = static_cast<int64_t>(rshape->strokeMiterlimit() * 65536.0f);

    //Save line join: it can be temporarily changed when stroking curves...
    stroke->joinSaved = stroke->join = rshape->strokeJoin();

    stroke->borders[0] = mpool->strokeLBorder(tid);
    stroke->borders[1] = mpool->strokeRBorder(tid);
}


bool strokeParseOutline(SwStroke* stroke, const SwOutline& outline, SwMpool* mpool, unsigned tid)
{
    uint32_t first = 0;
    uint32_t i = 0;

    ARRAY_FOREACH(p, outline.cntrs) {
        auto last = *p;           //index of last point in contour
        auto limit = outline.pts.data + last;
        ++i;

        //Skip empty points
        if (last <= first) {
            first = last + 1;
            continue;
        }

        auto start = outline.pts[first];
        auto pt = outline.pts.data + first;
        auto types = outline.types.data + first;
        auto type = types[0];

        //A contour cannot start with a cubic control point
        if (type == SW_CURVE_TYPE_CUBIC) return false;
        ++types;

        auto closed =  outline.closed.data ? outline.closed.data[i - 1]: false;

        _beginSubPath(*stroke, start, closed);

        while (pt < limit) {
            //emit a single line_to
            if (types[0] == SW_CURVE_TYPE_POINT) {
                ++pt;
                ++types;
                _lineTo(*stroke, *pt);
            //types cubic
            } else {
                pt += 3;
                types += 3;
                if (pt <= limit) _cubicTo(*stroke, pt[-2], pt[-1], pt[0]);
                else if (pt - 1 == limit) _cubicTo(*stroke, pt[-2], pt[-1], start);
                else goto close;
            }
        }
    close:
        if (!stroke->firstPt) _endSubPath(*stroke);
        first = last + 1;
    }
    return true;
}


SwOutline* strokeExportOutline(SwStroke* stroke, SwMpool* mpool, unsigned tid)
{
    auto reserve = stroke->borders[0]->pts.count + stroke->borders[1]->pts.count;
    auto outline = mpool->outline(tid);
    outline->pts.reserve(reserve);
    outline->types.reserve(reserve);
    outline->fillRule = FillRule::NonZero;

    _exportBorderOutline(*stroke, outline, 0);  //left
    _exportBorderOutline(*stroke, outline, 1);  //right

    return outline;
}
