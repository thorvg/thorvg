/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#include "tvgGlTessellator.h"

namespace tvg
{

static uint32_t _pushVertex(Array<float>& array, float x, float y)
{
    auto index = array.count / 2;
    array.grow(2);
    array.data[array.count++] = x;
    array.data[array.count++] = y;
    return index;
}

static void _pushTriangle(Array<uint32_t>& array, uint32_t a, uint32_t b, uint32_t c)
{
    array.grow(3);
    array.data[array.count++] = a;
    array.data[array.count++] = b;
    array.data[array.count++] = c;
}

Stroker::Stroker(GlGeometryBuffer* buffer, float width, StrokeCap cap, StrokeJoin join, float miterLimit, float qualityScale) : mBuffer(buffer), mWidth(width), mMiterLimit(miterLimit), mQualityScale(qualityScale), mCap(cap), mJoin(join)
{
}


RenderRegion Stroker::bounds() const
{
    return {{int32_t(floor(mLeftTop.x)), int32_t(floor(mLeftTop.y))}, {int32_t(ceil(mRightBottom.x)), int32_t(ceil(mRightBottom.y))}};
}


void Stroker::run(const RenderPath& path, bool thinFill)
{
    mThinFill = thinFill;
    overlapFree = !thinFill;
    mProbe = {};
    mDirections = 0;
    mBuffer->vertex.reserve(path.pts.count * 8 + 16);
    mBuffer->index.reserve(path.pts.count * 6);

    auto validStrokeCap = false;
    auto pts = path.pts.data;

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                if (validStrokeCap) { // check this, so we can skip if path only contains move instruction
                    cap();
                    validStrokeCap = false;
                }
                mState = {};
                mState.firstPt = mState.prevPt = *pts++;
                validStrokeCap = false;
            } break;
            case PathCommand::LineTo: {
                validStrokeCap = true;
                lineTo(*pts);
                pts++;
            } break;
            case PathCommand::CubicTo: {
                validStrokeCap = true;
                cubicTo(pts[0], pts[1], pts[2]);
                pts += 3;
            } break;
            case PathCommand::Close: {
                close();
                validStrokeCap = false;
            } break;
            default:
                break;
        }
    }
    if (validStrokeCap) cap();
}


void Stroker::cap()
{
    // Open contours monotone in both axes may turn both ways. Half-segment cuts
    // keep joins independent, and their forward boundaries separate the caps.
    auto monotone = (mDirections & 3) != 3 && (mDirections & 12) != 12;
    overlapFree &= mState.firstLength > 0.0f && (mCap == StrokeCap::Butt || mState.firstPt != mState.prevPt) &&
                   (monotone || (mProbe.convex && mProbe.xDirChanges <= 1 && mProbe.yDirChanges <= 1 &&
                    mProbe.winding * cross(mState.firstPtDir, mState.prevPtDir) >= 0.0f));
    if (mCap == StrokeCap::Butt) return;

    if (mCap == StrokeCap::Square) {
        if (mState.firstPt == mState.prevPt) squarePoint(mState.firstPt);
        else {
            square(mState.firstPt, {-mState.firstPtDir.x, -mState.firstPtDir.y});
            square(mState.prevPt, mState.prevPtDir);
        }
    } else if (mCap == StrokeCap::Round) {
        if (mState.firstPt == mState.prevPt) roundPoint(mState.firstPt);
        else {
            round(mState.firstPt, {-mState.firstPtDir.x, -mState.firstPtDir.y});
            round(mState.prevPt, mState.prevPtDir);
        }
    }
}


