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

#ifndef _TVG_WG_GEOMETRY_H_
#define _TVG_WG_GEOMETRY_H_

#include <cassert>
#include "tvgMath.h"
#include "tvgRender.h"


// default size of vertex and index buffers
#define WG_DEFAULT_BUFFER_SIZE 2048

struct WgVertexBuffer;
struct WgIndexedVertexBuffer;

struct WgGeometryBufferPool
{
private:
    Array<WgVertexBuffer*> vbuffers;
    Array<WgIndexedVertexBuffer*> ibuffers;

public:
    ~WgGeometryBufferPool();

    WgVertexBuffer* reqVertexBuffer(float scale = 1.0f);
    WgIndexedVertexBuffer* reqIndexedVertexBuffer(float scale = 1.0f);
    void retVertexBuffer(WgVertexBuffer* buffer);
    void retIndexedVertexBuffer(WgIndexedVertexBuffer* buffer);

    static WgGeometryBufferPool* instance();  //return the shared buffer pool
};


// simple vertex buffer
struct WgVertexBuffer
{
    Point* data;           // vertex buffer
    struct Distance {
        float interval;    // distance to previous point
        float length;      // distance to the first point through all previous points
    } *dist;
    uint32_t count = 0;
    uint32_t reserved = WG_DEFAULT_BUFFER_SIZE;
    float scale;           // tesselation scale
    bool closed = false;

    // callback for external process of polyline
    using onPolylineFn = std::function<void(const WgVertexBuffer& buff)>;

    WgVertexBuffer(float scale = 1.0f) : scale(scale)
    {
        data = tvg::malloc<Point*>(sizeof(Point) * reserved);
        dist = tvg::malloc<Distance*>(sizeof(Distance) * reserved);
    }

    ~WgVertexBuffer()
    {
        tvg::free(data);
        tvg::free(dist);
    }

    // reset buffer
    void reset(float scale)
    {
        count = 0;
        closed = false;
        this->scale = scale;
    }

    // get the last point with optional index offset from the end
    Point last(size_t offset = 0) const
    {
        return data[count - offset - 1];
    }

    // get the last distance with optional index offset from the end
    float lastDist(size_t offset = 0) const
    {
        return dist[count - offset - 1].interval;
    }

    // get total length
    float total() const
    {
        return (count == 0) ? 0.0f : dist[count-1].length;
    }

    // get next vertex index by length using binary search
    size_t getIndexByLength(float len) const
    {
        if (count <= 1) return 0;
        size_t left = 0;
        size_t right = count - 1;
        while (left <= right) {
            size_t mid = left + (right - left) / 2;
            if (dist[mid].length == len) return mid;
            else if (dist[mid].length < len) left = mid + 1;
            else right = mid - 1;
        }
        return right + 1;
    }

    // get min and max values of the buffer
    void getMinMax(Point& pmin, Point& pmax) const
    {
        if (count == 0) return;
        pmax = pmin = data[0];
        for (size_t i = 1; i < count; i++) {
            pmin = min(pmin, data[i]);
            pmax = max(pmax, data[i]);
        }
    }

    // update points distancess to the prev point and total length
    void updateDistances()
    {
        if (count == 0) return;
        dist[0].interval = 0.0f;
        dist[0].length = 0.0f;
        for (size_t i = 1; i < count; i++) {
            dist[i].interval = tvg::length(data[i-1] - data[i]);
            dist[i].length = dist[i-1].length + dist[i].interval;
        }
    }

    // close vertex buffer
    void close()
    {
        // check if last point is not to close to the first point
        if (length(data[0] - last()) > 0.015625f) {
            append(data[0]);
        }
        closed = true;
    }

    // append point
    void append(const Point& p)
    {
        if (count >= reserved) {
            reserved *= 2;
            data = tvg::realloc<Point*>(data, reserved * sizeof(Point));
            dist = tvg::realloc<Distance*>(dist, reserved * sizeof(Distance));
        }
        data[count++] = p;
    }

    // append source vertex buffer in index range from start to end (end not included)
    void appendRange(const WgVertexBuffer& buff, size_t start_index, size_t end_index)
    {
        for (size_t i = start_index; i < end_index; i++) {
            append(buff.data[i]);
        }
    }

