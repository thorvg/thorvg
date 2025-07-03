/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgMath.h"
#include "tvgRender.h"

/************************************************************************/
/* RenderMethod Class Implementation                                    */
/************************************************************************/

uint32_t RenderMethod::ref()
{
    ScopedLock lock(key);
    return (++refCnt);
}


uint32_t RenderMethod::unref()
{
    ScopedLock lock(key);
    return (--refCnt);
}


RenderRegion RenderMethod::viewport()
{
    return vport;
}


bool RenderMethod::viewport(const RenderRegion& vp)
{
    vport = vp;
    return true;
}


/************************************************************************/
/* RenderPath Class Implementation                                      */
/************************************************************************/

bool RenderPath::bounds(Matrix* m, float* x, float* y, float* w, float* h)
{
    //unexpected
    if (cmds.empty() || cmds.first() == PathCommand::CubicTo) return false;

    auto min = Point{FLT_MAX, FLT_MAX};
    auto max = Point{-FLT_MAX, -FLT_MAX};

    auto pt = pts.begin();
    auto cmd = cmds.begin();

    auto assign = [&](const Point& pt, Point& min, Point& max) -> void {
        if (pt.x < min.x) min.x = pt.x;
        if (pt.y < min.y) min.y = pt.y;
        if (pt.x > max.x) max.x = pt.x;
        if (pt.y > max.y) max.y = pt.y;
    };

    while (cmd < cmds.end()) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                //skip the invalid assignments
                if (cmd + 1 < cmds.end()) {
                    auto next = *(cmd + 1);
                    if (next == PathCommand::LineTo || next == PathCommand::CubicTo) {
                        assign(*pt * m, min, max);
                    }
                }
                ++pt;
                break;
            }
            case PathCommand::LineTo: {
                assign(*pt * m, min, max);
                ++pt;
                break;
            }
            case PathCommand::CubicTo: {
                Bezier bz = {pt[-1] * m, pt[0] * m, pt[1] * m, pt[2] * m};
                bz.bounds(min, max);
                pt += 3;
                break;
            }
            default: break;
        }
        ++cmd;
    }

    if (x) *x = min.x;
    if (y) *y = min.y;
    if (w) *w = max.x - min.x;
    if (h) *h = max.y - min.y;

    return true;
}

/************************************************************************/
/* RenderRegion Class Implementation                                    */
/************************************************************************/


void RenderRegion::intersect(const RenderRegion& rhs)
{
    if (min.x < rhs.min.x) min.x = rhs.min.x;
    if (min.y < rhs.min.y) min.y = rhs.min.y;
    if (max.x > rhs.max.x) max.x = rhs.max.x;
    if (max.y > rhs.max.y) max.y = rhs.max.y;

    // Not intersected: collapse to zero-area region
    if (max.x < min.x) max.x = min.x;
    if (max.y < min.y) max.y = min.y;
}

#ifdef THORVG_PARTIAL_RENDER_SUPPORT

#include <algorithm>

void RenderDirtyRegion::init(uint32_t w, uint32_t h)
{
    auto cnt = int(sqrt(PARTITIONING));
    auto px = int32_t(w / cnt);
    auto py = int32_t(h / cnt);
    auto lx = int32_t(w % cnt);
    auto ly = int32_t(h % cnt);

    //space partitioning
    for (int y = 0; y < cnt; ++y) {
        for (int x = 0; x < cnt; ++x) {
            auto& partition = partitions[y * cnt + x];
            partition.list[0].reserve(64);
            auto& region = partition.region;
            region.min = {x * px, y * py};
            region.max = {region.min.x + px, region.min.y + py};
            //leftovers
            if (x == cnt -1) region.max.x += lx;
            if (y == cnt -1) region.max.y += ly;
        }
    }
}


bool RenderDirtyRegion::add(const RenderRegion& bbox)
{
    for (int idx = 0; idx < PARTITIONING; ++idx) {
        auto& partition = partitions[idx];
        if (bbox.max.y <= partition.region.min.y) break;
        if (bbox.intersected(partition.region)) {
            ScopedLock lock(key);
            partition.list[partition.current].push(RenderRegion::intersect(bbox, partition.region));
        }
    }
    return true;
}


bool RenderDirtyRegion::add(const RenderRegion& prv, const RenderRegion& cur)
{
    if (prv == cur) return add(prv);

    for (int idx = 0; idx < PARTITIONING; ++idx) {
        auto& partition = partitions[idx];
        if (prv.intersected(partition.region)) {
            ScopedLock lock(key);
            partition.list[partition.current].push(RenderRegion::intersect(prv, partition.region));
        }
        if (cur.intersected(partition.region)) {
            ScopedLock lock(key);
            partition.list[partition.current].push(RenderRegion::intersect(cur, partition.region));
        }
    }
    return true;
}


