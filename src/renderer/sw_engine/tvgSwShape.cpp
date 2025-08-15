/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

static bool _outlineBegin(SwOutline& outline)
{
    //Make a contour if lineTo/curveTo without calling close or moveTo beforehand.
    if (outline.pts.empty()) return false;
    outline.cntrs.push(outline.pts.count - 1);
    outline.closed.push(false);
    outline.pts.push(outline.pts[outline.cntrs.last()]);
    outline.types.push(SW_CURVE_TYPE_POINT);
    return false;
}


static bool _outlineEnd(SwOutline& outline)
{
    if (outline.pts.empty()) return false;
    outline.cntrs.push(outline.pts.count - 1);
    outline.closed.push(false);
    return false;
}


static bool _outlineMoveTo(SwOutline& outline, const SwPoint& to, bool closed = false)
{
    //make it a contour, if the last contour is not closed yet.
    if (!closed) _outlineEnd(outline);

    outline.pts.push(to);
    outline.types.push(SW_CURVE_TYPE_POINT);
    return false;
}


static bool _outlineMoveTo(SwOutline& outline, const Point& to, bool closed = false)
{
    return _outlineMoveTo(outline, TO_SWPOINT(to), closed);
}


static void _outlineLineTo(SwOutline& outline, const SwPoint& to)
{
    outline.pts.push(to);
    outline.types.push(SW_CURVE_TYPE_POINT);
}


static void _outlineLineTo(SwOutline& outline, const Point& to)
{
    _outlineLineTo(outline, TO_SWPOINT(to));
}


static void _outlineCubicTo(SwOutline& outline, const SwPoint& ctrl1, const SwPoint& ctrl2, const SwPoint& to)
{
    outline.pts.push(ctrl1);
    outline.types.push(SW_CURVE_TYPE_CUBIC);

    outline.pts.push(ctrl2);
    outline.types.push(SW_CURVE_TYPE_CUBIC);    

    outline.pts.push(to);
    outline.types.push(SW_CURVE_TYPE_POINT);
}


static void _outlineCubicTo(SwOutline& outline, const Point& ctrl1, const Point& ctrl2, const Point& to)
{
    _outlineCubicTo(outline, TO_SWPOINT(ctrl1), TO_SWPOINT(ctrl2), TO_SWPOINT(to));
}


static bool _outlineClose(SwOutline& outline)
{
    uint32_t i;
    if (outline.cntrs.count > 0) i = outline.cntrs.last() + 1;
    else i = 0;

    //Make sure there is at least one point in the current path
    if (outline.pts.count == i) return false;

    //Close the path
    outline.pts.push(outline.pts[i]);
    outline.cntrs.push(outline.pts.count - 1);
    outline.types.push(SW_CURVE_TYPE_POINT);
    outline.closed.push(true);

    return true;
}


static void _drawPoint(SwDashStroke& dash, const Point& start)
{
    if (dash.move || dash.pattern[dash.curIdx] < FLOAT_EPSILON) {
        _outlineMoveTo(*dash.outline, start);
        dash.move = false;
    }
    _outlineLineTo(*dash.outline, start);
}


static void _dashLineTo(SwDashStroke& dash, const Point& to, const Matrix& transform, bool validPoint)
{
    Line cur = {dash.ptCur, to};
    auto len = cur.length();
    if (tvg::zero(len)) {
        _outlineMoveTo(*dash.outline, dash.ptCur * transform);
    //draw the current line fully
    } else if (len <= dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, dash.ptCur * transform);
                dash.move = false;
            }
            _outlineLineTo(*dash.outline, to * transform);
        }
    //draw the current line partially
    } else {
        while (len - dash.curLen > DASH_PATTERN_THRESHOLD) {
            Line left, right;
            if (dash.curLen > 0) {
                len -= dash.curLen;
                cur.split(dash.curLen, left, right);
                if (!dash.curOpGap) {
                    if (dash.move || dash.pattern[dash.curIdx] - dash.curLen < FLOAT_EPSILON) {
                        _outlineMoveTo(*dash.outline, left.pt1 * transform);
                        dash.move = false;
                    }
                    _outlineLineTo(*dash.outline, left.pt2 * transform);
                }
            } else {
                if (validPoint && !dash.curOpGap) _drawPoint(dash, cur.pt1 * transform);
                right = cur;
            }
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
            cur = right;
            dash.ptCur = cur.pt1;
            dash.move = true;
        }
        //leftovers
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, cur.pt1 * transform);
                dash.move = false;
            }
            _outlineLineTo(*dash.outline, cur.pt2 * transform);
        }
        if (dash.curLen < 1 && TO_SWCOORD(len) > 1) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = to;
}


