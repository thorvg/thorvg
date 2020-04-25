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
#ifndef _TVG_SHAPE_PATH_CPP_
#define _TVG_SHAPE_PATH_CPP_

#include "tvgCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

constexpr auto PATH_KAPPA = 0.552284f;

struct ShapePath;

static float _arcAngle(float angle);
static int _arcToCubic(ShapePath& path, const Point* pts, size_t ptsCnt);
static void _findEllipseCoords(float x, float y, float w, float h, float startAngle, float sweepAngle, Point& ptStart, Point& ptEnd);

struct ShapePath
{
    PathCommand* cmds = nullptr;
    size_t cmdCnt = 0;
    size_t reservedCmdCnt = 0;

    Point *pts = nullptr;
    size_t ptsCnt = 0;
    size_t reservedPtsCnt = 0;


    ~ShapePath()
    {
        if (cmds) delete(cmds);
        if (pts) delete(pts);
    }

    int reserveCmd(size_t cmdCnt)
    {
        if (cmdCnt > reservedCmdCnt) {
            reservedCmdCnt = cmdCnt;
            cmds = static_cast<PathCommand*>(realloc(cmds, sizeof(PathCommand) * reservedCmdCnt));
            assert(cmds);
        }
        return 0;
    }

    int reservePts(size_t ptsCnt)
    {
        if (ptsCnt > reservedPtsCnt) {
            reservedPtsCnt = ptsCnt;
            pts = static_cast<Point*>(realloc(pts, sizeof(Point) * reservedPtsCnt));
            assert(pts);
        }
        return 0;
    }

    int reserve(size_t cmdCnt, size_t ptsCnt)
    {
        reserveCmd(cmdCnt);
        reservePts(ptsCnt);

        return 0;
    }

    int clear()
    {
        cmdCnt = 0;
        ptsCnt = 0;

        return 0;
    }

    int moveTo(float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 2 > reservedPtsCnt) reservePts((ptsCnt + 2) * 2);

        cmds[cmdCnt++] = PathCommand::MoveTo;
        pts[ptsCnt++] = {x, y};