void RenderDirtyRegion::clear()
{
    for (int idx = 0; idx < PARTITIONING; ++idx) {
        partitions[idx].list[0].clear();
        partitions[idx].list[1].clear();
    }
}


void RenderDirtyRegion::subdivide(Array<RenderRegion>& targets, uint32_t idx, RenderRegion& lhs, RenderRegion& rhs)
{
    RenderRegion temp[5];
    int cnt = 0;
    temp[cnt++] = RenderRegion::intersect(lhs, rhs);
    auto max = std::min(lhs.max.x, rhs.max.x);

    auto subtract = [&](RenderRegion& lhs, RenderRegion& rhs) {
        //top
        if (rhs.min.y < lhs.min.y) {
            temp[cnt++] = {{rhs.min.x, rhs.min.y}, {rhs.max.x, lhs.min.y}};
            rhs.min.y = lhs.min.y;
        }
        //bottom
        if (rhs.max.y > lhs.max.y) {
            temp[cnt++] = {{rhs.min.x, lhs.max.y}, {rhs.max.x, rhs.max.y}};
            rhs.max.y = lhs.max.y;
        }
        //left
        if (rhs.min.x < lhs.min.x) {
            temp[cnt++] = {{rhs.min.x, rhs.min.y}, {lhs.min.x, rhs.max.y}};
            rhs.min.x = lhs.min.x;
        }
        //right
        if (rhs.max.x > lhs.max.x) {
            temp[cnt++] = {{lhs.max.x, rhs.min.y}, {rhs.max.x, rhs.max.y}};
            //rhs.max.x = lhs.max.x;
        }
    };

    subtract(temp[0], lhs);
    subtract(temp[0], rhs);

    //Please reserve memory enough with targets.reserve()
    if (targets.count + cnt - 1 > targets.reserved) {
        TVGERR("RENDERER", "reserved(%d), required(%d)", targets.reserved, targets.count + cnt - 1);
        return;
    }

    /* Considered using a list to avoid memory shifting,
       but ultimately, the array outperformed the list due to better cache locality. */

    //shift data
    auto dst = &targets[idx + cnt];
    memmove(dst, &targets[idx + 1], sizeof(RenderRegion) * (targets.count - idx - 1));
    memcpy(&targets[idx], temp, sizeof(RenderRegion) * cnt);
    targets.count += (cnt - 1);

    //sorting by x coord again, only for the updated region
    while (dst < targets.end() && dst->min.x < max) ++dst;
    stable_sort(&targets[idx], dst, [](const RenderRegion& a, const RenderRegion& b) -> bool {
        return a.min.x < b.min.x;
    });
}


void RenderDirtyRegion::commit()
{
    if (disabled) return;

    for (int idx = 0; idx < PARTITIONING; ++idx) {
        auto current = partitions[idx].current;
        auto& targets = partitions[idx].list[current];
        if (targets.empty()) continue;

        current = !current; //swapping buffers
        auto& output = partitions[idx].list[current];

        targets.reserve(targets.count * 10);  //one intersection can be divided up to 5
        output.reserve(targets.count);

        partitions[idx].current = current;

        //sorting by x coord. guarantee the stable performance: O(NlogN)
        stable_sort(targets.begin(), targets.end(), [](const RenderRegion& a, const RenderRegion& b) -> bool {
            return a.min.x < b.min.x;
        });

        //Optimized using sweep-line algorithm: O(NlogN)
        for (uint32_t i = 0; i < targets.count; ++i) {
            auto& lhs = targets[i];
            if (lhs.invalid()) continue;
            auto merged = false;

            for (uint32_t j = i + 1; j < targets.count; ++j) {
                auto& rhs = targets[j];
                if (rhs.invalid()) continue;
                if (lhs.max.x < rhs.min.x) break;   //line sweeping

                //fully overlapped. drop lhs
                if (rhs.contained(lhs)) {
                    merged = true;
                    break;
                }
                //fully overlapped. replace the lhs with rhs
                if (lhs.contained(rhs)) {
                    rhs = {};
                    continue;
                }
                //just merge & expand on x axis
                if (lhs.min.y == rhs.min.y && lhs.max.y == rhs.max.y) {
                    if (lhs.min.x <= rhs.max.x && rhs.min.x <= lhs.max.x) {
                        rhs.min.x = std::min(lhs.min.x, rhs.min.x);
                        rhs.max.x = std::max(lhs.max.x, rhs.max.x);
                        merged = true;
                        break;
                    }
                }
                //just merge & expand on y axis
                if (lhs.min.x == rhs.min.x && lhs.max.x == rhs.max.x) {
                    if (lhs.min.y <= rhs.max.y && rhs.min.y < lhs.max.y) {
                        rhs.min.y = std::min(lhs.min.y, rhs.min.y);
                        rhs.max.y = std::max(lhs.max.y, rhs.max.y);
                        merged = true;
                        break;
                    }
                }
                //subdivide regions
                if (lhs.intersected(rhs)) {
                    subdivide(targets, j, lhs, rhs);
                    merged = true;
                    break;
                }
            }
            if (!merged) output.push(lhs);  //this region is complete isolated
            lhs = {};
        }
    }
}

