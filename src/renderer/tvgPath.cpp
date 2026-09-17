/*
 * Copyright (c) 2026 the ThorVG project. All rights reserved.

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
#include "tvgInlist.h"
#include "tvgPath.h"

#define PATHOP_EPSILON 1e-5f
#define PATHOP_TOLERANCE 1e-5f
#define PATHOP_FLATNESS 2e-4f
#define PATHOP_DEPTH 24
#define PATHOP_OVERLAP 9

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

struct Segment;


struct Normalizer
{
    Point offset{0.0f, 0.0f};
    float scale = 1.0f;

    Normalizer(const BBox& box)
    {
        offset = {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f};
        auto span = fmaxf(box.max.x - box.min.x, box.max.y - box.min.y);
        if (!(span > 0.0f) || !std::isfinite(span)) return;
        auto exp = fmaxf(-126.0f, fminf(126.0f, ceilf(log2f(span))));
        scale = exp2f(-exp);
    }

    Point in(const Point& pt) const { return (pt - offset) * scale; }
    Point out(const Point& pt) const { return pt * (1.0f / scale) + offset; }
};


struct Contour
{
    INLIST_ITEM(Contour);

    Inlist<Segment> segments;
    uint32_t depth;
    bool rhs;
    bool turn;
};


struct Intersection
{
    INLIST_ITEM(Intersection);

    Segment* segment;
    Intersection* pair;

    Bezier prevCurve, nextCurve;
    Point at;
    float t;
    bool inside;
    bool crossing = true;
    bool visited;
};


struct Segment
{
    INLIST_ITEM(Segment);

    Bezier curve;
    Contour* parent;
    Inlist<Intersection> intersections;
    Segment* twin;
    int32_t coincident;
    bool visited;

    void insert(Intersection* hit)
    {
        INLIST_FOREACH(intersections, cur) {
            if (cur->t > hit->t) return intersections.insert(hit, cur);
        }
        intersections.back(hit);
    }

    void split()
    {
        INLIST_FOREACH(intersections, cur) {
            auto from = cur->prev ? cur->prev->t : 0.0f;
            auto to = cur->next ? cur->next->t : 1.0f;
            cur->prevCurve = curve.sub(from, cur->t);
            cur->nextCurve = curve.sub(cur->t, to);
            cur->prevCurve.start = cur->prev ? cur->prev->at : curve.start;
            cur->prevCurve.end = cur->nextCurve.start = cur->at;
            cur->nextCurve.end = cur->next ? cur->next->at : curve.end;
        }
    }

    Segment* nextSegment() { return next ? next : parent->segments.head; }
    Segment* prevSegment() { return prev ? prev : parent->segments.tail; }
};


struct Root
{
    float t, u; //lhs(t) = rhs(u)
};


#define DEFAULT_POOL_SIZE 16384

struct PathPool
{
    Array<uint8_t*> blocks;
    uint32_t block = 0;     //current block
    uint32_t used = 0;      //used bytes in the current block

    Inlist<Contour>* path() { return new (alloc(sizeof(Inlist<Contour>))) Inlist<Contour>(); }
    Contour* contour() { return new (alloc(sizeof(Contour))) Contour(); }
    Segment* segment() { return new (alloc(sizeof(Segment))) Segment(); }
    Intersection* intersection() { return new (alloc(sizeof(Intersection))) Intersection(); }

    void* alloc(uint32_t size)
    {
        if (used + size > DEFAULT_POOL_SIZE) {
            ++block;
            used = 0;
        }
        if (block >= blocks.count) blocks.push(tvg::malloc<uint8_t>(DEFAULT_POOL_SIZE));
        auto ret = blocks[block] + used;
        used += size;
        return ret;
    }

    static PathPool& scratch()
    {
        static thread_local PathPool pool;
        pool.block = pool.used = 0;
        return pool;
    }

    ~PathPool()
    {
        ARRAY_FOREACH(p, blocks) tvg::free(*p);
    }
};


struct Cut
{
    float t;
    Point at;

    void insert(Array<Cut>& cuts) const
    {
        uint32_t idx = 0;
        for (; idx < cuts.count; ++idx) {
            if (fabsf(cuts[idx].t - t) < PATHOP_EPSILON) return;
            if (cuts[idx].t > t) break;
        }
        cuts.push(*this);
        for (auto i = cuts.count - 1; i > idx; --i) std::swap(cuts[i], cuts[i - 1]);
    }
};


static bool _straight(const Bezier& bz)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    auto chord = bz.end - bz.start;
    auto leng = length(chord);
    if (leng < 1e-6f) return length2(bz.ctrl1 - bz.start) < tolerance && length2(bz.ctrl2 - bz.start) < tolerance;
    return fabsf(cross(chord, bz.ctrl1 - bz.start)) / leng < PATHOP_TOLERANCE && fabsf(cross(chord, bz.ctrl2 - bz.start)) / leng < PATHOP_TOLERANCE;
}


static int32_t _overlapped(const Bezier& lhs, const Bezier& rhs)
{
    auto same = [](const Point& lhs, const Point& rhs) {
        return length2(lhs - rhs) < PATHOP_TOLERANCE * PATHOP_TOLERANCE;
    };
    if (_straight(lhs) && _straight(rhs)) {
        if (same(lhs.start, rhs.start) && same(lhs.end, rhs.end)) return 1;
        if (same(lhs.start, rhs.end) && same(lhs.end, rhs.start)) return -1;
        return 0;
    }

    if (same(lhs.start, rhs.start) && same(lhs.ctrl1, rhs.ctrl1) && same(lhs.ctrl2, rhs.ctrl2) && same(lhs.end, rhs.end)) return 1;
    if (same(lhs.start, rhs.end) && same(lhs.ctrl1, rhs.ctrl2) && same(lhs.ctrl2, rhs.ctrl1) && same(lhs.end, rhs.start)) return -1;
    return 0;
}


static bool _holds(const Bezier& bz, const Point& pt)
{
    auto box = bz.hull();
    return pt.x >= box.min.x - PATHOP_TOLERANCE && pt.x <= box.max.x + PATHOP_TOLERANCE && pt.y >= box.min.y - PATHOP_TOLERANCE && pt.y <= box.max.y + PATHOP_TOLERANCE;
}


static float _project(const Bezier& bz, const Point& pt)
{
    auto curve = bz;
    auto lo = 0.0f, hi = 1.0f;

    for (uint32_t i = 0; i < PATHOP_DEPTH; ++i) {
        Bezier left, right;
        curve.split(left, right);
        auto l = _holds(left, pt), r = _holds(right, pt);

        if (l && r) {
            if (length2(left.at(0.5f) - pt) <= length2(right.at(0.5f) - pt)) r = false;
            else l = false;
        }

        auto mid = (lo + hi) * 0.5f;
        if (l) {
            curve = left;
            hi = mid;
        } else if (r) {
            curve = right;
            lo = mid;
        } else return -1.0f;
    }
    return (lo + hi) * 0.5f;
}


static void _refine(const Bezier& lhs, const Bezier& rhs, Root& root)
{
    for (uint32_t i = 0; i < 8; ++i) {
        auto diff = lhs.at(root.t) - rhs.at(root.u);
        if (length2(diff) < PATHOP_TOLERANCE * PATHOP_TOLERANCE) return;

        auto dl = lhs.tangent(root.t);
        auto dr = rhs.tangent(root.u);
        auto det = cross(dl, dr) * -1.0f;
        if (fabsf(det) < PATHOP_EPSILON) return;

        root.t -= (diff.x * -dr.y - diff.y * -dr.x) / det;
        root.u -= (dl.x * diff.y - dl.y * diff.x) / det;
        root.t = fmaxf(0.0f, fminf(1.0f, root.t));
        root.u = fmaxf(0.0f, fminf(1.0f, root.u));
    }
}


//bezier subdivision
static void _isolate(const Bezier& lhs, float lt0, float lt1, const Bezier& rhs, float rt0, float rt1, uint32_t depth, Array<Root>& roots)
{
    if (!lhs.hull().intersected(rhs.hull())) return;

    auto lflat = lhs.flatten(PATHOP_FLATNESS);
    auto rflat = rhs.flatten(PATHOP_FLATNESS);

    if (depth >= PATHOP_DEPTH || (lflat && rflat)) {
        auto r = lhs.end - lhs.start;
        auto s = rhs.end - rhs.start;
        auto denom = cross(r, s);
        if (fabsf(denom) < PATHOP_EPSILON * PATHOP_EPSILON) return;

        auto qp = rhs.start - lhs.start;
        auto t = cross(qp, s) / denom;
        auto u = cross(qp, r) / denom;
        if (t < -PATHOP_EPSILON || t > 1.0f + PATHOP_EPSILON) return;
        if (u < -PATHOP_EPSILON || u > 1.0f + PATHOP_EPSILON) return;

        roots.push({lt0 + (lt1 - lt0) * fmaxf(0.0f, fminf(1.0f, t)), rt0 + (rt1 - rt0) * fmaxf(0.0f, fminf(1.0f, u))});
        return;
    }

    Bezier ll, lr, rl, rr;
    auto lmid = (lt0 + lt1) * 0.5f;
    auto rmid = (rt0 + rt1) * 0.5f;

    if (lflat) {
        rhs.split(rl, rr);
        _isolate(lhs, lt0, lt1, rl, rt0, rmid, depth + 1, roots);
        _isolate(lhs, lt0, lt1, rr, rmid, rt1, depth + 1, roots);
    } else if (rflat) {
        lhs.split(ll, lr);
        _isolate(ll, lt0, lmid, rhs, rt0, rt1, depth + 1, roots);
        _isolate(lr, lmid, lt1, rhs, rt0, rt1, depth + 1, roots);
    } else {
        lhs.split(ll, lr);
        rhs.split(rl, rr);
        _isolate(ll, lt0, lmid, rl, rt0, rmid, depth + 1, roots);
        _isolate(ll, lt0, lmid, rr, rmid, rt1, depth + 1, roots);
        _isolate(lr, lmid, lt1, rl, rt0, rmid, depth + 1, roots);
        _isolate(lr, lmid, lt1, rr, rmid, rt1, depth + 1, roots);
    }
}


static BBox _bounds(const RenderPath& path)
{
    BBox box;
    box.init();

    ARRAY_FOREACH(pt, path.pts) {
        box.min = {fminf(box.min.x, pt->x), fminf(box.min.y, pt->y)};
        box.max = {fmaxf(box.max.x, pt->x), fmaxf(box.max.y, pt->y)};
    }
    return box;
}


static void _copy(const RenderPath& path, RenderPath& out)
{
    out.cmds.push(path.cmds);
    out.pts.push(path.pts);
    out.close();
}


static void _append(PathPool& pool, Contour* contour, const Bezier& curve)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    if (length2(curve.end - curve.start) < tolerance && length2(curve.ctrl1 - curve.start) < tolerance && length2(curve.ctrl2 - curve.start) < tolerance) return;

    auto segment = pool.segment();
    segment->curve = curve;
    segment->parent = contour;
    contour->segments.back(segment);
}


static void _contour(PathPool& pool, const RenderPath& path, bool rhs, const Normalizer& norm, Inlist<Contour>& out)
{
    auto pts = path.pts.data;
    Contour* contour = nullptr;
    Point start{}, cur{};

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                if (contour) _append(pool, contour, Bezier::line(cur, start));
                contour = pool.contour();
                contour->rhs = rhs;
                out.back(contour);
                start = cur = norm.in(*pts++);
                break;
            }
            case PathCommand::LineTo: {
                if (contour) _append(pool, contour, Bezier::line(cur, norm.in(*pts)));
                cur = norm.in(*pts++);
                break;
            }
            case PathCommand::CubicTo: {
                if (contour) _append(pool, contour, Bezier{cur, norm.in(pts[0]), norm.in(pts[1]), norm.in(pts[2])});
                cur = norm.in(pts[2]);
                pts += 3;
                break;
            }
            case PathCommand::Close: {
                if (contour) _append(pool, contour, Bezier::line(cur, start));
                cur = start;
                break;
            }
        }
    }
    if (contour) _append(pool, contour, Bezier::line(cur, start));

    {
        INLIST_SAFE_FOREACH(out, bare) {
            if (bare->segments.count == 0 || (bare->segments.count < 2 && _straight(bare->segments.head->curve))) out.remove(bare);
        }
    }
}


static void _reverse(Contour* contour)
{
    auto& segments = contour->segments;

    INLIST_SAFE_FOREACH(segments, segment) {
        segment->curve = segment->curve.reverse();
        std::swap(segment->prev, segment->next);
    }
    std::swap(segments.head, segments.tail);
}


static float _area(const Contour* contour)
{
    auto sum = 0.0f;

    INLIST_FOREACH(contour->segments, segment) {
        auto& bz = segment->curve;
        sum += 6.0f * cross(bz.start, bz.ctrl1) + 3.0f * cross(bz.start, bz.ctrl2) + cross(bz.start, bz.end) + 3.0f * cross(bz.ctrl1, bz.ctrl2) + 3.0f * cross(bz.ctrl1, bz.end) + 6.0f * cross(bz.ctrl2, bz.end);
    }
    return 0.05f * sum;
}


static uint32_t _turns(const Bezier& bz, float* out)
{
    auto d1 = bz.ctrl1.y - bz.start.y;
    auto d2 = bz.ctrl2.y - bz.ctrl1.y;
    auto d3 = bz.end.y - bz.ctrl2.y;

    //y'(t) = 3(at² + bt + c)
    auto a = d1 - 2.0f * d2 + d3;
    auto b = 2.0f * (d2 - d1);
    auto c = d1;

    auto scale = fmaxf(fabsf(a), fmaxf(fabsf(b), fabsf(c)));
    if (scale < FLT_MIN) return 0;

    uint32_t cnt = 0;
    if (fabsf(a) < scale * PATHOP_EPSILON) {
        if (fabsf(b) >= scale * PATHOP_EPSILON) out[cnt++] = -c / b;
    } else {
        auto disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return 0;
        disc = sqrtf(disc);
        out[cnt++] = (-b + disc) / (2.0f * a);
        out[cnt++] = (-b - disc) / (2.0f * a);
    }

    uint32_t n = 0;
    for (uint32_t i = 0; i < cnt; ++i) {
        if (out[i] > 0.0f && out[i] < 1.0f) out[n++] = out[i];
    }
    if (n == 2 && out[0] > out[1]) std::swap(out[0], out[1]);
    return n;
}


static int32_t _winding(const Contour* contour, const Point& pt)
{
    int32_t winding = 0;
    float turns[2];

    INLIST_FOREACH(contour->segments, segment) {
        auto& bz = segment->curve;
        auto cnt = _turns(bz, turns);
        auto t0 = 0.0f;
        auto y0 = bz.start.y;

        for (uint32_t i = 0; i <= cnt; ++i) {
            auto t1 = (i < cnt) ? turns[i] : 1.0f;
            auto y1 = (i < cnt) ? bz.at(t1).y : bz.end.y;
            auto up = (y0 <= pt.y && pt.y < y1);
            auto down = (y1 <= pt.y && pt.y < y0);

            if (up || down) {
                auto lo = t0, hi = t1;
                for (uint32_t k = 0; k < 30; ++k) {
                    auto mid = (lo + hi) * 0.5f;
                    if ((bz.at(mid).y <= pt.y) == up) lo = mid;
                    else hi = mid;
                }
                if (bz.at((lo + hi) * 0.5f).x > pt.x) winding += up ? 1 : -1;
            }
            t0 = t1;
            y0 = y1;
        }
    }
    return winding;
}


static int32_t _winding(const Inlist<Contour>& path, const Point& pt)
{
    int32_t winding = 0;
    INLIST_FOREACH(path, contour) winding += _winding(contour, pt);
    return winding;
}


template<typename T>
static bool _within(const T& other, const Contour* contour)
{
    constexpr float probes[] = {0.317f, 0.641f};

    auto votes = contour->segments.count * uint32_t(sizeof(probes) / sizeof(probes[0]));
    uint32_t in = 0, out = 0;

    INLIST_FOREACH(contour->segments, segment) {
        for (auto t : probes) {
            if (_winding(other, segment->curve.at(t)) != 0) ++in;
            else ++out;
            if (in * 2 > votes) return true;
            if (out * 2 >= votes) return false;
        }
    }
    return in > out;
}


static void _orient(Inlist<Contour>& path)
{
    INLIST_FOREACH(path, contour) contour->depth = 0;

    INLIST_FOREACH(path, contour) {
        INLIST_FOREACH(path, other) {
            if (other != contour && _within(other, contour)) ++contour->depth;
        }
    }

    INLIST_FOREACH(path, contour) {
        auto facing = _area(contour);

        if (contour->depth > 0) {
            INLIST_FOREACH(path, other) {
                if (other == contour || other->depth > 0) continue;
                if (!_within(other, contour)) continue;
                facing = _area(other);
                break;
            }
        }
        contour->turn = (facing < 0.0f);
    }

    INLIST_FOREACH(path, contour) {
        if (contour->turn) _reverse(contour);
    }
}


static bool _repeats(const Contour* lhs, const Contour* rhs)
{
    if (lhs->segments.count != rhs->segments.count) return false;

    auto theirs = rhs->segments.head;
    INLIST_FOREACH(lhs->segments, ours) {
        auto& a = ours->curve;
        auto& b = theirs->curve;
        if (a.start != b.start || a.ctrl1 != b.ctrl1 || a.ctrl2 != b.ctrl2 || a.end != b.end) return false;
        theirs = theirs->next;
    }
    return true;
}


static void _prune(Inlist<Contour>& path)
{
    if (path.count < 2) return;

    INLIST_SAFE_FOREACH(path, contour) {
        int32_t around = 0;
        auto holds = false;

        INLIST_FOREACH(path, other) {
            if (other == contour) continue;
            if (_repeats(contour, other) || _within(other, contour)) around += _area(other) < 0.0f ? -1 : 1;
            else if (_within(contour, other)) {
                holds = true;
                break;
            }
        }
        if (holds) continue;

        auto mine = _area(contour) < 0.0f ? -1 : 1;
        if ((around != 0) != ((around + mine) != 0)) continue;

        path.remove(contour);
    }
}


static void _pair(PathPool& pool, Segment* lhs, float lt, Segment* rhs, float rt, const Point& at)
{
    auto a = pool.intersection();
    auto b = pool.intersection();

    a->segment = lhs;
    a->pair = b;
    a->at = at;
    a->t = lt;

    b->segment = rhs;
    b->pair = a;
    b->at = at;
    b->t = rt;

    lhs->insert(a);
    rhs->insert(b);
}


static bool _duplicated(const Array<Root>& roots, const Root& root)
{
    ARRAY_FOREACH(r, roots) {
        if (fabsf(r->t - root.t) < 1e-3f && fabsf(r->u - root.u) < 1e-3f) return true;
    }
    return false;
}


static float _site(Segment* segment, Segment* other)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    auto& corner = other->curve.start;
    if (!_holds(segment->curve, corner)) return -1.0f;

    if (length2(corner - segment->curve.start) < tolerance) return -1.0f;
    if (length2(corner - segment->curve.end) < tolerance) return -1.0f;

    auto t = _project(segment->curve, corner);
    if (t < 0.0f) return -1.0f;

    auto before = other->prevSegment()->curve.at(0.95f);
    auto after = other->curve.at(0.05f);
    if (_project(segment->curve, before) < 0.0f && _project(segment->curve, after) < 0.0f) return -1.0f;

    return t;
}


static void _cut(PathPool& pool, Segment* segment, const Array<Cut>& cuts)
{
    auto& list = segment->parent->segments;
    auto from = 0.0f;
    const Point* prev = nullptr;

    ARRAY_FOREACH(cut, cuts) {
        auto piece = pool.segment();
        piece->curve = segment->curve.sub(from, cut->t);
        if (prev) piece->curve.start = *prev;
        piece->curve.end = cut->at;
        piece->parent = segment->parent;
        list.insert(piece, segment);
        from = cut->t;
        prev = &cut->at;
    }
    segment->curve = segment->curve.sub(from, 1.0f);
    if (prev) segment->curve.start = *prev;
}


static void _slice(PathPool& pool, Inlist<Contour>& path, const Inlist<Contour>& other)
{
    Array<Cut> cuts;

    INLIST_FOREACH(path, contour) {
        INLIST_FOREACH(contour->segments, segment) {
            cuts.clear();

            INLIST_FOREACH(other, oc) {
                INLIST_FOREACH(oc->segments, os) {
                    Cut cut = {_site(segment, os), os->curve.start};
                    if (cut.t >= 0.0f) cut.insert(cuts);
                }
            }
            if (!cuts.empty()) _cut(pool, segment, cuts);
        }
    }
}


static uint32_t _intersect(PathPool& pool, Inlist<Contour>& lhs, Inlist<Contour>& rhs)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    struct Hit
    {
        Segment* lhs;
        Segment* rhs;
        float t, u;
    };

    Array<Root> roots, merged;
    Array<Hit> pending;
    uint32_t cnt = 0;

    //O(n^2)
    INLIST_FOREACH(lhs, lc) {
        INLIST_FOREACH(lc->segments, ls) {
            if (ls->coincident) continue;
            INLIST_FOREACH(rhs, rc) {
                INLIST_FOREACH(rc->segments, rs) {
                    if (rs->coincident) continue;

                    if (auto dir = _overlapped(ls->curve, rs->curve)) {
                        ls->coincident = rs->coincident = dir;
                        ls->twin = rs;
                        rs->twin = ls;
                        break;
                    }

                    if (!ls->curve.hull().intersected(rs->curve.hull())) continue;

                    roots.clear();
                    merged.clear();
                    _isolate(ls->curve, 0.0f, 1.0f, rs->curve, 0.0f, 1.0f, 0, roots);

                    ARRAY_FOREACH(root, roots) {
                        _refine(ls->curve, rs->curve, *root);

                        auto hit = ls->curve.at(root->t);
                        auto vertex = [&](const Point& pt) { return length2(hit - pt) < tolerance; };

                        if (root->t > 1.0f - PATHOP_EPSILON || root->u > 1.0f - PATHOP_EPSILON) continue;
                        if (vertex(ls->curve.end) || vertex(rs->curve.end)) continue;
                        if (vertex(ls->curve.start)) root->t = PATHOP_EPSILON;
                        if (vertex(rs->curve.start)) root->u = PATHOP_EPSILON;
                        if (root->t < PATHOP_EPSILON) root->t = PATHOP_EPSILON;
                        if (root->u < PATHOP_EPSILON) root->u = PATHOP_EPSILON;

                        if (_duplicated(merged, *root)) continue;
                        merged.push(*root);
                    }

                    if (merged.count > PATHOP_OVERLAP) {
                        ls->coincident = rs->coincident = 1;
                        continue;
                    }

                    ARRAY_FOREACH(root, merged) pending.push({ls, rs, root->t, root->u});
                }
                if (ls->twin) break;
            }
        }
    }

    ARRAY_FOREACH(hit, pending) {
        if (hit->lhs->coincident || hit->rhs->coincident) continue;
        _pair(pool, hit->lhs, hit->t, hit->rhs, hit->u, (hit->lhs->curve.at(hit->t) + hit->rhs->curve.at(hit->u)) * 0.5f);
        ++cnt;
    }

    return cnt;
}


static Segment* _segmentAt(Contour* contour, const Point& at, float& t)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    //the segment leaving the point comes first, the one arriving at it otherwise
    INLIST_FOREACH(contour->segments, segment) {
        if (segment->coincident || length2(segment->curve.start - at) >= tolerance) continue;
        t = PATHOP_EPSILON;
        return segment;
    }
    INLIST_FOREACH(contour->segments, segment) {
        if (segment->coincident || length2(segment->curve.end - at) >= tolerance) continue;
        t = 1.0f - PATHOP_EPSILON;
        return segment;
    }
    return nullptr;
}


static bool _noded(const Segment* segment)
{
    INLIST_FOREACH(segment->intersections, hit) {
        if (hit->t <= PATHOP_EPSILON * 2.0f || hit->t >= 1.0f - PATHOP_EPSILON * 2.0f) return true;
    }
    return false;
}


static uint32_t _bridge(PathPool& pool, Inlist<Contour>& lhs)
{
    uint32_t cnt = 0;

    INLIST_FOREACH(lhs, contour) {
        INLIST_FOREACH(contour->segments, segment) {
            if (!segment->coincident || !segment->twin) continue;

            Point ends[2] = {segment->curve.start, segment->curve.end};
            for (auto& at : ends) {
                float ourT, theirT;
                auto ours = _segmentAt(contour, at, ourT);
                auto theirs = _segmentAt(segment->twin->parent, at, theirT);
                if (!ours || !theirs) continue;
                if (_noded(ours) || _noded(theirs)) continue;
                _pair(pool, ours, ourT, theirs, theirT, at);
                ++cnt;
            }
        }
    }
    return cnt;
}


static Intersection* _nextIntersection(const Intersection* hit)
{
    if (hit->next) return hit->next;

    auto segment = hit->segment->nextSegment();
    while (segment->intersections.empty()) segment = segment->nextSegment();
    return segment->intersections.head;
}


static Point _ahead(const Intersection* hit)
{
    auto cur = hit;

    do {
        auto piece = cur->segment->curve.sub(cur->t, cur->next ? cur->next->t : 1.0f);
        if (length2(piece.end - piece.start) > PATHOP_TOLERANCE * PATHOP_TOLERANCE) return piece.at(0.5f);
        cur = _nextIntersection(cur);
    } while (cur != hit);

    return cur->segment->curve.sub(cur->t, cur->next ? cur->next->t : 1.0f).at(0.5f);
}


static void _mark(Inlist<Contour>& path, const Inlist<Contour>& other)
{
    INLIST_FOREACH(path, contour) {
        Intersection* first = nullptr;
        INLIST_FOREACH(contour->segments, segment) {
            segment->split();
            if (!first && !segment->intersections.empty()) first = segment->intersections.head;
        }
        if (!first) continue;

        auto held = (_winding(other, _ahead(first)) != 0);
        first->inside = held;

        for (auto next = _nextIntersection(first); next != first; next = _nextIntersection(next)) {
            next->inside = (_winding(other, _ahead(next)) != 0);
            next->crossing = (next->inside != held);
            held = next->inside;
        }
        first->crossing = (first->inside != held);
    }
}


static bool _onward(const Segment* segment, PathOp op)
{
    return (segment->coincident > 0) ? (op != PathOp::Subtract) : (op == PathOp::Subtract);
}


static bool _owned(const Segment* segment, PathOp op)
{
    return _onward(segment, op) && !segment->parent->rhs;
}


static Segment* _handover(const Segment* segment, bool& forward, PathOp op)
{
    auto twin = segment->twin;

    if (_owned(twin, op)) {
        if (segment->coincident < 0) forward = !forward;
        return twin;
    }

    if (segment->coincident > 0) forward = !forward;
    return forward ? twin->nextSegment() : twin->prevSegment();
}


static void _emit(RenderPath& out, const Bezier& curve, bool forward)
{
    auto bz = forward ? curve : curve.reverse();
    if (_straight(bz)) out.lineTo(bz.end);
    else out.cubicTo(bz.ctrl1, bz.ctrl2, bz.end);
}


static Intersection* _advance(RenderPath& out, Intersection* from, bool& forward, PathOp op)
{
    _emit(out, forward ? from->nextCurve : from->prevCurve, forward);
    if (auto hit = forward ? from->next : from->prev) return hit;

    auto segment = forward ? from->segment->nextSegment() : from->segment->prevSegment();
    auto opening = segment;
    do {
        if (!segment->intersections.empty()) break;
        if (segment->twin && !_onward(segment, op)) {
            segment = _handover(segment, forward, op);
            continue;
        }
        _emit(out, segment->curve, forward);
        segment = forward ? segment->nextSegment() : segment->prevSegment();
    } while (segment != opening);

    if (segment->intersections.empty()) return nullptr;

    auto hit = forward ? segment->intersections.head : segment->intersections.tail;
    _emit(out, forward ? hit->prevCurve : hit->nextCurve, forward);
    return hit;
}


static bool _entry(PathOp op, bool rhs)
{
    return rhs ? (op != PathOp::Add) : (op == PathOp::Intersect);
}


static bool _behind(const Intersection* hit)
{
    if (hit->prev) return hit->prev->inside;

    auto segment = hit->segment->prevSegment();
    while (segment->intersections.empty()) segment = segment->prevSegment();
    return segment->intersections.tail->inside;
}


static void _merge(Inlist<Contour>& lhs, Inlist<Contour>& rhs, PathOp op, RenderPath& out)
{
    Inlist<Contour>* sides[2] = {&lhs, &rhs};

    for (auto side : sides) {
        auto& path = *side;
        auto entry = _entry(op, side == &rhs);

        INLIST_FOREACH(path, contour) {
            INLIST_FOREACH(contour->segments, segment) {
                INLIST_FOREACH(segment->intersections, head) {
                    if (head->visited || head->inside != entry) continue;

                    out.moveTo(head->at);

                    auto cur = head;
                    auto forward = true;
                    while (cur && !cur->visited) {
                        cur->visited = true;
                        auto next = _advance(out, cur, forward, op);
                        if (!next || next == head) break;

                        if (!next->crossing) {
                            cur = next;
                            continue;
                        }

                        auto twin = next->pair;
                        auto wanted = _entry(op, twin->segment->parent->rhs);
                        if (twin->inside != wanted && _behind(twin) != wanted) {
                            cur = next;
                            continue;
                        }

                        next->visited = true;
                        cur = twin;
                        forward = (cur->inside == wanted);
                    }
                    out.close();
                }
            }
        }
    }
}


static void _copy(const Contour* contour, bool flip, RenderPath& out)
{
    if (flip) {
        out.moveTo(contour->segments.tail->curve.end);
        for (auto segment = contour->segments.tail; segment; segment = segment->prev) _emit(out, segment->curve, false);
    } else {
        out.moveTo(contour->segments.head->curve.start);
        INLIST_FOREACH(contour->segments, segment) _emit(out, segment->curve, true);
    }
    out.close();
}


static void _stitch(Segment* from, PathOp op, RenderPath& out)
{
    auto segment = from;
    auto forward = true;

    out.moveTo(segment->curve.start);

    do {
        if (segment->visited) break;
        segment->visited = true;
        if (segment->twin && !_owned(segment, op)) {
            segment = _handover(segment, forward, op);
            continue;
        }
        _emit(out, segment->curve, forward);
        segment = forward ? segment->nextSegment() : segment->prevSegment();
    } while (segment != from);
    out.close();
}


static void _uncrossed(Inlist<Contour>& path, const Inlist<Contour>& other, PathOp op, bool lhs, RenderPath& out)
{
    INLIST_FOREACH(path, contour) {
        auto crossed = false;
        auto shared = true;
        auto shares = false;
        INLIST_FOREACH(contour->segments, segment) {
            if (!segment->intersections.empty()) {
                crossed = true;
                break;
            }
            if (segment->coincident) shares = true;
            else shared = false;
        }
        if (crossed) continue;

        if (shared) {
            if (_owned(contour->segments.head, op)) _copy(contour, false, out);
            continue;
        }

        if (shares) {
            if (lhs) {
                INLIST_FOREACH(contour->segments, segment) {
                    if (segment->twin || segment->visited) continue;
                    _stitch(segment, op, out);
                }
            }
            continue;
        }

        auto inside = _within(other, contour);
        auto keep = false;
        auto flip = false;

        if (op == PathOp::Add) keep = !inside;
        else if (op == PathOp::Intersect) keep = inside;
        else {
            keep = (lhs != inside);
            flip = !lhs;
        }
        if (keep) _copy(contour, flip, out);
    }
}


static bool _sound(const RenderPath& path)
{
    ARRAY_FOREACH(pt, path.pts) {
        if (!std::isfinite(pt->x) || !std::isfinite(pt->y)) return false;
    }
    return true;
}


static void _op(const RenderPath& lhs, const RenderPath& rhs, RenderPath& out, PathOp op)
{
    auto lbox = _bounds(lhs);
    auto rbox = _bounds(rhs);

    //fast path: no overlap between the bounding boxes
    if (!lhs.pts.empty() && !rhs.pts.empty() && !lbox.intersected(rbox)) {
        if (op != PathOp::Intersect) {
            _copy(lhs, out);
            if (op == PathOp::Add) _copy(rhs, out);
        }
        return;
    }

    Normalizer norm({{fminf(lbox.min.x, rbox.min.x), fminf(lbox.min.y, rbox.min.y)},
                     {fmaxf(lbox.max.x, rbox.max.x), fmaxf(lbox.max.y, rbox.max.y)}});

    auto& pool = PathPool::scratch();

    auto& a = *pool.path();
    auto& b = *pool.path();

    _contour(pool, lhs, false, norm, a);
    _contour(pool, rhs, true, norm, b);

    if (a.empty() || b.empty()) {
        if (op == PathOp::Add) {
            if (!a.empty()) _copy(lhs, out);
            if (!b.empty()) _copy(rhs, out);
        } else if (op == PathOp::Subtract && !a.empty()) _copy(lhs, out);
        return;
    }

    auto mark = out.pts.count;

    //align the directions
    _orient(a);
    _orient(b);
    _prune(a);
    _prune(b);

    _slice(pool, a, b);
    _slice(pool, b, a);

    auto crossings = _intersect(pool, a, b);
    crossings += _bridge(pool, a);

    if (crossings > 0) {
        _mark(a, b);
        _mark(b, a);
        _merge(a, b, op, out);
    }

    _uncrossed(a, b, op, true, out);
    _uncrossed(b, a, op, false, out);

    for (auto i = mark; i < out.pts.count; ++i) out.pts[i] = norm.out(out.pts[i]);
}


bool tvg::pathop(const RenderPath& lhs, const RenderPath& rhs, RenderPath& out, PathOp op)
{
    if (!_sound(lhs) || !_sound(rhs)) return false;

    RenderPath tmp;

    //exclude intersections == (a - b) + (b - a)
    if (op == PathOp::Difference) {
        _op(lhs, rhs, tmp, PathOp::Subtract);
        _op(rhs, lhs, tmp, PathOp::Subtract);
    } else _op(lhs, rhs, tmp, op);

    tmp.cmds.move(out.cmds);
    tmp.pts.move(out.pts);

    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

Path::Path() = default;


struct Path* Path::gen() noexcept
{
    return new PathImpl;
}


Result Path::add(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt) noexcept
{
    if (cmdCnt == 0 || ptsCnt == 0 || !cmds || !pts) return Result::InvalidArguments;

    auto& path = PATH(this)->path;
    path.cmds.grow(cmdCnt);
    path.pts.grow(ptsCnt);
    memcpy(path.cmds.end(), cmds, sizeof(PathCommand) * cmdCnt);
    memcpy(path.pts.end(), pts, sizeof(Point) * ptsCnt);
    path.cmds.count += cmdCnt;
    path.pts.count += ptsCnt;

    return Result::Success;
}


Result Path::get(const PathCommand*& cmds, uint32_t* cmdsCnt, const Point*& pts, uint32_t* ptsCnt) const noexcept
{
    auto& path = CONST_PATH(this)->path;

    cmds = path.cmds.data;
    if (cmdsCnt) *cmdsCnt = path.cmds.count;

    pts = path.pts.data;
    if (ptsCnt) *ptsCnt = path.pts.count;

    return Result::Success;
}


void Path::reset() noexcept
{
    PATH(this)->path.clear();
}


Result Path::add(const Path& rhs) noexcept
{
    return add(*this, rhs, *this);
}


Result Path::subtract(const Path& rhs) noexcept
{
    return subtract(*this, rhs, *this);
}


Result Path::intersect(const Path& rhs) noexcept
{
    return intersect(*this, rhs, *this);
}


Result Path::difference(const Path& rhs) noexcept
{
    return difference(*this, rhs, *this);
}


Result Path::add(const Path& lhs, const Path& rhs, Path& out) noexcept
{
    return pathop(CONST_PATH(&lhs)->path, CONST_PATH(&rhs)->path, PATH(&out)->path, PathOp::Add) ? Result::Success : Result::InvalidArguments;
}


Result Path::subtract(const Path& lhs, const Path& rhs, Path& out) noexcept
{
    return pathop(CONST_PATH(&lhs)->path, CONST_PATH(&rhs)->path, PATH(&out)->path, PathOp::Subtract) ? Result::Success : Result::InvalidArguments;
}


Result Path::intersect(const Path& lhs, const Path& rhs, Path& out) noexcept
{
    return pathop(CONST_PATH(&lhs)->path, CONST_PATH(&rhs)->path, PATH(&out)->path, PathOp::Intersect) ? Result::Success : Result::InvalidArguments;
}


Result Path::difference(const Path& lhs, const Path& rhs, Path& out) noexcept
{
    return pathop(CONST_PATH(&lhs)->path, CONST_PATH(&rhs)->path, PATH(&out)->path, PathOp::Difference) ? Result::Success : Result::InvalidArguments;
}
