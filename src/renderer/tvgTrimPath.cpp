/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

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


#include "tvgTrimPath.h"
#include "tvgMath.h"
#include "tvgRender.h"

#define EPSILON 1e-4f

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static float _pathLength(const PathCommand* cmds, uint32_t cmdsCnt, const Point* pts, uint32_t ptsCnt)
{
    if (ptsCnt < 2) return 0.0f;

    auto start = pts;
    auto totalLength = 0.0f;

    while (cmdsCnt-- > 0) {
        switch (*cmds) {
            case PathCommand::Close: {
                totalLength += length(pts - 1, start);
                break;
            }
            case PathCommand::MoveTo: {
                start = pts;
                ++pts;
                break;
            }
            case PathCommand::LineTo: {
                totalLength += length(pts - 1, pts);
                ++pts;
                break;
            }
            case PathCommand::CubicTo: {
                totalLength += Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.length();
                pts += 3;
                break;
            }
        }
        ++cmds;
    }

    return totalLength;
}


static void _trimAt(const PathCommand* cmds, const Point* pts, Point& moveTo, float at1, float at2, bool start, RenderPath& out)
{
    switch (*cmds) {
        case PathCommand::MoveTo:
            break;
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


static void _trimPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, TVG_UNUSED uint32_t inPtsCnt, float trimStart, float trimEnd, RenderPath& out)
{
    auto cmds = const_cast<PathCommand*>(inCmds);
    auto pts = const_cast<Point*>(inPts);
    auto moveToTrimmed = *pts;
    auto moveTo = *pts;
    auto len = 0.0f;

    auto _length = [&]() -> float {
        switch (*cmds) {
            case PathCommand::MoveTo:
                return 0.0f;
            case PathCommand::LineTo:
                return length(pts - 1, pts);
            case PathCommand::CubicTo:
                return Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.length();
            case PathCommand::Close:
                return length(pts - 1, &moveTo);
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

    bool start = true;
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


static void _trim(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, float begin, float end, RenderPath& out)
{
    auto totalLength = _pathLength(inCmds, inCmdsCnt, inPts, inPtsCnt);
    auto trimStart = begin * totalLength;
    auto trimEnd = end * totalLength;

    if (fabsf(begin - end) < EPSILON) {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, totalLength, out);
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, 0.0f, trimStart, out);
    } else if (begin > end) {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, totalLength, out);
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, 0.0f, trimEnd, out);
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


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


bool TrimPath::valid() const
{
    if (begin == 0.0f && end == 1.0f) return false;
    return true;
}


bool TrimPath::trim(const RenderPath& in, RenderPath& out) const
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
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, out);
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
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, out);
                    startPts = pts;
                    startCmds = cmds;
                    break;
                }
            }
            i++;
        }
        if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, out);
    } else {
        _trim(in.cmds.data, in.cmds.count, in.pts.data, in.pts.count, begin, end, out);
    }

    return out.pts.count >= 2;
}
