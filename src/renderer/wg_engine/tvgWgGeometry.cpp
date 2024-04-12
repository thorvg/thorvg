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
    const uint32_t nPoints = 360;
    sinus.reserve(360);
    cosin.reserve(360);
    for (uint32_t i = 0; i < nPoints; i++) {
        float angle = i * (2 * M_PI) / nPoints;
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
    pts.reserve(1024);
    dist.reserve(1014);
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


void WgPolyline::appendCubic(WgPoint p1, WgPoint p2, WgPoint p3)
{
    WgPoint p0 = pts.count > 0 ? pts.last() : WgPoint(0.0f, 0.0f);
    size_t segs = ((uint32_t)(p0.dist(p3) / 8.0f));
    segs = segs == 0 ? 1 : segs;
    for (size_t i = 1; i <= segs; i++) {
        float t = i / (float)segs;
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
}


bool WgPolyline::isClosed() const
{
    return (pts.count >= 2) && (pts[0].dist2(pts.last()) == 0.0f);
}


void WgPolyline::clear()
{
    // reset min and max indexes and total length
    iminx = imaxx = iminy = imaxy = 0;
    len = 0.0f;
    // clear points and distances
    pts.clear();
    dist.clear();
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
    positions.pts.reserve(10240);
    texCoords.reserve(10240);
    indexes.reserve(10240);
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


void WgGeometryData::appendMesh(const RenderMesh* rmesh)
{
    assert(rmesh);
    positions.pts.reserve(rmesh->triangleCnt * 3);
    texCoords.reserve(rmesh->triangleCnt * 3);
    indexes.reserve(rmesh->triangleCnt * 3);
    for (uint32_t i = 0; i < rmesh->triangleCnt; i++) {
        positions.appendPoint(rmesh->triangles[i].vertex[0].pt);
        positions.appendPoint(rmesh->triangles[i].vertex[1].pt);
        positions.appendPoint(rmesh->triangles[i].vertex[2].pt);
        texCoords.push(rmesh->triangles[i].vertex[0].uv);
        texCoords.push(rmesh->triangles[i].vertex[1].uv);
        texCoords.push(rmesh->triangles[i].vertex[2].uv);
        indexes.push(i*3 + 0);
        indexes.push(i*3 + 1);
        indexes.push(i*3 + 2);
    }
}


void WgGeometryData::appendStrokeDashed(const WgPolyline* polyline, const RenderStroke *stroke)
{
    assert(stroke);
    assert(polyline);
    static WgPolyline dashed;
    dashed.clear();
    // append single point polyline
    if (polyline->pts.count == 1)
        appendStroke(polyline, stroke);
    // append multy points dashed polyline
    else if (polyline->pts.count >= 2) {
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


void WgGeometryData::appendStroke(const WgPolyline* polyline, const RenderStroke *stroke)
{
    assert(stroke);
    assert(polyline);
    float wdt = stroke->width / 2;
    // single point sub-path
    if (polyline->pts.count == 1) {
        if (stroke->cap == StrokeCap::Round) {
            appendCircle(polyline->pts[0], wdt);
        } else if (stroke->cap == StrokeCap::Butt) {
            // for zero length sub-paths no stroke is rendered
        } else if (stroke->cap == StrokeCap::Square) {
            appendRect(
                polyline->pts[0] + WgPoint(+wdt, +wdt),
                polyline->pts[0] + WgPoint(+wdt, -wdt),
                polyline->pts[0] + WgPoint(-wdt, +wdt),
                polyline->pts[0] + WgPoint(-wdt, -wdt)
            );
        }
    } else
    
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
    } else

    // multi-lined sub-path
    if (polyline->pts.count > 2) {
        if (polyline->isClosed()) {
            if (stroke->join == StrokeJoin::Round) {
                appendCircle(polyline->pts[0], wdt);
            } else {
                float dist0 = polyline->dist.last();
                float dist1 = polyline->dist[1];
                WgPoint v0 = polyline->pts[polyline->pts.count - 2];
                WgPoint v1 = polyline->pts[0];
                WgPoint v2 = polyline->pts[1];
                WgPoint dir0 = (v1 - v0) / dist0;
                WgPoint dir1 = (v2 - v1) / dist1;
                WgPoint nrm0 { +dir0.y, -dir0.x };
                WgPoint nrm1 { +dir1.y, -dir1.x };
                WgPoint offset0 = nrm0 * wdt;
                WgPoint offset1 = nrm1 * wdt;
                if (stroke->join == StrokeJoin::Bevel) {
                    appendRect(v1 - offset0, v1 + offset0, v1 - offset1, v1 + offset1);
                } else if (stroke->join == StrokeJoin::Miter) {
                    WgPoint nrm = (nrm0 + nrm1).normal();
                    float cosine = nrm.dot(nrm0);
                    if ((cosine != 0.0f) && (abs(cosine) != 1.0f) && (abs(wdt / cosine) <= stroke->miterlimit * 2)) {
                        appendRect(v1 + nrm * (wdt / cosine), v1 + offset0, v1 + offset1, v1 - nrm * (wdt / cosine));
                    } else {
                        appendRect(v1 - offset0, v1 + offset0, v1 - offset1, v1 + offset1);
                    }
                }
            }
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
                if (stroke->join == StrokeJoin::Round) {
                    appendCircle(v1, wdt);
                } else {
                    float dist0 = polyline->dist[i + 0];
                    WgPoint v0 = polyline->pts[i - 1];
                    WgPoint dir0 = (v1 - v0) / dist0;
                    WgPoint nrm0 { +dir0.y, -dir0.x };
                    WgPoint offset0 = nrm0 * wdt;
                    if (stroke->join == StrokeJoin::Bevel) {
                        appendRect(v1 - offset0, v1 + offset0, v1 - offset1, v1 + offset1);
                    } else if (stroke->join == StrokeJoin::Miter) {
                        WgPoint nrm = (nrm0 + nrm1).normal();
                        float cosine = nrm.dot(nrm0);
                        if ((cosine != 0.0f) && (abs(cosine) != 1.0f) && (abs(wdt / cosine) <= stroke->miterlimit * 2)) {
                            appendRect(v1 + nrm * (wdt / cosine), v1 + offset0, v1 + offset1, v1);
                            appendRect(v1 - nrm * (wdt / cosine), v1 - offset0, v1 - offset1, v1);
                        } else {
                            appendRect(v1 - offset0, v1 + offset0, v1 - offset1, v1 + offset1);
                        }
                    }
                }
            }
        }
    }
}
