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
// WgGeometryData
//***********************************************************************

void WgGeometryData::computeTriFansIndexes()
{
    if (positions.count <= 2) return;
    indexes.reserve((positions.count - 2) * 3);
    for (size_t i = 0; i < positions.count - 2; i++) {
        indexes.push(0);
        indexes.push(i + 1);
        indexes.push(i + 2);
    }
};


void WgGeometryData::computeContour(WgGeometryData* data)
{
    assert(data);
    assert(data->positions.count > 1);
    clear();
    uint32_t istart = data->getIndexMinX(); // index start
    uint32_t icnt = data->positions.count;  // index count

    uint32_t iprev = istart == 0 ? icnt - 1 : istart - 1;
    uint32_t inext = (istart + 1) % icnt;   // index current
    WgPoint p0 = data->positions[istart];   // current segment start
    bool isIntersected = false;
    bool isClockWise = !isCW(data->positions[iprev], data->positions[istart], data->positions[inext]);

    uint32_t ii{}; // intersected segment index
    WgPoint pi{};  // intersection point
    positions.push(p0);
    while(!((inext == istart) && (!isIntersected))) {
        if (data->getClosestIntersection(p0, data->positions[inext], pi, ii)) {
            isIntersected = true;
            p0 = pi;
            // operate intersection point
            if (isClockWise) { // clock wise behavior
                if (isCW(p0, pi, data->positions[ii])) {
                    inext = ii;
                } else {
                    inext = ((ii + 1) % icnt);
                }
            } else { // contr-clock wise behavior
                if (isCW(p0, pi, data->positions[ii])) {
                    inext = ((ii + 1) % icnt);
                } else {
                    inext = ii;
                }
            }
        } else { // simple next point
            isIntersected = false;
            p0 = data->positions[inext];
            inext = (inext + 1) % icnt;
        }
        positions.push(p0);
    }
}


void WgGeometryData::appendCubic(WgPoint p1, WgPoint p2, WgPoint p3)
{
    WgPoint p0 = positions.count > 0 ? positions.last() : WgPoint(0.0f, 0.0f);
    const size_t segs = 16;
    for (size_t i = 1; i <= segs; i++) {
        float t = i / (float)segs;
        // get cubic spline interpolation coefficients
        float t0 = 1 * (1.0f - t) * (1.0f - t) * (1.0f - t);
        float t1 = 3 * (1.0f - t) * (1.0f - t) * t;
        float t2 = 3 * (1.0f - t) * t * t;
        float t3 = 1 * t * t * t;
        positions.push(p0 * t0 + p1 * t1 + p2 * t2 + p3 * t3);
    }
};


void WgGeometryData::appendBox(WgPoint pmin, WgPoint pmax)
{
    appendRect(
        { pmin.x, pmin.y},
        { pmax.x, pmin.y},
        { pmin.x, pmax.y},
        { pmax.x, pmax.y});
};


void WgGeometryData::appendRect(WgPoint p0, WgPoint p1, WgPoint p2, WgPoint p3)
{
    uint32_t index = positions.count;
    positions.push(p0); // +0
    positions.push(p1); // +1
    positions.push(p2); // +2
    positions.push(p3); // +3
    indexes.push(index + 0);
    indexes.push(index + 1);
    indexes.push(index + 2);
    indexes.push(index + 1);
    indexes.push(index + 3);
    indexes.push(index + 2);
};


// TODO: optimize vertex and index count
void WgGeometryData::appendCircle(WgPoint center, float radius)
{
    uint32_t index = positions.count;
    uint32_t nSegments = std::trunc(radius);
    for (uint32_t i = 0; i < nSegments; i++) {
        float angle0 = (float)(i + 0) / nSegments * (float)M_PI * 2.0f;
        float angle1 = (float)(i + 1) / nSegments * (float)M_PI * 2.0f;
        WgPoint p0 = center + WgPoint(sin(angle0) * radius, cos(angle0) * radius);
        WgPoint p1 = center + WgPoint(sin(angle1) * radius, cos(angle1) * radius);
        positions.push(center); // +0
        positions.push(p0);     // +1
        positions.push(p1);     // +2
        indexes.push(index + 0);
        indexes.push(index + 1);
        indexes.push(index + 2);
        index += 3;
    }
};