    // append circle (list of triangles)
    void appendCircle(float radius)
    {
        // get approx circle length
        float clen = 2.0f * radius * MATH_PI;
        size_t nsegs = std::max((uint32_t)(clen * scale / 8), 16U);
        // append circle^
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
    void appendCubic(const Point& v0, const Point& v1, const Point& v2, const Point& v3)
    {
        // get approx cubic length
        float clen = (tvg::length(v0 - v1) + tvg::length(v1 - v2) + tvg::length(v2 - v3));
        size_t nsegs = std::max((uint32_t)(clen * scale / 16), 16U);
        // append cubic
        Bezier bezier{v0, v1, v2, v3};
        for (size_t i = 1; i <= nsegs; i++) {
            append(bezier.at((float)i / nsegs));
        }
    }

    // decode path with callback for external prcesses
    void decodePath(const RenderShape& rshape, bool update_dist, onPolylineFn onPolyline, bool trim = false)
    {
        // decode path
        reset(scale);

        PathCommand *cmds, *trimmedCmds = nullptr;
        Point *pts, *trimmedPts = nullptr;
        uint32_t cmdCnt{};

        if (trim) {
            RenderPath trimmedPath;
            if (!rshape.stroke->trim.trim(rshape.path, trimmedPath)) return;

            cmds = trimmedCmds = trimmedPath.cmds.data;
            cmdCnt = trimmedPath.cmds.count;
            pts = trimmedPts = trimmedPath.pts.data;

            trimmedPath.cmds.data = nullptr;
            trimmedPath.pts.data = nullptr;
        } else {
            cmds = rshape.path.cmds.data;
            cmdCnt = rshape.path.cmds.count;
            pts = rshape.path.pts.data;
        }

        size_t pntIndex = 0;
        for (uint32_t i = 0; i < cmdCnt; i++) {
            auto& cmd = cmds[i];
            if (cmd == PathCommand::MoveTo) {
                // after path decoding we need to update distances and total length
                if (update_dist) updateDistances();
                if ((onPolyline) && (count > 0)) onPolyline(*this);
                reset(scale);
                append(pts[pntIndex]);
                pntIndex++;
            } else if (cmd == PathCommand::LineTo) {
                append(pts[pntIndex]);
                pntIndex++;
            } else if (cmd == PathCommand::Close) {
                close();
                // proceed path if close command is not the last command and next command is LineTo or CubicTo
                if (i + 1 < cmdCnt && (cmds[i + 1] == PathCommand::LineTo || cmds[i + 1] == PathCommand::CubicTo)) {
                    // proceed current path
                    if (update_dist) updateDistances();
                    if ((count > 0) && (onPolyline)) onPolyline(*this);
                    // append closing point of current path as a first point of the new path
                    Point last_pt = last();
                    reset(scale);
                    append(last_pt);
                }
            } else if (cmd == PathCommand::CubicTo) {
                // append tesselated cubic spline with scale param
                appendCubic(data[count - 1], pts[pntIndex + 0], pts[pntIndex + 1], pts[pntIndex + 2]);
                pntIndex += 3;
            }
        }

        tvg::free(trimmedCmds);
        tvg::free(trimmedPts);

        // after path decoding we need to update distances and total length
        if (update_dist) updateDistances();
        if ((count > 0) && (onPolyline)) onPolyline(*this);
        reset(scale);
    }
};


struct WgIndexedVertexBuffer
{
    Point* vbuff;
    uint32_t* ibuff;
    uint32_t vcount = 0, icount = 0;
    size_t vreserved = WG_DEFAULT_BUFFER_SIZE;
    size_t ireserved = WG_DEFAULT_BUFFER_SIZE * 2;
    WgGeometryBufferPool* pool;
    WgVertexBuffer* dashed;   // intermediate buffer for stroke dashing
    float scale;

    WgIndexedVertexBuffer(WgGeometryBufferPool* pool, float scale = 1.0f) : pool(pool), scale(scale)
    {
        vbuff = tvg::malloc<Point*>(sizeof(Point) * vreserved);
        ibuff = tvg::malloc<uint32_t*>(sizeof(uint32_t) * ireserved);
        dashed = pool->reqVertexBuffer();
    }

    ~WgIndexedVertexBuffer()
    {
        pool->retVertexBuffer(dashed);
        tvg::free(vbuff);
        tvg::free(ibuff);
    }

    // reset buffer
    void reset(float scale)
    {
        icount = vcount = 0;
        this->scale = scale;
    }

    void growIndex(size_t grow)
    {
        if (icount + grow >= ireserved) {
            ireserved *= 2;
            ibuff = tvg::realloc<uint32_t*>(ibuff, ireserved * sizeof(uint32_t));
        }
    }

