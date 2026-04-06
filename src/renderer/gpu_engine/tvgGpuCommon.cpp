/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#include "tvgGpuCommon.h"

/************************************************************************/
/* Utility Functions Implementation                                     */
/************************************************************************/

namespace tvg
{

// Optimize path in screen space by collapsing zero length lines  and removing unnecessary cubic beziers.
void gpuOptimize(const RenderPath& in, RenderPath& out, const Matrix& matrix, bool& thin)
{
    static constexpr auto PX_TOLERANCE = 0.25f;

    thin = false;
    if (in.empty()) return;

    out.cmds.clear();
    out.pts.clear();
    out.cmds.reserve(in.cmds.count);
    out.pts.reserve(in.pts.count);

    auto cmds = in.cmds.data;
    auto cmdCnt = in.cmds.count;
    auto pts = in.pts.data;

    Point lastOutT{};  // The suffix "T" indicates that the point is transformed.
    Point subpathStartT{};
    Point thinLineStart{};
    Point thinLineVec{};
    auto drawableSubpathCnt = 0u;
    auto thinLineVecLen = 0.0f;
    auto thinCandidate = true;
    auto thinLineReady = false;
    auto subpathOpen = false;
    auto subpathHasSegment = false;

    auto finalizeSubpath = [&]() {
        if (!subpathHasSegment) return;
        ++drawableSubpathCnt;
        if (drawableSubpathCnt > 1) thinCandidate = false;
        subpathHasSegment = false;
    };

    // vecLen is guaranteed to be non-zero since closed points are already merged
    auto point2Line = [](const Point& point, const Point& start, const Point& vec, float vecLen, float& maxDist, float& minT, float& maxT) {
        Point offset = point - start;
        auto dist = fabsf(tvg::cross(vec, offset)) / vecLen;
        if (dist > maxDist) maxDist = dist;
        auto t = tvg::dot(offset, vec) / (vecLen * vecLen);
        if (t < minT) minT = t;
        if (t > maxT) maxT = t;
    };

    auto validateCubic = [&point2Line](const Point& start, const Point& ctrl1, const Point& ctrl2, const Point& end, float& maxDist, float& minT, float& maxT, float& vecLen) {
        auto vec = end - start;
        vecLen = sqrtf(vec.x * vec.x + vec.y * vec.y);
        maxDist = 0.0f;
        minT = FLT_MAX;
        maxT = FLT_MIN;
        point2Line(ctrl1, start, vec, vecLen, maxDist, minT, maxT);
        point2Line(ctrl2, start, vec, vecLen, maxDist, minT, maxT);
    };

    auto checkThinPoint = [&](const Point& ptT) {
        if (!thinCandidate || !thinLineReady) return;
        auto dist = fabsf(tvg::cross(thinLineVec, ptT - thinLineStart)) / thinLineVecLen;
        if (dist > PX_TOLERANCE) thinCandidate = false;
    };

    auto collectThinSegment = [&](const Point& startT, const Point& endT) {
        subpathHasSegment = true;
        if (!thinCandidate) return;

        if (!thinLineReady) {
            if (!tvg::closed(startT, endT, PX_TOLERANCE)) {
                thinLineStart = startT;
                thinLineVec = endT - startT;
                thinLineVecLen = sqrtf(thinLineVec.x * thinLineVec.x + thinLineVec.y * thinLineVec.y);
                if (!tvg::zero(thinLineVecLen)) thinLineReady = true;
            }
        } else {
            checkThinPoint(startT);
            checkThinPoint(endT);
        }
    };

    auto addLineCmd = [&](const Point& startT, const Point& ptT) {
        out.cmds.push(PathCommand::LineTo);
        out.pts.push(ptT);
        lastOutT = ptT;
        collectThinSegment(startT, ptT);
    };

    auto processCubicTo = [&](const Point* cubicPts, const Point& startT) {
        auto ctrl1T = cubicPts[0] * matrix;
        auto ctrl2T = cubicPts[1] * matrix;
        auto endT = cubicPts[2] * matrix;
        if (tvg::closed(startT, endT, PX_TOLERANCE)) return;
        float maxDist, minT, maxT, vecLen;
        validateCubic(startT, ctrl1T, ctrl2T, endT, maxDist, minT, maxT, vecLen);
        auto flat = (maxDist <= PX_TOLERANCE);
        auto tEps = PX_TOLERANCE / vecLen;
        auto inSpan = (minT >= -tEps) && (maxT <= 1.0f + tEps);
        if (flat && inSpan) {
            addLineCmd(startT, endT);
        } else {
            out.cmds.push(PathCommand::CubicTo);
            out.pts.push(ctrl1T);
            out.pts.push(ctrl2T);
            out.pts.push(endT);
            lastOutT = endT;
            subpathHasSegment = true;
            thinCandidate = false;
        }
    };

    for (uint32_t i = 0; i < cmdCnt; i++) {
        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                finalizeSubpath();
                auto ptT = (*pts) * matrix;
                out.cmds.push(PathCommand::MoveTo);
                out.pts.push(ptT);
                lastOutT = ptT;
                subpathStartT = ptT;
                subpathOpen = true;
                pts++;
                break;
            }
            case PathCommand::LineTo: {
                auto startT = lastOutT;
                auto ptT = (*pts) * matrix;
                if (tvg::closed(startT, ptT, PX_TOLERANCE)) {
                    pts++;
                    break;
                }
                addLineCmd(startT, ptT);
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                processCubicTo(pts, lastOutT);
                pts += 3;
                break;
            }
            case PathCommand::Close: {
                if (subpathOpen && !tvg::closed(lastOutT, subpathStartT, PX_TOLERANCE)) collectThinSegment(lastOutT, subpathStartT);
                out.cmds.push(PathCommand::Close);
                lastOutT = subpathStartT;
                break;
            }
            default: break;
        }
    }
    finalizeSubpath();
    thin = thinCandidate && thinLineReady && (drawableSubpathCnt == 1);
}