void WgGeometryData::appendImageBox(float w, float h)
{
    positions.push({ 0.0f, 0.0f });
    positions.push({ w   , 0.0f });
    positions.push({ w   , h });
    positions.push({ 0.0f, h });
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
};


void WgGeometryData::appendBlitBox()
{
    positions.push({ -1.0f, +1.0f });
    positions.push({ +1.0f, +1.0f });
    positions.push({ +1.0f, -1.0f });
    positions.push({ -1.0f, -1.0f });
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
    positions.reserve(rmesh->triangleCnt * 3);
    texCoords.reserve(rmesh->triangleCnt * 3);
    indexes.reserve(rmesh->triangleCnt * 3);
    for (uint32_t i = 0; i < rmesh->triangleCnt; i++) {
        positions.push(rmesh->triangles[i].vertex[0].pt);
        positions.push(rmesh->triangles[i].vertex[1].pt);
        positions.push(rmesh->triangles[i].vertex[2].pt);
        texCoords.push(rmesh->triangles[i].vertex[0].uv);
        texCoords.push(rmesh->triangles[i].vertex[1].uv);
        texCoords.push(rmesh->triangles[i].vertex[2].uv);
        indexes.push(i*3 + 0);
        indexes.push(i*3 + 1);
        indexes.push(i*3 + 2);
    }
};


WgPoint WgGeometryData::interpolate(float t, uint32_t& index)
{
    assert(t >= 0.0f);
    assert(t <= 1.0f);
    // get total length
    float totalLength = 0.0f;
    for (uint32_t i = 0; i < positions.count - 1; i++)
        totalLength += positions[i].dist(positions[i+1]);
    float targetLength = totalLength * t;
    // find current segment index and interpolation factor
    float currentFactor = 0.0f;
    float currentLength = 0.0f;
    for (index = 0; index < positions.count - 1; index++) {
        float segmentLength = positions[index].dist(positions[index+1]);
        currentLength += segmentLength;
        if(currentLength >= targetLength) {
            currentFactor = 1.0f -(currentLength - targetLength) / segmentLength;
            break;
        }
    }
    // get interpolated position and segment index
    return positions[index] * (1.0f - currentFactor) + positions[index + 1] * currentFactor;
}


float WgGeometryData::getLength()
{
    float length = 0.0f;
    for (uint32_t i = 0; i < positions.count - 1; i++)
        length += positions[i].dist(positions[i+1]);
    return length;
}


bool WgGeometryData::getClosestIntersection(WgPoint p1, WgPoint p2, WgPoint& pi, uint32_t& index)
{
    bool finded = false;
    pi = p2;
    float dist = p1.dist(p2);
    for (uint32_t i = 0; i < positions.count - 1; i++) {
        WgPoint p3 = positions[i+0];
        WgPoint p4 = positions[i+1];
        float bot = (p1.x - p2.x)*(p3.y - p4.y) - (p1.y - p2.y)*(p3.x - p4.x);
        if (bot != 0) {
            float top0 = (p1.x - p3.x)*(p3.y - p4.y) - (p1.y - p3.y)*(p3.x - p4.x);
            float top1 = (p1.x - p2.x)*(p1.y - p3.y) - (p1.y - p2.y)*(p1.x - p3.x);
            float t0 = +top0 / bot;
            float t1 = -top1 / bot;
            if ((t0 > 0.0f) && (t0 < 1.0f) && (t1 > 0.0f) && (t1 < 1.0f)) {
                WgPoint p = { p1.x + (p2.x - p1.x) * top0 / bot, p1.y + (p2.y - p1.y) * top0 / bot };
                float d = p.dist(p1);
                if (d < dist) {
                    pi = p;
                    index = i;
                    dist = d;
                    finded = true;
                }
            }
        }
    }
    return finded;
}

bool WgGeometryData::isCW(WgPoint p1, WgPoint p2, WgPoint p3)
{
    WgPoint v1 = p2 - p1;
    WgPoint v2 = p3 - p1;
    return (v1.x*v2.y - v1.y*v2.x) < 0.0;
}


uint32_t WgGeometryData::getIndexMinX()
{
    assert(positions.count > 0);
    uint32_t index = 0;
    for (uint32_t i = 1; i < positions.count; i++)
        if (positions[index].x > positions[i].x) index = i;
    return index;
}


uint32_t WgGeometryData::getIndexMaxX()
{
    assert(positions.count > 0);
    uint32_t index = 0;
    for (uint32_t i = 1; i < positions.count; i++)
        if (positions[index].x < positions[i].x) index = i;
    return index;
}


