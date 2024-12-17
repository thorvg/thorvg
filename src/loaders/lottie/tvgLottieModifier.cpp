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

#include "tvgLottieModifier.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static void _roundCorner(Array<PathCommand>& cmds, Array<Point>& pts, const Point& prev, const Point& curr, const Point& next, float r)
{
    auto lenPrev = length(prev - curr);
    auto rPrev = lenPrev > 0.0f ? 0.5f * std::min(lenPrev * 0.5f, r) / lenPrev : 0.0f;
    auto lenNext = length(next - curr);
    auto rNext = lenNext > 0.0f ? 0.5f * std::min(lenNext * 0.5f, r) / lenNext : 0.0f;

    auto dPrev = rPrev * (curr - prev);
    auto dNext = rNext * (curr - next);

    pts.push(curr - 2.0f * dPrev);
    pts.push(curr - dPrev);
    pts.push(curr - dNext);
    pts.push(curr - 2.0f * dNext);
    cmds.push(PathCommand::LineTo);
    cmds.push(PathCommand::CubicTo);
}


static bool _zero(const Point& p1, const Point& p2)
{
    constexpr float epsilon = 1e-3f;
    return fabsf(p1.x / p2.x - 1.0f) < epsilon && fabsf(p1.y / p2.y - 1.0f) < epsilon;
}


static bool _intersect(const Line& line1, const Line& line2, Point& intersection, bool& inside)
{
    if (_zero(line1.pt2, line2.pt1)) {
        intersection = line1.pt2;
        inside = true;
        return true;
    }

    constexpr float epsilon = 1e-3f;
    float denom = (line1.pt2.x - line1.pt1.x) * (line2.pt2.y - line2.pt1.y) - (line1.pt2.y - line1.pt1.y) * (line2.pt2.x - line2.pt1.x);
    if (fabsf(denom) < epsilon) return false;

    float t = ((line2.pt1.x - line1.pt1.x) * (line2.pt2.y - line2.pt1.y) - (line2.pt1.y - line1.pt1.y) * (line2.pt2.x - line2.pt1.x)) / denom;
    float u = ((line2.pt1.x - line1.pt1.x) * (line1.pt2.y - line1.pt1.y) - (line2.pt1.y - line1.pt1.y) * (line1.pt2.x - line1.pt1.x)) / denom;

    intersection.x = line1.pt1.x + t * (line1.pt2.x - line1.pt1.x);
    intersection.y = line1.pt1.y + t * (line1.pt2.y - line1.pt1.y);
    inside = t >= -epsilon && t <= 1.0f + epsilon && u >= -epsilon && u <= 1.0f + epsilon;

    return true;
}


static Line _offset(const Point& p1, const Point& p2, float offset)
{
    auto scaledNormal = normal(p1, p2) * offset;
    return {p1 + scaledNormal, p2 + scaledNormal};
}


static bool _clockwise(const Point* pts, uint32_t n)
{
    auto area = 0.0f;

    for (uint32_t i = 0; i < n - 1; i++) {
        area += cross(pts[i], pts[i + 1]);
    }
    area += cross(pts[n - 1], pts[0]);;

    return area < 0.0f;
}


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


void LottieOffsetModifier::corner(const Line& line, const Line& nextLine, uint32_t movetoOutIndex, bool nextClose, Array<PathCommand>& outCmds, Array<Point>& outPts) const
{
    bool inside{};
    Point intersect{};
    if (_intersect(line, nextLine, intersect, inside)) {
        if (inside) {
            if (nextClose) outPts[movetoOutIndex] = intersect;
            outPts.push(intersect);
        } else {
            outPts.push(line.pt2);
            if (join == StrokeJoin::Round) {
                outCmds.push(PathCommand::CubicTo);
                outPts.push((line.pt2 + intersect) * 0.5f);
                outPts.push((nextLine.pt1 + intersect) * 0.5f);
                outPts.push(nextLine.pt1);
            } else if (join == StrokeJoin::Miter) {
                auto norm = normal(line.pt1, line.pt2);
                auto nextNorm = normal(nextLine.pt1, nextLine.pt2);
                auto miterDirection = (norm + nextNorm) / length(norm + nextNorm);
                outCmds.push(PathCommand::LineTo);
                if (1.0f <= miterLimit * fabsf(miterDirection.x * norm.x + miterDirection.y * norm.y)) outPts.push(intersect);
                else outPts.push(nextLine.pt1);
            } else {
                outCmds.push(PathCommand::LineTo);
                outPts.push(nextLine.pt1);
            }
        }
    } else outPts.push(line.pt2);
}


