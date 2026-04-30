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
    array.push(x);
    array.push(y);
    return (array.count - 2) / 2;
}

Stroker::Stroker(GlGeometryBuffer* buffer, float width, StrokeCap cap, StrokeJoin join, float miterLimit, const Matrix* transform) : mBuffer(buffer), mTransform(transform), mWidth(width), mScale(transform ? gpuMaxScale(*transform) : 1.0f), mMiterLimit(miterLimit), mCap(cap), mJoin(join)
{
}


RenderRegion Stroker::bounds() const
{
    return {{int32_t(floor(mLeftTop.x)), int32_t(floor(mLeftTop.y))}, {int32_t(ceil(mRightBottom.x)), int32_t(ceil(mRightBottom.y))}};
}


uint32_t Stroker::pushVertex(const Point& p)
{
    auto pt = mTransform ? p * (*mTransform) : p;
    auto index = _pushVertex(mBuffer->vertex, pt.x, pt.y);
    if (index == 0) mLeftTop = mRightBottom = pt;
    else {
        mLeftTop.x = std::min(mLeftTop.x, pt.x);
        mLeftTop.y = std::min(mLeftTop.y, pt.y);
        mRightBottom.x = std::max(mRightBottom.x, pt.x);
        mRightBottom.y = std::max(mRightBottom.y, pt.y);
    }
    return index;
}


void Stroker::run(const RenderPath& path)
{
    mBuffer->vertex.reserve(path.pts.count * 4 + 16);
    mBuffer->index.reserve(path.pts.count * 3);

    auto validStrokeCap = false;
    auto pts = path.pts.data;

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                if (validStrokeCap) { // check this, so we can skip if path only contains move instruction
                    cap();
                    validStrokeCap = false;
                }
                mState.firstPt = *pts;
                mState.firstPtDir = {0.0f, 0.0f};
                mState.prevPt = *pts;
                mState.prevPtDir = {0.0f, 0.0f};
                pts++;
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
    normalize(dir);

    if (dir.x == 0.f && dir.y == 0.f) return;  //same point

    auto normal = Point{-dir.y, dir.x};
    auto a = mState.prevPt + normal * radius();
    auto b = mState.prevPt - normal * radius();
    auto c = curr + normal * radius();
    auto d = curr - normal * radius();

    auto ia = pushVertex(a);
    auto ib = pushVertex(b);
    auto ic = pushVertex(c);
    auto id = pushVertex(d);

    /**
     *   a --------- c
     *   |           |
     *   |           |
     *   b-----------d
     */

    mBuffer->index.push(ia);
    mBuffer->index.push(ib);
    mBuffer->index.push(ic);

    mBuffer->index.push(ib);
    mBuffer->index.push(id);
    mBuffer->index.push(ic);

    if (mState.prevPt == mState.firstPt) {
        // first point after moveTo
        mState.prevPt = curr;
        mState.prevPtDir = dir;
        mState.firstPtDir = dir;
    } else {
        join(dir);
        mState.prevPtDir = dir;
        mState.prevPt = curr;
    }

}


void Stroker::cubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    Bezier curve{ mState.prevPt, cnt1, cnt2, end };

    auto count = mTransform ? (curve * (*mTransform)).segments() : curve.segments();
    auto step = 1.f / count;

    for (uint32_t i = 0; i <= count; i++) {
        lineTo(curve.at(step * i));
    }
}


void Stroker::close()
{
    if (length(mState.prevPt - mState.firstPt) > 0.015625f) {
        lineTo(mState.firstPt);
    }

    // join firstPt with prevPt
    join(mState.firstPtDir);
}


void Stroker::join(const Point& dir)
{
    auto orient = orientation(mState.prevPt - mState.prevPtDir, mState.prevPt, mState.prevPt + dir);

    if (orient == Orientation::Linear) {
        if (mState.prevPtDir == dir) return;      // check is same direction
        if (mJoin != StrokeJoin::Round) return;   // opposite direction

        auto normal = Point{-dir.y, dir.x};
        auto p1 = mState.prevPt + normal * radius();
        auto p2 = mState.prevPt - normal * radius();
        auto oc = mState.prevPt + dir * radius();

        round(p1, oc, mState.prevPt);
        round(oc, p2, mState.prevPt);

    } else {
        auto normal = Point{-dir.y, dir.x};
        auto prevNormal = Point{-mState.prevPtDir.y, mState.prevPtDir.x};
        Point prevJoin, currJoin;

        if (orient == Orientation::CounterClockwise) {
            prevJoin = mState.prevPt + prevNormal * radius();
            currJoin = mState.prevPt + normal * radius();
        } else {
            prevJoin = mState.prevPt - prevNormal * radius();
            currJoin = mState.prevPt - normal * radius();
        }

        if (mJoin == StrokeJoin::Miter) miter(prevJoin, currJoin, mState.prevPt);
        else if (mJoin == StrokeJoin::Bevel) bevel(prevJoin, currJoin, mState.prevPt);
        else round(prevJoin, currJoin, mState.prevPt);
    }
}