bool gpuEdgesCross(const Point& p0, const Point& p1, const Point& p2, const Point& p3)
{
    auto orientSign = [](const Point& a, const Point& b, const Point& c) {
        auto value = cross(b - a, c - a);
        if (zero(value)) return int8_t{0};
        return (value > 0.0f) ? int8_t{1} : int8_t{-1};
    };

    auto straddlesLine = [&](const Point& a, const Point& b, const Point& c, const Point& d) {
        return orientSign(a, b, c) * orientSign(a, b, d) < 0;
    };

    return straddlesLine(p0, p1, p2, p3) && straddlesLine(p2, p3, p0, p1);
}

/************************************************************************/
/* GpuConvexProbe Class Implementation                                  */
/************************************************************************/

void GpuConvexProbe::addEdge(const Point& edge)
{
    if (zero(edge)) return;

    contourHasEdges = true;
    if (!convex) return;

    if (zero(firstEdge)) firstEdge = edge;

    updateDir(edge.x, prevXDir, xDirChanges);
    updateDir(edge.y, prevYDir, yDirChanges);
    if (!convex) return;

    if (zero(prevEdge)) {
        prevEdge = edge;
        return;
    }

    auto turn = cross(prevEdge, edge);
    if (zero(turn)) {
        if (dot(prevEdge, edge) < 0.0f && ++reversals > MaxCollinearReversals) convex = false;
    } else {
        auto sign = (turn > 0.0f) ? int8_t{1} : int8_t{-1};
        if (winding == 0) winding = sign;
        else if (sign != winding) convex = false;
    }

    prevEdge = edge;
}

void GpuConvexProbe::updateDir(float value, int8_t& prevDir, uint8_t& changes)
{
    int8_t dir = 0;
    if (!zero(value)) dir = (value > 0.0f) ? int8_t{1} : int8_t{-1};
    if (dir == 0 || !convex) return;

    if (prevDir != 0 && prevDir != dir && ++changes > MaxAxisDirChanges) {
        convex = false;
        return;
    }
    prevDir = dir;
}

/************************************************************************/
/* StrokeDashPath Class Implementation                                  */
/************************************************************************/

struct StrokeDashPath
{
    StrokeDashPath(RenderStroke::Dash dash) :
        dash(dash) {}
    bool gen(const RenderPath& in, RenderPath& out, bool drawPoint, const Matrix* transform = nullptr);

private:
    void lineTo(RenderPath& out, const Point& pt, bool drawPoint);
    void cubicTo(RenderPath& out, const Point& pt1, const Point& pt2, const Point& pt3, bool drawPoint);
    void point(RenderPath& out, const Point& p);
    Point map(const Point& pt) const { return applyTransform ? pt * (*transform) : pt; }
    template<typename Segment, typename LengthFn, typename SplitFn, typename DrawFn, typename PointFn>
    void segment(Segment seg, float len, RenderPath& out, bool allowDot, LengthFn lengthFn, SplitFn splitFn, DrawFn drawFn, PointFn getStartPt, const Point& endPos);

    RenderStroke::Dash dash;
    float curLen = 0.0f;
    int32_t curIdx = 0;
    Point curPos{};
    bool opGap = false;
    bool move = true;
    const Matrix* transform = nullptr;
    bool applyTransform = false;
};