void LottieOffsetModifier::line(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t& currentPt, uint32_t currentCmd, State& state, bool degenerated, Array<PathCommand>& outCmds, Array<Point>& outPts, float offset) const
{
    if (tvg::zero(inPts[currentPt - 1] - inPts[currentPt])) {
        ++currentPt;
        return;
    }

    if (inCmds[currentCmd - 1] != PathCommand::LineTo) state.line = _offset(inPts[currentPt - 1], inPts[currentPt], offset);

    if (state.moveto) {
        outCmds.push(PathCommand::MoveTo);
        state.movetoOutIndex = outPts.count;
        outPts.push(state.line.pt1);
        state.firstLine = state.line;
        state.moveto = false;
    }

    auto nonDegeneratedCubic = [&](uint32_t cmd, uint32_t pt) {
        return inCmds[cmd] == PathCommand::CubicTo && !tvg::zero(inPts[pt] - inPts[pt + 1]) && !tvg::zero(inPts[pt + 2] - inPts[pt + 3]);
    };

    outCmds.push(PathCommand::LineTo);

    if (currentCmd + 1 == inCmdsCnt || inCmds[currentCmd + 1] == PathCommand::MoveTo || nonDegeneratedCubic(currentCmd + 1, currentPt + degenerated)) {
        outPts.push(state.line.pt2);
        ++currentPt;
        return;
    }

    Line nextLine = state.firstLine;
    if (inCmds[currentCmd + 1] == PathCommand::LineTo) nextLine = _offset(inPts[currentPt + degenerated], inPts[currentPt + 1 + degenerated], offset);
    else if (inCmds[currentCmd + 1] == PathCommand::CubicTo) nextLine = _offset(inPts[currentPt + 1 + degenerated], inPts[currentPt + 2 + degenerated], offset);
    else if (inCmds[currentCmd + 1] == PathCommand::Close && !_zero(inPts[currentPt + degenerated], inPts[state.movetoInIndex + degenerated]))
        nextLine = _offset(inPts[currentPt + degenerated], inPts[state.movetoInIndex + degenerated], offset);

    corner(state.line, nextLine, state.movetoOutIndex, inCmds[currentCmd + 1] == PathCommand::Close, outCmds, outPts);

    state.line = nextLine;
    ++currentPt;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieRoundnessModifier::modifyPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, Array<PathCommand>& outCmds, Array<Point>& outPts, Matrix* transform) const
{
    outCmds.reserve(inCmdsCnt * 2);
    outPts.reserve((uint32_t)(inPtsCnt * 1.5));
    auto ptsCnt = outPts.count;

    uint32_t startIndex = 0;
    for (uint32_t iCmds = 0, iPts = 0; iCmds < inCmdsCnt; ++iCmds) {
        switch (inCmds[iCmds]) {
            case PathCommand::MoveTo: {
                startIndex = outPts.count;
                outCmds.push(PathCommand::MoveTo);
                outPts.push(inPts[iPts++]);
                break;
            }
            case PathCommand::CubicTo: {
                auto& prev = inPts[iPts - 1];
                auto& curr = inPts[iPts + 2];
                if (iCmds < inCmdsCnt - 1 &&
                    tvg::zero(inPts[iPts - 1] - inPts[iPts]) &&
                    tvg::zero(inPts[iPts + 1] - inPts[iPts + 2])) {
                    if (inCmds[iCmds + 1] == PathCommand::CubicTo &&
                        tvg::zero(inPts[iPts + 2] - inPts[iPts + 3]) &&
                        tvg::zero(inPts[iPts + 4] - inPts[iPts + 5])) {
                        _roundCorner(outCmds, outPts, prev, curr, inPts[iPts + 5], r);
                        iPts += 3;
                        break;
                    } else if (inCmds[iCmds + 1] == PathCommand::Close) {
                        _roundCorner(outCmds, outPts, prev, curr, inPts[2], r);
                        outPts[startIndex] = outPts.last();
                        iPts += 3;
                        break;
                    }
                }
                outCmds.push(PathCommand::CubicTo);
                outPts.push(inPts[iPts++]);
                outPts.push(inPts[iPts++]);
                outPts.push(inPts[iPts++]);
                break;
            }
            case PathCommand::Close: {
                outCmds.push(PathCommand::Close);
                break;
            }
            default: break;
        }
    }
    if (transform) {
        for (auto i = ptsCnt; i < outPts.count; ++i) {
            outPts[i] *= *transform;
        }
    }
    return true;
}