void Stroker::lineTo(const Point& curr)
{
    auto dir = (curr - mState.prevPt);
    auto len = length(dir);
    if (tvg::zero(len) || !std::isfinite(len)) return;
    dir *= 1.0f / len;
    if (overlapFree) {
        if (mState.firstLength == 0.0f) overlapFree &= !mProbe.contourHasEdges;
        // Keep collecting after the convexity probe rejects a change of winding.
        mDirections |= (dir.x > 0.0f) | ((dir.x < 0.0f) << 1) | ((dir.y > 0.0f) << 2) | ((dir.y < 0.0f) << 3);
        mProbe.addEdge(dir);
    }

    auto normal = Point{-dir.y, dir.x};
    auto a = mState.prevPt + normal * radius();
    auto b = mState.prevPt - normal * radius();
    auto c = curr + normal * radius();
    auto d = curr - normal * radius();

    auto ia = _pushVertex(mBuffer->vertex, a.x, a.y);
    auto ib = _pushVertex(mBuffer->vertex, b.x, b.y);
    auto ic = _pushVertex(mBuffer->vertex, c.x, c.y);
    auto id = _pushVertex(mBuffer->vertex, d.x, d.y);

    /**
     *   a --------- c
     *   |           |
     *   |           |
     *   b-----------d
     */

    _pushTriangle(mBuffer->index, ia, ib, ic);
    _pushTriangle(mBuffer->index, ib, id, ic);

    if (mState.firstLength == 0.0f) {
        mState.firstPtDir = dir;
        mState.firstLength = len;
        mState.firstIndex = ia;
    } else join(dir, len, ia);
    mState.prevPtDir = dir;
    mState.prevPt = curr;
    mState.prevLength = len;
    mState.prevIndex = ia;

    if (ia == 0) {
        mRightBottom.x = mLeftTop.x = curr.x;
        mRightBottom.y = mLeftTop.y = curr.y;
    }

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::cubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    Bezier curve{ mState.prevPt, cnt1, cnt2, end };

    auto count = curve.segments(mQualityScale);
    // Each segment emits a quad (8 floats, 6 indices). Reserve the fixed join
    // costs as well; round joins stay dynamic because their arc count varies.
    auto joinCount = count - ((mState.firstLength == 0.0f) ? 1u : 0u);
    auto vertexCount = count * 8u;
    auto indexCount = count * 6u;
    if (mJoin == StrokeJoin::Bevel) {
        vertexCount += joinCount * 6u;
        indexCount += joinCount * 3u;
    } else if (mJoin == StrokeJoin::Miter) {
        vertexCount += joinCount * 8u;
        indexCount += joinCount * 6u;
    }
    mBuffer->vertex.grow(vertexCount);
    mBuffer->index.grow(indexCount);

    auto step = 1.f / count;

    for (uint32_t i = 1; i <= count; ++i) {
        lineTo(curve.at(step * i));
    }
}


void Stroker::close()
{
    if (mState.firstLength == 0.0f) return;
    auto delta = mState.prevPt - mState.firstPt;
    if (dot(delta, delta) > 0.015625f * 0.015625f) {
        lineTo(mState.firstPt);
    }

    // A skipped closing edge cannot share an inner corner.
    auto len = (mState.prevPt.x == mState.firstPt.x && mState.prevPt.y == mState.firstPt.y) ? mState.firstLength : 0.0f;
    if (len == 0.0f) overlapFree = false;
    if (overlapFree) mProbe.addEdge(mState.firstPtDir);
    join(mState.firstPtDir, len, mState.firstIndex);
    overlapFree &= mProbe.convex;
    auto first = mState.firstPt;
    mState = {};
    mState.firstPt = mState.prevPt = first;
}