#endif

/************************************************************************/
/* RenderTrimPath Class Implementation                                  */
/************************************************************************/

#define EPSILON 1e-4f


static void _trimAt(const PathCommand* cmds, const Point* pts, Point& moveTo, float at1, float at2, bool start, RenderPath& out)
{
    switch (*cmds) {
        case PathCommand::LineTo: {
            Line tmp, left, right;
            Line{*(pts - 1), *pts}.split(at1, left, tmp);
            tmp.split(at2, left, right);
            if (start) {
                out.pts.push(left.pt1);
                moveTo = left.pt1;
                out.cmds.push(PathCommand::MoveTo);
            }
            out.pts.push(left.pt2);
            out.cmds.push(PathCommand::LineTo);
            break;
        }
        case PathCommand::CubicTo: {
            Bezier tmp, left, right;
            Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.split(at1, left, tmp);
            tmp.split(at2, left, right);
            if (start) {
                moveTo = left.start;
                out.pts.push(left.start);
                out.cmds.push(PathCommand::MoveTo);
            }
            out.pts.push(left.ctrl1);
            out.pts.push(left.ctrl2);
            out.pts.push(left.end);
            out.cmds.push(PathCommand::CubicTo);
            break;
        }
        case PathCommand::Close: {
            Line tmp, left, right;
            Line{*(pts - 1), moveTo}.split(at1, left, tmp);
            tmp.split(at2, left, right);
            if (start) {
                moveTo = left.pt1;
                out.pts.push(left.pt1);
                out.cmds.push(PathCommand::MoveTo);
            }
            out.pts.push(left.pt2);
            out.cmds.push(PathCommand::LineTo);
            break;
        }
        default: break;
    }
}


static void _add(const PathCommand* cmds, const Point* pts, const Point& moveTo, bool& start, RenderPath& out)
{
    switch (*cmds) {
        case PathCommand::MoveTo: {
            out.cmds.push(PathCommand::MoveTo);
            out.pts.push(*pts);
            start = false;
            break;
        }
        case PathCommand::LineTo: {
            if (start) {
                out.cmds.push(PathCommand::MoveTo);
                out.pts.push(*(pts - 1));
            }
            out.cmds.push(PathCommand::LineTo);
            out.pts.push(*pts);
            start = false;
            break;
        }
        case PathCommand::CubicTo: {
            if (start) {
                out.cmds.push(PathCommand::MoveTo);
                out.pts.push(*(pts - 1));
            }
            out.cmds.push(PathCommand::CubicTo);
            out.pts.push(*pts);
            out.pts.push(*(pts + 1));
            out.pts.push(*(pts + 2));
            start = false;
            break;
        }
        case PathCommand::Close: {
            if (start) {
                out.cmds.push(PathCommand::MoveTo);
                out.pts.push(*(pts - 1));
            }
            out.cmds.push(PathCommand::LineTo);
            out.pts.push(moveTo);
            start = true;
            break;
        }
    }
}


