/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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


Stroker::Stroker(GlGeometryBuffer* buffer, const Matrix& matrix) : mBuffer(buffer), mMatrix(matrix)
{
}


void Stroker::stroke(const RenderShape *rshape)
{
    if (isinf(mMatrix.e11)) {
        auto strokeWidth = rshape->strokeWidth() * scaling(mMatrix);
        if (strokeWidth <= MIN_GL_STROKE_WIDTH) strokeWidth = MIN_GL_STROKE_WIDTH;
        mStrokeWidth = strokeWidth / mMatrix.e11;
    } else {
        mStrokeWidth = rshape->strokeWidth();
    }

    if (tvg::zero(mStrokeWidth)) return;  //invalid stroke

    mMiterLimit = rshape->strokeMiterlimit();
    mStrokeCap = rshape->strokeCap();
    mStrokeJoin = rshape->strokeJoin();

    RenderPath dashed;
    if (rshape->strokeDash(dashed)) doStroke(dashed);
    else if (rshape->trimpath()) {
        RenderPath trimmedPath;
        if (rshape->stroke->trim.trim(rshape->path, trimmedPath)) doStroke(trimmedPath);
    } else doStroke(rshape->path);
}


RenderRegion Stroker::bounds() const
{
    return {{int32_t(floor(mLeftTop.x)), int32_t(floor(mLeftTop.y))}, {int32_t(ceil(mRightBottom.x)), int32_t(ceil(mRightBottom.y))}};
}


void Stroker::doStroke(const RenderPath& path)
{
    mBuffer->vertex.reserve(path.pts.count * 4 + 16);
    mBuffer->index.reserve(path.pts.count * 3);

    auto validStrokeCap = false;
    auto pts = path.pts.data;

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                if (validStrokeCap) { // check this, so we can skip if path only contains move instruction
                    strokeCap();
                    validStrokeCap = false;
                }
                mStrokeState.firstPt = *pts;
                mStrokeState.firstPtDir = {0.0f, 0.0f};
                mStrokeState.prevPt = *pts;
                mStrokeState.prevPtDir = {0.0f, 0.0f};
                pts++;
                validStrokeCap = false;
            } break;
            case PathCommand::LineTo: {
                validStrokeCap = true;
                this->strokeLineTo(*pts);
                pts++;
            } break;
            case PathCommand::CubicTo: {
                validStrokeCap = true;
                this->strokeCubicTo(pts[0], pts[1], pts[2]);
                pts += 3;
            } break;
            case PathCommand::Close: {
                this->strokeClose();

                validStrokeCap = false;
            } break;
            default:
                break;
        }
    }
    if (validStrokeCap) strokeCap();
}


void Stroker::strokeCap()
{
    if (mStrokeCap == StrokeCap::Butt) return;

    if (mStrokeCap == StrokeCap::Square) {
        if (mStrokeState.firstPt == mStrokeState.prevPt) strokeSquarePoint(mStrokeState.firstPt);
        else {
            strokeSquare(mStrokeState.firstPt, {-mStrokeState.firstPtDir.x, -mStrokeState.firstPtDir.y});
            strokeSquare(mStrokeState.prevPt, mStrokeState.prevPtDir);
        }
    } else if (mStrokeCap == StrokeCap::Round) {
        if (mStrokeState.firstPt == mStrokeState.prevPt) strokeRoundPoint(mStrokeState.firstPt);
        else {
            strokeRound(mStrokeState.firstPt, {-mStrokeState.firstPtDir.x, -mStrokeState.firstPtDir.y});
            strokeRound(mStrokeState.prevPt, mStrokeState.prevPtDir);
        }
    }
}