void Stroker::join(const Point& dir, float len, uint32_t index)
{
    auto turn = cross(mState.prevPtDir, dir);

    if (tvg::zero(turn)) {
        overlapFree &= mState.prevPtDir.x == dir.x && mState.prevPtDir.y == dir.y;
        if (mState.prevPtDir == dir) return;      // check is same direction
        if (mJoin != StrokeJoin::Round) return;   // opposite direction

        round(mState.prevPt, dir);

    } else {
        auto normal = Point{-dir.y, dir.x};
        auto prevNormal = Point{-mState.prevPtDir.y, mState.prevPtDir.x};
        auto offset = turn < 0.0f ? radius() : -radius();
        auto prevJoin = mState.prevPt + prevNormal * offset;
        auto currJoin = mState.prevPt + normal * offset;
        auto apex = mState.prevPt;
        auto denom = 1.0f + dot(mState.prevPtDir, dir);
        auto trimmed = false;

        if (!mThinFill && denom > FLOAT_EPSILON) {
            auto setback = radius() * fabsf(turn) / denom;
            auto scale = std::max(std::max(fabsf(apex.x), fabsf(apex.y)), std::max(radius(), std::max(mState.prevLength, len)));
            // Keep each cut within its half-segment so neighboring cuts and caps stay independent.
            if (2.0f * setback + FLOAT_EPSILON * scale < std::min(mState.prevLength, len)) {
                auto inner = apex - (prevNormal + normal) * (offset / denom);
                auto side = turn < 0.0f ? 1u : 0u;
                auto vertices = mBuffer->vertex.data;
                auto prev = (mState.prevIndex + 2 + side) * 2;
                auto curr = (index + side) * 2;
                vertices[prev] = vertices[curr] = inner.x;
                vertices[prev + 1] = vertices[curr + 1] = inner.y;
                apex = inner;
                trimmed = true;
            }
        }
        overlapFree &= trimmed;

        if (mJoin == StrokeJoin::Miter) miter(prevJoin, currJoin, mState.prevPt, apex);
        else if (mJoin == StrokeJoin::Bevel) bevel(prevJoin, currJoin, apex);
        else round(prevJoin, currJoin, mState.prevPt, apex);
    }
}


