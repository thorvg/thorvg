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

#include <new>
#include "tvgMath.h"
#include "tvgInlist.h"
#include "tvgLottiePathOps.h"

#define PATHOP_EPSILON 1e-5f
#define PATHOP_DEPTH 24
#define PATHOP_OVERLAP 9

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

namespace {

struct Segment;
struct Contour;


template<class T>
struct List
{
    T* head = nullptr;
    T* tail = nullptr;
    uint32_t count = 0;

    void back(T* element)
    {
        element->prev = tail;
        element->next = nullptr;
        if (tail) tail->next = element;
        else head = element;
        tail = element;
        ++count;
    }

    void front(T* element)
    {
        element->prev = nullptr;
        element->next = head;
        if (head) head->prev = element;
        else tail = element;
        head = element;
        ++count;
    }

    T* pop()
    {
        if (!head) return nullptr;
        auto t = head;
        head = t->next;
        if (head) head->prev = nullptr;
        else tail = nullptr;
        --count;
        return t;
    }

    void remove(T* element)
    {
        if (element->prev) element->prev->next = element->next;
        else head = element->next;
        if (element->next) element->next->prev = element->prev;
        else tail = element->prev;
        --count;
    }

    void clear() { head = tail = nullptr; count = 0; }
    bool empty() const { return head ? false : true; }
};


struct Intersection
{
    INLIST_ITEM(Intersection);

    Segment* segment;
    Intersection* pair;

    Bezier prevCurve, nextCurve;
    float t;
    bool inside;
    bool visited;
};


struct Segment
{
    INLIST_ITEM(Segment);

    Bezier curve;
    Contour* parent;
    List<Intersection> intersections;
    Segment* twin;
    int32_t coincident;     //0 if apart, +1 if the twin runs along, -1 if against

    void sort();
    void split();

    Segment* nextSegment();
    Segment* prevSegment();
};


struct Contour
{
    INLIST_ITEM(Contour);

    List<Segment> segments;
    bool rhs;               //the right hand operand, the one that gives its runs up
};


Segment* Segment::nextSegment() { return next ? next : parent->segments.head; }
Segment* Segment::prevSegment() { return prev ? prev : parent->segments.tail; }


struct Root
{
    float t, u;
};


template<class T, uint32_t N>
struct Pool
{
    Array<T*> blocks;
    uint32_t used = 0;

    T* alloc()
    {
        auto idx = used++;
        if (idx / N >= blocks.count) blocks.push(tvg::malloc<T>(sizeof(T) * N));
        return new (blocks[idx / N] + (idx % N)) T();
    }

    void reset() { used = 0; }

    ~Pool()
    {
        ARRAY_FOREACH(p, blocks) tvg::free(*p);
    }
};


struct Hit
{
    Segment* lhs;
    Segment* rhs;
    float t, u;
};


struct Workspace
{
    Pool<Contour, 16> contours;
    Pool<Segment, 128> segments;
    Pool<Intersection, 128> intersections;
    Array<Root> roots, merged;
    Array<Hit> pending;

    void reset()
    {
        contours.reset();
        segments.reset();
        intersections.reset();
    }
};

}


static Workspace& _workspace()
{
    static thread_local Workspace ws;
    return ws;
}


//0 apart, +1 running along, -1 against
static int32_t _overlapped(const Bezier& lhs, const Bezier& rhs)
{
    auto same = [](const Point& lhs, const Point& rhs) {
        return length2(lhs - rhs) < PATHOP_EPSILON;
    };
    if (same(lhs.start, rhs.start) && same(lhs.ctrl1, rhs.ctrl1) && same(lhs.ctrl2, rhs.ctrl2) && same(lhs.end, rhs.end)) return 1;
    if (same(lhs.start, rhs.end) && same(lhs.ctrl1, rhs.ctrl2) && same(lhs.ctrl2, rhs.ctrl1) && same(lhs.end, rhs.start)) return -1;
    return 0;
}