void Stroker::strokeLineTo(const Point& curr)
{
    auto dir = (curr - mStrokeState.prevPt);
    normalize(dir);

    if (dir.x == 0.f && dir.y == 0.f) return;  //same point

    auto normal = Point{-dir.y, dir.x};
    auto a = mStrokeState.prevPt + normal * strokeRadius();
    auto b = mStrokeState.prevPt - normal * strokeRadius();
    auto c = curr + normal * strokeRadius();
    auto d = curr - normal * strokeRadius();

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

    this->mBuffer->index.push(ia);
    this->mBuffer->index.push(ib);
    this->mBuffer->index.push(ic);

    this->mBuffer->index.push(ib);
    this->mBuffer->index.push(id);
    this->mBuffer->index.push(ic);

    if (mStrokeState.prevPt == mStrokeState.firstPt) {
        // first point after moveTo
        mStrokeState.prevPt = curr;
        mStrokeState.prevPtDir = dir;
        mStrokeState.firstPtDir = dir;
    } else {
        this->strokeJoin(dir);
        mStrokeState.prevPtDir = dir;
        mStrokeState.prevPt = curr;
    }

    if (ia == 0) {
        mRightBottom.x = mLeftTop.x = curr.x;
        mRightBottom.y = mLeftTop.y = curr.y;
    }

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::strokeCubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    Bezier curve{ mStrokeState.prevPt, cnt1, cnt2, end };

    auto count = (curve * mMatrix).segments();
    auto step = 1.f / count;

    for (uint32_t i = 0; i <= count; i++) {
        strokeLineTo(curve.at(step * i));
    }
}


void Stroker::strokeClose()
{
    if (length(mStrokeState.prevPt - mStrokeState.firstPt) > 0.015625f) {
        this->strokeLineTo(mStrokeState.firstPt);
    }

    // join firstPt with prevPt
    this->strokeJoin(mStrokeState.firstPtDir);
}


void Stroker::strokeJoin(const Point& dir)
{
    auto orient = orientation(mStrokeState.prevPt - mStrokeState.prevPtDir, mStrokeState.prevPt, mStrokeState.prevPt + dir);

    if (orient == Orientation::Linear) {
        if (mStrokeState.prevPtDir == dir) return;      // check is same direction
        if (mStrokeJoin != StrokeJoin::Round) return;   // opposite direction

        auto normal = Point{-dir.y, dir.x};
        auto p1 = mStrokeState.prevPt + normal * strokeRadius();
        auto p2 = mStrokeState.prevPt - normal * strokeRadius();
        auto oc = mStrokeState.prevPt + dir * strokeRadius();

        this->strokeRound(p1, oc, mStrokeState.prevPt);
        this->strokeRound(oc, p2, mStrokeState.prevPt);

    } else {
        auto normal = Point{-dir.y, dir.x};
        auto prevNormal = Point{-mStrokeState.prevPtDir.y, mStrokeState.prevPtDir.x};
        Point prevJoin, currJoin;

        if (orient == Orientation::CounterClockwise) {
            prevJoin = mStrokeState.prevPt + prevNormal * strokeRadius();
            currJoin = mStrokeState.prevPt + normal * strokeRadius();
        } else {
            prevJoin = mStrokeState.prevPt - prevNormal * strokeRadius();
            currJoin = mStrokeState.prevPt - normal * strokeRadius();
        }

        if (mStrokeJoin == StrokeJoin::Miter) strokeMiter(prevJoin, currJoin, mStrokeState.prevPt);
        else if (mStrokeJoin == StrokeJoin::Bevel) strokeBevel(prevJoin, currJoin, mStrokeState.prevPt);
        else this->strokeRound(prevJoin, currJoin, mStrokeState.prevPt);
    }
}


