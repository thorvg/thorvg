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

#ifndef _TVG_WG_GEOMETRY_H_
#define _TVG_WG_GEOMETRY_H_

#include <cassert>
#include <functional>
#include "tvgMath.h"
#include "tvgRender.h"

// base vector operations
static Point operator-(const Point& a) { return {-a.x, -a.y}; }
static inline float length2(const Point& a) { return a.x*a.x+a.y*a.y; };
static inline float distance2(const Point& a, const Point& b) { return length2(a - b); };
static inline float distance(const Point& a, const Point& b) { return length(a - b); };
static inline float dot(const Point& a, const Point& b) { return a.x*b.x + a.y*b.y; };
static inline Point min(const Point& a, const Point& b) { return { std::min(a.x, b.x), std::min(a.y, b.y) }; };
static inline Point max(const Point& a, const Point& b) { return { std::max(a.x, b.x), std::max(a.y, b.y) }; };
static inline Point lerp(const Point& a, const Point& b, float t) { return a * (1.0f - t) + b * t; };
static inline Point normalize(const Point& a) { float rlen = 1.0f / length(a); return { a.x * rlen, a.y * rlen }; }

// default size of vertex and index buffers
#define WG_POINTS_COUNT 16384

// simple vertex buffer
struct WgVertexBuffer {
    Point vbuff[WG_POINTS_COUNT]; // vertex buffer
    float vdist[WG_POINTS_COUNT]; // distance to previous point
    float vleng[WG_POINTS_COUNT]; // distance to the first point through all previous points
    size_t vcount{};
    bool closed{};

    // callback for external process of polyline
    using onPolylineFn = std::function<void(const WgVertexBuffer& buff)>;

    // reset buffer
    void reset() {
        vcount = 0;
        closed = false;
    }

    // get the last point with optional index offset from the end
    Point last(size_t offset = 0) const {
        return vbuff[vcount - offset - 1];
    }

    // get the last distance with optional index offset from the end
    float lastDist(size_t offset = 0) const {
        return vdist[vcount - offset - 1];
    }

    // get total length
    float total() const {
        return (vcount == 0) ? 0.0f : vleng[vcount-1];
    }

    // get next vertex index by length using binary search
    size_t getIndexByLength(float len) const {
        if (vcount <= 1) return 0;
        size_t left = 0;
        size_t right = vcount - 1;
        while (left <= right) {
            size_t mid = left + (right - left) / 2;
            if (vleng[mid] == len) return mid;
            else if (vleng[mid] < len) left = mid + 1;
            else right = mid - 1;
        }
        return right + 1;
    }

    // get min and max values of the buffer
    void getMinMax(Point& pmin, Point& pmax) const {
        if (vcount == 0) return;
        pmax = pmin = vbuff[0];
        for (size_t i = 1; i < vcount; i++) {
            pmin = min(pmin, vbuff[i]);
            pmax = max(pmax, vbuff[i]);
        }
    }

    // update points distancess to the prev point and total length
    void updateDistances() {
        if (vcount == 0) return;
        vdist[0] = 0.0f;
        vleng[0] = 0.0f;
        for (size_t i = 1; i < vcount; i++) {
            vdist[i] = distance(vbuff[i-1], vbuff[i]);
            vleng[i] = vleng[i-1] + vdist[i];
        }
    }

    // close vertex buffer
    void close() {
        // check if last point is not to close to the first point
        if (!tvg::zero(distance2(vbuff[0], last())))
            append(vbuff[0]);
        closed = true;
    }

    // append point
    void append(const Point& p) {
        vbuff[vcount] = p;
        vcount++;
    }

    // append source vertex buffer in index range from start to end (end not included)
    void appendRange(const WgVertexBuffer& buff, size_t start_index, size_t end_index, bool loop) {
        if (loop) {
            for (size_t i = start_index; i < buff.vcount; i++)
                append(buff.vbuff[i]);
            for (size_t i = 0; i < end_index; i++)
                append(buff.vbuff[i]);
        } else {
            for (size_t i = start_index; i < end_index; i++)
                append(buff.vbuff[i]);
        }
    }

    // append circle (list of triangles)
    void appendCircle(float radius) {
        // get approx circle length
        float clen = 2.0f * radius * MATH_PI;
        size_t nsegs = std::max((uint32_t)(clen / 8), 16U);
        // append circle
        Point prev { std::sin(0.0f) * radius, std::cos(0.0f) * radius };
        for (size_t i = 1; i <= nsegs; i++) {
            float t = (2.0f * MATH_PI * i) / nsegs;
            Point curr { std::sin(t) * radius, std::cos(t) * radius };
            append(Point{0.0f, 0.0f});
            append(prev);
            append(curr);
            prev = curr;
        }
    }

