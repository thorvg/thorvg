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

#include "tvgMath.h"
#include "tvgRender.h"

/************************************************************************/
/* RenderMethod Class Implementation                                    */
/************************************************************************/

uint32_t RenderMethod::ref()
{
    ScopedLock lock(key);
    return (++refCnt);
}


uint32_t RenderMethod::unref()
{
    ScopedLock lock(key);
    return (--refCnt);
}

/************************************************************************/
/* RenderPath Class Implementation                                      */
/************************************************************************/

bool RenderPath::bounds(Matrix* m, float* x, float* y, float* w, float* h)
{
    //unexpected
    if (cmds.empty() || cmds.first() == PathCommand::CubicTo) return false;

    auto min = Point{FLT_MAX, FLT_MAX};
    auto max = Point{-FLT_MAX, -FLT_MAX};

    auto pt = pts.begin();
    auto cmd = cmds.begin();

    auto assign = [&](const Point& pt, Point& min, Point& max) -> void {
        if (pt.x < min.x) min.x = pt.x;
        if (pt.y < min.y) min.y = pt.y;
        if (pt.x > max.x) max.x = pt.x;
        if (pt.y > max.y) max.y = pt.y;
    };

    while (cmd < cmds.end()) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                //skip the invalid assignments
                if (cmd + 1 < cmds.end()) {
                    auto next = *(cmd + 1);
                    if (next == PathCommand::LineTo || next == PathCommand::CubicTo) {
                        assign(*pt * m, min, max);
                    }
                }
                ++pt;
                break;
            }
            case PathCommand::LineTo: {
                assign(*pt * m, min, max);
                ++pt;
                break;
            }
            case PathCommand::CubicTo: {
                Bezier bz = {pt[-1] * m, pt[0] * m, pt[1] * m, pt[2] * m};
                bz.bounds(min, max);
                pt += 3;
                break;
            }
            default: break;
        }
        ++cmd;
    }

    if (x) *x = min.x;
    if (y) *y = min.y;
    if (w) *w = max.x - min.x;
    if (h) *h = max.y - min.y;

    return true;
}

/************************************************************************/
/* RenderRegion Class Implementation                                    */
/************************************************************************/


void RenderRegion::intersect(const RenderRegion& rhs)
{
    auto x1 = x + w;
    auto y1 = y + h;
    auto x2 = rhs.x + rhs.w;
    auto y2 = rhs.y + rhs.h;

    x = (x > rhs.x) ? x : rhs.x;
    y = (y > rhs.y) ? y : rhs.y;
    w = ((x1 < x2) ? x1 : x2) - x;
    h = ((y1 < y2) ? y1 : y2) - y;

    if (w < 0) w = 0;
    if (h < 0) h = 0;
}


void RenderRegion::add(const RenderRegion& rhs)
{
    if (rhs.x < x) {
        w += (x - rhs.x);
        x = rhs.x;
    }
    if (rhs.y < y) {
        h += (y - rhs.y);
        y = rhs.y;
    }
    if (rhs.x + rhs.w > x + w) w = (rhs.x + rhs.w) - x;
    if (rhs.y + rhs.h > y + h) h = (rhs.y + rhs.h) - y;
}

/************************************************************************/
/* RenderTrimPath Class Implementation                                  */
/************************************************************************/

#define EPSILON 1e-4f


static void _trimAt(const PathCommand* cmds, const Point* pts, Point& moveTo, float at1, float at2, bool start, RenderPath& out)
{
    switch (*cmds) {
        case PathCommand::LineTo: {
            Line tmp, left, right;
            Line{*(pts - 1), *pts}.split(at1, left, tmp);
            tmp.split(at2, left, right);
            if (start) {
                out.pts.push(left.pt1);
                moveTo = left.pt1;
                out.cmds.push(PathCommand::MoveTo);
            }
            out.pts.push(left.pt2);
            out.cmds.push(PathCommand::LineTo);
            break;
        }
        case PathCommand::CubicTo: {
            Bezier tmp, left, right;
            Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.split(at1, left, tmp);
            tmp.split(at2, left, right);
            if (start) {
                moveTo = left.start;
                out.pts.push(left.start);
                out.cmds.push(PathCommand::MoveTo);
            }
            out.pts.push(left.ctrl1);
            out.pts.push(left.ctrl2);
            out.pts.push(left.end);
            out.cmds.push(PathCommand::CubicTo);
            break;
        }
        case PathCommand::Close: {
            Line tmp, left, right;
            Line{*(pts - 1), moveTo}.split(at1, left, tmp);
            tmp.split(at2, left, right);
            if (start) {
                moveTo = left.pt1;
                out.pts.push(left.pt1);
                out.cmds.push(PathCommand::MoveTo);
            }
            out.pts.push(left.pt2);
            out.cmds.push(PathCommand::LineTo);
            break;
        }
        default: break;
    }
}


static void _add(const PathCommand* cmds, const Point* pts, const Point& moveTo, bool& start, RenderPath& out)
{
    switch (*cmds) {
        case PathCommand::MoveTo: {
            out.cmds.push(PathCommand::MoveTo);
            out.pts.push(*pts);
            start = false;
            break;
        }
        case PathCommand::LineTo: {
            if (start) {
                out.cmds.push(PathCommand::MoveTo);
                out.pts.push(*(pts - 1));
            }
            out.cmds.push(PathCommand::LineTo);
            out.pts.push(*pts);
            start = false;
            break;
        }
        case PathCommand::CubicTo: {
            if (start) {
                out.cmds.push(PathCommand::MoveTo);
                out.pts.push(*(pts - 1));
            }
            out.cmds.push(PathCommand::CubicTo);
            out.pts.push(*pts);
            out.pts.push(*(pts + 1));
            out.pts.push(*(pts + 2));
            start = false;
            break;
        }
        case PathCommand::Close: {
            if (start) {
                out.cmds.push(PathCommand::MoveTo);
                out.pts.push(*(pts - 1));
            }
            out.cmds.push(PathCommand::LineTo);
            out.pts.push(moveTo);
            start = true;
            break;
        }
    }
}


