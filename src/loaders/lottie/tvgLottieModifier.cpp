/*
 * Copyright (c) 2024 - 2025 the ThorVG project. All rights reserved.

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

static bool _colinear(const Point* p)
{
    return tvg::zero(*p - *(p + 1)) && tvg::zero(*(p + 2) - *(p + 3));
}


static Point _roundCorner(RenderPath& out, Point& prev, Point& curr, Point& next, float r)
{
    auto lenPrev = length(prev - curr);
    auto rPrev = lenPrev > 0.0f ? 0.5f * std::min(lenPrev * 0.5f, r) / lenPrev : 0.0f;
    auto lenNext = length(next - curr);
    auto rNext = lenNext > 0.0f ? 0.5f * std::min(lenNext * 0.5f, r) / lenNext : 0.0f;
    auto dPrev = rPrev * (curr - prev);
    auto dNext = rNext * (curr - next);

    out.lineTo(curr - 2.0f * dPrev);
    auto ret = curr - 2.0f * dNext;
    out.cubicTo(curr - dPrev, curr - dNext, ret);
    return ret;
}


static bool _intersect(Line& line1, Line& line2, Point& intersection, bool& inside)
{
    if (tvg::zero(line1.pt2 - line2.pt1)) {
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


static Line _offset(Point& p1, Point& p2, float offset)
{
    auto scaledNormal = normal(p1, p2) * offset;
    return {p1 + scaledNormal, p2 + scaledNormal};
}


static bool _clockwise(Point* pts, uint32_t n)
{
    auto area = 0.0f;

    for (uint32_t i = 0; i < n - 1; i++) {
        area += cross(pts[i], pts[i + 1]);
    }
    area += cross(pts[n - 1], pts[0]);;

    return area < 0.0f;
}


void LottieOffsetModifier::corner(RenderPath& out, Line& line, Line& nextLine, uint32_t movetoOutIndex, bool nextClose)
{
    bool inside{};
    Point intersect{};
    if (_intersect(line, nextLine, intersect, inside)) {
        if (inside) {
            if (nextClose) out.pts[movetoOutIndex] = intersect;
            out.pts.push(intersect);
        } else {
            out.pts.push(line.pt2);
            if (join == StrokeJoin::Round) {
                out.cubicTo((line.pt2 + intersect) * 0.5f, (nextLine.pt1 + intersect) * 0.5f, nextLine.pt1);
            } else if (join == StrokeJoin::Miter) {
                auto norm = normal(line.pt1, line.pt2);
                auto nextNorm = normal(nextLine.pt1, nextLine.pt2);
                auto miterDirection = (norm + nextNorm) / length(norm + nextNorm);
                if (1.0f <= miterLimit * fabsf(miterDirection.x * norm.x + miterDirection.y * norm.y)) out.lineTo(intersect);
                out.lineTo(nextLine.pt1);
            } else out.lineTo(nextLine.pt1);
        }
    } else out.pts.push(line.pt2);
}


void LottieOffsetModifier::line(RenderPath& out, PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t& curPt, uint32_t curCmd, State& state, float offset, bool degenerated)
{
    if (tvg::zero(inPts[curPt - 1] - inPts[curPt])) {
        ++curPt;
        return;
    }

    if (inCmds[curCmd - 1] != PathCommand::LineTo) state.line = _offset(inPts[curPt - 1], inPts[curPt], offset);

    if (state.moveto) {
        state.movetoOutIndex = out.pts.count;
        out.moveTo(state.line.pt1);
        state.firstLine = state.line;
        state.moveto = false;
    }

    auto nonDegeneratedCubic = [&](uint32_t cmd, uint32_t pt) {
        return inCmds[cmd] == PathCommand::CubicTo && !tvg::zero(inPts[pt] - inPts[pt + 1]) && !tvg::zero(inPts[pt + 2] - inPts[pt + 3]);
    };

    out.cmds.push(PathCommand::LineTo);

    if (curCmd + 1 == inCmdsCnt || inCmds[curCmd + 1] == PathCommand::MoveTo || nonDegeneratedCubic(curCmd + 1, curPt + degenerated)) {
        out.pts.push(state.line.pt2);
        ++curPt;
        return;
    }

    Line nextLine = state.firstLine;
    if (inCmds[curCmd + 1] == PathCommand::LineTo) nextLine = _offset(inPts[curPt + degenerated], inPts[curPt + 1 + degenerated], offset);
    else if (inCmds[curCmd + 1] == PathCommand::CubicTo) nextLine = _offset(inPts[curPt + 1 + degenerated], inPts[curPt + 2 + degenerated], offset);
    else if (inCmds[curCmd + 1] == PathCommand::Close && !tvg::zero(inPts[curPt + degenerated] - inPts[state.movetoInIndex + degenerated]))
        nextLine = _offset(inPts[curPt + degenerated], inPts[state.movetoInIndex + degenerated], offset);

    corner(out, state.line, nextLine, state.movetoOutIndex, inCmds[curCmd + 1] == PathCommand::Close);

    state.line = nextLine;
    ++curPt;
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool LottieRoundnessModifier::modifyPath(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, Matrix* transform, RenderPath& out)
{
    buffer->clear();

    auto& path = (next) ? *buffer : out;
    path.cmds.reserve(inCmdsCnt * 2);
    path.pts.reserve((uint32_t)(inPtsCnt * 1.5));
    auto pivot = path.pts.count;
    uint32_t startIndex = 0;
    auto rounded = false;
    Point roundTo;

    for (uint32_t iCmds = 0, iPts = 0; iCmds < inCmdsCnt; ++iCmds) {
        switch (inCmds[iCmds]) {
            case PathCommand::MoveTo: {
                startIndex = path.pts.count;
                path.moveTo(inPts[iPts++]);
                break;
            }
            case PathCommand::CubicTo: {
                if (iCmds < inCmdsCnt - 1 && _colinear(inPts + iPts - 1)) {
                    auto& prev = inPts[iPts - 1];
                    auto& curr = inPts[iPts + 2];
                    if (inCmds[iCmds + 1] == PathCommand::CubicTo && _colinear(inPts + iPts + 2)) {
                        roundTo = _roundCorner(path, prev, curr, inPts[iPts + 5], r);
                        iPts += 3;
                        rounded = true;
                        continue;
                    } else if (inCmds[iCmds + 1] == PathCommand::Close) {
                        roundTo = _roundCorner(path, prev, curr, inPts[2], r);
                        path.pts[startIndex] = path.pts.last();
                        iPts += 3;
                        rounded = true;
                        continue;
                    }
                }
                path.cubicTo(rounded ? roundTo : inPts[iPts], inPts[iPts + 1], inPts[iPts + 2]);
                iPts += 3;
                break;
            }
            case PathCommand::Close: {
                path.close();
                break;
            }
            default: break;
        }
        rounded = false;
    }
    if (transform) {
        for (auto i = pivot; i < path.pts.count; ++i) {
            path.pts[i] *= *transform;
        }
    }

    if (next) return next->modifyPath(path.cmds.data, path.cmds.count, path.pts.data, path.pts.count, transform, out);

    return true;
}


bool LottieRoundnessModifier::modifyPolystar(RenderPath& in, RenderPath& out, float outerRoundness, bool hasRoundness)
{
    constexpr auto ROUNDED_POLYSTAR_MAGIC_NUMBER = 0.47829f;

    buffer->clear();

    auto& path = (next) ? *buffer : out;

    auto len = length(in.pts[1] - in.pts[2]);
    auto r = len > 0.0f ? ROUNDED_POLYSTAR_MAGIC_NUMBER * std::min(len * 0.5f, this->r) / len : 0.0f;

    if (hasRoundness) {
        path.cmds.grow((uint32_t)(1.5 * in.cmds.count));
        path.pts.grow((uint32_t)(4.5 * in.cmds.count));

        int start = 3 * tvg::zero(outerRoundness);
        path.moveTo(in.pts[start]);

        for (uint32_t i = 1 + start; i < in.pts.count; i += 6) {
            auto& prev = in.pts[i];
            auto& curr = in.pts[i + 2];
            auto& next = (i < in.pts.count - start) ? in.pts[i + 4] : in.pts[2];
            auto& nextCtrl = (i < in.pts.count - start) ? in.pts[i + 5] : in.pts[3];
            auto dNext = r * (curr - next);
            auto dPrev = r * (curr - prev);

            auto p0 = curr - 2.0f * dPrev;
            auto p1 = curr - dPrev;
            auto p2 = curr - dNext;
            auto p3 = curr - 2.0f * dNext;

            path.cubicTo(prev, p0, p0);
            path.cubicTo(p1, p2, p3);
            path.cubicTo(p3, next, nextCtrl);
        }
    } else {
        path.cmds.grow(2 * in.cmds.count);
        path.pts.grow(4 * in.cmds.count);

        auto dPrev = r * (in.pts[1] - in.pts[0]);
        auto p = in.pts[0] + 2.0f * dPrev;
        path.moveTo(p);

        for (uint32_t i = 1; i < in.pts.count; ++i) {
            auto& curr = in.pts[i];
            auto& next = (i == in.pts.count - 1) ? in.pts[1] : in.pts[i + 1];
            auto dNext = r * (curr - next);

            auto p0 = curr - 2.0f * dPrev;
            auto p1 = curr - dPrev;
            auto p2 = curr - dNext;
            auto p3 = curr - 2.0f * dNext;

            path.lineTo(p0);
            path.cubicTo(p1, p2, p3);

            dPrev = -1.0f * dNext;
        }
    }
    path.cmds.push(PathCommand::Close);

    if (next) return next->modifyPolystar(path, out, outerRoundness, hasRoundness);

    return true;
}


bool LottieRoundnessModifier::modifyRect(Point& size, float& r)
{
    r = std::min(this->r, std::max(size.x, size.y) * 0.5f);
    return true;
}


bool LottieOffsetModifier::modifyPath(PathCommand* inCmds, uint32_t inCmdsCnt, Point* inPts, uint32_t inPtsCnt, TVG_UNUSED Matrix* transform, RenderPath& out)
{
    if (next) TVGERR("LOTTIE", "Offset has a next modifier?");

    out.cmds.reserve(inCmdsCnt * 2);
    out.pts.reserve(inPtsCnt * (join == StrokeJoin::Round ? 4 : 2));

    Array<Bezier> stack{5};
    State state;
    auto offset = _clockwise(inPts, inPtsCnt) ? this->offset : -this->offset;
    auto threshold = 1.0f / fabsf(offset) + 1.0f;

    for (uint32_t iCmd = 0, iPt = 0; iCmd < inCmdsCnt; ++iCmd) {
        if (inCmds[iCmd] == PathCommand::MoveTo) {
            state.moveto = true;
            state.movetoInIndex = iPt++;
        } else if (inCmds[iCmd] == PathCommand::LineTo) {
            line(out, inCmds, inCmdsCnt, inPts, iPt, iCmd, state, offset, false);
        } else if (inCmds[iCmd] == PathCommand::CubicTo) {
            //cubic degenerated to a line
            if (tvg::zero(inPts[iPt - 1] - inPts[iPt]) || tvg::zero(inPts[iPt + 1] - inPts[iPt + 2])) {
                ++iPt;
                line(out, inCmds, inCmdsCnt, inPts, iPt, iCmd, state, offset, true);
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
                    state.movetoOutIndex = out.pts.count;
                    out.moveTo(line1.pt1);
                    state.firstLine = line1;
                    state.moveto = false;
                }

                bool inside{};
                Point intersect{};
                _intersect(line1, line2, intersect, inside);
                out.pts.push(intersect);
                _intersect(line2, line3, intersect, inside);
                out.pts.push(intersect);
                out.pts.push(line3.pt2);
                out.cmds.push(PathCommand::CubicTo);
            }

            iPt += 3;
        }
        else {
            if (!tvg::zero(inPts[iPt - 1] - inPts[state.movetoInIndex])) {
                out.cmds.push(PathCommand::LineTo);
                corner(out, state.line, state.firstLine, state.movetoOutIndex, true);
            }
            out.cmds.push(PathCommand::Close);
        }
    }
    return true;
}


bool LottieOffsetModifier::modifyPolystar(RenderPath& in, RenderPath& out, TVG_UNUSED float, TVG_UNUSED bool)
{
    return modifyPath(in.cmds.data, in.cmds.count, in.pts.data, in.pts.count, nullptr, out);
}


bool LottieOffsetModifier::modifyRect(RenderPath& in, RenderPath& out)
{
    return modifyPath(in.cmds.data, in.cmds.count, in.pts.data, in.pts.count, nullptr, out);
}


bool LottieOffsetModifier::modifyEllipse(Point& radius)
{
    radius.x += offset;
    radius.y += offset;
    return true;
}