static void _dashCubicTo(SwDashStroke& dash, const Point& ctrl1, const Point& ctrl2, const Point& to, const Matrix& transform, bool validPoint)
{
    Bezier cur = {dash.ptCur, ctrl1, ctrl2, to};
    auto len = cur.length();

    //draw the current line fully
    if (tvg::zero(len)) {
        _outlineMoveTo(*dash.outline, dash.ptCur * transform);
    } else if (len <= dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, dash.ptCur * transform);
                dash.move = false;
            }
            _outlineCubicTo(*dash.outline, ctrl1 * transform, ctrl2 * transform, to * transform);
        }
    //draw the current line partially
    } else {
        while ((len - dash.curLen) > DASH_PATTERN_THRESHOLD) {
            Bezier left, right;
            if (dash.curLen > 0) {
                len -= dash.curLen;
                cur.split(dash.curLen, left, right);
                if (!dash.curOpGap) {
                    if (dash.move || dash.pattern[dash.curIdx] - dash.curLen < FLOAT_EPSILON) {
                        _outlineMoveTo(*dash.outline, left.start * transform);
                        dash.move = false;
                    }
                    _outlineCubicTo(*dash.outline, left.ctrl1 * transform, left.ctrl2 * transform, left.end * transform);
                }
            } else {
                if (validPoint && !dash.curOpGap) _drawPoint(dash, cur.start * transform);
                right = cur;
            }
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
            cur = right;
            dash.ptCur = right.start;
            dash.move = true;
        }
        //leftovers
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                _outlineMoveTo(*dash.outline, cur.start * transform);
                dash.move = false;
            }
            _outlineCubicTo(*dash.outline, cur.ctrl1 * transform, cur.ctrl2 * transform, cur.end * transform);
        }
        if (dash.curLen < 0.1f && TO_SWCOORD(len) > 1) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = to;
}


static void _dashClose(SwDashStroke& dash, const Matrix& transform, bool validPoint)
{
    _dashLineTo(dash, dash.ptStart, transform, validPoint);
}


static void _dashMoveTo(SwDashStroke& dash, uint32_t offIdx, float offset, const Point* pts)
{
    dash.curIdx = offIdx % dash.cnt;
    dash.curLen = dash.pattern[dash.curIdx] - offset;
    dash.curOpGap = offIdx % 2;
    dash.ptStart = dash.ptCur = *pts;
    dash.move = true;
}