bool LottieRoundnessModifier::modifyPolystar(TVG_UNUSED const Array<PathCommand>& inCmds, const Array<Point>& inPts, Array<PathCommand>& outCmds, Array<Point>& outPts, float outerRoundness, bool hasRoundness) const
{
    static constexpr auto ROUNDED_POLYSTAR_MAGIC_NUMBER = 0.47829f;

    auto len = length(inPts[1] - inPts[2]);
    auto r = len > 0.0f ? ROUNDED_POLYSTAR_MAGIC_NUMBER * std::min(len * 0.5f, this->r) / len : 0.0f;

    if (hasRoundness) {
        outCmds.grow((uint32_t)(1.5 * inCmds.count));
        outPts.grow((uint32_t)(4.5 * inCmds.count));

        int start = 3 * tvg::zero(outerRoundness);
        outCmds.push(PathCommand::MoveTo);
        outPts.push(inPts[start]);

        for (uint32_t i = 1 + start; i < inPts.count; i += 6) {
            auto& prev = inPts[i];
            auto& curr = inPts[i + 2];
            auto& next = (i < inPts.count - start) ? inPts[i + 4] : inPts[2];
            auto& nextCtrl = (i < inPts.count - start) ? inPts[i + 5] : inPts[3];
            auto dNext = r * (curr - next);
            auto dPrev = r * (curr - prev);

            auto p0 = curr - 2.0f * dPrev;
            auto p1 = curr - dPrev;
            auto p2 = curr - dNext;
            auto p3 = curr - 2.0f * dNext;

            outCmds.push(PathCommand::CubicTo);
            outPts.push(prev); outPts.push(p0); outPts.push(p0);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(p1); outPts.push(p2); outPts.push(p3);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(p3); outPts.push(next); outPts.push(nextCtrl);
        }
    } else {
        outCmds.grow(2 * inCmds.count);
        outPts.grow(4 * inCmds.count);

        auto dPrev = r * (inPts[1] - inPts[0]);
        auto p = inPts[0] + 2.0f * dPrev;
        outCmds.push(PathCommand::MoveTo);
        outPts.push(p);

        for (uint32_t i = 1; i < inPts.count; ++i) {
            auto& curr = inPts[i];
            auto& next = (i == inPts.count - 1) ? inPts[1] : inPts[i + 1];
            auto dNext = r * (curr - next);

            auto p0 = curr - 2.0f * dPrev;
            auto p1 = curr - dPrev;
            auto p2 = curr - dNext;
            auto p3 = curr - 2.0f * dNext;

            outCmds.push(PathCommand::LineTo);
            outPts.push(p0);
            outCmds.push(PathCommand::CubicTo);
            outPts.push(p1); outPts.push(p2); outPts.push(p3);

            dPrev = -1.0f * dNext;
        }
    }
    outCmds.push(PathCommand::Close);
    return true;
}


bool LottieRoundnessModifier::modifyRect(const Point& size, float& r) const
{
    r = std::min(this->r, std::max(size.x, size.y) * 0.5f);
    return true;
}


