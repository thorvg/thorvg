/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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


#include "tvgTrim.h"
#include "tvgMath.h"


static float _pathLength(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt)
{
    if (inCmdsCnt <= 0 || inPtsCnt <= 0) return 0.0f;

    auto start = inPts;
    auto totalLength = 0.0f;

    auto i = 0;
    while (i < inCmdsCnt) {
        switch (inCmds[i]) {
            case PathCommand::Close: {
                totalLength += length(inPts - 1, start);
                break;
            }
            case PathCommand::MoveTo: {
                start = inPts;
                ++inPts;
                break;
            }
            case PathCommand::LineTo: {
                totalLength += length(inPts - 1, inPts);
                ++inPts;
                break;
            }
            case PathCommand::CubicTo: {
                totalLength += Bezier{*(inPts - 1), *inPts, *(inPts + 1), *(inPts + 2)}.length();
                inPts += 3;
                break;
            }
        }
        ++i;
    }

    return totalLength;
}


static void _trimStart(const PathCommand* cmds, const Point* pts, Point& moveTo, float at, Array<PathCommand>& outCmds, Array<Point>& outPts)
{
    switch (*cmds) {
        case PathCommand::MoveTo:
            break;
        case PathCommand::LineTo: {
            Line right, left;
            Line{*(pts - 1), *pts}.split(at, left, right);
            outCmds.push(PathCommand::MoveTo);
            outCmds.push(PathCommand::LineTo);
            outPts.push(right.pt1);
            outPts.push(right.pt2);
            moveTo = right.pt1;
            break;
        }
        case PathCommand::CubicTo: {
            Bezier left, right;
            Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.split(at, left, right);
            outCmds.push(PathCommand::MoveTo);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(right.start);
            outPts.push(right.ctrl1);
            outPts.push(right.ctrl2);
            outPts.push(right.end);
            moveTo = right.start;
            break;
        }
        case PathCommand::Close: {
            Line right, left;
            Line{*(pts - 1), moveTo}.split(at, left, right);
            outCmds.push(PathCommand::MoveTo);
            outCmds.push(PathCommand::LineTo);
            outPts.push(right.pt1);
            outPts.push(right.pt2);
            break;
        }
    }
}


void _trimStartEnd(const PathCommand* cmds, const Point* pts, Point& moveTo, float at1, float at2, Array<PathCommand>& outCmds, Array<Point>& outPts)
{
    switch (*cmds) {
        case PathCommand::MoveTo:
            break;
        case PathCommand::LineTo: {
            Line tmp, left, right;
            Line{*(pts - 1), *pts}.split(at1, left, tmp);
            tmp.split(at2 - at1, left, right);
            outCmds.push(PathCommand::MoveTo);
            outCmds.push(PathCommand::LineTo);
            outPts.push(left.pt1);
            outPts.push(left.pt2);
            moveTo = left.pt1;
            break;
        }
        case PathCommand::CubicTo: {
            Bezier tmp, left, right;
            Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.split(at1, left, tmp);
            tmp.split(at2 - at1, left, right);
            outCmds.push(PathCommand::MoveTo);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(left.start);
            outPts.push(left.ctrl1);
            outPts.push(left.ctrl2);
            outPts.push(left.end);
            moveTo = left.start;
            break;
        }
        case PathCommand::Close: {
            Line tmp, left, right;
            Line{*(pts - 1), moveTo}.split(at1, left, tmp);
            tmp.split(at2 - at1, left, right);
            outCmds.push(PathCommand::MoveTo);
            outCmds.push(PathCommand::LineTo);
            outPts.push(left.pt1);
            outPts.push(left.pt2);
            break;
        }
    }
}


void _trimEnd(const PathCommand* cmds, const Point* pts, const Point& moveTo, float at, Array<PathCommand>& outCmds, Array<Point>& outPts)
{
    switch (*cmds) {
        case PathCommand::MoveTo:
            break;
        case PathCommand::LineTo: {
            Line right, left;
            Line{*(pts - 1), *pts}.split(at, left, right);
            outCmds.push(PathCommand::LineTo);
            outPts.push(right.pt1);
            break;
        }
        case PathCommand::CubicTo: {
            Bezier left, right;
            Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.split(at, left, right);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(left.ctrl1);
            outPts.push(left.ctrl2);
            outPts.push(left.end);
            break;
        }
        case PathCommand::Close: {
            Line right, left;
            Line{*(pts - 1), moveTo}.split(at, left, right);
            outCmds.push(PathCommand::LineTo);
            outPts.push(right.pt1);
            break;
        }
    }
}


