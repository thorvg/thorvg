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

#include "tvgWgTessellator.h"
#include "tvgMath.h"

WgStroker::WgStroker(WgMeshData* buffer, const Matrix& matrix) : mBuffer(buffer), mMatrix(matrix)
{
}


void WgStroker::stroke(const RenderShape *rshape, const RenderPath& path)
{
    mMiterLimit = rshape->strokeMiterlimit();
    mStrokeCap = rshape->strokeCap();
    mStrokeJoin = rshape->strokeJoin();
    mStrokeWidth = rshape->strokeWidth();

    if (isinf(mMatrix.e11)) {
        auto strokeWidth = rshape->strokeWidth() * scaling(mMatrix);
        if (strokeWidth <= MIN_WG_STROKE_WIDTH) strokeWidth = MIN_WG_STROKE_WIDTH;
        mStrokeWidth = strokeWidth / mMatrix.e11;
    }

    auto& dash = rshape->stroke->dash;
    if (dash.length < DASH_PATTERN_THRESHOLD) doStroke(path);
    else doDashStroke(path, dash.pattern, dash.count, dash.offset, dash.length);
}


RenderRegion WgStroker::bounds() const
{
    return {{int32_t(floor(mLeftTop.x)), int32_t(floor(mLeftTop.y))}, {int32_t(ceil(mRightBottom.x)), int32_t(ceil(mRightBottom.y))}};
}