    void growVertex(size_t grow)
    {
        if (vcount + grow >= vreserved) {
            vreserved *= 2;
            vbuff = tvg::realloc<Point*>(vbuff, vreserved * sizeof(Point));
        }
    }

    // get min and max values of the buffer
    void getMinMax(Point& pmin, Point& pmax) const
    {
        if (vcount == 0) return;
        pmax = pmin = vbuff[0];
        for (size_t i = 1; i < vcount; i++) {
            pmin = min(pmin, vbuff[i]);
            pmax = max(pmax, vbuff[i]);
        }
    }

    // append quad - two triangles formed from four points
    void appendQuad(const Point& p0, const Point& p1, const Point& p2, const Point& p3)
    {
        growVertex(4);
        vbuff[vcount+0] = p0;
        vbuff[vcount+1] = p1;
        vbuff[vcount+2] = p2;
        vbuff[vcount+3] = p3;

        growIndex(6);
        ibuff[icount+0] = vcount + 0;
        ibuff[icount+1] = vcount + 1;
        ibuff[icount+2] = vcount + 2;
        ibuff[icount+3] = vcount + 1;
        ibuff[icount+4] = vcount + 3;
        ibuff[icount+5] = vcount + 2;

        vcount += 4;
        icount += 6;
    }


    void appendStrokesDashed(const WgVertexBuffer& buff, const RenderStroke* rstroke)
    {
        dashed->reset(scale);
        auto& dash = rstroke->dash;

        if (buff.count < 2 || tvg::zero(dash.length)) return;

        uint32_t index = 0;
        auto total = dash.pattern[index];
        auto length = (dash.count % 2) ? dash.length * 2 : dash.length;

        // normalize dash offset
        auto dashOffset = dash.offset;
        while(dashOffset < 0) dashOffset += length;
        while(dashOffset > length) dashOffset -= length;
        auto gap = false;

        // scip dashes by offset
        while(total <= dashOffset) {
            index = (index + 1) % dash.count;
            total += dash.pattern[index];
            gap = !gap;
        }
        total -= dashOffset;

        // iterate by polyline points
        for (uint32_t i = 0; i < buff.count - 1; i++) {
            if (!gap) dashed->append(buff.data[i]);
            // move inside polyline segment
            while(total < buff.dist[i+1].interval) {
                // get current point
                dashed->append(tvg::lerp(buff.data[i], buff.data[i+1], total / buff.dist[i+1].interval));
                // update current state
                index = (index + 1) % dash.count;
                total += dash.pattern[index];
                // preceed stroke if dash
                if (!gap) {
                    dashed->updateDistances();
                    appendStrokes(*dashed, rstroke);
                    dashed->reset(scale);
                }
                gap = !gap;
            }
            // update current subline length
            total -= buff.dist[i+1].interval;
        }
        // draw last subline
        if (!gap) {
            dashed->append(buff.last());
            dashed->updateDistances();
            appendStrokes(*dashed, rstroke);
        }
    }

    // append buffer with optional offset
    void appendBuffer(const WgVertexBuffer& buff, Point offset = Point{0.0f, 0.0f})
    {
        growVertex(buff.count);
        growIndex(buff.count);

        for (uint32_t i = 0; i < buff.count; i++) {
            vbuff[vcount + i] = buff.data[i] + offset;
            ibuff[icount + i] = vcount + i;
        }
        vcount += buff.count;
        icount += buff.count;
    };

    void appendLine(const Point& v0, const Point& v1, float dist, float halfWidth)
    {
        if(tvg::zero(dist)) return;
        Point sub = v1 - v0;
        Point nrm = {sub.y / dist * halfWidth, -sub.x / dist * halfWidth};
        appendQuad(v0 - nrm, v0 + nrm, v1 - nrm, v1 + nrm);
    }

    void appendBevel(const Point& v0, const Point& v1, const Point& v2, float dist1, float dist2, float halfWidth)
    {
        if(tvg::zero(dist1) || tvg::zero(dist2)) return;
        Point sub1 = v1 - v0;
        Point sub2 = v2 - v1;
        Point nrm1 {sub1.y / dist1 * halfWidth, -sub1.x / dist1 * halfWidth};
        Point nrm2 {sub2.y / dist2 * halfWidth, -sub2.x / dist2 * halfWidth};
        appendQuad(v1 - nrm1, v1 + nrm1, v1 - nrm2, v1 + nrm2);
    }

