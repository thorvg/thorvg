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

static bool _bezIsFlatten(const Bezier& bz)
{
    float diff1_x = fabs((bz.ctrl1.x * 3.f) - (bz.start.x * 2.f) - bz.end.x);
    float diff1_y = fabs((bz.ctrl1.y * 3.f) - (bz.start.y * 2.f) - bz.end.y);
    float diff2_x = fabs((bz.ctrl2.x * 3.f) - (bz.end.x * 2.f) - bz.start.x);
    float diff2_y = fabs((bz.ctrl2.y * 3.f) - (bz.end.y * 2.f) - bz.start.y);

    if (diff1_x < diff2_x) diff1_x = diff2_x;
    if (diff1_y < diff2_y) diff1_y = diff2_y;

    if (diff1_x + diff1_y <= 0.5f) return true;

    return false;
}


static int32_t _bezierCurveCount(const Bezier &curve)
{

    if (_bezIsFlatten(curve)) {
        return 1;
    }

    Bezier left{};
    Bezier right{};

    curve.split(left, right);

    return _bezierCurveCount(left) + _bezierCurveCount(right);
}


static Bezier _bezFromArc(const Point& start, const Point& end, float radius)
{
    // Calculate the angle between the start and end points
    auto angle = tvg::atan2(end.y - start.y, end.x - start.x);

    // Calculate the control points of the cubic bezier curve
    auto c = radius * 0.552284749831f;  // c = radius * (4/3) * tan(pi/8)

    Bezier bz;
    bz.start = {start.x, start.y};
    bz.ctrl1 = {start.x + radius * cos(angle), start.y + radius * sin(angle)};
    bz.ctrl2 = {end.x - c * cosf(angle), end.y - c * sinf(angle)};
    bz.end = {end.x, end.y};

    return bz;
}


static uint32_t _pushVertex(Array<float>& array, float x, float y)
{
    array.push(x);
    array.push(y);
    return (array.count - 2) / 2;
}


enum class Orientation
{
    Linear,
    Clockwise,
    CounterClockwise,
};


static Orientation _calcOrientation(const Point& p1, const Point& p2, const Point& p3)
{
    auto val = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
    if (std::abs(val) < 0.0001f) return Orientation::Linear;
    else return val > 0 ? Orientation::Clockwise : Orientation::CounterClockwise;
}


Stroker::Stroker(GlGeometryBuffer* buffer, const Matrix& matrix) : mBuffer(buffer), mMatrix(matrix)
{
}


void Stroker::stroke(const RenderShape *rshape, const RenderPath& path)
{
    mMiterLimit = rshape->strokeMiterlimit();
    mStrokeCap = rshape->strokeCap();
    mStrokeJoin = rshape->strokeJoin();
    mStrokeWidth = rshape->strokeWidth();

    if (isinf(mMatrix.e11)) {
        auto strokeWidth = rshape->strokeWidth() * getScaleFactor(mMatrix);
        if (strokeWidth <= MIN_GL_STROKE_WIDTH) strokeWidth = MIN_GL_STROKE_WIDTH;
        mStrokeWidth = strokeWidth / mMatrix.e11;
    }

    auto& dash = rshape->stroke->dash;
    if (dash.length < DASH_PATTERN_THRESHOLD) doStroke(path);
    else doDashStroke(path, dash.pattern, dash.count, dash.offset, dash.length);
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


void Stroker::doDashStroke(const RenderPath& path, const float *patterns, uint32_t patternCnt, float offset, float length)
{
    RenderPath dpath;

    dpath.cmds.reserve(20 * path.cmds.count);
    dpath.pts.reserve(20 * path.pts.count);

    DashStroke dash(&dpath.cmds, &dpath.pts, patterns, patternCnt, offset, length);
    dash.doStroke(path, mStrokeCap != StrokeCap::Butt);
    doStroke(dpath);
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

    auto count = _bezierCurveCount(relCurve);
    auto step = 1.f / count;

    for (int32_t i = 0; i <= count; i++) {
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
    auto orientation = _calcOrientation(mStrokeState.prevPt - mStrokeState.prevPtDir, mStrokeState.prevPt, mStrokeState.prevPt + dir);

    if (orientation == Orientation::Linear) {
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

        if (orientation == Orientation::CounterClockwise) {
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
    if (_calcOrientation(prev, center, curr) == Orientation::Linear) return;

    mLeftTop.x = std::min(mLeftTop.x, std::min(center.x, std::min(prev.x, curr.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(center.y, std::min(prev.y, curr.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(center.x, std::max(prev.x, curr.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(center.y, std::max(prev.y, curr.y)));

    // Fixme: just use bezier curve to calculate step count
    auto count = _bezierCurveCount(_bezFromArc(prev, curr, strokeRadius()));
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
    auto count = _bezierCurveCount(_bezFromArc(p, p, strokeRadius())) * 2;
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


DashStroke::DashStroke(Array<PathCommand> *cmds, Array<Point> *pts, const float *patterns, uint32_t patternCnt, float offset, float length)
    : mCmds(cmds),
      mPts(pts),
      mDashPattern(patterns),
      mDashCount(patternCnt),
      mDashOffset(offset),
      mDashLength(length)
{
}


void DashStroke::doStroke(const RenderPath& path, bool validPoint)
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


void DashStroke::drawPoint(const Point& p)
{
    if (mMove || mDashPattern[mCurrIdx] < FLOAT_EPSILON) {
        this->moveTo(p);
        mMove = false;
    }
    this->lineTo(p);
}


void DashStroke::dashLineTo(const Point& to, bool validPoint)
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


void DashStroke::dashCubicTo(const Point& cnt1, const Point& cnt2, const Point& end, bool validPoint)
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


void DashStroke::moveTo(const Point& pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::MoveTo);
}


void DashStroke::lineTo(const Point& pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::LineTo);
}


void DashStroke::cubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    mPts->push({cnt1.x, cnt1.y});
    mPts->push({cnt2.x, cnt2.y});
    mPts->push({end.x, end.y});
    mCmds->push(PathCommand::CubicTo);
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
                Bezier relCurve {pts[-1], pts[0], pts[1], pts[2]};
                relCurve.start *= matrix;
                relCurve.ctrl1 *= matrix;
                relCurve.ctrl2 *= matrix;
                relCurve.end *= matrix;

                auto stepCount = _bezierCurveCount(relCurve);
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