        return 0;
    }

    int lineTo(float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 2 > reservedPtsCnt) reservePts((ptsCnt + 2) * 2);

        cmds[cmdCnt++] = PathCommand::LineTo;
        pts[ptsCnt++] = {x, y};

        return 0;
    }

    int cubicTo(float cx1, float cy1, float cx2, float cy2, float x, float y)
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        if (ptsCnt + 3 > reservedPtsCnt) reservePts((ptsCnt + 3) * 2);

        cmds[cmdCnt++] = PathCommand::CubicTo;
        pts[ptsCnt++] = {cx1, cy1};
        pts[ptsCnt++] = {cx2, cy2};
        pts[ptsCnt++] = {x, y};

        return 0;
    }


    int arcTo(float x, float y, float w, float h, float startAngle, float sweepAngle)
    {
        if ((fabsf(w) < FLT_EPSILON) || (fabsf(h) < FLT_EPSILON)) return -1;
        if (fabsf(sweepAngle) < FLT_EPSILON) return -1;

        if (sweepAngle > 360) sweepAngle = 360;
        else if (sweepAngle < -360) sweepAngle = -360;

        auto half_w = w * 0.5f;
        auto half_h = h * 0.5f;
        auto half_w_kappa = half_w * PATH_KAPPA;
        auto half_h_kappa = half_h * PATH_KAPPA;

        //Curves for arc
        Point pts[13] {
            //start point: 0 degree
            {x + w, y + half_h},

            //0 -> 90 degree
            {x + w, y + half_h + half_h_kappa},
            {x + half_w + half_w_kappa, y + h},
            {x + half_w, y + h},

            //90 -> 180 degree
            {x + half_w - half_w_kappa, y + h},
            {x, y + half_h + half_h_kappa},
            {x, y + half_h},

            //180 -> 270 degree
            {x, y + half_h - half_h_kappa},
            {x + half_w - half_w_kappa, y},
            {x + half_w, y},

            //270 -> 0 degree
            {x + half_w + half_w_kappa, y},
            {x + w, y + half_h - half_h_kappa},
            {x + w, y + half_w}
        };

        auto ptsCnt = 1;    //one is reserved for the start point
        Point curves[13];

        //perfect circle: special case fast paths
        if (fabsf(startAngle) <= FLT_EPSILON) {
            if (fabsf(sweepAngle - 360) <= FLT_EPSILON) {
                for (int i = 11; i >= 0; --i) {
                    curves[ptsCnt++] = pts[i];
                }
                curves[0] = pts[12];
                return _arcToCubic(*this, curves, ptsCnt);
            } else if (fabsf(sweepAngle + 360) <= FLT_EPSILON) {
                for (int i = 1; i <= 12; ++i) {
                    curves[ptsCnt++] = pts[i];
                }
                curves[0] = pts[0];
                return _arcToCubic(*this, curves, ptsCnt);
            }
        }

        auto startSegment = static_cast<int>(floor(startAngle / 90));
        auto endSegment = static_cast<int>(floor((startAngle + sweepAngle) / 90));
        auto startDelta = (startAngle - (startSegment * 90)) / 90;
        auto endDelta = ((startAngle + sweepAngle) - (endSegment * 90)) / 90;
        auto delta = sweepAngle > 0 ? 1 : -1;

        if (delta < 0) {
            startDelta = 1 - startDelta;
            endDelta = 1 - endDelta;
        }

        //avoid empty start segment
        if (fabsf(startDelta - 1) < FLT_EPSILON) {
            startDelta = 0;
            startSegment += delta;
        }

        //avoid empty end segment
        if (fabsf(endDelta) < FLT_EPSILON) {
            endDelta = 1;
            endSegment -= delta;
        }

        startDelta = _arcAngle(startDelta * 90);
        endDelta = _arcAngle(endDelta * 90);

        auto splitAtStart = (fabsf(startDelta) >= FLT_EPSILON) ? true : false;
        auto splitAtEnd = (fabsf(endDelta - 1.0f) >= FLT_EPSILON) ? true : false;
        auto end = endSegment + delta;

        //empty arc?
        if (startSegment == end) {
            auto quadrant = 3 - ((startSegment % 4) + 4) % 4;
            auto i = 3 * quadrant;
            curves[0] = (delta > 0) ? pts[i + 3] : pts[i];
            return _arcToCubic(*this, curves, ptsCnt);
        }

        Point ptStart, ptEnd;
        _findEllipseCoords(x, y, w, h, startAngle, sweepAngle, ptStart, ptEnd);

        for (auto i = startSegment; i != end; i += delta) {
            //auto quadrant = 3 - ((i % 4) + 4) % 4;
            //auto j = 3 * quadrant;

            if (delta > 0) {
                //TODO: bezier
            } else {
                //TODO: bezier
            }

            //empty arc?
            if (startSegment == endSegment && (fabsf(startDelta - endDelta) < FLT_EPSILON)) {
                curves[0] = ptStart;
                return _arcToCubic(*this, curves, ptsCnt);
            }

            if (i == startSegment) {
                if (i == endSegment && splitAtEnd) {
                    //TODO: bezier
                } else if (splitAtStart) {
                    //TODO: bezier
                }
            } else if (i == endSegment && splitAtEnd) {
                //TODO: bezier
            }

            //push control points
            //curves[ptsCnt++] = ctrlPt1;
            //curves[ptsCnt++] = ctrlPt2;
            //curves[ptsCnt++] = endPt;
            cout << "ArcTo: Not Implemented!" << endl;
        }

        curves[ptsCnt - 1] = ptEnd;

        return _arcToCubic(*this, curves, ptsCnt);
    }

    int close()
    {
        if (cmdCnt + 1 > reservedCmdCnt) reserveCmd((cmdCnt + 1) * 2);
        cmds[cmdCnt++] = PathCommand::Close;

        return 0;
    }

    int bounds(float& x, float& y, float& w, float& h)
    {
        if (ptsCnt == 0) return -1;

        Point min = { pts[0].x, pts[0].y };
        Point max = { pts[0].x, pts[0].y };

        for(size_t i = 1; i <= ptsCnt; ++i) {
            if (pts[i].x < min.x) min.x = pts[i].x;
            if (pts[i].y < min.y) min.y = pts[i].y;
            if (pts[i].x > max.x) max.x = pts[i].x;
            if (pts[i].y > max.y) max.y = pts[i].y;
        }

        x = min.x;
        y = min.y;
        w = max.x - min.x;
        h = max.y - min.y;

        return 0;
    }
};

