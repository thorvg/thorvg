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
#ifndef _TVG_SW_SHAPE_H_
#define _TVG_SW_SHAPE_H_

#include "tvgSwCommon.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Line
{
    Point pt1;
    Point pt2;
};


struct Bezier
{
    Point start;
    Point ctrl1;
    Point ctrl2;
    Point end;
};


static float _lineLength(const Point& pt1, const Point& pt2)
{
    /* approximate sqrt(x*x + y*y) using alpha max plus beta min algorithm.
       With alpha = 1, beta = 3/8, giving results with the largest error less
       than 7% compared to the exact value. */
    Point diff = {pt2.x - pt1.x, pt2.y - pt1.y};
    if (diff.x < 0) diff.x = -diff.x;
    if (diff.y < 0) diff.y = -diff.y;
    return (diff.x > diff.y) ? (diff.x + diff.y * 0.375f) : (diff.y + diff.x * 0.375f);
}


static void _lineSplitAt(const Line& cur, float at, Line& left, Line& right)
{
    auto len = _lineLength(cur.pt1, cur.pt2);
    auto dx = ((cur.pt2.x - cur.pt1.x) / len) * at;
    auto dy = ((cur.pt2.y - cur.pt1.y) / len) * at;
    left.pt1 = cur.pt1;
    left.pt2.x = left.pt1.x + dx;
    left.pt2.y = left.pt1.y + dy;
    right.pt1 = left.pt2;
    right.pt2 = cur.pt2;
}


static void _bezSplit(const Bezier&cur, Bezier& left, Bezier& right)
{
    auto c = (cur.ctrl1.x + cur.ctrl2.x) * 0.5f;
    left.ctrl1.x = (cur.start.x + cur.ctrl1.x) * 0.5f;
    right.ctrl2.x = (cur.ctrl2.x + cur.end.x) * 0.5f;
    left.start.x = cur.start.x;
    right.end.x = cur.end.x;
    left.ctrl2.x = (left.ctrl1.x + c) * 0.5f;
    right.ctrl1.x = (right.ctrl2.x + c) * 0.5f;
    left.end.x = right.start.x = (left.ctrl2.x + right.ctrl1.x) * 0.5f;

    c = (cur.ctrl1.y + cur.ctrl2.y) * 0.5f;
    left.ctrl1.y = (cur.start.y + cur.ctrl1.y) * 0.5f;
    right.ctrl2.y = (cur.ctrl2.y + cur.end.y) * 0.5f;
    left.start.y = cur.start.y;
    right.end.y = cur.end.y;
    left.ctrl2.y = (left.ctrl1.y + c) * 0.5f;
    right.ctrl1.y = (right.ctrl2.y + c) * 0.5f;
    left.end.y = right.start.y = (left.ctrl2.y + right.ctrl1.y) * 0.5f;
}


static float _bezLength(const Bezier& cur)
{
    Bezier left, right;
    auto len = _lineLength(cur.start, cur.ctrl1) + _lineLength(cur.ctrl1, cur.ctrl2) + _lineLength(cur.ctrl2, cur.end);
    auto chord = _lineLength(cur.start, cur.end);

    if (fabs(len - chord) > FLT_EPSILON) {
        _bezSplit(cur, left, right);
        return _bezLength(left) + _bezLength(right);
    }
    return len;
}


static void _bezSplitLeft(Bezier& cur, float at, Bezier& left)
{
    left.start = cur.start;

    left.ctrl1.x = cur.start.x + at * (cur.ctrl1.x - cur.start.x);
    left.ctrl1.y = cur.start.y + at * (cur.ctrl1.y - cur.start.y);

    left.ctrl2.x = cur.ctrl1.x + at * (cur.ctrl2.x - cur.ctrl1.x); // temporary holding spot
    left.ctrl2.y = cur.ctrl1.y + at * (cur.ctrl2.y - cur.ctrl1.y); // temporary holding spot

    cur.ctrl2.x = cur.ctrl2.x + at * (cur.end.x - cur.ctrl2.x);
    cur.ctrl2.y = cur.ctrl2.y + at * (cur.end.y - cur.ctrl2.y);

    cur.ctrl1.x = left.ctrl2.x + at * (cur.ctrl2.x - left.ctrl2.x);
    cur.ctrl1.y = left.ctrl2.y + at * (cur.ctrl2.y - left.ctrl2.y);

    left.ctrl2.x = left.ctrl1.x + at * (left.ctrl2.x - left.ctrl1.x);
    left.ctrl2.y = left.ctrl1.y + at * (left.ctrl2.y - left.ctrl1.y);

    left.end.x = cur.start.x = left.ctrl2.x + at * (cur.ctrl1.x - left.ctrl2.x);
    left.end.y = cur.start.y = left.ctrl2.y + at * (cur.ctrl1.y - left.ctrl2.y);
}


