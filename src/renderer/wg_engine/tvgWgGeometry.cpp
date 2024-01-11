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


void WgGeometryData::close()
{
    if (positions.count > 1) {
        positions.push(positions[0]);
    }
};

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
    if (rshape.stroke->dashPattern) {
        // first step: decode path data
        WgGeometryDataGroup segments{};
        decodePath(rshape, &segments);
        // second step: split path to segments using dash patterns
        WgGeometryDataGroup outlines{};
        strokeSegments(rshape, &segments, &outlines);
        // third step: create geometry for strokes
        auto strokeData = new WgGeometryData;
        strokeSublines(rshape, &outlines, strokeData);
        // append strokes geometry data
        geometries.push(strokeData);
    } else {
        // first step: decode path data
        WgGeometryDataGroup outlines{};
        decodePath(rshape, &outlines);
        // second step: create geometry for strokes
        auto strokeData = new WgGeometryData;
        strokeSublines(rshape, &outlines, strokeData);
        // append strokes geometry data
        geometries.push(strokeData);
    }
}


void WgGeometryDataGroup::release()
{
    for (uint32_t i = 0; i < geometries.count; i++)
        delete geometries[i];
    geometries.clear();
}


void WgGeometryDataGroup::decodePath(const RenderShape& rshape, WgGeometryDataGroup* outlines)
{
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        if (cmd == PathCommand::MoveTo) {
            outlines->geometries.push(new WgGeometryData);
            auto outline = outlines->geometries.last();
            outline->positions.push(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::LineTo) {
            auto outline = outlines->geometries.last();
            if (outline)
                outline->positions.push(rshape.path.pts[pntIndex]);
            pntIndex++;
        } else if (cmd == PathCommand::Close) {
            auto outline = outlines->geometries.last();
            if ((outline) && (outline->positions.count > 0))
                outline->positions.push(outline->positions[0]);
        } else if (cmd == PathCommand::CubicTo) {
            auto outline = outlines->geometries.last();
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


void WgGeometryDataGroup::strokeSegments(const RenderShape& rshape, WgGeometryDataGroup* outlines, WgGeometryDataGroup* segments)
{
    for (uint32_t i = 0; i < outlines->geometries.count; i++) {
        auto& vlist = outlines->geometries[i]->positions;
        
        // append single point segment
        if (vlist.count == 1) {
            auto segment = new WgGeometryData;
            segment->positions.push(vlist.last());
            segments->geometries.push(segment);
        }

        if (vlist.count >= 2) {
            uint32_t icurr = 1;
            uint32_t ipatt = 0;
            WgPoint vcurr = vlist[0];
            while (icurr < vlist.count) {
                if (ipatt % 2 == 0) {
                    segments->geometries.push(new WgGeometryData);
                    segments->geometries.last()->positions.push(vcurr);
                }
                float lcurr = rshape.stroke->dashPattern[ipatt];
                while ((icurr < vlist.count) && (vlist[icurr].dist(vcurr) < lcurr)) {
                    lcurr -= vlist[icurr].dist(vcurr);
                    vcurr = vlist[icurr];
                    icurr++;
                    if (ipatt % 2 == 0) segments->geometries.last()->positions.push(vcurr);
                }
                if (icurr < vlist.count) {
                    vcurr = vcurr + (vlist[icurr] - vlist[icurr-1]).normal() * lcurr;
                    if (ipatt % 2 == 0) segments->geometries.last()->positions.push(vcurr);
                }
                ipatt = (ipatt + 1) % rshape.stroke->dashCnt;
            }
        }
    }
}


void WgGeometryDataGroup::strokeSublines(const RenderShape& rshape, WgGeometryDataGroup* outlines, WgGeometryData* strokes)
{
    float wdt = rshape.stroke->width / 2;
    for (uint32_t i = 0; i < outlines->geometries.count; i++) {
        auto outline = outlines->geometries[i];

        // single point sub-path
        if (outline->positions.count == 1) {
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes->appendCircle(outline->positions[0], wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                // for zero length sub-paths no stroke is rendered
            } else if (rshape.stroke->cap == StrokeCap::Square) {
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
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes->appendRect(
                    v0 - nrm0 * wdt, v0 + nrm0 * wdt, 
                    v1 - nrm0 * wdt, v1 + nrm0 * wdt);
                strokes->appendCircle(outline->positions[0], wdt);
                strokes->appendCircle(outline->positions[1], wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                strokes->appendRect(
                    v0 - nrm0 * wdt, v0 + nrm0 * wdt,
                    v1 - nrm0 * wdt, v1 + nrm0 * wdt
                );
            } else if (rshape.stroke->cap == StrokeCap::Square) {
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
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes->appendCircle(v0, wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (rshape.stroke->cap == StrokeCap::Square) {
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
            if (rshape.stroke->cap == StrokeCap::Round) {
                strokes->appendCircle(v1, wdt);
            } else if (rshape.stroke->cap == StrokeCap::Butt) {
                // no cap needed
            } else if (rshape.stroke->cap == StrokeCap::Square) {
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
                if (rshape.stroke->join == StrokeJoin::Round) {
                    strokes->appendCircle(v1, wdt);
                } else if (rshape.stroke->join == StrokeJoin::Bevel) {
                    strokes->appendRect(
                        v1 - nrm0 * wdt, v1 - nrm1 * wdt,
                        v1 + nrm1 * wdt, v1 + nrm0 * wdt
                    );
                } else if (rshape.stroke->join == StrokeJoin::Miter) {
                    WgPoint nrm = (dir0 + dir1).normal();
                    float cosine = nrm.dot(nrm0);
                    if ((cosine != 0.0f) && (abs(wdt / cosine) <= rshape.stroke->miterlimit * 2)) {
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