static SwOutline* _genDashOutline(const RenderShape* rshape, const Matrix& transform, SwMpool* mpool, unsigned tid, bool trimmed)
{
    PathCommand *cmds, *trimmedCmds = nullptr;
    Point *pts, *trimmedPts = nullptr;
    uint32_t cmdCnt, ptsCnt;

    if (trimmed) {
        RenderPath trimmedPath;
        if (!rshape->stroke->trim.trim(rshape->path, trimmedPath)) return nullptr;
        cmds = trimmedCmds = trimmedPath.cmds.data;
        cmdCnt = trimmedPath.cmds.count;
        pts = trimmedPts = trimmedPath.pts.data;
        ptsCnt = trimmedPath.pts.count;

        trimmedPath.cmds.data = nullptr;
        trimmedPath.pts.data = nullptr;
    } else {
        cmds = rshape->path.cmds.data;
        cmdCnt = rshape->path.cmds.count;
        pts = rshape->path.pts.data;
        ptsCnt = rshape->path.pts.count;
    }

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return nullptr;

    SwDashStroke dash;
    dash.pattern = rshape->stroke->dash.pattern;
    dash.cnt = rshape->stroke->dash.count;
    auto offset = rshape->stroke->dash.offset;

    //offset
    uint32_t offIdx = 0;
    if (!tvg::zero(offset)) {
        auto length = rshape->stroke->dash.length;
        bool isOdd = dash.cnt % 2;
        if (isOdd) length *= 2;

        offset = fmodf(offset, length);
        if (offset < 0) offset += length;

        for (size_t i = 0; i < dash.cnt * (1 + (size_t)isOdd); ++i, ++offIdx) {
            auto curPattern = dash.pattern[i % dash.cnt];
            if (offset < curPattern) break;
            offset -= curPattern;
        }
    }

    dash.outline = mpoolReqDashOutline(mpool, tid);

    //must begin with moveTo
    if (cmds[0] == PathCommand::MoveTo) {
        _dashMoveTo(dash, offIdx, offset, pts);
        cmds++;
        pts++;
    }

    //zero length segment with non-butt cap still should be rendered as a point - only the caps are visible
    auto validPoint = rshape->stroke->cap != StrokeCap::Butt;
    while (--cmdCnt > 0) {
        switch (*cmds) {
            case PathCommand::Close: {
                _dashClose(dash, transform, validPoint);
                break;
            }
            case PathCommand::MoveTo: {
                _dashMoveTo(dash, offIdx, offset, pts);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                _dashLineTo(dash, pts[0], transform, validPoint);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                _dashCubicTo(dash, pts[0], pts[1], pts[2], transform, validPoint);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    tvg::free(trimmedCmds);
    tvg::free(trimmedPts);

    _outlineEnd(*dash.outline);

    return dash.outline;
}


static bool _axisAlignedRect(const SwOutline* outline)
{
    //Fast Track: axis-aligned rectangle?
    if (outline->pts.count != 5) return false;
    if (outline->types[2] == SW_CURVE_TYPE_CUBIC) return false;

    auto pt1 = outline->pts.data + 0;
    auto pt2 = outline->pts.data + 1;
    auto pt3 = outline->pts.data + 2;
    auto pt4 = outline->pts.data + 3;

    auto a = SwPoint{pt1->x, pt3->y};
    auto b = SwPoint{pt3->x, pt1->y};

    if ((*pt2 == a && *pt4 == b) || (*pt2 == b && *pt4 == a)) return true;

    return false;
}


static SwOutline* _genOutline(SwShape& shape, const RenderShape* rshape, const Matrix& transform, RenderRegion& renderBox, SwMpool* mpool, unsigned tid, bool hasComposite, bool trimmed)
{
    PathCommand *cmds, *trimmedCmds = nullptr;
    Point *pts, *trimmedPts = nullptr;
    uint32_t cmdCnt, ptsCnt;

    if (trimmed) {
        RenderPath trimmedPath;
        if (!rshape->stroke->trim.trim(rshape->path, trimmedPath)) return nullptr;

        cmds = trimmedCmds = trimmedPath.cmds.data;
        cmdCnt = trimmedPath.cmds.count;
        pts = trimmedPts = trimmedPath.pts.data;
        ptsCnt = trimmedPath.pts.count;

        trimmedPath.cmds.data = nullptr;
        trimmedPath.pts.data = nullptr;
    } else {
        cmds = rshape->path.cmds.data;
        cmdCnt = rshape->path.cmds.count;
        pts = rshape->path.pts.data;
        ptsCnt = rshape->path.pts.count;
    }

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return nullptr;

    auto outline = mpoolReqOutline(mpool, tid);
    auto closed = false;

    //capture the render box
    renderBox = {{INT32_MAX, INT32_MAX}, {-INT32_MAX, -INT32_MAX}};

    auto capture = [](RenderRegion& bbox, const SwPoint& pts) {
        if (pts.x < bbox.min.x) bbox.min.x = pts.x;
        if (pts.x > bbox.max.x) bbox.max.x = pts.x;
        if (pts.y < bbox.min.y) bbox.min.y = pts.y;
        if (pts.y > bbox.max.y) bbox.max.y = pts.y;
    };

    //Generate Outlines
    while (cmdCnt-- > 0) {
        switch (*cmds) {
            case PathCommand::Close: {
                if (!closed) closed = _outlineClose(*outline);
                break;
            }
            case PathCommand::MoveTo: {
                auto p = TO_SWPOINT(*pts * transform);
                closed = _outlineMoveTo(*outline, p, closed);
                capture(renderBox, p);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                if (closed) closed = _outlineBegin(*outline);
                auto p = TO_SWPOINT(*pts * transform);
                _outlineLineTo(*outline, p);
                capture(renderBox, p);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                if (closed) closed = _outlineBegin(*outline);
                auto p0 = TO_SWPOINT(*(pts - 1) * transform);
                auto p1 = TO_SWPOINT(*(pts + 0) * transform);
                auto p2 = TO_SWPOINT(*(pts + 1) * transform);
                auto p3 = TO_SWPOINT(*(pts + 2) * transform);
                _outlineCubicTo(*outline, p1, p2, p3);
                capture(renderBox, p0);
                capture(renderBox, p1);
                capture(renderBox, p2);
                capture(renderBox, p3);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    if (!closed) _outlineEnd(*outline);

    outline->fillRule = rshape->rule;

    tvg::free(trimmedCmds);
    tvg::free(trimmedPts);

    shape.fastTrack = (!hasComposite && _axisAlignedRect(outline));
    return outline;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool shapePrepare(SwShape& shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid, bool hasComposite)
{
    if ((shape.outline = _genOutline(shape, rshape, transform, renderBox, mpool, tid, hasComposite, rshape->trimpath()))) { 
        if (mathUpdateBBox(clipBox, renderBox, shape.fastTrack)) {
            shape.bbox = renderBox;
            return true;
        }
    }
    return false;
}


bool shapeGenRle(SwShape& shape, const RenderRegion& bbox, bool antiAlias)
{
    //Case A: Fast Track Rectangle Drawing
    if (shape.fastTrack) return true;

    //Case B: Normal Shape RLE Drawing
    if ((shape.rle = rleRender(shape.rle, shape.outline, bbox, antiAlias))) return true;

    return false;
}


void shapeDelOutline(SwShape& shape, SwMpool* mpool, uint32_t tid)
{
    mpoolRetOutline(mpool, tid);
    shape.outline = nullptr;
}


void shapeReset(SwShape& shape)
{
    rleReset(shape.rle);
    shape.bbox.reset();
    shape.fastTrack = false;
}


void shapeFree(SwShape& shape)
{
    rleFree(shape.rle);
    shape.rle = nullptr;

    shapeDelFill(shape);

    if (shape.stroke) {
        rleFree(shape.strokeRle);
        shape.strokeRle = nullptr;
        strokeFree(shape.stroke);
        shape.stroke = nullptr;
    }
}


void shapeDelStroke(SwShape& shape)
{
    if (!shape.stroke) return;
    rleFree(shape.strokeRle);
    shape.strokeRle = nullptr;
    strokeFree(shape.stroke);
    shape.stroke = nullptr;
}


void shapeResetStroke(SwShape& shape, const RenderShape* rshape, const Matrix& transform)
{
    if (!shape.stroke) shape.stroke = tvg::calloc<SwStroke*>(1, sizeof(SwStroke));
    auto stroke = shape.stroke;
    if (!stroke) return;

    strokeReset(stroke, rshape, transform);
    rleReset(shape.strokeRle);
}


bool shapeGenStrokeRle(SwShape& shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid)
{
    SwOutline* shapeOutline = nullptr;
    SwOutline* strokeOutline = nullptr;
    auto dashStroking = false;
    auto ret = true;

    //Dash style with/without trimming
    if (rshape->stroke->dash.length > DASH_PATTERN_THRESHOLD) {
        shapeOutline = _genDashOutline(rshape, transform, mpool, tid, rshape->trimpath());
        if (!shapeOutline) return false;
        dashStroking = true;
    //Trimming & Normal style
    } else {
        if (!shape.outline) {
            shape.outline = _genOutline(shape, rshape, transform, renderBox, mpool, tid, false, rshape->trimpath());
            if (!shape.outline) return false;
        }
        shapeOutline = shape.outline;
    }

    if (!strokeParseOutline(shape.stroke, *shapeOutline)) {
        ret = false;
        goto clear;
    }

    if ((strokeOutline = strokeExportOutline(shape.stroke, renderBox, transform, mpool, tid))) {
        if (!mathUpdateBBox(clipBox, renderBox, false)) {
            ret = false;
            goto clear;
        }
        shape.strokeRle = rleRender(shape.strokeRle, strokeOutline, renderBox, true);
    }

clear:
    if (dashStroking) mpoolRetDashOutline(mpool, tid);
    mpoolRetStrokeOutline(mpool, tid);

    return ret;
}


bool shapeGenFillColors(SwShape& shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable)
{
    return fillGenColorTable(shape.fill, fill, transform, surface, opacity, ctable);
}


bool shapeGenStrokeFillColors(SwShape& shape, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable)
{
    return fillGenColorTable(shape.stroke->fill, fill, transform, surface, opacity, ctable);
}


void shapeResetFill(SwShape& shape)
{
    if (!shape.fill) {
        shape.fill = tvg::calloc<SwFill*>(1, sizeof(SwFill));
        if (!shape.fill) return;
    }
    fillReset(shape.fill);
}


void shapeResetStrokeFill(SwShape& shape)
{
    if (!shape.stroke->fill) {
        shape.stroke->fill = tvg::calloc<SwFill*>(1, sizeof(SwFill));
        if (!shape.stroke->fill) return;
    }
    fillReset(shape.stroke->fill);
}


void shapeDelFill(SwShape& shape)
{
    if (shape.fill) {
        fillFree(shape.fill);
        shape.fill = nullptr;
    }
}


bool shapeStrokeBBox(SwShape& shape, const RenderShape* rshape, Point* pt4, const Matrix& m, SwMpool* mpool)
{
    RenderRegion renderBox;
    auto outline = _genOutline(shape, rshape, m, renderBox, mpool, 0, false, rshape->trimpath());
    if (!outline) return false;

    if (rshape->strokeWidth() > 0.0f) {
        strokeReset(shape.stroke, rshape, m);
        strokeParseOutline(shape.stroke, *outline);

        auto func = [](SwStrokeBorder& border, RenderRegion& renderBox) {
            if (border.ptsCnt == 0) return;
            auto pts = border.pts;
            auto cnt = border.ptsCnt;
            while (cnt-- > 0) {
                if (pts->x < renderBox.min.x) renderBox.min.x = pts->x;
                if (pts->x > renderBox.max.x) renderBox.max.x = pts->x;
                if (pts->y < renderBox.min.y) renderBox.min.y = pts->y;
                if (pts->y > renderBox.max.y) renderBox.max.y = pts->y;
                ++pts;
            }
        };

        renderBox = {{INT32_MAX, INT32_MAX}, {-INT32_MAX, -INT32_MAX}};
        func(shape.stroke->borders[0], renderBox);
        func(shape.stroke->borders[1], renderBox);

        mpoolRetStrokeOutline(mpool, 0);
    }

    pt4[0] = SwPoint{renderBox.min.x, renderBox.min.y}.toPoint();
    pt4[1] = SwPoint{renderBox.max.x, renderBox.min.y}.toPoint();
    pt4[2] = SwPoint{renderBox.max.x, renderBox.max.y}.toPoint();
    pt4[3] = SwPoint{renderBox.min.x, renderBox.max.y}.toPoint();


    shapeDelOutline(shape, mpool, 0);

    return true;
}