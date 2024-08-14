/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#include "tvgWgGeometry.h"

//***********************************************************************
// WgPolyline
//***********************************************************************

WgMath* WgGeometryData::gMath = nullptr;

void WgMath::initialize()
{
    if (initialized) return;
    initialized = true;
    constexpr uint32_t nPoints = 360;
    sinus.reserve(nPoints);
    cosin.reserve(nPoints);
    for (uint32_t i = 0; i < nPoints; i++) {
        float angle = i * (2 * MATH_PI) / nPoints;
        sinus.push(sin(angle));
        cosin.push(cos(angle));
    }
};


void WgMath::release() {
    sinus.clear();
    cosin.clear();
};

//***********************************************************************
// WgPolyline
//***********************************************************************

WgPolyline::WgPolyline()
{
    constexpr uint32_t nPoints = 360;
    pts.reserve(nPoints);
    dist.reserve(nPoints);
    closed = false;
}


void WgPolyline::appendPoint(WgPoint pt)
{
    if (pts.count > 0) {
        float distance = pts.last().dist(pt);
        if (distance > 0) {
            // update min and max indexes
            iminx = pts[iminx].x >= pt.x ? pts.count : iminx;
            imaxx = pts[imaxx].x <= pt.x ? pts.count : imaxx;
            iminy = pts[iminy].y >= pt.y ? pts.count : iminy;
            imaxy = pts[imaxy].y <= pt.y ? pts.count : imaxy;
            // update total length
            len += distance;
            // update points and distances
            pts.push(pt);
            dist.push(distance);
        };
    } else {
        // reset min and max indexes and total length
        iminx = imaxx = iminy = imaxy = 0;
        len = 0.0f;
        // update points and distances
        pts.push(pt);
        dist.push(0.0f);
    }
}


void WgPolyline::appendCubic(WgPoint p1, WgPoint p2, WgPoint p3, size_t nsegs)
{
    WgPoint p0 = pts.count > 0 ? pts.last() : WgPoint(0.0f, 0.0f);
    nsegs = nsegs == 0 ? 1 : nsegs;
    float dt = 1.0f / (float)nsegs;
    float t = 0.0f;
    for (size_t i = 1; i <= nsegs; i++) {
        t += dt;
        // get cubic spline interpolation coefficients
        float t0 = 1.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
        float t1 = 3.0f * (1.0f - t) * (1.0f - t) * t;
        float t2 = 3.0f * (1.0f - t) * t * t;
        float t3 = 1.0f * t * t * t;
        appendPoint(p0 * t0 + p1 * t1 + p2 * t2 + p3 * t3);
    }
}


void WgPolyline::trim(WgPolyline* polyline, float trimBegin, float trimEnd) const
{
    assert(polyline);
    polyline->clear();
    float begLen = len * trimBegin;
    float endLen = len * trimEnd;
    float currentLength = 0.0f;
    // find start point
    uint32_t indexStart = 0;
    WgPoint pointStart{};
    currentLength = 0.0f;
    for (indexStart = 1; indexStart < pts.count; indexStart++) {
        currentLength += dist[indexStart];
        if(currentLength >= begLen) {
            float t = 1.0f - (currentLength - begLen) / dist[indexStart];
            pointStart = pts[indexStart-1] * (1.0f - t) + pts[indexStart] * t;
            break;
        }
    }
    if (indexStart >= pts.count) return;
    // find end point
    uint32_t indexEnd = 0;
    WgPoint pointEnd{};
    currentLength = 0.0f;
    for (indexEnd = 1; indexEnd < pts.count; indexEnd++) {
        currentLength += dist[indexEnd];
        if(currentLength >= endLen) {
            float t = 1.0f - (currentLength - endLen) / dist[indexEnd];
            pointEnd = pts[indexEnd-1] * (1.0f - t) + pts[indexEnd] * t;
            break;
        }
    }
    if (indexEnd >= pts.count) return;
    // fill polyline
    polyline->appendPoint(pointStart);
    for (uint32_t i = indexStart; i <= indexEnd - 1; i++)
        polyline->appendPoint(pts[i]);
    polyline->appendPoint(pointEnd);
}


void WgPolyline::close()
{
    if (pts.count > 0) appendPoint(pts[0]);
    closed = true;
}


void WgPolyline::clear()
{
    // reset min and max indexes and total length
    iminx = imaxx = iminy = imaxy = 0;
    len = 0.0f;
    // clear points and distances
    pts.clear();
    dist.clear();
    closed = false;
}


void WgPolyline::getBBox(WgPoint& pmin, WgPoint& pmax) const
{
    pmin.x = pts[iminx].x;
    pmin.y = pts[iminy].y;
    pmax.x = pts[imaxx].x;
    pmax.y = pts[imaxy].y;
}

//***********************************************************************
// WgGeometryData
//***********************************************************************

WgGeometryData::WgGeometryData() {
    constexpr uint32_t nPoints = 10240;
    positions.pts.reserve(nPoints);
    texCoords.reserve(nPoints);
    indexes.reserve(nPoints);
}


