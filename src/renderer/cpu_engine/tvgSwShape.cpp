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

static void _drawPoint(SwDashStroke& dash, const Point& start)
{
    if (dash.move || dash.pattern[dash.curIdx] < FLOAT_EPSILON) {
        dash.path->moveTo(start);
        dash.move = false;
    }
    dash.path->lineTo(start);
}

static void _dashLineTo(SwDashStroke& dash, const Point& to, bool validPoint)
{
    Line cur = {dash.ptCur, to};
    auto len = cur.length();
    if (tvg::zero(len)) {
        dash.path->moveTo(dash.ptCur);
        // draw the current line fully
    } else if (len <= dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                dash.path->moveTo(dash.ptCur);
                dash.move = false;
            }
            dash.path->lineTo(to);
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
                        dash.path->moveTo(left.pt1);
                        dash.move = false;
                    }
                    dash.path->lineTo(left.pt2);
                }
            } else {
                if (validPoint && !dash.curOpGap) _drawPoint(dash, cur.pt1);
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
                dash.path->moveTo(cur.pt1);
                dash.move = false;
            }
            dash.path->lineTo(cur.pt2);
        }
        if (dash.curLen < 1.0f && !tvg::zero(len)) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = to;
}

static void _dashCubicTo(SwDashStroke& dash, const Point& ctrl1, const Point& ctrl2, const Point& to, bool validPoint)
{
    Bezier cur = {dash.ptCur, ctrl1, ctrl2, to};
    auto len = cur.length();

    //draw the current line fully
    if (tvg::zero(len)) {
        dash.path->moveTo(dash.ptCur);
    } else if (len <= dash.curLen) {
        dash.curLen -= len;
        if (!dash.curOpGap) {
            if (dash.move) {
                dash.path->moveTo(dash.ptCur);
                dash.move = false;
            }
            dash.path->cubicTo(ctrl1, ctrl2, to);
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
                        dash.path->moveTo(left.start);
                        dash.move = false;
                    }
                    dash.path->cubicTo(left.ctrl1, left.ctrl2, left.end);
                }
            } else {
                if (validPoint && !dash.curOpGap) _drawPoint(dash, cur.start);
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
                dash.path->moveTo(cur.start);
                dash.move = false;
            }
            dash.path->cubicTo(cur.ctrl1, cur.ctrl2, cur.end);
        }
        if (dash.curLen < 0.1f && !tvg::zero(len)) {
            //move to next dash
            dash.curIdx = (dash.curIdx + 1) % dash.cnt;
            dash.curLen = dash.pattern[dash.curIdx];
            dash.curOpGap = !dash.curOpGap;
        }
    }
    dash.ptCur = to;
}

static void _dashClose(SwDashStroke& dash, bool validPoint)
{
    _dashLineTo(dash, dash.ptStart, validPoint);
}

static void _dashMoveTo(SwDashStroke& dash, uint32_t offIdx, float offset, const Point& pts)
{
    dash.curIdx = offIdx % dash.cnt;
    dash.curLen = dash.pattern[dash.curIdx] - offset;
    dash.curOpGap = offIdx % 2;
    dash.ptStart = dash.ptCur = pts;
    dash.move = true;
}