void Stroker::strokeRound(const Point &prev, const Point& curr, const Point& center)
{
    if (orientation(prev, center, curr) == Orientation::Linear) return;

    mLeftTop.x = std::min(mLeftTop.x, std::min(center.x, std::min(prev.x, curr.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(center.y, std::min(prev.y, curr.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(center.x, std::max(prev.x, curr.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(center.y, std::max(prev.y, curr.y)));

    // Fixme: just use bezier curve to calculate step count
    auto count = Bezier(prev, curr, strokeRadius()).segments();
    auto c = _pushVertex(mBuffer->vertex, center.x, center.y);
    auto pi = _pushVertex(mBuffer->vertex, prev.x, prev.y);
    auto step = 1.f / (count - 1);
    auto dir = curr - prev;

    for (uint32_t i = 1; i < static_cast<uint32_t>(count); i++) {
        auto t = i * step;
        auto p = prev + dir * t;
        auto o_dir = p - center;
        normalize(o_dir);

        auto out = center + o_dir * strokeRadius();
        auto oi = _pushVertex(mBuffer->vertex, out.x, out.y);

        mBuffer->index.push(c);
        mBuffer->index.push(pi);
        mBuffer->index.push(oi);

        pi = oi;

        mLeftTop.x = std::min(mLeftTop.x, out.x);
        mLeftTop.y = std::min(mLeftTop.y, out.y);
        mRightBottom.x = std::max(mRightBottom.x, out.x);
        mRightBottom.y = std::max(mRightBottom.y, out.y);
    }
}


void Stroker::strokeRoundPoint(const Point &p)
{
    // Fixme: just use bezier curve to calculate step count
    auto count = Bezier(p, p, strokeRadius()).segments() * 2;
    auto c = _pushVertex(mBuffer->vertex, p.x, p.y);
    auto step = 2 * MATH_PI / (count - 1);

    for (uint32_t i = 1; i <= static_cast<uint32_t>(count); i++) {
        float angle = i * step;
        Point dir = {cos(angle), sin(angle)};
        Point out = p + dir * strokeRadius();
        auto oi = _pushVertex(mBuffer->vertex, out.x, out.y);

        if (oi > 1) {
            mBuffer->index.push(c);
            mBuffer->index.push(oi);
            mBuffer->index.push(oi - 1);
        }
    }

    mLeftTop.x = std::min(mLeftTop.x, p.x - strokeRadius());
    mLeftTop.y = std::min(mLeftTop.y, p.y - strokeRadius());
    mRightBottom.x = std::max(mRightBottom.x, p.x + strokeRadius());
    mRightBottom.y = std::max(mRightBottom.y, p.y + strokeRadius());
}


void Stroker::strokeMiter(const Point& prev, const Point& curr, const Point& center)
{
    auto pp1 = prev - center;
    auto pp2 = curr - center;
    auto out = pp1 + pp2;
    auto k = 2.f * strokeRadius() * strokeRadius() / (out.x * out.x + out.y * out.y);
    auto pe = out * k;

    if (length(pe) >= mMiterLimit * strokeRadius()) {
        this->strokeBevel(prev, curr, center);
        return;
    }

    auto join = center + pe;
    auto c = _pushVertex(mBuffer->vertex, center.x, center.y);
    auto cp1 = _pushVertex(mBuffer->vertex, prev.x, prev.y);
    auto cp2 = _pushVertex(mBuffer->vertex, curr.x, curr.y);
    auto e = _pushVertex(mBuffer->vertex, join.x, join.y);

    mBuffer->index.push(c);
    mBuffer->index.push(cp1);
    mBuffer->index.push(e);

    mBuffer->index.push(e);
    mBuffer->index.push(cp2);
    mBuffer->index.push(c);

    mLeftTop.x = std::min(mLeftTop.x, join.x);
    mLeftTop.y = std::min(mLeftTop.y, join.y);

    mRightBottom.x = std::max(mRightBottom.x, join.x);
    mRightBottom.y = std::max(mRightBottom.y, join.y);
}


void Stroker::strokeBevel(const Point& prev, const Point& curr, const Point& center)
{
    auto a = _pushVertex(mBuffer->vertex, prev.x, prev.y);
    auto b = _pushVertex(mBuffer->vertex, curr.x, curr.y);
    auto c = _pushVertex(mBuffer->vertex, center.x, center.y);

    mBuffer->index.push(a);
    mBuffer->index.push(b);
    mBuffer->index.push(c);
}


void Stroker::strokeSquare(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};

    auto a = p + normal * strokeRadius();
    auto b = p - normal * strokeRadius();
    auto c = a + outDir * strokeRadius();
    auto d = b + outDir * strokeRadius();

    auto ai = _pushVertex(mBuffer->vertex, a.x, a.y);
    auto bi = _pushVertex(mBuffer->vertex, b.x, b.y);
    auto ci = _pushVertex(mBuffer->vertex, c.x, c.y);
    auto di = _pushVertex(mBuffer->vertex, d.x, d.y);

    mBuffer->index.push(ai);
    mBuffer->index.push(bi);
    mBuffer->index.push(ci);

    mBuffer->index.push(ci);
    mBuffer->index.push(bi);
    mBuffer->index.push(di);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::strokeSquarePoint(const Point& p)
{
    auto offsetX = Point{strokeRadius(), 0.0f};
    auto offsetY = Point{0.0f, strokeRadius()};

    auto a = p + offsetX + offsetY;
    auto b = p - offsetX + offsetY;
    auto c = p - offsetX - offsetY;
    auto d = p + offsetX - offsetY;

    auto ai = _pushVertex(mBuffer->vertex, a.x, a.y);
    auto bi = _pushVertex(mBuffer->vertex, b.x, b.y);
    auto ci = _pushVertex(mBuffer->vertex, c.x, c.y);
    auto di = _pushVertex(mBuffer->vertex, d.x, d.y);

    mBuffer->index.push(ai);
    mBuffer->index.push(bi);
    mBuffer->index.push(ci);

    mBuffer->index.push(ci);
    mBuffer->index.push(di);
    mBuffer->index.push(ai);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::strokeRound(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};
    auto a = p + normal * strokeRadius();
    auto b = p - normal * strokeRadius();
    auto c = p + outDir * strokeRadius();

    strokeRound(a, c, p);
    strokeRound(c, b, p);
}


BWTessellator::BWTessellator(GlGeometryBuffer* buffer): mBuffer(buffer)
{
}


void BWTessellator::tessellate(const RenderPath& path, const Matrix& matrix)
{
    auto cmds = path.cmds.data;
    auto cmdCnt = path.cmds.count;
    auto pts = path.pts.data;
    auto ptsCnt = path.pts.count;

    if (ptsCnt <= 2) return;

    uint32_t firstIndex = 0;
    uint32_t prevIndex = 0;

    mBuffer->vertex.reserve(ptsCnt * 2);
    mBuffer->index.reserve((ptsCnt - 2) * 3);

    for (uint32_t i = 0; i < cmdCnt; i++) {
        switch(cmds[i]) {
            case PathCommand::MoveTo: {
                firstIndex = pushVertex(pts->x, pts->y);
                prevIndex = 0;
                pts++;
            } break;
            case PathCommand::LineTo: {
                if (prevIndex == 0) {
                    prevIndex = pushVertex(pts->x, pts->y);
                    pts++;
                } else {
                    auto currIndex = pushVertex(pts->x, pts->y);
                    pushTriangle(firstIndex, prevIndex, currIndex);
                    prevIndex = currIndex;
                    pts++;
                }
            } break;
            case PathCommand::CubicTo: {
                Bezier curve{pts[-1], pts[0], pts[1], pts[2]};

                auto stepCount = (curve * matrix).segments();
                if (stepCount <= 1) stepCount = 2;

                float step = 1.f / stepCount;

                for (uint32_t s = 1; s <= static_cast<uint32_t>(stepCount); s++) {
                    auto pt = curve.at(step * s);
                    auto currIndex = pushVertex(pt.x, pt.y);

                    if (prevIndex == 0) {
                        prevIndex = currIndex;
                        continue;
                    }

                    pushTriangle(firstIndex, prevIndex, currIndex);
                    prevIndex = currIndex;
                }

                pts += 3;
            } break;
            case PathCommand::Close:
            default:
                break;
        }
    }
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