static void _trimPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, TVG_UNUSED uint32_t inPtsCnt, float trimStart, float trimEnd, RenderPath& out, bool connect = false)
{
    auto cmds = const_cast<PathCommand*>(inCmds);
    auto pts = const_cast<Point*>(inPts);
    auto moveToTrimmed = *pts;
    auto moveTo = *pts;
    auto len = 0.0f;

    auto _length = [&]() -> float {
        switch (*cmds) {
            case PathCommand::MoveTo: return 0.0f;
            case PathCommand::LineTo: return tvg::length(pts - 1, pts);
            case PathCommand::CubicTo: return Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.length();
            case PathCommand::Close: return tvg::length(pts - 1, &moveTo);
        }
        return 0.0f;
    };

    auto _shift = [&]() -> void {
        switch (*cmds) {
            case PathCommand::MoveTo:
                moveTo = *pts;
                moveToTrimmed = *pts;
                ++pts;
                break;
            case PathCommand::LineTo:
                ++pts;
                break;
            case PathCommand::CubicTo:
                pts += 3;
                break;
            case PathCommand::Close:
                break;
        }
        ++cmds;
    };

    auto start = !connect;

    for (uint32_t i = 0; i < inCmdsCnt; ++i) {
        auto dLen = _length();

        //very short segments are skipped since due to the finite precision of Bezier curve subdivision and length calculation (1e-2),
        //trimming may produce very short segments that would effectively have zero length with higher computational accuracy.
        if (len <= trimStart) {
            //cut the segment at the beginning and at the end
            if (len + dLen > trimEnd) {
                _trimAt(cmds, pts, moveToTrimmed, trimStart - len, trimEnd - trimStart, start, out);
                start = false;
                //cut the segment at the beginning
            } else if (len + dLen > trimStart + EPSILON) {
                _trimAt(cmds, pts, moveToTrimmed, trimStart - len, len + dLen - trimStart, start, out);
                start = false;
            }
        } else if (len <= trimEnd - EPSILON) {
            //cut the segment at the end
            if (len + dLen > trimEnd) {
                _trimAt(cmds, pts, moveTo, 0.0f, trimEnd - len, start, out);
                start = true;
            //add the whole segment
            } else if (len + dLen > trimStart + EPSILON) _add(cmds, pts, moveTo, start, out);
        }

        len += dLen;
        _shift();
    }
}


static void _trim(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, float begin, float end, bool connect, RenderPath& out)
{
    auto totalLength = tvg::length(inCmds, inCmdsCnt, inPts, inPtsCnt);
    auto trimStart = begin * totalLength;
    auto trimEnd = end * totalLength;

    if (begin >= end) {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, totalLength, out);
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, 0.0f, trimEnd, out, connect);
    } else {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, trimEnd, out);
    }
}


static void _get(float& begin, float& end)
{
    auto loop = true;

    if (begin > 1.0f && end > 1.0f) loop = false;
    if (begin < 0.0f && end < 0.0f) loop = false;
    if (begin >= 0.0f && begin <= 1.0f && end >= 0.0f  && end <= 1.0f) loop = false;

    if (begin > 1.0f) begin -= 1.0f;
    if (begin < 0.0f) begin += 1.0f;
    if (end > 1.0f) end -= 1.0f;
    if (end < 0.0f) end += 1.0f;

    if ((loop && begin < end) || (!loop && begin > end)) std::swap(begin, end);
}


bool RenderTrimPath::trim(const RenderPath& in, RenderPath& out) const
{
    if (in.pts.count < 2 || tvg::zero(begin - end)) return false;

    float begin = this->begin, end = this->end;
    _get(begin, end);

    out.cmds.reserve(in.cmds.count * 2);
    out.pts.reserve(in.pts.count * 2);

    auto pts = in.pts.data;
    auto cmds = in.cmds.data;

    if (simultaneous) {
        auto startCmds = cmds;
        auto startPts = pts;
        uint32_t i = 0;
        while (i < in.cmds.count) {
            switch (in.cmds[i]) {
                case PathCommand::MoveTo: {
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, *(cmds - 1) == PathCommand::Close, out);
                    startPts = pts;
                    startCmds = cmds;
                    ++pts;
                    ++cmds;
                    break;
                }
                case PathCommand::LineTo: {
                    ++pts;
                    ++cmds;
                    break;
                }
                case PathCommand::CubicTo: {
                    pts += 3;
                    ++cmds;
                    break;
                }
                case PathCommand::Close: {
                    ++cmds;
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, *(cmds - 1) == PathCommand::Close, out);
                    startPts = pts;
                    startCmds = cmds;
                    break;
                }
            }
            i++;
        }
        if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, *(cmds - 1) == PathCommand::Close, out);
    } else {
        _trim(in.cmds.data, in.cmds.count, in.pts.data, in.pts.count, begin, end, false, out);
    }

    return out.pts.count >= 2;
}