void _add(const PathCommand* cmds, const Point* pts, const Point& moveTo, Array<PathCommand>& outCmds, Array<Point>& outPts)
{
    switch (*cmds) {
        case PathCommand::MoveTo: {
            outCmds.push(PathCommand::MoveTo);
            outPts.push(*pts);
            break;
        }
        case PathCommand::LineTo: {
            outCmds.push(PathCommand::LineTo);
            outPts.push(*pts);
            break;
        }
        case PathCommand::CubicTo: {
            outCmds.push(PathCommand::CubicTo);
            outPts.push(*pts);
            outPts.push(*(pts + 1));
            outPts.push(*(pts + 2));
            break;
        }
        case PathCommand::Close: {
            outCmds.push(PathCommand::LineTo);
            outPts.push(moveTo);
            break;
        }
    }
}


static void _trimPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, TVG_UNUSED uint32_t inPtsCnt, float trimStart, float trimEnd, Array<PathCommand>& outCmds, Array<Point>& outPts)
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

    for (uint32_t i = 0; i < inCmdsCnt; ++i) {
        auto dLen = _length();

        if (len + dLen < trimStart || len > trimEnd) {}
        else if (len < trimStart) {
            if (len + dLen > trimEnd) {
                _trimStartEnd(cmds, pts, moveToTrimmed, trimStart - len, trimEnd - len, outCmds, outPts);
            } else {
                _trimStart(cmds, pts, moveToTrimmed, trimStart - len, outCmds, outPts);
            }
        } else if (len + dLen > trimEnd) _trimEnd(cmds, pts, moveTo, trimEnd - len, outCmds, outPts);
        else _add(cmds, pts, moveTo, outCmds, outPts);

        len += dLen;
        _shift();
    }
}


static void _trim(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, float start, float end, Array<PathCommand>& outCmds, Array<Point>& outPts)
{
    auto totalLength = _pathLength(inCmds, inCmdsCnt, inPts, inPtsCnt);
    auto trimStart = start * totalLength;
    auto trimEnd = end * totalLength;

    if (trimStart > trimEnd) {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, totalLength, outCmds, outPts);
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, 0.0f, trimEnd, outCmds, outPts);
    } else {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, trimEnd, outCmds, outPts);
    }
}


bool Trim::valid() const
{
    if (start == 0.0f && end == 1.0f) return false;
    if (fabsf(end - start) >= 1.0f) return false;
    return true;
}


bool Trim::trim(float& start, float& end) const
{
    start = this->start;
    end = this->end;

    if (fabsf(end - start) >= 1.0f) {
        start = 0.0f;
        end = 1.0f;
        return false;
    }

    auto loop = true;

    if (start > 1.0f && end > 1.0f) loop = false;
    if (start < 0.0f && end < 0.0f) loop = false;
    if (start >= 0.0f && start <= 1.0f && end >= 0.0f  && end <= 1.0f) loop = false;

    if (start > 1.0f) start -= 1.0f;
    if (start < 0.0f) start += 1.0f;
    if (end > 1.0f) end -= 1.0f;
    if (end < 0.0f) end += 1.0f;

    if ((loop && start < end) || (!loop && start > end)) std::swap(start, end);
    return true;
}


bool Trim::trim(const Array<PathCommand>& inCmds, const Array<Point>& inPts, Array<PathCommand>& outCmds, Array<Point>& outPts) const
{
    if (inCmds.empty() || inPts.empty() || tvg::zero(start - end)) return false;

    outCmds.reserve(inCmds.count * 2);
    outPts.reserve(inPts.count * 2);

    auto pts = inPts.data;
    auto cmds = inCmds.data;

    if (simultaneous) {
        auto startCmds = cmds;
        auto startPts = pts;
        auto i = 0;
        while (i < inCmds.count) {
            switch (inCmds[i]) {
                case PathCommand::MoveTo: {
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, start, end, outCmds, outPts);
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
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, start, end, outCmds, outPts);
                    startPts = pts;
                    startCmds = cmds;
                    break;
                }
            }
            i++;
        }
        if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, start, end, outCmds, outPts);
    } else {
        _trim(inCmds.data, inCmds.count, inPts.data, inPts.count, start, end, outCmds, outPts);
    }

    return true;
}