    // append cubic spline
    void appendCubic(const Point& v0, const Point& v1, const Point& v2, const Point& v3) {
        // get approx cubic length
        float clen = distance(v0, v1) + distance(v1, v2) + distance(v2, v3);
        size_t nsegs = std::max((uint32_t)(clen / 8), 16U);
        // append cubic
        Bezier bezier{v0, v1, v2, v3};
        for (size_t i = 1; i <= nsegs; i++)
            append(bezier.at((float)i / nsegs));
    }

    // trim source buffer
    void trim(const WgVertexBuffer& buff, float beg, float end) {
        // empty buffer guard
        if (buff.vcount == 0) return;
        // initialize
        float len_beg = buff.total() * beg;
        float len_end = buff.total() * end;
        // find points
        size_t index_beg = buff.getIndexByLength(len_beg);
        size_t index_end = buff.getIndexByLength(len_end);
        float len_total_beg = buff.vleng[index_beg];
        float len_total_end = buff.vleng[index_end];
        float len_seg_beg = buff.vdist[index_beg];
        float len_seg_end = buff.vdist[index_end];
        // append points
        float t_beg = len_seg_beg > 0.0f ? 1.0f - (len_total_beg - len_beg) / len_seg_beg : 0.0f;
        float t_end = len_seg_end > 0.0f ? 1.0f - (len_total_end - len_end) / len_seg_end : 0.0f;
        if (index_beg > 0) append(lerp(buff.vbuff[index_beg-1], buff.vbuff[index_beg], t_beg));
        appendRange(buff, index_beg, index_end, t_beg > t_end);
        if (index_end > 0) append(lerp(buff.vbuff[index_end-1], buff.vbuff[index_end], t_end));
    }


    // decode path with callback for external prcesses
    void decodePath(const RenderShape& rshape, bool update_dist, onPolylineFn onPolyline) {
        // decode path
        reset();
        size_t pntIndex = 0;
        for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
            PathCommand cmd = rshape.path.cmds[cmdIndex];
            if (cmd == PathCommand::MoveTo) {
                // after path decoding we need to update distances and total length
                if (update_dist) updateDistances();
                if ((onPolyline) && (vcount != 0))
                    onPolyline(*this);
                reset();
                append(rshape.path.pts[pntIndex]);
                pntIndex++;
            } else if (cmd == PathCommand::LineTo) {
                append(rshape.path.pts[pntIndex]);
                pntIndex++;
            } else if (cmd == PathCommand::Close) {
                close();
            } else if (cmd == PathCommand::CubicTo) {
                appendCubic(vbuff[vcount - 1], rshape.path.pts[pntIndex + 0], rshape.path.pts[pntIndex + 1], rshape.path.pts[pntIndex + 2]);
                pntIndex += 3;
            }
        }
        // after path decoding we need to update distances and total length
        if (update_dist) updateDistances();
        if ((onPolyline) && (vcount != 0))
            onPolyline(*this);
    }
};

// simple indexed vertex buffer
struct WgVertexBufferInd {
    Point vbuff[WG_POINTS_COUNT*2];
    Point tbuff[WG_POINTS_COUNT*2];
    uint32_t ibuff[WG_POINTS_COUNT*4];
    size_t vcount = 0;
    size_t icount = 0;

    // reset buffer
    void reset() {
        icount = vcount = 0;
    }

    // get min and max values of the buffer
    void getMinMax(Point& pmin, Point& pmax) const {
        if (vcount == 0) return;
        pmax = pmin = vbuff[0];
        for (size_t i = 1; i < vcount; i++) {
            pmin = min(pmin, vbuff[i]);
            pmax = max(pmax, vbuff[i]);
        }
    }

    // append image box with tex coords
    void appendImageBox(float w, float h) {
        Point points[4] { { 0.0f, 0.0f }, { w, 0.0f }, { w, h }, { 0.0f, h } };
        appendImageBox(points);
    }

    // append blit box with tex coords
    void appendBlitBox() {
        Point points[4] { { -1.0f, +1.0f }, { +1.0f, +1.0f }, { +1.0f, -1.0f }, { -1.0f, -1.0f } };
        appendImageBox(points);
    }

