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

#include "tvgWgTessellator.h"
#include "tvgMath.h"


WgStroker::WgStroker(WgMeshData* buffer, float width) : mBuffer(buffer), mWidth(width)
{
}


void WgStroker::run(const RenderShape& rshape, const Matrix& m)
{
    mMiterLimit = rshape.strokeMiterlimit();
    mCap = rshape.strokeCap();
    mJoin = rshape.strokeJoin();

    RenderPath dashed;
    if (rshape.strokeDash(dashed)) run(dashed, m);
    else if (rshape.trimpath()) {
        RenderPath trimmedPath;
        if (rshape.stroke->trim.trim(rshape.path, trimmedPath)) {
            run(trimmedPath, m);
        }
    } else {
        run(rshape.path, m);
    }
}


RenderRegion WgStroker::bounds() const
{
    return {{int32_t(floor(mLeftTop.x)), int32_t(floor(mLeftTop.y))}, {int32_t(ceil(mRightBottom.x)), int32_t(ceil(mRightBottom.y))}};
}


BBox WgStroker::getBBox() const
{
    return {mLeftTop, mRightBottom};
}

void WgStroker::run(const RenderPath& path, const Matrix& m)
{
    mBuffer->vbuffer.reserve(path.pts.count * 4 + 16);
    mBuffer->ibuffer.reserve(path.pts.count * 3);

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
                this->lineTo(*pts);
                pts++;
            } break;
            case PathCommand::CubicTo: {
                validStrokeCap = true;
                this->cubicTo(pts[0], pts[1], pts[2], m);
                pts += 3;
            } break;
            case PathCommand::Close: {
                this->close();

                validStrokeCap = false;
            } break;
            default:
                break;
        }
    }
    if (validStrokeCap) cap();
}


void WgStroker::cap()
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


