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

static uint32_t _pushVertex(Array<Point>& array, const Point& point)
{
    array.grow(1);
    array.data[array.count] = point;
    return array.count++;
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
    return {int32_t(floor(mLeftTop.x)), int32_t(floor(mLeftTop.y)), int32_t(ceil(mRightBottom.x)), int32_t(ceil(mRightBottom.y))};
}


void Stroker::run(const RenderPath& path, bool thinFill)
{
    mThinFill = thinFill;
    overlapFree = !thinFill;
    mProbe = {};
    mDirections = 0;
    mBuffer->vertex.reserve(path.pts.count * 4 + 8);
    mBuffer->index.reserve(path.pts.count * 6);

    auto validStrokeCap = false;
    auto pts = path.pts.data;

    for (auto& cmd : path.cmds) {
        switch (cmd) {
            case PathCommand::MoveTo: {
                if (validStrokeCap) { // check this, so we can skip if path only contains move instruction
                    cap();
                    validStrokeCap = false;
                }
                mState.firstLength = 0.0f;  // The first segment initializes the remaining edge state.
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
    // Open contours monotone in either axis may turn both ways. Half-segment cuts
    // keep joins independent, and their forward boundaries separate the caps.
    auto monotone = (mDirections & 3) != 3 || (mDirections & 12) != 12;
    overlapFree = overlapFree && mState.firstLength > 0.0f && (mCap == StrokeCap::Butt || mState.firstPt != mState.prevPt) &&
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

    auto ia = _pushVertex(mBuffer->vertex, a);
    auto ib = _pushVertex(mBuffer->vertex, b);
    auto ic = _pushVertex(mBuffer->vertex, c);
    auto id = _pushVertex(mBuffer->vertex, d);

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
    auto vertexCount = count * 4u;
    auto indexCount = count * 6u;
    if (mJoin == StrokeJoin::Bevel) {
        vertexCount += joinCount * 3u;
        indexCount += joinCount * 3u;
    } else if (mJoin == StrokeJoin::Miter) {
        vertexCount += joinCount * 4u;
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
    mState.firstLength = 0.0f;
    mState.prevPt = mState.firstPt;
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
        auto cosine = dot(mState.prevPtDir, dir);
        auto denom = 1.0f + cosine;
        auto trimmed = false;

        if (!mThinFill && denom > FLOAT_EPSILON) {
            // Choose the half-angle ratio that avoids cancellation near a reversal.
            auto setback = radius() * (cosine < 0.0f ? (1.0f - cosine) / fabsf(turn) : fabsf(turn) / denom);
            auto scale = std::max(std::max(fabsf(apex.x), fabsf(apex.y)), std::max(radius(), std::max(mState.prevLength, len)));
            // Keep each cut within its half-segment so neighboring cuts and caps stay independent.
            if (2.0f * setback + FLOAT_EPSILON * scale < std::min(mState.prevLength, len)) {
                auto inner = apex - mState.prevPtDir * setback - prevNormal * offset;
                auto side = turn < 0.0f ? 1u : 0u;
                auto vertices = mBuffer->vertex.data;
                vertices[mState.prevIndex + 2 + side] = vertices[index + side] = inner;
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
    auto count = std::max(2u, gpuArcSegmentsCnt(arcAngle, radius() * mQualityScale));

    auto c = _pushVertex(mBuffer->vertex, apex);
    auto pi = _pushVertex(mBuffer->vertex, prev);
    auto step = (endAngle - startAngle) / (count - 1);

    for (uint32_t i = 1; i < count; i++) {
        auto angle = startAngle + step * i;
        // Keep the shared edge exact so adjacent arcs and segments do not overlap.
        Point out = (i + 1 == count) ? curr : Point{center.x + cos(angle) * radius(), center.y + sin(angle) * radius()};
        auto oi = _pushVertex(mBuffer->vertex, out);

        _pushTriangle(mBuffer->index, c, pi, oi);

        pi = oi;

        mLeftTop.x = std::min(mLeftTop.x, out.x);
        mLeftTop.y = std::min(mLeftTop.y, out.y);
        mRightBottom.x = std::max(mRightBottom.x, out.x);
        mRightBottom.y = std::max(mRightBottom.y, out.y);
    }
}


void Stroker::roundPoint(const Point &p)
{
    auto count = gpuArcSegmentsCnt(2.0f * MATH_PI, radius() * mQualityScale);
    auto c = _pushVertex(mBuffer->vertex, p);
    auto step = 2.0f * MATH_PI / (count - 1);

    for (uint32_t i = 1; i <= static_cast<uint32_t>(count); i++) {
        float angle = i * step;
        Point dir = {cos(angle), sin(angle)};
        Point out = p + dir * radius();
        auto oi = _pushVertex(mBuffer->vertex, out);

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
    auto c = _pushVertex(mBuffer->vertex, apex);
    auto cp1 = _pushVertex(mBuffer->vertex, prev);
    auto cp2 = _pushVertex(mBuffer->vertex, curr);
    auto e = _pushVertex(mBuffer->vertex, join);

    _pushTriangle(mBuffer->index, c, cp1, e);
    _pushTriangle(mBuffer->index, e, cp2, c);

    mLeftTop.x = std::min(mLeftTop.x, join.x);
    mLeftTop.y = std::min(mLeftTop.y, join.y);

    mRightBottom.x = std::max(mRightBottom.x, join.x);
    mRightBottom.y = std::max(mRightBottom.y, join.y);
}


void Stroker::bevel(const Point& prev, const Point& curr, const Point& apex)
{
    auto a = _pushVertex(mBuffer->vertex, prev);
    auto b = _pushVertex(mBuffer->vertex, curr);
    auto c = _pushVertex(mBuffer->vertex, apex);

    _pushTriangle(mBuffer->index, a, b, c);
}


void Stroker::square(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};

    auto a = p + normal * radius();
    auto b = p - normal * radius();
    auto c = a + outDir * radius();
    auto d = b + outDir * radius();

    auto ai = _pushVertex(mBuffer->vertex, a);
    auto bi = _pushVertex(mBuffer->vertex, b);
    auto ci = _pushVertex(mBuffer->vertex, c);
    auto di = _pushVertex(mBuffer->vertex, d);

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

    auto ai = _pushVertex(mBuffer->vertex, a);
    auto bi = _pushVertex(mBuffer->vertex, b);
    auto ci = _pushVertex(mBuffer->vertex, c);
    auto di = _pushVertex(mBuffer->vertex, d);

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

}  // namespace tvg
