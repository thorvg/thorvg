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

Stroker::Stroker(GlGeometryBuffer* buffer, float width, StrokeCap cap, StrokeJoin join, float miterLimit, float qualityScale, bool trimJoin) : mBuffer(buffer), mWidth(width), mMiterLimit(miterLimit), mQualityScale(qualityScale), mCap(cap), mJoin(join), mTrimJoin(trimJoin)
{
}


RenderRegion Stroker::bounds() const
{
    return {{int32_t(floor(mLeftTop.x)), int32_t(floor(mLeftTop.y))}, {int32_t(ceil(mRightBottom.x)), int32_t(ceil(mRightBottom.y))}};
}


void Stroker::run(const RenderPath& path)
{
    mContour.reserve(path.pts.count);
    mBuffer->vertex.reserve(path.pts.count * 8 + 16);
    mBuffer->index.reserve(path.pts.count * 6);

    auto validStrokeCap = false;
    auto pts = path.pts.data;

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                if (validStrokeCap) tessellate(false);
                mContour.clear();
                mContour.push({*pts++});
                validStrokeCap = false;
            } break;
            case PathCommand::LineTo: {
                validStrokeCap = true;
                lineTo(*pts++);
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
    if (validStrokeCap) tessellate(false);
}


void Stroker::cap()
{
    if (mCap == StrokeCap::Butt) return;

    auto& first = mContour.first();
    auto& last = mContour.last();
    if (first.pt == last.pt) {
        if (mCap == StrokeCap::Square) squarePoint(first.pt);
        else roundPoint(first.pt);
    } else {
        auto& lastDir = mContour[mContour.count - 2].dir;
        if (mCap == StrokeCap::Square) {
            square(first.pt, -first.dir);
            square(last.pt, lastDir);
        } else {
            round(first.pt, -first.dir);
            round(last.pt, lastDir);
        }
    }
}


void Stroker::lineTo(const Point& curr)
{
    if (tvg::zero(curr - mContour.last().pt)) return;
    mContour.push({curr});
}


void Stroker::cubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    Bezier curve{mContour.last().pt, cnt1, cnt2, end};
    auto count = curve.segments(mQualityScale);
    mContour.grow(count);
    auto step = 1.f / count;

    for (uint32_t i = 1; i <= count; ++i) {
        lineTo(curve.at(step * i));
    }
}


void Stroker::close()
{
    if (mContour.count < 2) return;

    auto first = mContour.first().pt;
    auto delta = mContour.last().pt - first;
    if (dot(delta, delta) > 0.015625f * 0.015625f) lineTo(first);
    tessellate(true);

    mContour.clear();
    mContour.push({first});
}


void Stroker::tessellate(bool closed)
{
    auto end = mContour.count - 1;
    if (end == 0) {
        if (!closed) cap();
        return;
    }

    // Prepare the complete contour before emitting any triangles.
    for (uint32_t i = 0; i < end; ++i) {
        auto& vertex = mContour[i];
        vertex.dir = mContour[i + 1].pt - vertex.pt;
        vertex.length = length(vertex.dir);
        vertex.dir *= 1.0f / vertex.length;
        vertex.inner = vertex.pt;
        vertex.side = 0;
    }
    auto& last = mContour[end];
    last.inner = last.pt;
    last.side = 0;
    if (closed) {
        last.dir = mContour[0].dir;
        last.length = mContour[0].length;
    }
    // A skipped near-closing edge does not share an exact endpoint.
    auto connected = closed && last.pt.x == mContour[0].pt.x && last.pt.y == mContour[0].pt.y;
    auto joinCount = closed ? end : end - 1;
    if (mTrimJoin) {
        for (uint32_t i = 1; i <= joinCount; ++i) {
            if (i == end && !connected) continue;
            prepareJoin(mContour[i - 1], mContour[i]);
        }
    }

    auto vertexCount = end * 8u;
    auto indexCount = end * 6u;
    if (mJoin == StrokeJoin::Bevel) {
        vertexCount += joinCount * 6u;
        indexCount += joinCount * 3u;
    } else if (mJoin == StrokeJoin::Miter) {
        vertexCount += joinCount * 8u;
        indexCount += joinCount * 6u;
    }
    mBuffer->vertex.grow(vertexCount);
    mBuffer->index.grow(indexCount);

    // Each segment reads both prepared joins; no emitted vertex is revised.
    for (uint32_t i = 0; i < end; ++i) {
        auto& startJoin = (i == 0 && connected) ? last : mContour[i];
        segment(mContour[i], mContour[i + 1], startJoin);
        if (i < joinCount) join(mContour[i], mContour[i + 1]);
    }
    if (!closed) cap();
}