static void _trimPath(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, TVG_UNUSED uint32_t inPtsCnt, float trimStart, float trimEnd, RenderPath& out, bool connect = false)
{
    auto cmds = const_cast<PathCommand*>(inCmds);
    auto pts = const_cast<Point*>(inPts);
    auto moveToTrimmed = *pts;
    auto moveTo = *pts;
    auto len = 0.0f;

    auto _length = [&]() -> float {
        switch (*cmds) {
            case PathCommand::MoveTo: return 0.0f;
            case PathCommand::LineTo: return tvg::length(pts - 1, pts);
            case PathCommand::CubicTo: return Bezier{*(pts - 1), *pts, *(pts + 1), *(pts + 2)}.length();
            case PathCommand::Close: return tvg::length(pts - 1, &moveTo);
        }
        return 0.0f;
    };

    auto _shift = [&]() -> void {
        switch (*cmds) {
            case PathCommand::MoveTo:
                moveTo = *pts;
                moveToTrimmed = *pts;
                ++pts;
                break;
            case PathCommand::LineTo:
                ++pts;
                break;
            case PathCommand::CubicTo:
                pts += 3;
                break;
            case PathCommand::Close:
                break;
        }
        ++cmds;
    };

    auto start = !connect;

    for (uint32_t i = 0; i < inCmdsCnt; ++i) {
        auto dLen = _length();

        //very short segments are skipped since due to the finite precision of Bezier curve subdivision and length calculation (1e-2),
        //trimming may produce very short segments that would effectively have zero length with higher computational accuracy.
        if (len <= trimStart) {
            //cut the segment at the beginning and at the end
            if (len + dLen > trimEnd) {
                _trimAt(cmds, pts, moveToTrimmed, trimStart - len, trimEnd - trimStart, start, out);
                start = false;
                //cut the segment at the beginning
            } else if (len + dLen > trimStart + EPSILON) {
                _trimAt(cmds, pts, moveToTrimmed, trimStart - len, len + dLen - trimStart, start, out);
                start = false;
            }
        } else if (len <= trimEnd - EPSILON) {
            //cut the segment at the end
            if (len + dLen > trimEnd) {
                _trimAt(cmds, pts, moveTo, 0.0f, trimEnd - len, start, out);
                start = true;
            //add the whole segment
            } else if (len + dLen > trimStart + EPSILON) _add(cmds, pts, moveTo, start, out);
        }

        len += dLen;
        _shift();
    }
}


static void _trim(const PathCommand* inCmds, uint32_t inCmdsCnt, const Point* inPts, uint32_t inPtsCnt, float begin, float end, bool connect, RenderPath& out)
{
    auto totalLength = tvg::length(inCmds, inCmdsCnt, inPts, inPtsCnt);
    auto trimStart = begin * totalLength;
    auto trimEnd = end * totalLength;

    if (begin >= end) {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, totalLength, out);
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, 0.0f, trimEnd, out, connect);
    } else {
        _trimPath(inCmds, inCmdsCnt, inPts, inPtsCnt, trimStart, trimEnd, out);
    }
}


static void _get(float& begin, float& end)
{
    auto loop = true;

    if (begin > 1.0f && end > 1.0f) loop = false;
    if (begin < 0.0f && end < 0.0f) loop = false;
    if (begin >= 0.0f && begin <= 1.0f && end >= 0.0f  && end <= 1.0f) loop = false;

    if (begin > 1.0f) begin -= 1.0f;
    if (begin < 0.0f) begin += 1.0f;
    if (end > 1.0f) end -= 1.0f;
    if (end < 0.0f) end += 1.0f;

    if ((loop && begin < end) || (!loop && begin > end)) std::swap(begin, end);
}


bool RenderTrimPath::trim(const RenderPath& in, RenderPath& out) const
{
    if (in.pts.count < 2 || tvg::zero(begin - end)) return false;

    float begin = this->begin, end = this->end;
    _get(begin, end);

    out.cmds.reserve(in.cmds.count * 2);
    out.pts.reserve(in.pts.count * 2);

    auto pts = in.pts.data;
    auto cmds = in.cmds.data;

    if (simultaneous) {
        auto startCmds = cmds;
        auto startPts = pts;
        uint32_t i = 0;
        while (i < in.cmds.count) {
            switch (in.cmds[i]) {
                case PathCommand::MoveTo: {
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, *(cmds - 1) == PathCommand::Close, out);
                    startPts = pts;
                    startCmds = cmds;
                    ++pts;
                    ++cmds;
                    break;
                }
                case PathCommand::LineTo: {
                    ++pts;
                    ++cmds;
                    break;
                }
                case PathCommand::CubicTo: {
                    pts += 3;
                    ++cmds;
                    break;
                }
                case PathCommand::Close: {
                    ++cmds;
                    if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, *(cmds - 1) == PathCommand::Close, out);
                    startPts = pts;
                    startCmds = cmds;
                    break;
                }
            }
            i++;
        }
        if (startCmds != cmds) _trim(startCmds, cmds - startCmds, startPts, pts - startPts, begin, end, *(cmds - 1) == PathCommand::Close, out);
    } else {
        _trim(in.cmds.data, in.cmds.count, in.pts.data, in.pts.count, begin, end, false, out);
    }

    return out.pts.count >= 2;
}