    void appendMiter(const Point& v0, const Point& v1, const Point& v2, float dist1, float dist2, float halfWidth, float miterLimit)
    {
        if(tvg::zero(dist1) || tvg::zero(dist2)) return;
        auto sub1 = v1 - v0;
        auto sub2 = v2 - v1;
        auto nrm1 = Point{+sub1.y / dist1, -sub1.x / dist1};
        auto nrm2 = Point{+sub2.y / dist2, -sub2.x / dist2};
        auto offset1 = nrm1 * halfWidth;
        auto offset2 = nrm2 * halfWidth;
        auto nrm = nrm1 + nrm2;
        normalize(nrm);
        float cosine = dot(nrm, nrm1);
        if (tvg::zero(cosine)) return;
        float angle = std::acos(dot(nrm1, -nrm2));
        if (tvg::zero(angle) || tvg::equal(angle, MATH_PI)) return;
        float miterRatio = 1.0f / (std::sin(angle) * 0.5f);
        if (miterRatio <= miterLimit) {
            appendQuad(v1 + nrm * (halfWidth / cosine), v1 + offset2, v1 + offset1, v1);
            appendQuad(v1 - nrm * (halfWidth / cosine), v1 - offset2, v1 - offset1, v1);
        } else {
            appendQuad(v1 - offset1, v1 + offset2, v1 - offset2, v1 + offset1);
        }
    }

    void appendSquare(Point v0, Point v1, float dist, float halfWidth)
    {
        if(tvg::zero(dist)) return;
        Point sub = v1 - v0;
        Point offset = sub / dist * halfWidth;
        Point nrm = {+offset.y, -offset.x};
        appendQuad(v1 - nrm, v1 + nrm, v1 + offset - nrm, v1 + offset + nrm);
    }

    void appendStrokes(const WgVertexBuffer& buff, const RenderStroke* rstroke)
    {
        assert(rstroke);
        // empty buffer gueard
        if (buff.count < 2) return;

        float halfWidth = rstroke->width * 0.5f;

        // append core lines
        for (size_t i = 1; i < buff.count; i++) {
            appendLine(buff.data[i-1], buff.data[i], buff.dist[i].interval, halfWidth);
        }

        // append caps (square)
        if ((rstroke->cap == StrokeCap::Square) && !buff.closed) {
            appendSquare(buff.data[1], buff.data[0], buff.dist[1].interval, halfWidth);
            appendSquare(buff.last(1), buff.last(0), buff.lastDist(0), halfWidth);
        }

        // append round joints and caps
        if ((rstroke->join == StrokeJoin::Round) || (rstroke->cap == StrokeCap::Round)) {
            // create mesh for circle
            WgVertexBuffer circle;
            circle.reset(buff.scale);
            circle.appendCircle(halfWidth);
            // append caps (round)
            if (rstroke->cap == StrokeCap::Round) {
                appendBuffer(circle, buff.data[0]);
                // append ending cap if polyline is not closed
                if (!buff.closed) appendBuffer(circle, buff.last());
            }
            // append joints (round)
            if (rstroke->join == StrokeJoin::Round) {
                for (size_t i = 1; i < buff.count - 1; i++) {
                    appendBuffer(circle, buff.data[i]);
                }
                if (buff.closed) appendBuffer(circle, buff.last());
            }
        }

        // append closed endings
        if (buff.closed) {
            // close by bevel
            if (rstroke->join == StrokeJoin::Bevel) {
                appendBevel(buff.last(1), buff.data[0], buff.data[1], buff.lastDist(0), buff.dist[1].interval, halfWidth);
            // close by mitter
            } else if (rstroke->join == StrokeJoin::Miter) {
                appendMiter(buff.last(1), buff.data[0], buff.data[1], buff.lastDist(0), buff.dist[1].interval, halfWidth, rstroke->miterlimit);
            }
        }

        // append joints (bevel)
        if (rstroke->join == StrokeJoin::Bevel) {
            for (size_t i = 1; i < buff.count - 1; i++) {
                appendBevel(buff.data[i-1], buff.data[i], buff.data[i+1], buff.dist[i].interval, buff.dist[i+1].interval, halfWidth);
            }
        // append joints (mitter)
        } else if (rstroke->join == StrokeJoin::Miter) {
            for (size_t i = 1; i < buff.count - 1; i++) {
                appendMiter(buff.data[i-1], buff.data[i], buff.data[i+1], buff.dist[i].interval, buff.dist[i+1].interval, halfWidth, rstroke->miterlimit);
            }
        }
    }
};

#endif // _TVG_WG_GEOMETRY_H_