bool LottieOffsetModifier::modifyPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, Array<PathCommand>& outCmds, Array<Point>& outPts) const
{
    outCmds.reserve(inCmdsCnt * 2);
    outPts.reserve(inPtsCnt * (join == StrokeJoin::Round ? 4 : 2));

    Array<Bezier> stack{5};
    State state;
    auto offset = _clockwise(inPts, inPtsCnt) ? this->offset : -this->offset;
    auto threshold = 1.0f / fabsf(offset) + 1.0f;

    for (uint32_t iCmd = 0, iPt = 0; iCmd < inCmdsCnt; ++iCmd) {
        if (inCmds[iCmd] == PathCommand::MoveTo) {
            state.moveto = true;
            state.movetoInIndex = iPt++;
        } else if (inCmds[iCmd] == PathCommand::LineTo) {
            line(inCmds, inCmdsCnt, inPts, iPt, iCmd, state, false, outCmds, outPts, offset);
        } else if (inCmds[iCmd] == PathCommand::CubicTo) {
            //cubic degenerated to a line
            if (tvg::zero(inPts[iPt - 1] - inPts[iPt]) || tvg::zero(inPts[iPt + 1] - inPts[iPt + 2])) {
                ++iPt;
                line(inCmds, inCmdsCnt, inPts, iPt, iCmd, state, true, outCmds, outPts, offset);
                ++iPt;
                continue;
            }

            stack.push({inPts[iPt - 1], inPts[iPt], inPts[iPt + 1], inPts[iPt + 2]});
            while (!stack.empty()) {
                auto& bezier = stack.last();
                auto len = tvg::length(bezier.start - bezier.ctrl1) + tvg::length(bezier.ctrl1 - bezier.ctrl2) + tvg::length(bezier.ctrl2 - bezier.end);

                if (len >  threshold * bezier.length()) {
                    Bezier next;
                    bezier.split(0.5f, next);
                    stack.push(next);
                    continue;
                }
                stack.pop();

                auto line1 = _offset(bezier.start, bezier.ctrl1, offset);
                auto line2 = _offset(bezier.ctrl1, bezier.ctrl2, offset);
                auto line3 = _offset(bezier.ctrl2, bezier.end, offset);

                if (state.moveto) {
                    outCmds.push(PathCommand::MoveTo);
                    state.movetoOutIndex = outPts.count;
                    outPts.push(line1.pt1);
                    state.firstLine = line1;
                    state.moveto = false;
                }

                bool inside{};
                Point intersect{};
                _intersect(line1, line2, intersect, inside);
                outPts.push(intersect);
                _intersect(line2, line3, intersect, inside);
                outPts.push(intersect);
                outPts.push(line3.pt2);
                outCmds.push(PathCommand::CubicTo);
            }

            iPt += 3;
        }
        else {
            if (!_zero(inPts[iPt - 1], inPts[state.movetoInIndex])) {
                outCmds.push(PathCommand::LineTo);
                corner(state.line, state.firstLine, state.movetoOutIndex, true, outCmds, outPts);
            }
            outCmds.push(PathCommand::Close);
        }
    }
    return true;
}


bool LottieOffsetModifier::modifyPolystar(const Array<PathCommand>& inCmds, const Array<Point>& inPts, Array<PathCommand>& outCmds, Array<Point>& outPts) const {
    return modifyPath(inCmds.data, inCmds.count, inPts.data, inPts.count, outCmds, outPts);
}


bool LottieOffsetModifier::modifyRect(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, Array<PathCommand>& outCmds, Array<Point>& outPts) const
{
    return modifyPath(inCmds, inCmdsCnt, inPts, inPtsCnt, outCmds, outPts);
}


bool LottieOffsetModifier::modifyEllipse(float& rx, float& ry) const
{
    rx += offset;
    ry += offset;
    return true;
}


bool LottieTrimpathModifier::modifyPath(const Array<PathCommand>& inCmds, const Array<Point>& inPts, Array<PathCommand>& outCmds, Array<Point>& outPts) const
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