void Stroker::round(const Point &prev, const Point& curr, const Point& center, const Point& apex)
{
    auto turn = cross(center - prev, curr - prev);
    if (turn == 0.0f) return;

    mLeftTop.x = std::min(mLeftTop.x, std::min(center.x, std::min(prev.x, curr.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(center.y, std::min(prev.y, curr.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(center.x, std::max(prev.x, curr.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(center.y, std::max(prev.y, curr.y)));

    auto startAngle = tvg::atan(prev - center);
    auto endAngle = tvg::atan(curr - center);

    if (turn > 0.0f) {
        if (endAngle > startAngle) endAngle -= 2 * MATH_PI;
    } else {
        if (endAngle < startAngle) endAngle += 2 * MATH_PI;
    }

    auto arcAngle = endAngle - startAngle;
    auto count = gpuArcSegmentsCnt(arcAngle, radius() * mQualityScale);

    auto c = _pushVertex(mBuffer->vertex, apex.x, apex.y);
    auto pi = _pushVertex(mBuffer->vertex, prev.x, prev.y);
    auto step = (endAngle - startAngle) / (count - 1);

    for (uint32_t i = 1; i + 1 < count; i++) {
        auto angle = startAngle + step * i;
        Point out = {center.x + cos(angle) * radius(), center.y + sin(angle) * radius()};
        auto oi = _pushVertex(mBuffer->vertex, out.x, out.y);

        _pushTriangle(mBuffer->index, c, pi, oi);

        pi = oi;

        mLeftTop.x = std::min(mLeftTop.x, out.x);
        mLeftTop.y = std::min(mLeftTop.y, out.y);
        mRightBottom.x = std::max(mRightBottom.x, out.x);
        mRightBottom.y = std::max(mRightBottom.y, out.y);
    }

    // Keep the shared edge exact so adjacent arcs and segments do not overlap.
    auto oi = _pushVertex(mBuffer->vertex, curr.x, curr.y);
    _pushTriangle(mBuffer->index, c, pi, oi);
}


void Stroker::roundPoint(const Point &p)
{
    auto count = gpuArcSegmentsCnt(2.0f * MATH_PI, radius() * mQualityScale);
    auto c = _pushVertex(mBuffer->vertex, p.x, p.y);
    auto step = 2.0f * MATH_PI / (count - 1);

    for (uint32_t i = 1; i <= static_cast<uint32_t>(count); i++) {
        float angle = i * step;
        Point dir = {cos(angle), sin(angle)};
        Point out = p + dir * radius();
        auto oi = _pushVertex(mBuffer->vertex, out.x, out.y);

        if (oi > 1) {
            _pushTriangle(mBuffer->index, c, oi, oi - 1);
        }
    }

    mLeftTop.x = std::min(mLeftTop.x, p.x - radius());
    mLeftTop.y = std::min(mLeftTop.y, p.y - radius());
    mRightBottom.x = std::max(mRightBottom.x, p.x + radius());
    mRightBottom.y = std::max(mRightBottom.y, p.y + radius());
}


void Stroker::miter(const Point& prev, const Point& curr, const Point& center, const Point& apex)
{
    auto pp1 = prev - center;
    auto pp2 = curr - center;
    auto out = pp1 + pp2;
    auto k = 2.f * radius() * radius() / (out.x * out.x + out.y * out.y);
    auto pe = out * k;

    // if (length(pe) >= mMiterLimit * radius())
    auto limit = mMiterLimit * radius();
    if (dot(pe, pe) >= limit * limit) {
        bevel(prev, curr, apex);
        return;
    }

    auto join = center + pe;
    auto c = _pushVertex(mBuffer->vertex, apex.x, apex.y);
    auto cp1 = _pushVertex(mBuffer->vertex, prev.x, prev.y);
    auto cp2 = _pushVertex(mBuffer->vertex, curr.x, curr.y);
    auto e = _pushVertex(mBuffer->vertex, join.x, join.y);

    _pushTriangle(mBuffer->index, c, cp1, e);
    _pushTriangle(mBuffer->index, e, cp2, c);

    mLeftTop.x = std::min(mLeftTop.x, join.x);
    mLeftTop.y = std::min(mLeftTop.y, join.y);

    mRightBottom.x = std::max(mRightBottom.x, join.x);
    mRightBottom.y = std::max(mRightBottom.y, join.y);
}


void Stroker::bevel(const Point& prev, const Point& curr, const Point& apex)
{
    auto a = _pushVertex(mBuffer->vertex, prev.x, prev.y);
    auto b = _pushVertex(mBuffer->vertex, curr.x, curr.y);
    auto c = _pushVertex(mBuffer->vertex, apex.x, apex.y);

    _pushTriangle(mBuffer->index, a, b, c);
}


void Stroker::square(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};

    auto a = p + normal * radius();
    auto b = p - normal * radius();
    auto c = a + outDir * radius();
    auto d = b + outDir * radius();

    auto ai = _pushVertex(mBuffer->vertex, a.x, a.y);
    auto bi = _pushVertex(mBuffer->vertex, b.x, b.y);
    auto ci = _pushVertex(mBuffer->vertex, c.x, c.y);
    auto di = _pushVertex(mBuffer->vertex, d.x, d.y);

    _pushTriangle(mBuffer->index, ai, bi, ci);
    _pushTriangle(mBuffer->index, ci, bi, di);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::squarePoint(const Point& p)
{
    auto offsetX = Point{radius(), 0.0f};
    auto offsetY = Point{0.0f, radius()};

    auto a = p + offsetX + offsetY;
    auto b = p - offsetX + offsetY;
    auto c = p - offsetX - offsetY;
    auto d = p + offsetX - offsetY;

    auto ai = _pushVertex(mBuffer->vertex, a.x, a.y);
    auto bi = _pushVertex(mBuffer->vertex, b.x, b.y);
    auto ci = _pushVertex(mBuffer->vertex, c.x, c.y);
    auto di = _pushVertex(mBuffer->vertex, d.x, d.y);

    _pushTriangle(mBuffer->index, ai, bi, ci);
    _pushTriangle(mBuffer->index, ci, di, ai);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::round(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};
    auto a = p + normal * radius();
    auto b = p - normal * radius();
    auto c = p + outDir * radius();

    round(a, c, p, p);
    round(c, b, p, p);
}


BWTessellator::BWTessellator(GlGeometryBuffer* buffer): mBuffer(buffer)
{
}

void BWTessellator::tessellate(const RenderPath& path)
{
    auto cmds = path.cmds.data;
    auto cmdCnt = path.cmds.count;
    auto pts = path.pts.data;
    auto ptsCnt = path.pts.count;

    if (ptsCnt <= 2) return;

    uint32_t firstIndex = 0;
    uint32_t prevIndex = 0;
    Point firstPt = {};
    Point prevPt = {};
    GpuConvexProbe probe;
    bool contourClosed = false;

    mBuffer->vertex.reserve(ptsCnt * 2);
    mBuffer->index.reserve((ptsCnt - 2) * 3);

    auto finishContour = [&]() {
        if (prevIndex == 0 || contourClosed) return;
        probe.addContourClose(firstPt - prevPt);
        contourClosed = true;
    };

    for (uint32_t i = 0; i < cmdCnt; i++) {
        switch(cmds[i]) {
            case PathCommand::MoveTo: {
                finishContour();
                probe.nextContour();
                firstIndex = pushVertex(pts->x, pts->y);
                firstPt = prevPt = *pts;
                prevIndex = 0;
                contourClosed = false;
                pts++;
            } break;
            case PathCommand::LineTo: {
                auto edge = *pts - prevPt;
                if (prevIndex == 0) {
                    prevIndex = pushVertex(pts->x, pts->y);
                    probe.addEdge(edge);
                    prevPt = *pts++;
                } else {
                    probe.addEdge(edge);
                    auto currIndex = pushVertex(pts->x, pts->y);
                    pushTriangle(firstIndex, prevIndex, currIndex);
                    prevIndex = currIndex;
                    prevPt = *pts++;
                }
            } break;
            case PathCommand::CubicTo: {
                Bezier curve{pts[-1], pts[0], pts[1], pts[2]};
                if (probe.convex && gpuEdgesCross(curve.start, curve.ctrl1, curve.ctrl2, curve.end)) probe.convex = false;

                auto stepCount = curve.segments();
                if (stepCount <= 1) stepCount = 2;
                auto subdivisionCount = static_cast<uint32_t>(stepCount);
                // Each subdivision adds one 2D vertex and at most one triangle.
                mBuffer->vertex.grow(subdivisionCount * 2u);
                mBuffer->index.grow(subdivisionCount * 3u);
                float step = 1.f / stepCount;
                auto curvePrevPt = prevPt;

                for (uint32_t s = 1; s <= subdivisionCount; ++s) {
                    auto pt = curve.at(step * s);
                    probe.addEdge(pt - curvePrevPt);
                    auto currIndex = pushVertex(pt.x, pt.y);
                    curvePrevPt = pt;
                    if (prevIndex == 0) { prevIndex = currIndex; continue; }
                    pushTriangle(firstIndex, prevIndex, currIndex);
                    prevIndex = currIndex;
                }
                prevPt = curve.end;
                pts += 3;
            } break;
            case PathCommand::Close: {
                finishContour();
            } break;
            default:
                break;
        }
    }

    finishContour();
    convex = probe.convex;
}


RenderRegion BWTessellator::bounds() const
{
    return {{int32_t(floor(bbox.min.x)), int32_t(floor(bbox.min.y))}, {int32_t(ceil(bbox.max.x)), int32_t(ceil(bbox.max.y))}};
}


uint32_t BWTessellator::pushVertex(float x, float y)
{
    auto index = _pushVertex(mBuffer->vertex, x, y);
    if (index == 0) bbox.max = bbox.min = {x, y};
    else bbox = {{std::min(bbox.min.x, x), std::min(bbox.min.y, y)}, {std::max(bbox.max.x, x), std::max(bbox.max.y, y)}};
    return index;
}


void BWTessellator::pushTriangle(uint32_t a, uint32_t b, uint32_t c)
{
    _pushTriangle(mBuffer->index, a, b, c);
}

}  // namespace tvg