void Stroker::round(const Point &prev, const Point& curr, const Point& center)
{
    auto orient = orientation(prev, center, curr);
    if (orient == Orientation::Linear) return;

    auto startAngle = tvg::atan2(prev.y - center.y, prev.x - center.x);
    auto endAngle = tvg::atan2(curr.y - center.y, curr.x - center.x);

    if (orient == Orientation::Clockwise) {
        if (endAngle > startAngle) endAngle -= 2 * MATH_PI;
    } else {
        if (endAngle < startAngle) endAngle += 2 * MATH_PI;
    }

    auto arcAngle = endAngle - startAngle;
    auto count = arcSegmentsCnt(arcAngle, radius() * mScale);

    auto c = pushVertex(center);
    auto pi = pushVertex(prev);
    auto step = (endAngle - startAngle) / (count - 1);

    for (uint32_t i = 1; i < static_cast<uint32_t>(count); i++) {
        auto angle = startAngle + step * i;
        Point out = {center.x + cos(angle) * radius(), center.y + sin(angle) * radius()};
        auto oi = pushVertex(out);

        mBuffer->index.push(c);
        mBuffer->index.push(pi);
        mBuffer->index.push(oi);

        pi = oi;
    }
}


void Stroker::roundPoint(const Point &p)
{
    auto count = arcSegmentsCnt(2.0f * MATH_PI, radius() * mScale);
    auto c = pushVertex(p);
    auto step = 2.0f * MATH_PI / (count - 1);

    for (uint32_t i = 1; i <= static_cast<uint32_t>(count); i++) {
        float angle = i * step;
        Point dir = {cos(angle), sin(angle)};
        Point out = p + dir * radius();
        auto oi = pushVertex(out);

        if (oi > 1) {
            mBuffer->index.push(c);
            mBuffer->index.push(oi);
            mBuffer->index.push(oi - 1);
        }
    }
}


void Stroker::miter(const Point& prev, const Point& curr, const Point& center)
{
    auto pp1 = prev - center;
    auto pp2 = curr - center;
    auto out = pp1 + pp2;
    auto k = 2.f * radius() * radius() / (out.x * out.x + out.y * out.y);
    auto pe = out * k;

    if (length(pe) >= mMiterLimit * radius()) {
        bevel(prev, curr, center);
        return;
    }

    auto join = center + pe;
    auto c = pushVertex(center);
    auto cp1 = pushVertex(prev);
    auto cp2 = pushVertex(curr);
    auto e = pushVertex(join);

    mBuffer->index.push(c);
    mBuffer->index.push(cp1);
    mBuffer->index.push(e);

    mBuffer->index.push(e);
    mBuffer->index.push(cp2);
    mBuffer->index.push(c);
}


void Stroker::bevel(const Point& prev, const Point& curr, const Point& center)
{
    auto a = pushVertex(prev);
    auto b = pushVertex(curr);
    auto c = pushVertex(center);

    mBuffer->index.push(a);
    mBuffer->index.push(b);
    mBuffer->index.push(c);
}


void Stroker::square(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};

    auto a = p + normal * radius();
    auto b = p - normal * radius();
    auto c = a + outDir * radius();
    auto d = b + outDir * radius();

    auto ai = pushVertex(a);
    auto bi = pushVertex(b);
    auto ci = pushVertex(c);
    auto di = pushVertex(d);

    mBuffer->index.push(ai);
    mBuffer->index.push(bi);
    mBuffer->index.push(ci);

    mBuffer->index.push(ci);
    mBuffer->index.push(bi);
    mBuffer->index.push(di);

}


void Stroker::squarePoint(const Point& p)
{
    auto offsetX = Point{radius(), 0.0f};
    auto offsetY = Point{0.0f, radius()};

    auto a = p + offsetX + offsetY;
    auto b = p - offsetX + offsetY;
    auto c = p - offsetX - offsetY;
    auto d = p + offsetX - offsetY;

    auto ai = pushVertex(a);
    auto bi = pushVertex(b);
    auto ci = pushVertex(c);
    auto di = pushVertex(d);

    mBuffer->index.push(ai);
    mBuffer->index.push(bi);
    mBuffer->index.push(ci);

    mBuffer->index.push(ci);
    mBuffer->index.push(di);
    mBuffer->index.push(ai);

}


void Stroker::round(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};
    auto a = p + normal * radius();
    auto b = p - normal * radius();
    auto c = p + outDir * radius();

    round(a, c, p);
    round(c, b, p);
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
                float step = 1.f / stepCount;
                auto curvePrevPt = prevPt;

                for (uint32_t s = 1; s <= static_cast<uint32_t>(stepCount); s++) {
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
    mBuffer->index.push(a);
    mBuffer->index.push(b);
    mBuffer->index.push(c);
}

}  // namespace tvg