uint32_t WgGeometryData::getIndexMinY()
{
    assert(positions.count > 0);
    uint32_t index = 0;
    for (uint32_t i = 1; i < positions.count; i++)
        if (positions[index].y > positions[i].y) index = i;
    return index;
}


uint32_t WgGeometryData::getIndexMaxY()
{
    assert(positions.count > 0);
    uint32_t index = 0;
    for (uint32_t i = 1; i < positions.count; i++)
        if (positions[index].y < positions[i].y) index = i;
    return index;
}


void WgGeometryData::close()
{
    if (positions.count > 1) {
        positions.push(positions[0]);
    }
};


void WgGeometryData::clear()
{
    indexes.clear();
    positions.clear();
    texCoords.clear();
}

//***********************************************************************
// WgGeometryDataGroup
//***********************************************************************

void WgGeometryDataGroup::getBBox(WgPoint& pmin, WgPoint& pmax) {
    assert(geometries.count > 0);
    assert(geometries[0]->positions.count > 0);
    pmin = geometries[0]->positions[0];
    pmax = geometries[0]->positions[0];
    for (uint32_t i = 0; i < geometries.count; i++) {
        for (uint32_t j = 0; j < geometries[i]->positions.count; j++) {
            pmin.x = std::min(pmin.x, geometries[i]->positions[j].x);
            pmin.y = std::min(pmin.y, geometries[i]->positions[j].y);
            pmax.x = std::max(pmax.x, geometries[i]->positions[j].x);
            pmax.y = std::max(pmax.y, geometries[i]->positions[j].y);
        }
    }
}


void WgGeometryDataGroup::tesselate(const RenderShape& rshape)
{
    decodePath(rshape, this);
    for (uint32_t i = 0; i < geometries.count; i++)
        geometries[i]->computeTriFansIndexes();
}


void WgGeometryDataGroup::stroke(const RenderShape& rshape)
{
    assert(rshape.stroke);
    auto strokeData = new WgGeometryData();
    // decode
    WgGeometryDataGroup polylines{};
    decodePath(rshape, &polylines);
    // trim -> split -> stroke
    if ((rshape.stroke->dashPattern) && ((rshape.stroke->trim.begin != 0.0f) || (rshape.stroke->trim.end != 1.0f))) {
        WgGeometryDataGroup trimmed{};
        WgGeometryDataGroup splitted{};
        trimPolyline(&polylines, &trimmed, rshape.stroke);
        splitPolyline(&trimmed, &splitted, rshape.stroke);
        strokePolyline(&splitted, strokeData, rshape.stroke);
    } else // trim -> stroke
    if ((rshape.stroke->trim.begin != 0.0f) || (rshape.stroke->trim.end != 1.0f)) {
        WgGeometryDataGroup trimmed{};
        trimPolyline(&polylines, &trimmed, rshape.stroke);
        strokePolyline(&trimmed, strokeData, rshape.stroke);
    } else // split -> stroke
    if (rshape.stroke->dashPattern) {
        WgGeometryDataGroup splitted{};
        splitPolyline(&polylines, &splitted, rshape.stroke);
        strokePolyline(&splitted, strokeData, rshape.stroke);
    } else { // stroke
        strokePolyline(&polylines, strokeData, rshape.stroke);
    }
    // append stroke geometry
    geometries.push(strokeData);
}


void WgGeometryDataGroup::contours(WgGeometryDataGroup& outlines) 
{
    for (uint32_t i = 0 ; i < outlines.geometries.count; i++) {
        WgGeometryData* geometry = new WgGeometryData();
        geometry->computeContour(outlines.geometries[i]);
        geometry->computeTriFansIndexes();
        this->geometries.push(geometry);
    }
}

void WgGeometryDataGroup::release()
{
    for (uint32_t i = 0; i < geometries.count; i++)
        delete geometries[i];
    geometries.clear();
}