void Stroker::prepareJoin(const Vertex& prev, Vertex& curr)
{
    auto turn = cross(prev.dir, curr.dir);
    auto denom = 1.0f + dot(prev.dir, curr.dir);
    if (tvg::zero(turn) || denom <= FLOAT_EPSILON) return;

    auto setback = radius() * fabsf(turn) / denom;
    auto scale = std::max(std::max(fabsf(curr.pt.x), fabsf(curr.pt.y)), std::max(prev.length, curr.length));
    // Confine each cut to its half-segment, independently of neighboring joins.
    if (!(2.0f * setback + FLOAT_EPSILON * scale < std::min(prev.length, curr.length))) return;

    auto normal = Point{-prev.dir.y - curr.dir.y, prev.dir.x + curr.dir.x};
    auto inner = curr.pt + normal * ((turn > 0.0f ? radius() : -radius()) / denom);
    if (!std::isfinite(inner.x) || !std::isfinite(inner.y)) return;
    curr.inner = inner;
    curr.side = turn > 0.0f ? 1 : -1;
}


void Stroker::segment(const Vertex& prev, const Vertex& curr, const Vertex& startJoin)
{
    auto normal = Point{-prev.dir.y, prev.dir.x} * radius();
    auto a = startJoin.side > 0 ? startJoin.inner : prev.pt + normal;
    auto b = startJoin.side < 0 ? startJoin.inner : prev.pt - normal;
    auto c = curr.side > 0 ? curr.inner : curr.pt + normal;
    auto d = curr.side < 0 ? curr.inner : curr.pt - normal;

    auto ia = _pushVertex(mBuffer->vertex, a.x, a.y);
    auto ib = _pushVertex(mBuffer->vertex, b.x, b.y);
    auto ic = _pushVertex(mBuffer->vertex, c.x, c.y);
    auto id = _pushVertex(mBuffer->vertex, d.x, d.y);

    _pushTriangle(mBuffer->index, ia, ib, ic);
    _pushTriangle(mBuffer->index, ib, id, ic);

    if (ia == 0) {
        mRightBottom = mLeftTop = prev.pt;
    }
    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::join(const Vertex& prev, const Vertex& curr)
{
    auto turn = cross(prev.dir, curr.dir);

    if (tvg::zero(turn)) {
        if (prev.dir == curr.dir) return;       // same direction
        if (mJoin != StrokeJoin::Round) return; // opposite direction

        auto normal = Point{-curr.dir.y, curr.dir.x} * radius();
        auto p1 = curr.pt + normal;
        auto p2 = curr.pt - normal;
        auto oc = curr.pt + curr.dir * radius();

        round(p1, oc, curr.pt, curr.pt);
        round(oc, p2, curr.pt, curr.pt);
    } else {
        auto normal = Point{-curr.dir.y, curr.dir.x};
        auto prevNormal = Point{-prev.dir.y, prev.dir.x};
        auto offset = turn < 0.0f ? radius() : -radius();
        auto prevJoin = curr.pt + prevNormal * offset;
        auto currJoin = curr.pt + normal * offset;

        if (mJoin == StrokeJoin::Miter) miter(prevJoin, currJoin, curr.pt, curr.inner);
        else if (mJoin == StrokeJoin::Bevel) bevel(prevJoin, currJoin, curr.inner);
        else round(prevJoin, currJoin, curr.pt, curr.inner);
    }
}


void Stroker::round(const Point &prev, const Point& curr, const Point& center, const Point& apex)
{
    auto turn = cross(center - prev, curr - prev);
    // The cross product scales with radius squared; small arcs still need a fan.
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