void WgStroker::doStroke(const RenderPath& path)
{
    mBuffer->vbuffer.reserve(path.pts.count * 4 + 16);
    mBuffer->ibuffer.reserve(path.pts.count * 3);

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


void WgStroker::doDashStroke(const RenderPath& path, const float *patterns, uint32_t patternCnt, float offset, float length)
{
    RenderPath dpath;

    dpath.cmds.reserve(20 * path.cmds.count);
    dpath.pts.reserve(20 * path.pts.count);

    WgDashStroke dash(&dpath.cmds, &dpath.pts, patterns, patternCnt, offset, length);
    dash.doStroke(path, mStrokeCap != StrokeCap::Butt);
    doStroke(dpath);
}


void WgStroker::strokeCap()
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


void WgStroker::strokeLineTo(const Point& curr)
{
    auto dir = (curr - mStrokeState.prevPt);
    normalize(dir);

    if (dir.x == 0.f && dir.y == 0.f) return;  //same point

    auto normal = Point{-dir.y, dir.x};
    auto a = mStrokeState.prevPt + normal * strokeRadius();
    auto b = mStrokeState.prevPt - normal * strokeRadius();
    auto c = curr + normal * strokeRadius();
    auto d = curr - normal * strokeRadius();

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


void WgStroker::strokeCubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    Bezier curve{};
    curve.start = {mStrokeState.prevPt.x, mStrokeState.prevPt.y};
    curve.ctrl1 = {cnt1.x, cnt1.y};
    curve.ctrl2 = {cnt2.x, cnt2.y};
    curve.end = {end.x, end.y};

    Bezier relCurve {curve.start, curve.ctrl1, curve.ctrl2, curve.end};
    relCurve.start *= mMatrix;
    relCurve.ctrl1 *= mMatrix;
    relCurve.ctrl2 *= mMatrix;
    relCurve.end *= mMatrix;

    auto count = relCurve.curveCount();
    auto step = 1.f / count;

    for (int32_t i = 0; i <= count; i++) {
        strokeLineTo(curve.at(step * i));
    }
}


void WgStroker::strokeClose()
{
    if (length(mStrokeState.prevPt - mStrokeState.firstPt) > 0.015625f) {
        this->strokeLineTo(mStrokeState.firstPt);
    }

    // join firstPt with prevPt
    this->strokeJoin(mStrokeState.firstPtDir);
}


void WgStroker::strokeJoin(const Point& dir)
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


void WgStroker::strokeRound(const Point &prev, const Point& curr, const Point& center)
{
    if (orientation(prev, center, curr) == Orientation::Linear) return;

    mLeftTop.x = std::min(mLeftTop.x, std::min(center.x, std::min(prev.x, curr.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(center.y, std::min(prev.y, curr.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(center.x, std::max(prev.x, curr.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(center.y, std::max(prev.y, curr.y)));

    // Fixme: just use bezier curve to calculate step count
    auto count = tvg::bezierFromArc(prev, curr, strokeRadius()).curveCount();
    auto c = mBuffer->ibuffer.count;  mBuffer->vbuffer.push(center);
    auto pi = mBuffer->ibuffer.count; mBuffer->vbuffer.push(prev);
    auto step = 1.f / (count - 1);
    auto dir = curr - prev;

    for (uint32_t i = 1; i < static_cast<uint32_t>(count); i++) {
        auto t = i * step;
        auto p = prev + dir * t;
        auto o_dir = p - center;
        normalize(o_dir);

        auto out = center + o_dir * strokeRadius();
        auto oi = mBuffer->ibuffer.count; mBuffer->vbuffer.push(out);

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


void WgStroker::strokeRoundPoint(const Point &p)
{
    // Fixme: just use bezier curve to calculate step count
    auto count = tvg::bezierFromArc(p, p, strokeRadius()).curveCount() * 2;
    auto c = mBuffer->vbuffer.count; mBuffer->vbuffer.push(p);
    auto step = 2 * MATH_PI / (count - 1);

    for (uint32_t i = 1; i <= static_cast<uint32_t>(count); i++) {
        float angle = i * step;
        Point dir = {cos(angle), sin(angle)};
        Point out = p + dir * strokeRadius();
        auto oi = mBuffer->vbuffer.count; mBuffer->vbuffer.push(out);

        if (oi > 1) {
            mBuffer->ibuffer.push(c);
            mBuffer->ibuffer.push(oi);
            mBuffer->ibuffer.push(oi - 1);
        }
    }

    mLeftTop.x = std::min(mLeftTop.x, p.x - strokeRadius());
    mLeftTop.y = std::min(mLeftTop.y, p.y - strokeRadius());
    mRightBottom.x = std::max(mRightBottom.x, p.x + strokeRadius());
    mRightBottom.y = std::max(mRightBottom.y, p.y + strokeRadius());
}


void WgStroker::strokeMiter(const Point& prev, const Point& curr, const Point& center)
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


void WgStroker::strokeBevel(const Point& prev, const Point& curr, const Point& center)
{
    auto a = mBuffer->vbuffer.count; mBuffer->vbuffer.push(prev);
    auto b = mBuffer->vbuffer.count; mBuffer->vbuffer.push(curr);
    auto c = mBuffer->vbuffer.count; mBuffer->vbuffer.push(center);

    mBuffer->ibuffer.push(a);
    mBuffer->ibuffer.push(b);
    mBuffer->ibuffer.push(c);
}


void WgStroker::strokeSquare(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};

    auto a = p + normal * strokeRadius();
    auto b = p - normal * strokeRadius();
    auto c = a + outDir * strokeRadius();
    auto d = b + outDir * strokeRadius();

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


void WgStroker::strokeSquarePoint(const Point& p)
{
    auto offsetX = Point{strokeRadius(), 0.0f};
    auto offsetY = Point{0.0f, strokeRadius()};

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


void WgStroker::strokeRound(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};
    auto a = p + normal * strokeRadius();
    auto b = p - normal * strokeRadius();
    auto c = p + outDir * strokeRadius();

    strokeRound(a, c, p);
    strokeRound(c, b, p);
}


WgDashStroke::WgDashStroke(Array<PathCommand> *cmds, Array<Point> *pts, const float *patterns, uint32_t patternCnt, float offset, float length)
    : mCmds(cmds),
      mPts(pts),
      mDashPattern(patterns),
      mDashCount(patternCnt),
      mDashOffset(offset),
      mDashLength(length)
{
}


void WgDashStroke::doStroke(const RenderPath& path, bool validPoint)
{
    //validPoint: zero length segment with non-butt cap still should be rendered as a point - only the caps are visible
    int32_t idx = 0;
    auto offset = mDashOffset;
    bool gap = false;
    if (!tvg::zero(mDashOffset)) {
        auto length = (mDashCount % 2) ? mDashLength * 2 : mDashLength;
        offset = fmodf(offset, length);
        if (offset < 0) offset += length;

        for (uint32_t i = 0; i < mDashCount * (mDashCount % 2 + 1); ++i, ++idx) {
            auto curPattern = mDashPattern[i % mDashCount];
            if (offset < curPattern) break;
            offset -= curPattern;
            gap = !gap;
        }
        idx = idx % mDashCount;
    }

    auto pts = path.pts.data;
    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::Close: {
                this->dashLineTo(mPtStart, validPoint);
                break;
            }
            case PathCommand::MoveTo: {
                // reset the dash state
                mCurrIdx = idx;
                mCurrLen = mDashPattern[idx] - offset;
                mCurOpGap = gap;
                mMove = true;
                mPtStart = mPtCur = *pts;
                pts++;
                break;
            }
            case PathCommand::LineTo: {
                this->dashLineTo(*pts, validPoint);
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                this->dashCubicTo(pts[0], pts[1], pts[2], validPoint);
                pts += 3;
                break;
            }
            default: break;
        }
    }
}


void WgDashStroke::drawPoint(const Point& p)
{
    if (mMove || mDashPattern[mCurrIdx] < FLOAT_EPSILON) {
        this->moveTo(p);
        mMove = false;
    }
    this->lineTo(p);
}


void WgDashStroke::dashLineTo(const Point& to, bool validPoint)
{
    auto len = length(mPtCur - to);

    if (tvg::zero(len)) {
        this->moveTo(mPtCur);
    } else if (len <= mCurrLen) {
        mCurrLen -= len;
        if (!mCurOpGap) {
            if (mMove) {
                this->moveTo(mPtCur);
                mMove = false;
            }
            this->lineTo(to);
        }
    } else {
        Line curr = {mPtCur, to};

        while (len - mCurrLen > DASH_PATTERN_THRESHOLD) {
            Line right;
            if (mCurrLen > 0.0f) {
                Line left;
                curr.split(mCurrLen, left, right);
                len -= mCurrLen;
                if (!mCurOpGap) {
                    if (mMove || mDashPattern[mCurrIdx] - mCurrLen < FLOAT_EPSILON) {
                        this->moveTo(left.pt1);
                        mMove = false;
                    }
                    this->lineTo(left.pt2);
                }
            } else {
                if (validPoint && !mCurOpGap) drawPoint(curr.pt1);
                right = curr;
            }
            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
            curr = right;
            mPtCur = curr.pt1;
            mMove = true;
        }
        mCurrLen -= len;
        if (!mCurOpGap) {
            if (mMove) {
                this->moveTo(curr.pt1);
                mMove = false;
            }
            this->lineTo(curr.pt2);
        }

        if (mCurrLen < 0.1f) {
            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
        }
    }

    mPtCur = to;
}


void WgDashStroke::dashCubicTo(const Point& cnt1, const Point& cnt2, const Point& end, bool validPoint)
{
    Bezier cur;
    cur.start = {mPtCur.x, mPtCur.y};
    cur.ctrl1 = {cnt1.x, cnt1.y};
    cur.ctrl2 = {cnt2.x, cnt2.y};
    cur.end = {end.x, end.y};

    auto len = cur.length();

    if (tvg::zero(len)) {
        this->moveTo(mPtCur);
    } else if (len <= mCurrLen) {
        mCurrLen -= len;
        if (!mCurOpGap) {
            if (mMove) {
                this->moveTo(mPtCur);
                mMove = false;
            }
            this->cubicTo(cnt1, cnt2, end);
        }
    } else {
        while (len - mCurrLen > DASH_PATTERN_THRESHOLD) {
            Bezier right;
            if (mCurrLen > 0.0f) {
                Bezier left;
                cur.split(mCurrLen, left, right);
                len -= mCurrLen;
                if (!mCurOpGap) {
                    if (mMove || mDashPattern[mCurrIdx] - mCurrLen < FLOAT_EPSILON) {
                        this->moveTo(left.start);
                        mMove = false;
                    }
                    this->cubicTo(left.ctrl1, left.ctrl2, left.end);
                }
            } else {
                if (validPoint && !mCurOpGap) drawPoint(cur.start);
                right = cur;
            }
            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
            cur = right;
            mPtCur = cur.start;
            mMove = true;
        }

        mCurrLen -= len;
        if (!mCurOpGap) {
            if (mMove) {
                this->moveTo(cur.start);
                mMove = false;
            }
            this->cubicTo(cur.ctrl1, cur.ctrl2, cur.end);
        }

        if (mCurrLen < 0.1f) {
            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
        }
    }
    mPtCur = end;
}


void WgDashStroke::moveTo(const Point& pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::MoveTo);
}


void WgDashStroke::lineTo(const Point& pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::LineTo);
}


void WgDashStroke::cubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    mPts->push({cnt1.x, cnt1.y});
    mPts->push({cnt2.x, cnt2.y});
    mPts->push({end.x, end.y});
    mCmds->push(PathCommand::CubicTo);
}


WgBWTessellator::WgBWTessellator(WgMeshData* buffer): mBuffer(buffer)
{
}


void WgBWTessellator::tessellate(const RenderPath& path, const Matrix& matrix)
{
    auto cmds = path.cmds.data;
    auto cmdCnt = path.cmds.count;
    auto pts = path.pts.data;
    auto ptsCnt = path.pts.count;

    if (ptsCnt <= 2) return;

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

                auto stepCount = relCurve.curveCount();
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