static float _bezAt(const Bezier& bz, float at)
{
    auto len = _bezLength(bz);
    auto biggest = 1.0f;

    if (at >= len) return 1.0f;

    at *= 0.5f;

    while (true) {
        auto right = bz;
        Bezier left;
        _bezSplitLeft(right, at, left);
        auto len2 = _bezLength(left);

        if (fabs(len2 - len) < FLT_EPSILON) break;

        if (len2 < len) {
            at += (biggest - at) * 0.5f;
        } else {
            biggest = at;
            at -= (at * 0.5f);
        }
    }
    return at;
}


static void _bezSplitAt(const Bezier& cur, float at, Bezier& left, Bezier& right)
{
    right = cur;
    auto t = _bezAt(right, at);
    _bezSplitLeft(right, t, left);
}


static void _growOutlineContour(SwOutline& outline, uint32_t n)
{
    if (outline.reservedCntrsCnt >= outline.cntrsCnt + n) return;
    outline.reservedCntrsCnt = outline.cntrsCnt + n;
    outline.cntrs = static_cast<uint32_t*>(realloc(outline.cntrs, outline.reservedCntrsCnt * sizeof(uint32_t)));
    assert(outline.cntrs);
}


static void _growOutlinePoint(SwOutline& outline, uint32_t n)
{
    if (outline.reservedPtsCnt >= outline.ptsCnt + n) return;
    outline.reservedPtsCnt = outline.ptsCnt + n;
    outline.pts = static_cast<SwPoint*>(realloc(outline.pts, outline.reservedPtsCnt * sizeof(SwPoint)));
    assert(outline.pts);
    outline.types = static_cast<uint8_t*>(realloc(outline.types, outline.reservedPtsCnt * sizeof(uint8_t)));
    assert(outline.types);
}


static void _delOutline(SwOutline* outline)
{
    if (!outline) return;

    if (outline->cntrs) free(outline->cntrs);
    if (outline->pts) free(outline->pts);
    if (outline->types) free(outline->types);
    free(outline);
}


static void _outlineEnd(SwOutline& outline)
{
    _growOutlineContour(outline, 1);
    if (outline.ptsCnt > 0) {
        outline.cntrs[outline.cntrsCnt] = outline.ptsCnt - 1;
        ++outline.cntrsCnt;
    }
}


static void _outlineMoveTo(SwOutline& outline, const Point* to)
{
    assert(to);

    _growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = TO_SWPOINT(to);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;

    if (outline.ptsCnt > 0) {
        _growOutlineContour(outline, 1);
        outline.cntrs[outline.cntrsCnt] = outline.ptsCnt - 1;
        ++outline.cntrsCnt;
    }

    ++outline.ptsCnt;
}


static void _outlineLineTo(SwOutline& outline, const Point* to)
{
    assert(to);

    _growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = TO_SWPOINT(to);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;
    ++outline.ptsCnt;
}


static void _outlineCubicTo(SwOutline& outline, const Point* ctrl1, const Point* ctrl2, const Point* to)
{
    assert(ctrl1 && ctrl2 && to);

    _growOutlinePoint(outline, 3);

    outline.pts[outline.ptsCnt] = TO_SWPOINT(ctrl1);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = TO_SWPOINT(ctrl2);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_CUBIC;
    ++outline.ptsCnt;

    outline.pts[outline.ptsCnt] = TO_SWPOINT(to);
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;
    ++outline.ptsCnt;
}