void WgGeometryDataGroup::decodePath(const RenderShape& rshape, WgGeometryDataGroup* polyline)
{
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        if (cmd == PathCommand::MoveTo) {
            polyline->geometries.push(new WgGeometryData);
            auto outline = polyline->geometries.last();
            outline->positions.push(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::LineTo) {
            auto outline = polyline->geometries.last();
            if (outline)
                outline->positions.push(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::Close) {
            auto outline = polyline->geometries.last();
            if ((outline) && (outline->positions.count > 0))
                outline->positions.push(outline->positions[0]);
        } else if (cmd == PathCommand::CubicTo) {
            auto outline = polyline->geometries.last();
            if ((outline) && (outline->positions.count > 0))
                outline->appendCubic(
                    rshape.path.pts[pntIndex + 0],
                    rshape.path.pts[pntIndex + 1],
                    rshape.path.pts[pntIndex + 2]
                );
            pntIndex += 3;
        }
    }
}


void WgGeometryDataGroup::trimPolyline(WgGeometryDataGroup* polyline, WgGeometryDataGroup* trimmed, RenderStroke *stroke)
{
    assert(stroke);
    for (uint32_t i = 0; i < polyline->geometries.count; i++) {
        auto segment = new WgGeometryData;
        uint32_t is = 0;
        uint32_t ie = 0;
        WgPoint vs = polyline->geometries[i]->interpolate(stroke->trim.begin, is);
        WgPoint ve = polyline->geometries[i]->interpolate(stroke->trim.end, ie);
        segment->positions.push(vs);
        for (uint32_t j = is+1; j <= ie; j++)
            segment->positions.push(polyline->geometries[i]->positions[j]);
        segment->positions.push(ve);
        trimmed->geometries.push(segment);
    }
}


void WgGeometryDataGroup::splitPolyline(WgGeometryDataGroup* polyline, WgGeometryDataGroup* splitted, RenderStroke *stroke)
{
    assert(stroke);
    for (uint32_t i = 0; i < polyline->geometries.count; i++) {
        auto& vlist = polyline->geometries[i]->positions;
        
        // append single point segment
        if (vlist.count == 1) {
            auto segment = new WgGeometryData;
            segment->positions.push(vlist.last());
            splitted->geometries.push(segment);
        }

        if (vlist.count >= 2) {
            uint32_t icurr = 1;
            uint32_t ipatt = 0;
            WgPoint vcurr = vlist[0];
            while (icurr < vlist.count) {
                if (ipatt % 2 == 0) {
                    splitted->geometries.push(new WgGeometryData);
                    splitted->geometries.last()->positions.push(vcurr);
                }
                float lcurr = stroke->dashPattern[ipatt];
                while ((icurr < vlist.count) && (vlist[icurr].dist(vcurr) < lcurr)) {
                    lcurr -= vlist[icurr].dist(vcurr);
                    vcurr = vlist[icurr];
                    icurr++;
                    if (ipatt % 2 == 0) splitted->geometries.last()->positions.push(vcurr);
                }
                if (icurr < vlist.count) {
                    vcurr = vcurr + (vlist[icurr] - vlist[icurr-1]).normal() * lcurr;
                    if (ipatt % 2 == 0) splitted->geometries.last()->positions.push(vcurr);
                }
                ipatt = (ipatt + 1) % stroke->dashCnt;
            }
        }
    }
}


void WgGeometryDataGroup::strokePolyline(WgGeometryDataGroup* polyline, WgGeometryData* strokes, RenderStroke *stroke)
{
    float wdt = stroke->width / 2;
    for (uint32_t i = 0; i < polyline->geometries.count; i++) {
        auto outline = polyline->geometries[i];

        // single point sub-path
        if (outline->positions.count == 1) {
            if (stroke->cap == StrokeCap::Round) {
                strokes->appendCircle(outline->positions[0], wdt);
            } else if (stroke->cap == StrokeCap::Butt) {
                // for zero length sub-paths no stroke is rendered
            } else if (stroke->cap == StrokeCap::Square) {
                strokes->appendRect(
                    outline->positions[0] + WgPoint(+wdt, +wdt),
                    outline->positions[0] + WgPoint(+wdt, -wdt),
                    outline->positions[0] + WgPoint(-wdt, +wdt),
                    outline->positions[0] + WgPoint(-wdt, -wdt)
                );
            }
        }

        // single line sub-path
        if (outline->positions.count == 2) {
            WgPoint v0 = outline->positions[0];
            WgPoint v1 = outline->positions[1];
            WgPoint dir0 = (v1 - v0).normal();
            WgPoint nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (stroke->cap == StrokeCap::Round) {
                strokes->appendRect(
                    v0 - nrm0 * wdt, v0 + nrm0 * wdt, 
                    v1 - nrm0 * wdt, v1 + nrm0 * wdt);
                strokes->appendCircle(outline->positions[0], wdt);
                strokes->appendCircle(outline->positions[1], wdt);
            } else if (stroke->cap == StrokeCap::Butt) {
                strokes->appendRect(
                    v0 - nrm0 * wdt, v0 + nrm0 * wdt,
                    v1 - nrm0 * wdt, v1 + nrm0 * wdt
                );
            } else if (stroke->cap == StrokeCap::Square) {
                strokes->appendRect(
                    v0 - nrm0 * wdt - dir0 * wdt, v0 + nrm0 * wdt - dir0 * wdt,
                    v1 - nrm0 * wdt + dir0 * wdt, v1 + nrm0 * wdt + dir0 * wdt
                );
            }
        }

        // multi-lined sub-path
        if (outline->positions.count > 2) {
            // append first cap
            WgPoint v0 = outline->positions[0];
            WgPoint v1 = outline->positions[1];
            WgPoint dir0 = (v1 - v0).normal();
            WgPoint nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (stroke->cap == StrokeCap::Round) {
                strokes->appendCircle(v0, wdt);
            } else if (stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (stroke->cap == StrokeCap::Square) {
                strokes->appendRect(
                    v0 - nrm0 * wdt - dir0 * wdt,
                    v0 + nrm0 * wdt - dir0 * wdt,
                    v0 - nrm0 * wdt,
                    v0 + nrm0 * wdt
                );
            }

            // append last cap
            v0 = outline->positions[outline->positions.count - 2];
            v1 = outline->positions[outline->positions.count - 1];
            dir0 = (v1 - v0).normal();
            nrm0 = WgPoint{ -dir0.y, +dir0.x };
            if (stroke->cap == StrokeCap::Round) {
                strokes->appendCircle(v1, wdt);
            } else if (stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (stroke->cap == StrokeCap::Square) {
                strokes->appendRect(
                    v1 - nrm0 * wdt,
                    v1 + nrm0 * wdt,
                    v1 - nrm0 * wdt + dir0 * wdt,
                    v1 + nrm0 * wdt + dir0 * wdt
                );
            }

            // append sub-lines
            for (uint32_t j = 0; j < outline->positions.count - 1; j++) {
                WgPoint v0 = outline->positions[j + 0];
                WgPoint v1 = outline->positions[j + 1];
                WgPoint dir = (v1 - v0).normal();
                WgPoint nrm { -dir.y, +dir.x };
                strokes->appendRect(
                    v0 - nrm * wdt,
                    v0 + nrm * wdt,
                    v1 - nrm * wdt,
                    v1 + nrm * wdt
                );
            }

            // append joints (TODO: separate by joint types)
            for (uint32_t j = 1; j < outline->positions.count - 1; j++) {
                WgPoint v0 = outline->positions[j - 1];
                WgPoint v1 = outline->positions[j + 0];
                WgPoint v2 = outline->positions[j + 1];
                WgPoint dir0 = (v1 - v0).normal();
                WgPoint dir1 = (v1 - v2).normal();
                WgPoint nrm0 { -dir0.y, +dir0.x };
                WgPoint nrm1 { +dir1.y, -dir1.x };
                if (stroke->join == StrokeJoin::Round) {
                    strokes->appendCircle(v1, wdt);
                } else if (stroke->join == StrokeJoin::Bevel) {
                    strokes->appendRect(
                        v1 - nrm0 * wdt, v1 - nrm1 * wdt,
                        v1 + nrm1 * wdt, v1 + nrm0 * wdt
                    );
                } else if (stroke->join == StrokeJoin::Miter) {
                    WgPoint nrm = (dir0 + dir1).normal();
                    float cosine = nrm.dot(nrm0);
                    if ((cosine != 0.0f) && (abs(wdt / cosine) <= stroke->miterlimit * 2)) {
                        strokes->appendRect(v1 + nrm * (wdt / cosine), v1 + nrm0 * wdt, v1 + nrm1 * wdt, v1);
                        strokes->appendRect(v1 - nrm * (wdt / cosine), v1 - nrm0 * wdt, v1 - nrm1 * wdt, v1);
                    } else {
                        strokes->appendRect(
                            v1 - nrm0 * wdt, v1 - nrm1 * wdt,
                            v1 + nrm1 * wdt, v1 + nrm0 * wdt);
                    }
                }
            }
        }
    }
}