void WgGeometryData::clear()
{
    indexes.clear();
    positions.clear();
    texCoords.clear();
}


void WgGeometryData::appendBox(WgPoint pmin, WgPoint pmax)
{
    appendRect(
        { pmin.x, pmin.y },
        { pmax.x, pmin.y },
        { pmin.x, pmax.y },
        { pmax.x, pmax.y });
}


void WgGeometryData::appendRect(WgPoint p0, WgPoint p1, WgPoint p2, WgPoint p3)
{
    uint32_t index = positions.pts.count;
    positions.appendPoint(p0); // +0
    positions.appendPoint(p1); // +1
    positions.appendPoint(p2); // +2
    positions.appendPoint(p3); // +3
    indexes.push(index + 0);
    indexes.push(index + 1);
    indexes.push(index + 2);
    indexes.push(index + 1);
    indexes.push(index + 3);
    indexes.push(index + 2);
}


void WgGeometryData::appendCircle(WgPoint center, float radius)
{
    uint32_t indexCenter = positions.pts.count;
    positions.appendPoint(center);
    uint32_t index = positions.pts.count;
    positions.appendPoint({ center.x + gMath->sinus[0] * radius, center.y + gMath->cosin[0] * radius });
    uint32_t nPoints = (uint32_t)(radius * 2.0f);
    nPoints = nPoints < 8 ? 8 : nPoints;
    const uint32_t step = gMath->sinus.count / nPoints;
    for (uint32_t i = step; i < gMath->sinus.count; i += step) {
        positions.appendPoint({ center.x + gMath->sinus[i] * radius, center.y + gMath->cosin[i] * radius });
        indexes.push(index);       // prev point
        indexes.push(indexCenter); // center point
        indexes.push(index + 1);   // curr point
        index++;
    }
    positions.appendPoint({ center.x + gMath->sinus[0] * radius, center.y + gMath->cosin[0] * radius });
    indexes.push(index);       // prev point
    indexes.push(indexCenter); // center point
    indexes.push(index + 1);   // curr point
    positions.appendPoint(center);
}


void WgGeometryData::appendImageBox(float w, float h)
{
    positions.appendPoint({ 0.0f, 0.0f });
    positions.appendPoint({ w   , 0.0f });
    positions.appendPoint({ w   , h });
    positions.appendPoint({ 0.0f, h });
    texCoords.push({ 0.0f, 0.0f });
    texCoords.push({ 1.0f, 0.0f });
    texCoords.push({ 1.0f, 1.0f });
    texCoords.push({ 0.0f, 1.0f });
    indexes.push(0);
    indexes.push(1);
    indexes.push(2);
    indexes.push(0);
    indexes.push(2);
    indexes.push(3);
}


void WgGeometryData::appendBlitBox()
{
    positions.appendPoint({ -1.0f, +1.0f });
    positions.appendPoint({ +1.0f, +1.0f });
    positions.appendPoint({ +1.0f, -1.0f });
    positions.appendPoint({ -1.0f, -1.0f });
    texCoords.push({ 0.0f, 0.0f });
    texCoords.push({ 1.0f, 0.0f });
    texCoords.push({ 1.0f, 1.0f });
    texCoords.push({ 0.0f, 1.0f });
    indexes.push(0);
    indexes.push(1);
    indexes.push(2);
    indexes.push(0);
    indexes.push(2);
    indexes.push(3);
}


void WgGeometryData::appendStrokeDashed(const WgPolyline* polyline, const RenderStroke *stroke)
{
    assert(stroke);
    assert(polyline);
    static WgPolyline dashed;
    dashed.clear();
    // ignore singpe points polyline
    // append multy points dashed polyline
    if (polyline->pts.count >= 2) {
        auto& pts = polyline->pts;
        auto& dist = polyline->dist;
       	// starting state
        uint32_t dashIndex = 0;
        float currentLength = stroke->dashPattern[dashIndex];
        // iterate by polyline points
        for (uint32_t i = 0; i < pts.count - 1; i++) {
            // append current polyline point
            if (dashIndex % 2 == 0)
                dashed.appendPoint(pts[i]);
            // move inside polyline sergemnt
            while(currentLength < dist[i+1]) {
                // get current point
                float t = currentLength / dist[i+1];
                dashed.appendPoint(pts[i] + (pts[i+1] - pts[i]) * t);
                // update current state
                dashIndex = (dashIndex + 1) % stroke->dashCnt;
                currentLength += stroke->dashPattern[dashIndex];
                // append stroke if dash
                if (dashIndex % 2 != 0) {
                    appendStroke(&dashed, stroke);
                    dashed.clear();
                }
            }
            // update current subline length
            currentLength -= dist[i+1];
        }
        // draw last subline
        if (dashIndex % 2 == 0) {
            dashed.appendPoint(pts.last());
            appendStroke(&dashed, stroke);
            dashed.clear();
        }
    }
}