static void _outlineClose(SwOutline& outline)
{
    uint32_t i = 0;

    if (outline.cntrsCnt > 0) {
        i = outline.cntrs[outline.cntrsCnt - 1] + 1;
    } else {
        i = 0;   //First Path
    }

    //Make sure there is at least one point in the current path
    if (outline.ptsCnt == i) {
        outline.opened = true;
        return;
    }

    //Close the path
    _growOutlinePoint(outline, 1);

    outline.pts[outline.ptsCnt] = outline.pts[i];
    outline.types[outline.ptsCnt] = SW_CURVE_TYPE_POINT;
    ++outline.ptsCnt;

    outline.opened = false;
}


static void _initBBox(SwBBox& bbox)
{
    bbox.min.x = bbox.min.y = 0;
    bbox.max.x = bbox.max.y = 0;
}


static bool _updateBBox(SwOutline* outline, SwBBox& bbox)
{
    if (!outline) return false;

    auto pt = outline->pts;
    assert(pt);

    if (outline->ptsCnt <= 0) {
        _initBBox(bbox);
        return false;
    }

    auto xMin = pt->x;
    auto xMax = pt->x;
    auto yMin = pt->y;
    auto yMax = pt->y;

    ++pt;

    for(uint32_t i = 1; i < outline->ptsCnt; ++i, ++pt) {
        assert(pt);
        if (xMin > pt->x) xMin = pt->x;
        if (xMax < pt->x) xMax = pt->x;
        if (yMin > pt->y) yMin = pt->y;
        if (yMax < pt->y) yMax = pt->y;
    }
    bbox.min.x = xMin >> 6;
    bbox.max.x = (xMax + 63) >> 6;
    bbox.min.y = yMin >> 6;
    bbox.max.y = (yMax + 63) >> 6;

    if (xMax - xMin < 1 || yMax - yMin < 1) return false;

    return true;
}


static bool _checkValid(const SwOutline* outline, const SwBBox& bbox, const SwSize& clip)
{
    assert(outline);

    if (outline->ptsCnt == 0 || outline->cntrsCnt <= 0) return false;

    //Check boundary
    if ((bbox.min.x > clip.w || bbox.min.y > clip.h) ||
        (bbox.min.x + bbox.max.x < 0) ||
        (bbox.min.y + bbox.max.y < 0)) return false;

    return true;
}


static void _transformOutline(SwOutline* outline, const RenderTransform* transform)
{
    assert(outline);

    if (!transform) return;

    for(uint32_t i = 0; i < outline->ptsCnt; ++i) {
        auto dx = static_cast<float>(outline->pts[i].x >> 6);
        auto dy = static_cast<float>(outline->pts[i].y >> 6);
        auto tx = dx * transform->e11 + dy * transform->e12 + transform->e13;
        auto ty = dx * transform->e21 + dy * transform->e22 + transform->e23;
        auto pt = Point{tx + transform->e31, ty + transform->e32};
        outline->pts[i] = TO_SWPOINT(&pt);
    }
}


static void _dashLineTo(SwDashStroke& dash, const Point* to)
{
    _growOutlinePoint(*dash.outline, dash.outline->ptsCnt >> 1);
    _growOutlineContour(*dash.outline, dash.outline->cntrsCnt >> 1);

    Line cur = {dash.ptCur, *to};
    auto len = _lineLength(cur.pt1, cur.pt2);

    if (len < dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            _outlineMoveTo(*dash.outline, &dash.ptCur);
            _outlineLineTo(*dash.outline, to);
        }
    } else {
        while (len > dash.curLen) {
            len -= dash.curLen;
            Line left, right;
            _lineSplitAt(cur, dash.curLen, left, right);;
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            if (!dash.curOpGap) {
                _outlineMoveTo(*dash.outline, &left.pt1);
                _outlineLineTo(*dash.outline, &left.pt2);
            }
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
            cur = right;
            dash.ptCur = cur.pt1;
        }
        //leftovers
        dash.curLen -= len;
        if (!dash.curOpGap) {    
            _outlineMoveTo(*dash.outline, &cur.pt1);
            _outlineLineTo(*dash.outline, &cur.pt2);
        }
        if (dash.curLen < 1) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = *to;
}