static float _arcAngle(float angle)
 {
    if (angle < FLT_EPSILON) return 0;
    if (fabsf(angle - 90) < FLT_EPSILON) return 1;

    auto radian = (angle / 180) * M_PI;
    auto cosAngle = cos(radian);
    auto sinAngle = sin(radian);

    //initial guess
    auto tc = angle / 90;

    /* do some iterations of newton's method to approximate cosAngle
       finds the zero of the function b.pointAt(tc).x() - cosAngle */
    tc -= ((((2 - 3 * PATH_KAPPA) * tc + 3 * (PATH_KAPPA - 1)) * tc) * tc + 1 - cosAngle) // value
    / (((6 - 9 * PATH_KAPPA) * tc + 6 * (PATH_KAPPA - 1)) * tc);                          // derivative
    tc -= ((((2 - 3 * PATH_KAPPA) * tc + 3 * (PATH_KAPPA - 1)) * tc) * tc + 1 - cosAngle) // value
    / (((6 - 9 * PATH_KAPPA) * tc + 6 * (PATH_KAPPA - 1)) * tc);                          // derivative

    // initial guess
    auto ts = tc;

    /* do some iterations of newton's method to approximate sin_angle
       finds the zero of the function b.pointAt(tc).y() - sinAngle */
    ts -= ((((3 * PATH_KAPPA - 2) * ts - 6 * PATH_KAPPA + 3) * ts + 3 * PATH_KAPPA) * ts - sinAngle)
    / (((9 * PATH_KAPPA - 6) * ts + 12 * PATH_KAPPA - 6) * ts + 3 * PATH_KAPPA);
    ts -= ((((3 * PATH_KAPPA - 2) * ts - 6 * PATH_KAPPA + 3) * ts + 3 * PATH_KAPPA) * ts - sinAngle)
    / (((9 * PATH_KAPPA - 6) * ts + 12 * PATH_KAPPA - 6) * ts + 3 * PATH_KAPPA);

    //use the average of the t that best approximates cos_angle and the t that best approximates sin_angle
    return (0.5 * (tc + ts));
}


static int _arcToCubic(ShapePath& path, const Point* pts, size_t ptsCnt)
{
    assert(pts);

    if (path.cmdCnt > 0 && path.cmds[path.cmdCnt] != PathCommand::Close) {
        if (path.lineTo(pts[0].x, pts[0].y)) return -1;
    } else {
        if (path.moveTo(pts[0].x, pts[0].y)) return -1;
    }

    for (size_t i = 1; i < ptsCnt; i += 3) {
        if (path.cubicTo(pts[i].x, pts[i].y, pts[i+1].x, pts[i+1].y, pts[i+2].x, pts[i+2].y)) {
            return -1;
        }
    }

    return 0;
}


static void _findEllipseCoords(float x, float y, float w, float h, float startAngle, float sweepAngle, Point& ptStart, Point& ptEnd)
{
    float angles[2] = {startAngle, startAngle + sweepAngle};
    float half_w = w * 0.5f;
    float half_h = h * 0.5f;
    float cx = x + half_w;
    float cy = y + half_h;
    Point* pts[2] = {&ptStart, &ptEnd};

    for (auto i = 0; i < 2; ++i) {
        auto theta = angles[i] - 360 * floor(angles[i] / 360);
        auto t = theta / 90;
        auto quadrant = static_cast<int>(t);    //truncate
        t -= quadrant;
        t = _arcAngle(90 * t);

        //swap x and y?
        if (quadrant & 1) t = (1 - t);

        //bezier coefficients
        auto m = 1 - t;
        auto b = m * m;
        auto c = t * t;
        auto d = c * t;
        auto a = b * m;
        b *= 3 * t;
        c *= 3 * m;

        auto px = a + b + c * PATH_KAPPA;
        auto py = d + c + b * PATH_KAPPA;

        //left quadrants
        if (quadrant == 1 || quadrant == 2) px = -px;

        //top quadrants
        if (quadrant == 0 || quadrant == 1) py = -py;

        pts[i]->x = cx + half_w * px;
        pts[i]->y = cy + half_h * py;
    }
}


#endif //_TVG_SHAPE_PATH_CPP_