void WgGeometryData::appendStrokeJoin(const WgPoint& v0, const WgPoint& v1, const WgPoint& v2, StrokeJoin join, float halfWidth, float miterLimit)
{
    WgPoint dir0 = (v1 - v0).normal();
    WgPoint dir1 = (v2 - v1).normal();
    WgPoint nrm0 { +dir0.y, -dir0.x };
    WgPoint nrm1 { +dir1.y, -dir1.x };
    WgPoint offset0 = nrm0 * halfWidth;
    WgPoint offset1 = nrm1 * halfWidth;
    if (join == StrokeJoin::Round) {
        appendCircle(v1, halfWidth);
    } else if (join == StrokeJoin::Bevel) {
        appendRect(v1 - offset0, v1 + offset0, v1 - offset1, v1 + offset1);
    } else if (join == StrokeJoin::Miter) {
        WgPoint nrm = (nrm0 + nrm1);
        if (!mathZero(dir0.x * dir1.y -  dir0.y * dir1.x)) {
            nrm.normalize();
            float cosine = nrm.dot(nrm0);
            float angle = std::acos(dir0.dot(dir1.negative()));
            float miterRatio = 1.0f / (std::sin(angle) * 0.5);
            if (miterRatio <= miterLimit) {
                appendRect(v1 + nrm * (halfWidth / cosine), v1 + offset0, v1 + offset1, v1);
                appendRect(v1 - nrm * (halfWidth / cosine), v1 - offset0, v1 - offset1, v1);
            } else {
                appendRect(v1 - offset0, v1 + offset0, v1 - offset1, v1 + offset1);
            }
        }
    }
}


void WgGeometryData::appendStroke(const WgPolyline* polyline, const RenderStroke *stroke)
{
    assert(stroke);
    assert(polyline);
    float wdt = stroke->width * 0.5f;

    // single line sub-path
    if (polyline->pts.count == 2) {
        WgPoint v0 = polyline->pts[0];
        WgPoint v1 = polyline->pts[1];
        WgPoint dir0 = (v1 - v0) / polyline->dist[1];
        WgPoint nrm0 = WgPoint{ -dir0.y, +dir0.x };
        if (stroke->cap == StrokeCap::Round) {
            appendRect(v0 - nrm0 * wdt, v0 + nrm0 * wdt, v1 - nrm0 * wdt, v1 + nrm0 * wdt);
            appendCircle(polyline->pts[0], wdt);
            appendCircle(polyline->pts[1], wdt);
        } else if (stroke->cap == StrokeCap::Butt) {
            appendRect(v0 - nrm0 * wdt, v0 + nrm0 * wdt, v1 - nrm0 * wdt, v1 + nrm0 * wdt);
        } else if (stroke->cap == StrokeCap::Square) {
            appendRect(
                v0 - nrm0 * wdt - dir0 * wdt, v0 + nrm0 * wdt - dir0 * wdt,
                v1 - nrm0 * wdt + dir0 * wdt, v1 + nrm0 * wdt + dir0 * wdt
            );
        }
    } else if (polyline->pts.count > 2) {  // multi-lined sub-path
        if (polyline->closed) {
            WgPoint v0 = polyline->pts[polyline->pts.count - 2];
            WgPoint v1 = polyline->pts[0];
            WgPoint v2 = polyline->pts[1];
            appendStrokeJoin(v0, v1, v2, stroke->join, wdt, stroke->miterlimit);
        } else {
            // append first cap
            WgPoint v0 = polyline->pts[0];
            WgPoint v1 = polyline->pts[1];
            WgPoint dir0 = (v1 - v0) / polyline->dist[1];
            WgPoint nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (stroke->cap == StrokeCap::Round) {
                appendCircle(v0, wdt);
            } else if (stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (stroke->cap == StrokeCap::Square) {
                appendRect(v0 - nrm0 * wdt - dir0 * wdt, v0 + nrm0 * wdt - dir0 * wdt, v0 - nrm0 * wdt, v0 + nrm0 * wdt);
            }

            // append last cap
            v0 = polyline->pts[polyline->pts.count - 2];
            v1 = polyline->pts[polyline->pts.count - 1];
            dir0 = (v1 - v0) / polyline->dist[polyline->pts.count - 1];
            nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (stroke->cap == StrokeCap::Round) {
                appendCircle(v1, wdt);
            } else if (stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (stroke->cap == StrokeCap::Square) {
                appendRect(v1 - nrm0 * wdt, v1 + nrm0 * wdt, v1 - nrm0 * wdt + dir0 * wdt, v1 + nrm0 * wdt + dir0 * wdt);
            }
        }

        // append sub-lines
        for (uint32_t i = 0; i < polyline->pts.count - 1; i++) {
            float dist1 = polyline->dist[i + 1];
            WgPoint v1 = polyline->pts[i + 0];
            WgPoint v2 = polyline->pts[i + 1];
            WgPoint dir1 = (v2 - v1) / dist1;
            WgPoint nrm1 { +dir1.y, -dir1.x };
            WgPoint offset1 = nrm1 * wdt;
            appendRect(v1 - offset1, v1 + offset1, v2 - offset1, v2 + offset1);

            if (i > 0) {
                WgPoint v0 = polyline->pts[i - 1];
                appendStrokeJoin(v0, v1, v2, stroke->join, wdt, stroke->miterlimit);
            }
        }
    }
}