template<typename Segment, typename LengthFn, typename SplitFn, typename DrawFn, typename PointFn>
void StrokeDashPath::segment(Segment seg, float len, RenderPath& out, bool allowDot, LengthFn lengthFn, SplitFn splitFn, DrawFn drawFn, PointFn getStartPt, const Point& end)
{
    static constexpr float MIN_CURR_LEN_THRESHOLD = 0.1f;

    if (tvg::zero(len)) {
        out.moveTo(map(curPos));
    } else if (len <= curLen) {
        curLen -= len;
        if (!opGap) {
            if (move) {
                out.moveTo(map(curPos));
                move = false;
            }
            drawFn(seg);
        }
    } else {
        Segment left, right;
        while (len - curLen > DASH_PATTERN_THRESHOLD) {
            if (curLen > 0.0f) {
                splitFn(seg, curLen, left, right);
                len -= curLen;
                if (!opGap) {
                    if (move || dash.pattern[curIdx] - curLen < FLOAT_EPSILON) {
                        out.moveTo(map(getStartPt(left)));
                        move = false;
                    }
                    drawFn(left);
                }
            } else {
                if (allowDot && !opGap) point(out, getStartPt(seg));
                right = seg;
            }

            curIdx = (curIdx + 1) % dash.count;
            curLen = dash.pattern[curIdx];
            opGap = !opGap;
            seg = right;
            curPos = getStartPt(seg);
            move = true;
        }
        curLen -= len;
        if (!opGap) {
            if (move) {
                out.moveTo(map(getStartPt(seg)));
                move = false;
            }
            drawFn(seg);
        }
        if (curLen < MIN_CURR_LEN_THRESHOLD) {
            curIdx = (curIdx + 1) % dash.count;
            curLen = dash.pattern[curIdx];
            opGap = !opGap;
        }
    }
    curPos = end;
}

// allowDot: zero length segment with non-butt cap still should be rendered as a point - only the caps are visible
bool StrokeDashPath::gen(const RenderPath& in, RenderPath& out, bool allowDot, const Matrix* transform)
{
    this->transform = transform;
    this->applyTransform = (transform && !tvg::identity(transform));

    int32_t idx = 0;
    auto offset = dash.offset;
    auto gap = false;
    if (!tvg::zero(dash.offset)) {
        auto length = (dash.count % 2) ? dash.length * 2 : dash.length;
        offset = fmodf(offset, length);
        if (offset < 0) offset += length;

        for (uint32_t i = 0; i < dash.count * (dash.count % 2 + 1); ++i, ++idx) {
            auto curPattern = dash.pattern[i % dash.count];
            if (offset < curPattern) break;
            offset -= curPattern;
            gap = !gap;
        }
        idx = idx % dash.count;
    }

    auto pts = in.pts.data;
    Point start{};

    ARRAY_FOREACH(cmd, in.cmds)
    {
        switch (*cmd) {
            case PathCommand::Close: {
                lineTo(out, start, allowDot);
                break;
            }
            case PathCommand::MoveTo: {
                // reset the dash state
                curIdx = idx;
                curLen = dash.pattern[idx] - offset;
                opGap = gap;
                move = true;
                start = curPos = *pts;
                pts++;
                break;
            }
            case PathCommand::LineTo: {
                lineTo(out, *pts, allowDot);
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                cubicTo(out, pts[0], pts[1], pts[2], allowDot);
                pts += 3;
                break;
            }
            default: break;
        }
    }
    return true;
}

void StrokeDashPath::point(RenderPath& out, const Point& p)
{
    if (move || dash.pattern[curIdx] < FLOAT_EPSILON) {
        out.moveTo(map(p));
        move = false;
    }
    out.lineTo(map(p));
}

void StrokeDashPath::lineTo(RenderPath& out, const Point& to, bool allowDot)
{
    Line line = {curPos, to};
    auto len = length(to - curPos);
    segment<Line>(line, len, out, allowDot, [](const Line& l) { return length(l.pt2 - l.pt1); }, [](const Line& l, float len, Line& left, Line& right) { l.split(len, left, right); }, [&](const Line& l) { out.lineTo(map(l.pt2)); }, [](const Line& l) { return l.pt1; }, to);
}

void StrokeDashPath::cubicTo(RenderPath& out, const Point& cnt1, const Point& cnt2, const Point& end, bool allowDot)
{
    Bezier curve = {curPos, cnt1, cnt2, end};
    auto len = curve.length();
    segment<Bezier>(curve, len, out, allowDot, [](const Bezier& b) { return b.length(); }, [](const Bezier& b, float len, Bezier& left, Bezier& right) { b.split(len, left, right); }, [&](const Bezier& b) { out.cubicTo(map(b.ctrl1), map(b.ctrl2), map(b.end)); }, [](const Bezier& b) { return b.start; }, end);
}

bool gpuStrokeDash(const RenderShape& rs, RenderPath& out, const Matrix* transform)
{
    if (!rs.stroke || rs.stroke->dash.count == 0 || rs.stroke->dash.length < DASH_PATTERN_THRESHOLD) return false;

    out.cmds.reserve(20 * rs.path.cmds.count);
    out.pts.reserve(20 * rs.path.pts.count);

    StrokeDashPath dash(rs.stroke->dash);
    auto allowDot = rs.stroke->cap != StrokeCap::Butt;

    if (rs.trimpath()) {
        RenderPath tpath;
        if (rs.stroke->trim.trim(rs.path, tpath)) return dash.gen(tpath, out, allowDot, transform);
        else return false;
    }
    return dash.gen(rs.path, out, allowDot, transform);
}

}  // namespace tvg