    // append image box with tex coords
    void appendImageBox(Point points[4]) {
        // append vertexes
        vbuff[vcount+0] = points[0];
        vbuff[vcount+1] = points[1];
        vbuff[vcount+2] = points[2];
        vbuff[vcount+3] = points[3];
        // append tex coords
        tbuff[vcount+0] = { 0.0f, 0.0f };
        tbuff[vcount+1] = { 1.0f, 0.0f };
        tbuff[vcount+2] = { 1.0f, 1.0f };
        tbuff[vcount+3] = { 0.0f, 1.0f };
        // append indexes
        ibuff[icount+0] = vcount + 0;
        ibuff[icount+1] = vcount + 1;
        ibuff[icount+2] = vcount + 2;
        ibuff[icount+3] = vcount + 0;
        ibuff[icount+4] = vcount + 2;
        ibuff[icount+5] = vcount + 3;
        // update buffer
        vcount += 4;
        icount += 6;
    }

    // append quad - two triangles formed from four points
    void appendQuad(const Point& p0, const Point& p1, const Point& p2, const Point& p3) {
        // append vertexes
        vbuff[vcount+0] = p0;
        vbuff[vcount+1] = p1;
        vbuff[vcount+2] = p2;
        vbuff[vcount+3] = p3;
        // append indexes
        ibuff[icount+0] = vcount + 0;
        ibuff[icount+1] = vcount + 1;
        ibuff[icount+2] = vcount + 2;
        ibuff[icount+3] = vcount + 1;
        ibuff[icount+4] = vcount + 3;
        ibuff[icount+5] = vcount + 2;
        // update buffer
        vcount += 4;
        icount += 6;
    }
    
    // dash buffer by pattern
    void appendStrokesDashed(const WgVertexBuffer& buff, const RenderStroke* rstroke) {
        // dashed buffer
        WgVertexBuffer dashed;
        dashed.reset();
        // ignore single points polyline
        if (buff.vcount < 2) return;
        const float* dashPattern = rstroke->dashPattern;
        size_t dashCnt = rstroke->dashCnt;
        // starting state
        uint32_t index_dash = 0;
        float len_total = dashPattern[index_dash];
        // iterate by polyline points
        for (uint32_t i = 0; i < buff.vcount - 1; i++) {
            // append current polyline point
            if (index_dash % 2 == 0)
                dashed.append(buff.vbuff[i]);
            // move inside polyline segment
            while(len_total < buff.vdist[i+1]) {
                // get current point
                float t = len_total / buff.vdist[i+1];
                dashed.append(buff.vbuff[i] + (buff.vbuff[i+1] - buff.vbuff[i]) * t);
                // update current state
                index_dash = (index_dash + 1) % dashCnt;
                len_total += dashPattern[index_dash];
                // preceed stroke if dash
                if (index_dash % 2 != 0) {
                    dashed.updateDistances();
                    appendStrokes(dashed, rstroke);
                    dashed.reset();
                }
            }
            // update current subline length
            len_total -= buff.vdist[i+1];
        }
        // draw last subline
        if (index_dash % 2 == 0) {
            dashed.append(buff.last());
            dashed.updateDistances();
            appendStrokes(dashed, rstroke);
        }
    }

    // append buffer with optional offset
    void appendBuffer(const WgVertexBuffer& buff, Point offset = Point{0.0f, 0.0f}) {
        for (uint32_t i = 0; i < buff.vcount; i++ ) {
            vbuff[vcount + i] = buff.vbuff[i] + offset;
            ibuff[icount + i] = vcount + i;
        }
        vcount += buff.vcount;
        icount += buff.vcount;
    };

    // append line
    void appendLine(const Point& v0, const Point& v1, float dist, float halfWidth) {
        Point sub = v1 - v0;
        Point nrm = { +sub.y / dist * halfWidth, -sub.x / dist * halfWidth };
        appendQuad(v0 - nrm, v0 + nrm, v1 - nrm, v1 + nrm);
    }

    // append bevel joint
    void appendBevel(const Point& v0, const Point& v1, const Point& v2, float dist1, float dist2, float halfWidth) {
        Point sub1 = v1 - v0;
        Point sub2 = v2 - v1;
        Point nrm1 { +sub1.y / dist1 * halfWidth, -sub1.x / dist1 * halfWidth };
        Point nrm2 { +sub2.y / dist2 * halfWidth, -sub2.x / dist2 * halfWidth };
        appendQuad(v1 - nrm1, v1 + nrm1, v1 - nrm2, v1 + nrm2);
    }