static void _dashCubicTo(SwDashStroke& dash, const Point* ctrl1, const Point* ctrl2, const Point* to)
{
    _growOutlinePoint(*dash.outline, dash.outline->ptsCnt >> 1);
    _growOutlineContour(*dash.outline, dash.outline->cntrsCnt >> 1);

    Bezier cur = { dash.ptCur, *ctrl1, *ctrl2, *to};
    auto len = _bezLength(cur);

    if (len < dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            _outlineMoveTo(*dash.outline, &dash.ptCur);
            _outlineCubicTo(*dash.outline, ctrl1, ctrl2, to);
        }
    } else {
        while (len > dash.curLen) {
            Bezier left, right;
            len -= dash.curLen;
            _bezSplitAt(cur, dash.curLen, left, right);
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            if (!dash.curOpGap) {
                _outlineMoveTo(*dash.outline, &left.start);
                _outlineCubicTo(*dash.outline, &left.ctrl1, &left.ctrl2, &left.end);
            }
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
            cur = right;
            dash.ptCur = right.start;
        }
        //leftovers
        dash.curLen -= len;
        if (!dash.curOpGap) {
            _outlineMoveTo(*dash.outline, &cur.start);
            _outlineCubicTo(*dash.outline, &cur.ctrl1, &cur.ctrl2, &cur.end);
        }
        if (dash.curLen < 1) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = *to;
}