static bool _holds(const Bezier& bz, const Point& pt)
{
    auto lo = Point{fminf(fminf(bz.start.x, bz.ctrl1.x), fminf(bz.ctrl2.x, bz.end.x)), fminf(fminf(bz.start.y, bz.ctrl1.y), fminf(bz.ctrl2.y, bz.end.y))};
    auto hi = Point{fmaxf(fmaxf(bz.start.x, bz.ctrl1.x), fmaxf(bz.ctrl2.x, bz.end.x)), fmaxf(fmaxf(bz.start.y, bz.ctrl1.y), fmaxf(bz.ctrl2.y, bz.end.y))};
    auto slack = sqrtf(PATHOP_EPSILON);   //the tolerance is squared elsewhere
    return pt.x >= lo.x - slack && pt.x <= hi.x + slack && pt.y >= lo.y - slack && pt.y <= hi.y + slack;
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


//List carries no insert
static void _insert(List<Segment>& list, Segment* element, Segment* at)
{
    if (!at) {
        list.back(element);
        return;
    }
    element->prev = at->prev;
    element->next = at;
    if (at->prev) at->prev->next = element;
    else list.head = element;
    at->prev = element;
    ++list.count;
}


static inline bool _overlap(const BBox& lhs, const BBox& rhs)
{
    return !(lhs.max.x < rhs.min.x || rhs.max.x < lhs.min.x || lhs.max.y < rhs.min.y || rhs.max.y < lhs.min.y);
}


static Intersection* _merged(Intersection* lhs, Intersection* rhs)
{
    Intersection* out = nullptr;
    auto tail = &out;

    while (lhs && rhs) {
        if (lhs->t <= rhs->t) {
            *tail = lhs;
            lhs = lhs->next;
        } else {
            *tail = rhs;
            rhs = rhs->next;
        }
        tail = &(*tail)->next;
    }
    *tail = lhs ? lhs : rhs;

    return out;
}


static Intersection* _sorted(Intersection* head)
{
    if (!head || !head->next) return head;

    auto slow = head, fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    auto rhs = slow->next;
    slow->next = nullptr;

    return _merged(_sorted(head), _sorted(rhs));
}


void Segment::sort()
{
    if (intersections.count < 2) return;

    intersections.head = _sorted(intersections.head);

    Intersection* prev = nullptr;
    INLIST_FOREACH(intersections, cur) {
        cur->prev = prev;
        prev = cur;
    }
    intersections.tail = prev;
}


void Segment::split()
{
    INLIST_FOREACH(intersections, cur) {
        auto from = cur->prev ? cur->prev->t : 0.0f;
        auto to = cur->next ? cur->next->t : 1.0f;
        cur->prevCurve = curve.sub(from, cur->t);
        cur->nextCurve = curve.sub(cur->t, to);
    }
}


/************************************************************************/
/* Curve Math                                                           */
/************************************************************************/


static void _refine(const Bezier& lhs, const Bezier& rhs, Root& root)
{
    for (uint32_t i = 0; i < 8; ++i) {
        auto diff = lhs.at(root.t) - rhs.at(root.u);
        if (length2(diff) < PATHOP_EPSILON * PATHOP_EPSILON) return;

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


static void _isolate(const Bezier& lhs, float lt0, float lt1, const Bezier& rhs, float rt0, float rt1, uint32_t depth, Array<Root>& roots)
{
    if (!_overlap(lhs.hull(), rhs.hull())) return;

    constexpr float flatness = 0.05f;
    auto lflat = lhs.flatten(flatness);
    auto rflat = rhs.flatten(flatness);

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


/************************************************************************/
/* Path Build                                                           */
/************************************************************************/


static float _area(const RenderPath& path)
{
    auto pts = path.pts.data;
    Point start{}, cur{};
    auto sum = 0.0f;

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                sum += cross(cur, start);
                start = cur = *pts++;
                break;
            }
            case PathCommand::LineTo: {
                sum += cross(cur, *pts);
                cur = *pts++;
                break;
            }
            case PathCommand::CubicTo: {
                sum += cross(cur, pts[2]);
                cur = pts[2];
                pts += 3;
                break;
            }
            case PathCommand::Close: {
                sum += cross(cur, start);
                cur = start;
                break;
            }
        }
    }
    return 0.5f * (sum + cross(cur, start));
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


static void _append(Workspace& ws, Contour* contour, const Bezier& curve)
{
    if (length2(curve.end - curve.start) < PATHOP_EPSILON) return;

    auto segment = ws.segments.alloc();
    segment->curve = curve;
    segment->parent = contour;
    contour->segments.back(segment);
}


static void _contour(Workspace& ws, const RenderPath& path, bool rhs, List<Contour>& out)
{
    auto pts = path.pts.data;
    Contour* contour = nullptr;
    Point start{}, cur{};

    ARRAY_FOREACH(cmd, path.cmds) {
        switch (*cmd) {
            case PathCommand::MoveTo: {
                if (contour) _append(ws, contour, Bezier::line(cur, start));
                contour = ws.contours.alloc();
                contour->rhs = rhs;
                out.back(contour);
                start = cur = *pts++;
                break;
            }
            case PathCommand::LineTo: {
                if (contour) _append(ws, contour, Bezier::line(cur, *pts));
                cur = *pts++;
                break;
            }
            case PathCommand::CubicTo: {
                if (contour) _append(ws, contour, Bezier{cur, pts[0], pts[1], pts[2]});
                cur = pts[2];
                pts += 3;
                break;
            }
            case PathCommand::Close: {
                if (contour) _append(ws, contour, Bezier::line(cur, start));
                cur = start;
                break;
            }
        }
    }
    if (contour) _append(ws, contour, Bezier::line(cur, start));

    auto empty = out.head;
    while (empty) {
        auto next = empty->next;
        if (empty->segments.count < 2) out.remove(empty);
        empty = next;
    }
}


static void _reverse(Contour* contour)
{
    List<Segment> reversed;

    while (auto segment = contour->segments.pop()) {
        segment->curve = segment->curve.reverse();
        reversed.front(segment);
    }
    contour->segments = reversed;
}


static float _area(const Contour* contour)
{
    auto sum = 0.0f;
    INLIST_FOREACH(contour->segments, segment) sum += cross(segment->curve.start, segment->curve.end);
    return 0.5f * sum;
}


static uint32_t _turns(const Bezier& bz, float* out)
{
    auto d1 = bz.ctrl1.y - bz.start.y;
    auto d2 = bz.ctrl2.y - bz.ctrl1.y;
    auto d3 = bz.end.y - bz.ctrl2.y;

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
    if (n == 2 && out[0] > out[1]) {
        auto t = out[0];
        out[0] = out[1];
        out[1] = t;
    }
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


static int32_t _winding(const List<Contour>& path, const Point& pt)
{
    int32_t winding = 0;
    INLIST_FOREACH(path, contour) winding += _winding(contour, pt);
    return winding;
}


static float _site(Segment* segment, Segment* other)
{
    auto& corner = other->curve.start;
    if (!_holds(segment->curve, corner)) return -1.0f;

    if (length2(corner - segment->curve.start) < PATHOP_EPSILON) return -1.0f;
    if (length2(corner - segment->curve.end) < PATHOP_EPSILON) return -1.0f;

    auto t = _project(segment->curve, corner);
    if (t < 0.0f) return -1.0f;

    auto before = other->prevSegment()->curve.at(0.95f);
    auto after = other->curve.at(0.05f);
    if (_project(segment->curve, before) < 0.0f && _project(segment->curve, after) < 0.0f) return -1.0f;

    return t;
}


static void _cut(Workspace& ws, Segment* segment, const Array<float>& ts)
{
    auto& list = segment->parent->segments;
    auto from = 0.0f;

    ARRAY_FOREACH(t, ts) {
        auto piece = ws.segments.alloc();
        piece->curve = segment->curve.sub(from, *t);
        piece->parent = segment->parent;
        _insert(list, piece, segment);
        from = *t;
    }
    segment->curve = segment->curve.sub(from, 1.0f);
}


static void _slice(Workspace& ws, List<Contour>& path, const List<Contour>& other)
{
    Array<float> ts;

    INLIST_FOREACH(path, contour) {
        INLIST_FOREACH(contour->segments, segment) {
            ts.clear();

            INLIST_FOREACH(other, oc) {
                INLIST_FOREACH(oc->segments, os) {
                    auto t = _site(segment, os);
                    if (t < 0.0f) continue;

                    uint32_t at = 0;
                    auto seen = false;
                    for (; at < ts.count; ++at) {
                        if (fabsf(ts[at] - t) < PATHOP_EPSILON) { seen = true; break; }
                        if (ts[at] > t) break;
                    }
                    if (seen) continue;

                    ts.push(t);
                    for (auto i = ts.count - 1; i > at; --i) {
                        auto swap = ts[i];
                        ts[i] = ts[i - 1];
                        ts[i - 1] = swap;
                    }
                }
            }
            if (!ts.empty()) _cut(ws, segment, ts);
        }
    }
}


static void _orient(List<Contour>& path)
{
    INLIST_FOREACH(path, contour) {
        auto pt = contour->segments.head->curve.at(0.5f);
        auto depth = 0;

        INLIST_FOREACH(path, other) {
            if (other != contour && _winding(other, pt) != 0) ++depth;
        }

        auto outward = (depth % 2 == 0);
        if ((_area(contour) > 0.0f) != outward) _reverse(contour);
    }
}


/************************************************************************/
/* Intersection                                                         */
/************************************************************************/

static void _pair(Workspace& ws, Segment* lhs, float lt, Segment* rhs, float rt)
{
    auto a = ws.intersections.alloc();
    auto b = ws.intersections.alloc();

    a->segment = lhs;
    a->pair = b;
    a->t = lt;

    b->segment = rhs;
    b->pair = a;
    b->t = rt;

    lhs->intersections.back(a);
    rhs->intersections.back(b);
}


static bool _duplicated(const Array<Root>& roots, const Root& root)
{
    ARRAY_FOREACH(r, roots) {
        if (fabsf(r->t - root.t) < 1e-3f && fabsf(r->u - root.u) < 1e-3f) return true;
    }
    return false;
}


static uint32_t _intersect(Workspace& ws, List<Contour>& lhs, List<Contour>& rhs)
{
    auto& roots = ws.roots;
    auto& merged = ws.merged;
    auto& pending = ws.pending;
    uint32_t cnt = 0;

    pending.clear();

    INLIST_FOREACH(lhs, lc) {
        INLIST_FOREACH(lc->segments, ls) {
            if (ls->coincident) continue;
            INLIST_FOREACH(rhs, rc) {
                INLIST_FOREACH(rc->segments, rs) {
                    if (rs->coincident) continue;

                    //a run never crosses the other path, and its ends would look like one
                    if (auto dir = _overlapped(ls->curve, rs->curve)) {
                        ls->coincident = rs->coincident = dir;
                        ls->twin = rs;
                        rs->twin = ls;
                        break;
                    }

                    if (!_overlap(ls->curve.hull(), rs->curve.hull())) continue;

                    roots.clear();
                    merged.clear();
                    _isolate(ls->curve, 0.0f, 1.0f, rs->curve, 0.0f, 1.0f, 0, roots);

                    ARRAY_FOREACH(root, roots) {
                        _refine(ls->curve, rs->curve, *root);

                        //a hit on a shared vertex is reported by both neighbors
                        auto hit = ls->curve.at(root->t);
                        auto reach = fmaxf(fabsf(hit.x), fabsf(hit.y)) * 1e-6f;
                        auto vertex = [&](const Point& pt) { return length2(hit - pt) < reach * reach; };

                        //the parameter tells on a long piece, the distance on a short one
                        if (root->t > 1.0f - PATHOP_EPSILON || root->u > 1.0f - PATHOP_EPSILON) continue;
                        if (vertex(ls->curve.end) || vertex(rs->curve.end)) continue;
                        if (vertex(ls->curve.start)) root->t = PATHOP_EPSILON;
                        if (vertex(rs->curve.start)) root->u = PATHOP_EPSILON;
                        if (root->t < PATHOP_EPSILON) root->t = PATHOP_EPSILON;
                        if (root->u < PATHOP_EPSILON) root->u = PATHOP_EPSILON;

                        //a touch is not a crossing, the two parting on the same side
                        auto lt = ls->curve.tangent(root->t);
                        auto rt = rs->curve.tangent(root->u);
                        auto span = sqrtf(length2(lt)) * sqrtf(length2(rt));
                        if (span > 0.0f && fabsf(cross(lt, rt)) / span < sqrtf(PATHOP_EPSILON)) continue;

                        if (_duplicated(merged, *root)) continue;
                        merged.push(*root);
                    }

                    //3rd-order bezier curve cannot cross 9+ times unless it is overlapped
                    if (merged.count > PATHOP_OVERLAP) {
                        ls->coincident = rs->coincident = 1;
                        continue;
                    }

                    //held back until every run is known, a later one unmaking these
                    ARRAY_FOREACH(root, merged) pending.push({ls, rs, root->t, root->u});
                }
            }
        }
    }

    ARRAY_FOREACH(hit, pending) {
        if (hit->lhs->coincident || hit->rhs->coincident) continue;
        _pair(ws, hit->lhs, hit->t, hit->rhs, hit->u);
        ++cnt;
    }

    return cnt;
}


static void _prep(List<Contour>& path)
{
    INLIST_FOREACH(path, contour) {
        INLIST_FOREACH(contour->segments, segment) {
            segment->sort();
            segment->split();
        }
    }
}


static void _mark(List<Contour>& path, const List<Contour>& other)
{
    INLIST_FOREACH(path, contour) {
        Intersection* first = nullptr;
        INLIST_FOREACH(contour->segments, segment) {
            segment->sort();
            segment->split();
            if (!first && !segment->intersections.empty()) first = segment->intersections.head;
        }
        if (!first) continue;

        auto winding = _winding(other, first->nextCurve.at(0.5f));
        first->inside = (winding != 0);

        auto cur = first;
        while (true) {
            auto next = cur->next;
            if (!next) {
                auto segment = cur->segment->nextSegment();
                while (segment->intersections.empty()) segment = segment->nextSegment();
                next = segment->intersections.head;
            }
            if (next == first) break;

            auto turn = cross(next->pair->segment->curve.tangent(next->pair->t), next->segment->curve.tangent(next->t));
            if (fabsf(turn) < PATHOP_EPSILON) winding = _winding(other, next->nextCurve.at(0.5f));
            else winding += turn > 0.0f ? 1 : -1;
            next->inside = (winding != 0);
            cur = next;
        }
    }
}


/************************************************************************/
/* Walk                                                                 */
/************************************************************************/

//who draws a run the two paths share, the left hand operand always carrying it
static bool _owned(const Segment* segment, PathOp op)
{
    auto onward = (segment->coincident > 0) ? (op != PathOp::Difference) : (op == PathOp::Difference);
    return onward && !segment->parent->rhs;
}


//turns the walk onto the twin at the end it stands at
static Segment* _handover(const Segment* segment, bool& forward)
{
    auto twin = segment->twin;
    if (segment->coincident > 0) forward = !forward;
    return forward ? twin->nextSegment() : twin->prevSegment();
}


static void _emit(RenderPath& out, const Bezier& curve, bool forward)
{
    auto bz = forward ? curve : curve.reverse();
    if (bz.straight()) out.lineTo(bz.end);
    else out.cubicTo(bz.ctrl1, bz.ctrl2, bz.end);
}


static Intersection* _advance(RenderPath& out, Intersection* from, bool& forward, PathOp op)
{
    _emit(out, forward ? from->nextCurve : from->prevCurve, forward);
    if (auto hit = forward ? from->next : from->prev) return hit;

    auto segment = forward ? from->segment->nextSegment() : from->segment->prevSegment();
    while (segment->intersections.empty()) {
        if (segment->twin && !_owned(segment, op)) {
            segment = _handover(segment, forward);
            continue;
        }
        _emit(out, segment->curve, forward);
        segment = forward ? segment->nextSegment() : segment->prevSegment();
    }
    auto hit = forward ? segment->intersections.head : segment->intersections.tail;
    _emit(out, forward ? hit->prevCurve : hit->nextCurve, forward);
    return hit;
}


static void _merge(List<Contour>& lhs, PathOp op, RenderPath& out)
{
    auto entry = (op == PathOp::Intersect);

    INLIST_FOREACH(lhs, contour) {
        INLIST_FOREACH(contour->segments, segment) {
            INLIST_FOREACH(segment->intersections, head) {
                if (head->visited || head->inside != entry) continue;

                out.moveTo(head->segment->curve.at(head->t));

                auto cur = head;
                auto forward = true;
                while (cur && !cur->visited) {
                    cur->visited = true;
                    auto next = _advance(out, cur, forward, op);
                    if (!next) break;
                    next->visited = true;
                    cur = next->pair;
                    if (op == PathOp::Difference) forward = !forward;
                }
                out.close();
            }
        }
    }
}


static void _stitch(Segment* from, PathOp op, RenderPath& out)
{
    auto segment = from;
    auto forward = true;

    out.moveTo(segment->curve.start);

    do {
        if (segment->twin && !_owned(segment, op)) {
            segment = _handover(segment, forward);
            continue;
        }
        _emit(out, segment->curve, forward);
        segment = forward ? segment->nextSegment() : segment->prevSegment();
    } while (segment != from);
    out.close();
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


static void _uncrossed(List<Contour>& path, const List<Contour>& other, PathOp op, bool lhs, RenderPath& out)
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

        //only a part of it runs along, so it is stitched from the left hand operand
        if (shares) {
            if (lhs) {
                INLIST_FOREACH(contour->segments, segment) {
                    if (segment->twin) continue;
                    _stitch(segment, op, out);
                    break;
                }
            }
            continue;
        }

        auto inside = (_winding(other, contour->segments.head->curve.at(0.5f)) != 0);
        auto keep = false;
        auto flip = false;

        if (op == PathOp::Union) keep = !inside;
        else if (op == PathOp::Intersect) keep = inside;
        else {
            keep = (lhs != inside);
            flip = !lhs;
        }
        if (keep) _copy(contour, flip, out);
    }
}


static void _op(const RenderPath& lhs, const RenderPath& rhs, RenderPath& out, PathOp op)
{

    if (!_overlap(_bounds(lhs), _bounds(rhs)) && _area(lhs) * _area(rhs) > 0.0f) {
        if (op != PathOp::Intersect) {
            _copy(lhs, out);
            if (op == PathOp::Union) _copy(rhs, out);
        }
        return;
    }

    auto& ws = _workspace();
    ws.reset();

    List<Contour> a, b;

    _contour(ws, lhs, false, a);
    _contour(ws, rhs, true, b);
    if (a.empty() || b.empty()) return;

    //align the directions
    _orient(a);
    _orient(b);

    _slice(ws, a, b);
    _slice(ws, b, a);

    if (_intersect(ws, a, b) > 0) {
        _mark(a, b);
        _prep(b);
        _merge(a, op, out);
    }

    _uncrossed(a, b, op, true, out);
    _uncrossed(b, a, op, false, out);
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool tvg::pathop(const RenderPath& lhs, const RenderPath& rhs, RenderPath& out, PathOp op)
{
    if (lhs.cmds.empty() || rhs.cmds.empty()) return false;

    auto cnt = out.cmds.count;

    if (op == PathOp::Xor) {

        _op(lhs, rhs, out, PathOp::Difference);
        _op(rhs, lhs, out, PathOp::Difference);
    } else _op(lhs, rhs, out, op);

    return out.cmds.count > cnt;
}