static SwOutline* _genDashOutline(const RenderShape* rshape, SwMpool* mpool, unsigned tid, bool trimmed)
{
    if (!rshape->path.cmds.empty() && rshape->path.cmds[0] == PathCommand::CubicTo) return nullptr;

    RenderPath trimmedPath;
    auto path = &rshape->path;

    if (trimmed) {
        if (!rshape->stroke->trim.trim(rshape->path, trimmedPath)) return nullptr;
        path = &trimmedPath;
    }

    //No actual shape data
    if (path->cmds.empty() || path->pts.empty()) return nullptr;

    auto cmds = path->cmds.data;
    auto pts = path->pts.data;
    auto cmdCnt = path->cmds.count;

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

    auto outline = mpool->outline(tid);
    dash.path = &outline->synth;

    //must begin with moveTo
    if (cmds[0] == PathCommand::MoveTo) {
        _dashMoveTo(dash, offIdx, offset, *pts);
        cmds++;
        pts++;
    }

    //zero length segment with non-butt cap still should be rendered as a point - only the caps are visible
    auto validPoint = rshape->stroke->cap != StrokeCap::Butt;
    while (--cmdCnt > 0) {
        switch (*cmds) {
            case PathCommand::Close: {
                _dashClose(dash, validPoint);
                break;
            }
            case PathCommand::MoveTo: {
                _dashMoveTo(dash, offIdx, offset, *pts);
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                _dashLineTo(dash, *pts, validPoint);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                _dashCubicTo(dash, *pts, pts[1], pts[2], validPoint);
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    outline->path = &outline->synth;
    outline->fillRule = rshape->rule;
    return outline;
}

static SwPoint _transform(const Point& pt, const Matrix& transform)
{
    auto t = pt * transform;
    return {int32_t(t.x * 64.0f), int32_t(t.y * 64.0f)};
}

static bool _axisAlignedRect(const SwOutline* outline, const Matrix& transform)
{
    // TODO: We can return false if the coordinates have a fractional part, for smoother rectangle movement
    auto path = outline->path;
    if (path->cmds.count != 5) return false;

    //SVG paths may close a rectangle with a final LineTo back to its start.
    auto explicitClose = path->pts.count == 4 && path->cmds[4] == PathCommand::Close;
    auto implicitClose = path->pts.count == 5 && path->cmds[4] == PathCommand::LineTo;
    if ((!explicitClose && !implicitClose) || path->cmds[0] != PathCommand::MoveTo || path->cmds[1] != PathCommand::LineTo || path->cmds[2] != PathCommand::LineTo || path->cmds[3] != PathCommand::LineTo) return false;

    auto pt1 = _transform(path->pts[0], transform);
    auto pt2 = _transform(path->pts[1], transform);
    auto pt3 = _transform(path->pts[2], transform);
    auto pt4 = _transform(path->pts[3], transform);

    if (implicitClose && _transform(path->pts[4], transform) != pt1) return false;

    auto a = SwPoint{pt1.x, pt3.y};
    auto b = SwPoint{pt3.x, pt1.y};

    if ((pt2 == a && pt4 == b) || (pt2 == b && pt4 == a)) return true;

    return false;
}

static SwOutline* _genOutline(const RenderShape* rshape, SwMpool* mpool, unsigned tid, bool trimmed = false)
{
    if (!rshape->path.cmds.empty() && rshape->path.cmds[0] == PathCommand::CubicTo) return nullptr;

    auto outline = mpool->outline(tid);

    if (trimmed) {
        if (!rshape->stroke->trim.trim(rshape->path, outline->synth)) return nullptr;
        outline->path = &outline->synth;
    } else {
        outline->path = &rshape->path;
    }

    if (outline->path->cmds.empty() || outline->path->pts.empty()) return nullptr;
    outline->fillRule = rshape->rule;
    return outline;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool shapeGenRle(SwShape& shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid, bool composite, bool antiAlias)
{
    auto outline = _genOutline(rshape, mpool, tid, rshape->trimpath());
    if (!outline) {
        renderBox.reset();
        return false;
    }

    BBox bbox;
    utilExport(outline, transform, bbox);

    shape.outline = outline;
    shape.fastTrack = !composite && _axisAlignedRect(outline, transform);

    if (!utilBBox(bbox, clipBox, renderBox, shape.fastTrack)) return false;
    shape.bbox = renderBox;

    if (shape.fastTrack) return true;

    shape.rle = rleRender(shape.rle, outline, renderBox, mpool, tid, antiAlias);
    return shape.rle ? true : false;
}

void shapeDelOutline(SwShape& shape)
{
    shape.outline = nullptr;
}

void shapeReset(SwShape& shape)
{
    rleReset(shape.rle);
    shape.outline = nullptr;
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


void shapeResetStroke(SwShape& shape, const RenderShape* rshape, const Matrix& transform, SwMpool* mpool, unsigned tid)
{
    if (!shape.stroke) shape.stroke = tvg::calloc<SwStroke>(1, sizeof(SwStroke));
    strokeReset(shape.stroke, rshape, transform, mpool, tid);
    rleReset(shape.strokeRle);
}


bool shapeGenStrokeRle(SwShape& shape, const RenderShape* rshape, const Matrix& transform, const RenderRegion& clipBox, RenderRegion& renderBox, SwMpool* mpool, unsigned tid, bool antiAlias)
{
    shapeResetStroke(shape, rshape, transform, mpool, tid);

    auto dash = (rshape->stroke->dash.length > DASH_PATTERN_THRESHOLD);
    SwOutline* outline;

    if (dash) outline = _genDashOutline(rshape, mpool, tid, rshape->trimpath());
    // reuse outline generated by shape
    else outline = shape.outline ? shape.outline : _genOutline(rshape, mpool, tid, rshape->trimpath());
    if (!outline) return false;

    if (!strokeParsePath(shape.stroke, *outline->path)) return false;

    BBox bbox;
    outline = strokeExportOutline(shape.stroke, mpool, tid);
    if (outline->path->empty()) return false;
    utilExport(outline, transform, bbox);
    if (!utilBBox(bbox, clipBox, renderBox, false)) return false;

    shape.strokeRle = rleRender(shape.strokeRle, outline, renderBox, mpool, tid, antiAlias);
    return shape.strokeRle ? true : false;
}

bool shapeGenFillColors(SwFill*& out, const Fill* fill, const Matrix& transform, SwSurface* surface, uint8_t opacity, bool ctable)
{
    if (!fill) return true;  // a normal case

    if (!out) {
        out = tvg::calloc<SwFill>(1, sizeof(SwFill));
        ctable = true;
    } else if (ctable) {
        fillReset(out);
    }
    return fillGenColorTable(out, fill, transform, surface, opacity, ctable);
}

void shapeResetFill(SwShape& shape)
{
    if (!shape.fill) shape.fill = tvg::calloc<SwFill>(1, sizeof(SwFill));
    fillReset(shape.fill);
}

void shapeDelFill(SwShape& shape)
{
    if (!shape.fill) return;
    fillFree(shape.fill);
    shape.fill = nullptr;
}

bool shapeStrokeBBox(SwShape& shape, const RenderShape* rshape, Point* pt4, const Matrix& m, SwMpool* mpool)
{
    // TODO: We can skip generation here if the stroke hasn't been updated.
    if (rshape->strokeWidth() > 0.0f) {
        auto outline = _genOutline(rshape, mpool, 0, rshape->trimpath());
        if (!outline) return false;

        if (!shape.stroke) shape.stroke = tvg::calloc<SwStroke>(1, sizeof(SwStroke));
        strokeReset(shape.stroke, rshape, m, mpool, 0);
        strokeParsePath(shape.stroke, *outline->path);

        auto func = [](SwStrokeBorder* border, const Matrix& m, Point& min, Point& max) {
            ARRAY_FOREACH(pts, border->pts)
            {
                auto t = *pts * m;
                if (t.x < min.x) min.x = t.x;
                if (t.x > max.x) max.x = t.x;
                if (t.y < min.y) min.y = t.y;
                if (t.y > max.y) max.y = t.y;
            }
        };

        Point min = {FLT_MAX, FLT_MAX};
        Point max = {-FLT_MAX, -FLT_MAX};
        func(shape.stroke->borders[0], m, min, max);
        func(shape.stroke->borders[1], m, min, max);

        pt4[0] = min;
        pt4[1] = {max.x, min.y};
        pt4[2] = max;
        pt4[3] = {min.x, max.y};

        return true;
    }
    return false;
}