void WgStroker::lineTo(const Point& curr)
{
    auto dir = (curr - mState.prevPt);
    normalize(dir);

    if (dir.x == 0.f && dir.y == 0.f) return;  //same point

    auto normal = Point{-dir.y, dir.x};
    auto a = mState.prevPt + normal * radius();
    auto b = mState.prevPt - normal * radius();
    auto c = curr + normal * radius();
    auto d = curr - normal * radius();

    auto ia = mBuffer->vbuffer.count; mBuffer->vbuffer.push(a);
    auto ib = mBuffer->vbuffer.count; mBuffer->vbuffer.push(b);
    auto ic = mBuffer->vbuffer.count; mBuffer->vbuffer.push(c);
    auto id = mBuffer->vbuffer.count; mBuffer->vbuffer.push(d);

    /**
     *   a --------- c
     *   |           |
     *   |           |
     *   b-----------d
     */

    mBuffer->ibuffer.push(ia);
    mBuffer->ibuffer.push(ib);
    mBuffer->ibuffer.push(ic);
    mBuffer->ibuffer.push(ib);
    mBuffer->ibuffer.push(id);
    mBuffer->ibuffer.push(ic);

    if (mState.prevPt == mState.firstPt) {
        // first point after moveTo
        mState.prevPt = curr;
        mState.prevPtDir = dir;
        mState.firstPtDir = dir;
    } else {
        this->join(dir);
        mState.prevPtDir = dir;
        mState.prevPt = curr;
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


void WgStroker::cubicTo(const Point& cnt1, const Point& cnt2, const Point& end, const Matrix& m)
{
    Bezier curve {mState.prevPt, cnt1, cnt2, end};

    Bezier relCurve {curve.start, curve.ctrl1, curve.ctrl2, curve.end};
    relCurve.start *= m;
    relCurve.ctrl1 *= m;
    relCurve.ctrl2 *= m;
    relCurve.end *= m;

    auto count = relCurve.segments();
    auto step = 1.f / count;

    for (uint32_t i = 0; i <= count; i++) {
        lineTo(curve.at(step * i));
    }
}


void WgStroker::close()
{
    if (length(mState.prevPt - mState.firstPt) > 0.015625f) {
        this->lineTo(mState.firstPt);
    }

    // join firstPt with prevPt
    this->join(mState.firstPtDir);
}


void WgStroker::join(const Point& dir)
{
    auto orient = orientation(mState.prevPt - mState.prevPtDir, mState.prevPt, mState.prevPt + dir);

    if (orient == Orientation::Linear) {
        if (mState.prevPtDir == dir) return;      // check is same direction
        if (mJoin != StrokeJoin::Round) return;   // opposite direction

        auto normal = Point{-dir.y, dir.x};
        auto p1 = mState.prevPt + normal * radius();
        auto p2 = mState.prevPt - normal * radius();
        auto oc = mState.prevPt + dir * radius();

        this->round(p1, oc, mState.prevPt);
        this->round(oc, p2, mState.prevPt);

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
        else this->round(prevJoin, currJoin, mState.prevPt);
    }
}


void WgStroker::round(const Point &prev, const Point& curr, const Point& center)
{
    if (orientation(prev, center, curr) == Orientation::Linear) return;

    mLeftTop.x = std::min(mLeftTop.x, std::min(center.x, std::min(prev.x, curr.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(center.y, std::min(prev.y, curr.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(center.x, std::max(prev.x, curr.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(center.y, std::max(prev.y, curr.y)));

    // Fixme: just use bezier curve to calculate step count
    auto count = Bezier(prev, curr, radius()).segments();
    auto c = mBuffer->vbuffer.count;  mBuffer->vbuffer.push(center);
    auto pi = mBuffer->vbuffer.count; mBuffer->vbuffer.push(prev);
    auto step = 1.f / (count - 1);
    auto dir = curr - prev;

    for (uint32_t i = 1; i < static_cast<uint32_t>(count); i++) {
        auto t = i * step;
        auto p = prev + dir * t;
        auto o_dir = p - center;
        normalize(o_dir);

        auto out = center + o_dir * radius();
        auto oi = mBuffer->vbuffer.count; mBuffer->vbuffer.push(out);

        mBuffer->ibuffer.push(c);
        mBuffer->ibuffer.push(pi);
        mBuffer->ibuffer.push(oi);

        pi = oi;

        mLeftTop.x = std::min(mLeftTop.x, out.x);
        mLeftTop.y = std::min(mLeftTop.y, out.y);
        mRightBottom.x = std::max(mRightBottom.x, out.x);
        mRightBottom.y = std::max(mRightBottom.y, out.y);
    }
}


void WgStroker::roundPoint(const Point &p)
{
    // Fixme: just use bezier curve to calculate step count
    auto count = Bezier(p, p, radius()).segments() * 2;
    auto c = mBuffer->vbuffer.count; mBuffer->vbuffer.push(p);
    auto step = 2 * MATH_PI / (count - 1);

    for (uint32_t i = 1; i <= static_cast<uint32_t>(count); i++) {
        float angle = i * step;
        Point dir = {cos(angle), sin(angle)};
        Point out = p + dir * radius();
        auto oi = mBuffer->vbuffer.count; mBuffer->vbuffer.push(out);

        if (oi > 1) {
            mBuffer->ibuffer.push(c);
            mBuffer->ibuffer.push(oi);
            mBuffer->ibuffer.push(oi - 1);
        }
    }

    mLeftTop.x = std::min(mLeftTop.x, p.x - radius());
    mLeftTop.y = std::min(mLeftTop.y, p.y - radius());
    mRightBottom.x = std::max(mRightBottom.x, p.x + radius());
    mRightBottom.y = std::max(mRightBottom.y, p.y + radius());
}


void WgStroker::miter(const Point& prev, const Point& curr, const Point& center)
{
    auto pp1 = prev - center;
    auto pp2 = curr - center;
    auto out = pp1 + pp2;
    auto k = 2.f * radius() * radius() / (out.x * out.x + out.y * out.y);
    auto pe = out * k;

    if (length(pe) >= mMiterLimit * radius()) {
        this->bevel(prev, curr, center);
        return;
    }

    auto join = center + pe;
    auto c   = mBuffer->vbuffer.count; mBuffer->vbuffer.push(center);
    auto cp1 = mBuffer->vbuffer.count; mBuffer->vbuffer.push(prev);
    auto cp2 = mBuffer->vbuffer.count; mBuffer->vbuffer.push(curr);
    auto e   = mBuffer->vbuffer.count; mBuffer->vbuffer.push(join);

    mBuffer->ibuffer.push(c);
    mBuffer->ibuffer.push(cp1);
    mBuffer->ibuffer.push(e);

    mBuffer->ibuffer.push(e);
    mBuffer->ibuffer.push(cp2);
    mBuffer->ibuffer.push(c);

    mLeftTop.x = std::min(mLeftTop.x, join.x);
    mLeftTop.y = std::min(mLeftTop.y, join.y);

    mRightBottom.x = std::max(mRightBottom.x, join.x);
    mRightBottom.y = std::max(mRightBottom.y, join.y);
}


void WgStroker::bevel(const Point& prev, const Point& curr, const Point& center)
{
    auto a = mBuffer->vbuffer.count; mBuffer->vbuffer.push(prev);
    auto b = mBuffer->vbuffer.count; mBuffer->vbuffer.push(curr);
    auto c = mBuffer->vbuffer.count; mBuffer->vbuffer.push(center);

    mBuffer->ibuffer.push(a);
    mBuffer->ibuffer.push(b);
    mBuffer->ibuffer.push(c);
}


void WgStroker::square(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};

    auto a = p + normal * radius();
    auto b = p - normal * radius();
    auto c = a + outDir * radius();
    auto d = b + outDir * radius();

    auto ai = mBuffer->vbuffer.count; mBuffer->vbuffer.push(a);
    auto bi = mBuffer->vbuffer.count; mBuffer->vbuffer.push(b);
    auto ci = mBuffer->vbuffer.count; mBuffer->vbuffer.push(c);
    auto di = mBuffer->vbuffer.count; mBuffer->vbuffer.push(d);

    mBuffer->ibuffer.push(ai);
    mBuffer->ibuffer.push(bi);
    mBuffer->ibuffer.push(ci);

    mBuffer->ibuffer.push(ci);
    mBuffer->ibuffer.push(bi);
    mBuffer->ibuffer.push(di);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void WgStroker::squarePoint(const Point& p)
{
    auto offsetX = Point{radius(), 0.0f};
    auto offsetY = Point{0.0f, radius()};

    auto a = p + offsetX + offsetY;
    auto b = p - offsetX + offsetY;
    auto c = p - offsetX - offsetY;
    auto d = p + offsetX - offsetY;

    auto ai = mBuffer->vbuffer.count; mBuffer->vbuffer.push(a);
    auto bi = mBuffer->vbuffer.count; mBuffer->vbuffer.push(b);
    auto ci = mBuffer->vbuffer.count; mBuffer->vbuffer.push(c);
    auto di = mBuffer->vbuffer.count; mBuffer->vbuffer.push(d);

    mBuffer->ibuffer.push(ai);
    mBuffer->ibuffer.push(bi);
    mBuffer->ibuffer.push(ci);

    mBuffer->ibuffer.push(ci);
    mBuffer->ibuffer.push(di);
    mBuffer->ibuffer.push(ai);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void WgStroker::round(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};
    auto a = p + normal * radius();
    auto b = p - normal * radius();
    auto c = p + outDir * radius();

    round(a, c, p);
    round(c, b, p);
}


WgBWTessellator::WgBWTessellator(WgMeshData* buffer): mBuffer(buffer)
{
}


void WgBWTessellator::tessellate(const RenderPath& path, const Matrix& matrix)
{
    if (path.pts.count <= 2) return;

    auto cmds = path.cmds.data;
    auto cmdCnt = path.cmds.count;
    auto pts = path.pts.data;
    auto ptsCnt = path.pts.count;

    uint32_t firstIndex = 0;
    uint32_t prevIndex = 0;

    mBuffer->vbuffer.reserve(ptsCnt * 2);
    mBuffer->ibuffer.reserve((ptsCnt - 2) * 3);

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
                Bezier relCurve {pts[-1], pts[0], pts[1], pts[2]};
                relCurve.start *= matrix;
                relCurve.ctrl1 *= matrix;
                relCurve.ctrl2 *= matrix;
                relCurve.end *= matrix;

                auto stepCount = relCurve.segments();
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


RenderRegion WgBWTessellator::bounds() const
{
    return {{int32_t(floor(bbox.min.x)), int32_t(floor(bbox.min.y))}, {int32_t(ceil(bbox.max.x)), int32_t(ceil(bbox.max.y))}};
}


BBox WgBWTessellator::getBBox() const
{
    return bbox;
}


uint32_t WgBWTessellator::pushVertex(float x, float y)
{
    auto index = mBuffer->vbuffer.count;
    mBuffer->vbuffer.push({x, y});
    if (index == 0) bbox.max = bbox.min = {x, y};
    else bbox = {{std::min(bbox.min.x, x), std::min(bbox.min.y, y)}, {std::max(bbox.max.x, x), std::max(bbox.max.y, y)}};
    return index;
}


void WgBWTessellator::pushTriangle(uint32_t a, uint32_t b, uint32_t c)
{
    mBuffer->ibuffer.push(a);
    mBuffer->ibuffer.push(b);
    mBuffer->ibuffer.push(c);
}