SwOutline* _genDashOutline(const Shape& shape)
{
    const PathCommand* cmds = nullptr;
    auto cmdCnt = shape.pathCommands(&cmds);

    const Point* pts = nullptr;
    auto ptsCnt = shape.pathCoords(&pts);

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return nullptr;

    SwDashStroke dash;
    dash.curIdx = 0;
    dash.curLen = 0;
    dash.ptStart = {0, 0};
    dash.ptCur = {0, 0};
    dash.curOpGap = false;

    const float* pattern;
    dash.cnt = shape.strokeDash(&pattern);
    assert(dash.cnt > 0 && pattern);

    //Is it safe to mutual exclusive?
    dash.pattern = const_cast<float*>(pattern);
    dash.outline = static_cast<SwOutline*>(calloc(1, sizeof(SwOutline)));
    assert(dash.outline);
    dash.outline->opened = true;

    //smart reservation
    auto outlinePtsCnt = 0;
    auto outlineCntrsCnt = 0;

    for (uint32_t i = 0; i < cmdCnt; ++i) {
        switch(*(cmds + i)) {
            case PathCommand::Close: {
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::MoveTo: {
                ++outlineCntrsCnt;
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::LineTo: {
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::CubicTo: {
                outlinePtsCnt += 3;
                break;
            }
        }
    }

    ++outlinePtsCnt;    //for close
    ++outlineCntrsCnt;  //for end

    //Reserve Approximitely 20x...
    _growOutlinePoint(*dash.outline, outlinePtsCnt * 20);
    _growOutlineContour(*dash.outline, outlineCntrsCnt * 20);

    while (cmdCnt-- > 0) {
        switch(*cmds) {
            case PathCommand::Close: {
                _dashLineTo(dash, &dash.ptStart);
                break;
            }
            case PathCommand::MoveTo: {
                //reset the dash
                dash.curIdx = 0;
                dash.curLen = *dash.pattern;
                dash.curOpGap = false;
                dash.ptStart = dash.ptCur = *pts;
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                _dashLineTo(dash, pts);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                _dashCubicTo(dash, pts, pts + 1, pts + 2);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    _outlineEnd(*dash.outline);

    return dash.outline;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool shapeGenRle(SwShape& shape, const Shape& sdata, const SwSize& clip, const RenderTransform* transform)
{
    if (!shapeGenOutline(shape, sdata)) return false;

    _transformOutline(shape.outline, transform);

    if (!_updateBBox(shape.outline, shape.bbox)) goto end;

    if (!_checkValid(shape.outline, shape.bbox, clip)) goto end;

    shape.rle = rleRender(shape.outline, shape.bbox, clip);
end:
    if (shape.rle) return true;
    return false;
}


void shapeDelOutline(SwShape& shape)
{
    auto outline = shape.outline;
    _delOutline(outline);
    shape.outline = nullptr;
}


void shapeReset(SwShape& shape)
{
    shapeDelOutline(shape);
    rleFree(shape.rle);
    shape.rle = nullptr;
    _initBBox(shape.bbox);
}


bool shapeGenOutline(SwShape& shape, const Shape& sdata)
{
    const PathCommand* cmds = nullptr;
    auto cmdCnt = sdata.pathCommands(&cmds);

    const Point* pts = nullptr;
    auto ptsCnt = sdata.pathCoords(&pts);

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return false;

    //smart reservation
    auto outlinePtsCnt = 0;
    auto outlineCntrsCnt = 0;

    for (uint32_t i = 0; i < cmdCnt; ++i) {
        switch(*(cmds + i)) {
            case PathCommand::Close: {
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::MoveTo: {
                ++outlineCntrsCnt;
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::LineTo: {
                ++outlinePtsCnt;
                break;
            }
            case PathCommand::CubicTo: {
                outlinePtsCnt += 3;
                break;
            }
        }
    }

    ++outlinePtsCnt;    //for close
    ++outlineCntrsCnt;  //for end

    auto outline = shape.outline;
    if (!outline) outline = static_cast<SwOutline*>(calloc(1, sizeof(SwOutline)));
    assert(outline);
    outline->opened = true;

    _growOutlinePoint(*outline, outlinePtsCnt);
    _growOutlineContour(*outline, outlineCntrsCnt);

    //Generate Outlines
    while (cmdCnt-- > 0) {
        switch(*cmds) {
            case PathCommand::Close: {
                _outlineClose(*outline);
                break;
            }
            case PathCommand::MoveTo: {
                _outlineMoveTo(*outline, pts);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                _outlineLineTo(*outline, pts);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                _outlineCubicTo(*outline, pts, pts + 1, pts + 2);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    _outlineEnd(*outline);

    //FIXME:
    //outline->flags = SwOutline::FillRule::Winding;

    shape.outline = outline;

    return true;
}


void shapeFree(SwShape* sdata)
{
    assert(sdata);

    shapeDelOutline(*sdata);
    rleFree(sdata->rle);

    if (sdata->stroke) {
        rleFree(sdata->strokeRle);
        strokeFree(sdata->stroke);
    }

    free(sdata);
}


void shapeResetStroke(SwShape& shape, const Shape& sdata)
{
    if (!shape.stroke) shape.stroke = static_cast<SwStroke*>(calloc(1, sizeof(SwStroke)));
    auto stroke = shape.stroke;
    assert(stroke);

    strokeReset(*stroke, sdata);

    rleFree(shape.strokeRle);
    shape.strokeRle = nullptr;
}


bool shapeGenStrokeRle(SwShape& shape, const Shape& sdata, const SwSize& clip)
{
    SwOutline* shapeOutline = nullptr;

    //Dash Style Stroke
    if (sdata.strokeDash(nullptr) > 0) {
        shapeOutline = _genDashOutline(sdata);
        if (!shapeOutline) return false;

    //Normal Style stroke
    } else {
        if (!shape.outline) {
            if (!shapeGenOutline(shape, sdata)) return false;
        }
        shapeOutline = shape.outline;
    }

    if (!strokeParseOutline(*shape.stroke, *shapeOutline)) return false;

    auto strokeOutline = strokeExportOutline(*shape.stroke);
    if (!strokeOutline) return false;

    SwBBox bbox;
    _updateBBox(strokeOutline, bbox);

    if (!_checkValid(strokeOutline, bbox, clip)) return false;

    shape.strokeRle = rleRender(strokeOutline, bbox, clip);

    _delOutline(strokeOutline);

    return true;
}


#endif /* _TVG_SW_SHAPE_H_ */