    // append miter joint
    void appendMitter(const Point& v0, const Point& v1, const Point& v2, float dist1, float dist2, float halfWidth, float miterLimit) {
        Point sub1 = v1 - v0;
        Point sub2 = v2 - v1;
        Point nrm1 { +sub1.y / dist1, -sub1.x / dist1 };
        Point nrm2 { +sub2.y / dist2, -sub2.x / dist2 };
        Point offset1 = nrm1 * halfWidth;
        Point offset2 = nrm2 * halfWidth;
        Point nrm = normalize(nrm1 + nrm2);
        float cosine = dot(nrm, nrm1);
        float angle = std::acos(dot(nrm1, -nrm2));
        float miterRatio = 1.0f / (std::sin(angle) * 0.5f);
        if (miterRatio <= miterLimit) {
            appendQuad(v1 + nrm * (halfWidth / cosine), v1 + offset2, v1 + offset1, v1);
            appendQuad(v1 - nrm * (halfWidth / cosine), v1 - offset2, v1 - offset1, v1);
        } else {
            appendQuad(v1 - offset1, v1 + offset2, v1 - offset2, v1 + offset1);
        }
    }

    // append square cap
    void appendSquare(Point v0, Point v1, float dist, float halfWidth) {
        Point sub = v1 - v0;
        Point offset = sub / dist * halfWidth;
        Point nrm = { +offset.y, -offset.x };
        appendQuad(v1 - nrm, v1 + nrm, v1 + offset - nrm, v1 + offset + nrm);
    }

    // append strokes
    void appendStrokes(const WgVertexBuffer& buff, const RenderStroke* rstroke) {
        assert(rstroke);
        // empty buffer gueard
        if (buff.vcount < 2) return;
        float halfWidth = rstroke->width * 0.5f;

        // append core lines
        for (size_t i = 1; i < buff.vcount; i++)
            appendLine(buff.vbuff[i-1], buff.vbuff[i], buff.vdist[i], halfWidth);

        // append caps (square)
        if ((rstroke->cap == StrokeCap::Square) && !buff.closed) {
            appendSquare(buff.vbuff[1], buff.vbuff[0], buff.vdist[1], halfWidth);
            appendSquare(buff.last(1), buff.last(0), buff.lastDist(0), halfWidth);
        }

        // append round joints and caps
        if ((rstroke->join == StrokeJoin::Round) || (rstroke->cap == StrokeCap::Round)) {
            // create mesh for circle
            WgVertexBuffer circle;
            circle.appendCircle(halfWidth);
            // append caps (round)
            if (rstroke->cap == StrokeCap::Round) {
                appendBuffer(circle, buff.vbuff[0]);
                // append ending cap if polyline is not closed
                if (!buff.closed) 
                    appendBuffer(circle, buff.last());
            }
            // append joints (round)
            if (rstroke->join == StrokeJoin::Round) {
                for (size_t i = 1; i < buff.vcount - 1; i++)
                    appendBuffer(circle, buff.vbuff[i]);
                if (buff.closed) appendBuffer(circle, buff.last());
            }
        }

        // append closed endings
        if (buff.closed) {
            // close by bevel
            if (rstroke->join == StrokeJoin::Bevel)
                appendBevel(buff.last(1), buff.vbuff[0], buff.vbuff[1], buff.lastDist(0), buff.vdist[1], halfWidth);
            // close by mitter
            else if (rstroke->join == StrokeJoin::Miter) {
                appendMitter(buff.last(1), buff.vbuff[0], buff.vbuff[1], buff.lastDist(0), buff.vdist[1], halfWidth, rstroke->miterlimit);
            }
        }

        // append joints (bevel)
        if (rstroke->join == StrokeJoin::Bevel) {
            for (size_t i = 1; i < buff.vcount - 1; i++)
                appendBevel(buff.vbuff[i-1], buff.vbuff[i], buff.vbuff[i+1], buff.vdist[i], buff.vdist[i+1], halfWidth);
        // append joints (mitter)
        } else if (rstroke->join == StrokeJoin::Miter) {
            for (size_t i = 1; i < buff.vcount - 1; i++)
                appendMitter(buff.vbuff[i-1], buff.vbuff[i], buff.vbuff[i+1], buff.vdist[i], buff.vdist[i+1], halfWidth, rstroke->miterlimit);
        }
    }
};

#endif // _TVG_WG_GEOMETRY_H_