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

#define PATHOP_EPSILON 1e-5f      //the parameter distance two hits are the same within
#define PATHOP_TOLERANCE 1e-5f    //the point distance two points are the same within
#define PATHOP_FLATNESS 2e-4f
#define PATHOP_STRAIGHT 1e-5f
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

    //puts the element right before the given one, appending it when there is none
    void insert(T* element, T* at)
    {
        if (!at) {
            back(element);
            return;
        }
        element->prev = at->prev;
        element->next = at;
        if (at->prev) at->prev->next = element;
        else head = element;
        at->prev = element;
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


//brings both operands onto a common scale, so the tolerances mean the same thing everywhere
struct Normalizer
{
    Point offset{0.0f, 0.0f};
    float scale = 1.0f;

    Normalizer() {}

    Normalizer(const BBox& box)
    {
        offset = {(box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f};
        auto span = fmaxf(box.max.x - box.min.x, box.max.y - box.min.y);
        if (!(span > 0.0f) || !std::isfinite(span)) return;
        auto exp = fmaxf(-60.0f, fminf(60.0f, ceilf(log2f(span))));
        scale = exp2f(-exp);
    }

    Point in(const Point& pt) const { return (pt - offset) * scale; }
    Point out(const Point& pt) const { return pt * (1.0f / scale) + offset; }
};


struct Intersection
{
    INLIST_ITEM(Intersection);

    Segment* segment;
    Intersection* pair;

    Bezier prevCurve, nextCurve;
    float t;
    bool inside;
    bool crossing = true;   //the two paths actually swap sides here, rather than only touching
    bool visited;
};


struct Segment
{
    INLIST_ITEM(Segment);

    Bezier curve;
    Contour* parent;
    List<Intersection> intersections;
    Segment* twin;          //the piece of the other path this one runs along
    int32_t coincident;     //0 if apart, +1 if the twin runs along, -1 if against
    bool visited;

    void sort();
    void split();

    Segment* nextSegment();
    Segment* prevSegment();
};


struct Contour
{
    INLIST_ITEM(Contour);

    List<Segment> segments;
    uint32_t depth;         //how many contours of the same path hold this one
    bool rhs;               //the right hand operand, the one that gives its runs up
    bool turn;              //its direction disagrees with its nesting depth
};


Segment* Segment::nextSegment() { return next ? next : parent->segments.head; }
Segment* Segment::prevSegment() { return prev ? prev : parent->segments.tail; }


struct Root
{
    float t, u;             //lhs(t) = rhs(u)
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
    Array<float> ts;

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


/************************************************************************/
/* Curve Math                                                           */
/************************************************************************/

//Bezier::straight() takes any curve that returns to its start for a line, this one does not
static bool _straight(const Bezier& bz)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    auto chord = bz.end - bz.start;
    auto leng = length(chord);
    if (leng < 1e-6f) return length2(bz.ctrl1 - bz.start) < tolerance && length2(bz.ctrl2 - bz.start) < tolerance;
    return fabsf(cross(chord, bz.ctrl1 - bz.start)) / leng < PATHOP_STRAIGHT && fabsf(cross(chord, bz.ctrl2 - bz.start)) / leng < PATHOP_STRAIGHT;
}


//0 apart, +1 running along, -1 against
static int32_t _overlapped(const Bezier& lhs, const Bezier& rhs)
{
    auto same = [](const Point& lhs, const Point& rhs) {
        return length2(lhs - rhs) < PATHOP_TOLERANCE * PATHOP_TOLERANCE;
    };
    if (same(lhs.start, rhs.start) && same(lhs.ctrl1, rhs.ctrl1) && same(lhs.ctrl2, rhs.ctrl2) && same(lhs.end, rhs.end)) return 1;
    if (same(lhs.start, rhs.end) && same(lhs.ctrl1, rhs.ctrl2) && same(lhs.ctrl2, rhs.ctrl1) && same(lhs.end, rhs.start)) return -1;
    return 0;
}


static bool _holds(const Bezier& bz, const Point& pt)
{
    auto box = bz.hull();
    return pt.x >= box.min.x - PATHOP_TOLERANCE && pt.x <= box.max.x + PATHOP_TOLERANCE &&
           pt.y >= box.min.y - PATHOP_TOLERANCE && pt.y <= box.max.y + PATHOP_TOLERANCE;
}


//where the point sits on the curve, -1 if it sits off it
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


static inline bool _overlap(const BBox& lhs, const BBox& rhs)
{
    return !(lhs.max.x < rhs.min.x || rhs.max.x < lhs.min.x || lhs.max.y < rhs.min.y || rhs.max.y < lhs.min.y);
}


//refines a crossing on the intact curves, the isolation is only a seed
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


//bezier subdivision
static void _isolate(const Bezier& lhs, float lt0, float lt1, const Bezier& rhs, float rt0, float rt1, uint32_t depth, Array<Root>& roots)
{
    if (!_overlap(lhs.hull(), rhs.hull())) return;

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


/************************************************************************/
/* Intersection List                                                    */
/************************************************************************/

//loop style sorting instead of recursion
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


//hands each intersection the curve pieces on both sides
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
/* Path Build                                                           */
/************************************************************************/

//the signed area of the anchor polygon. only its sign is read, to tell the winding direction apart
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


//taken as it stands, only the implicit closing the solver would have added is made explicit
static void _copy(const RenderPath& path, RenderPath& out)
{
    out.cmds.push(path.cmds);
    out.pts.push(path.pts);
    out.close();
}


static void _append(Workspace& ws, Contour* contour, const Bezier& curve)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    //a piece that never leaves its start carries no part of the answer
    if (length2(curve.end - curve.start) < tolerance && length2(curve.ctrl1 - curve.start) < tolerance && length2(curve.ctrl2 - curve.start) < tolerance) return;

    auto segment = ws.segments.alloc();
    segment->curve = curve;
    segment->parent = contour;
    contour->segments.back(segment);
}


static void _contour(Workspace& ws, const RenderPath& path, bool rhs, const Normalizer& norm, List<Contour>& out)
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
                start = cur = norm.in(*pts++);
                break;
            }
            case PathCommand::LineTo: {
                if (contour) _append(ws, contour, Bezier::line(cur, norm.in(*pts)));
                cur = norm.in(*pts++);
                break;
            }
            case PathCommand::CubicTo: {
                if (contour) _append(ws, contour, Bezier{cur, norm.in(pts[0]), norm.in(pts[1]), norm.in(pts[2])});
                cur = norm.in(pts[2]);
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

    //a lone piece still encloses something, unless it is a straight line
    {
        INLIST_SAFE_FOREACH(out, bare) {
            if (bare->segments.count == 0 || (bare->segments.count < 2 && _straight(bare->segments.head->curve))) out.remove(bare);
        }
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


//count curve's Extrema points
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

    //only the ones strictly inside actually cut the curve
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


//ray casting the pt to every piece
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

            if (up || down) {   //intersection candidate
                //binary search the intersection point
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


//points that avoid the vertices, where the winding is not to be trusted
static constexpr float PROBES[] = {0.317f, 0.641f};


//whether the contour lies inside the other, asked of a contour or of a whole path
template<typename T>
static bool _within(const T& other, const Contour* contour)
{
    uint32_t in = 0, out = 0;

    INLIST_FOREACH(contour->segments, segment) {
        for (auto t : PROBES) {
            if (_winding(other, segment->curve.at(t)) != 0) ++in;
            else ++out;
        }
    }
    return in > out;
}


//turns every contour to agree with its nesting depth
static void _orient(List<Contour>& path)
{
    INLIST_FOREACH(path, contour) contour->depth = 0;

    INLIST_FOREACH(path, contour) {
        INLIST_FOREACH(path, other) {
            if (other != contour && _within(other, contour)) ++contour->depth;
        }
    }

    INLIST_FOREACH(path, contour) {
        auto facing = _area(contour);

        //a held contour follows the one holding it, so the nesting keeps its meaning
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


//where the other segment's corner lands on this one, -1 if it lands nowhere on it
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


static void _cut(Workspace& ws, Segment* segment, const Array<float>& ts)
{
    auto& list = segment->parent->segments;
    auto from = 0.0f;

    ARRAY_FOREACH(t, ts) {
        auto piece = ws.segments.alloc();
        piece->curve = segment->curve.sub(from, *t);
        piece->parent = segment->parent;
        list.insert(piece, segment);
        from = *t;
    }
    segment->curve = segment->curve.sub(from, 1.0f);
}


//puts a vertex on this path wherever the other one has a corner sitting on it
static void _slice(Workspace& ws, List<Contour>& path, const List<Contour>& other)
{
    auto& ts = ws.ts;

    INLIST_FOREACH(path, contour) {
        INLIST_FOREACH(contour->segments, segment) {
            ts.clear();

            INLIST_FOREACH(other, oc) {
                INLIST_FOREACH(oc->segments, os) {
                    auto t = _site(segment, os);
                    if (t < 0.0f) continue;

                    //kept sorted and free of repeats, the cut walks them in order
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


static uint32_t _intersect(Workspace& ws, List<Contour>& lhs, List<Contour>& rhs)
{
    constexpr auto tolerance = PATHOP_TOLERANCE * PATHOP_TOLERANCE;

    auto& roots = ws.roots;
    auto& merged = ws.merged;
    auto& pending = ws.pending;
    uint32_t cnt = 0;

    pending.clear();

    //O(n^2)
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
                        auto vertex = [&](const Point& pt) { return length2(hit - pt) < tolerance; };

                        //the parameter tells on a long piece, the distance on a short one
                        if (root->t > 1.0f - PATHOP_EPSILON || root->u > 1.0f - PATHOP_EPSILON) continue;
                        if (vertex(ls->curve.end) || vertex(rs->curve.end)) continue;
                        if (vertex(ls->curve.start)) root->t = PATHOP_EPSILON;
                        if (vertex(rs->curve.start)) root->u = PATHOP_EPSILON;
                        if (root->t < PATHOP_EPSILON) root->t = PATHOP_EPSILON;
                        if (root->u < PATHOP_EPSILON) root->u = PATHOP_EPSILON;

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

    //a piece still runs along the other path even where the twin was already taken
    INLIST_FOREACH(lhs, lc) {
        INLIST_FOREACH(lc->segments, ls) {
            if (ls->coincident) continue;
            INLIST_FOREACH(rhs, rc) {
                INLIST_FOREACH(rc->segments, rs) {
                    if (!rs->coincident) continue;
                    if (auto dir = _overlapped(ls->curve, rs->curve)) {
                        ls->coincident = dir;
                        break;
                    }
                }
                if (ls->coincident) break;
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


static Segment* _leaves(Contour* contour, const Point& at)
{
    INLIST_FOREACH(contour->segments, segment) {
        if (segment->coincident) continue;
        if (length2(segment->curve.start - at) < PATHOP_TOLERANCE * PATHOP_TOLERANCE) return segment;
    }
    return nullptr;
}


static Segment* _arrives(Contour* contour, const Point& at)
{
    INLIST_FOREACH(contour->segments, segment) {
        if (segment->coincident) continue;
        if (length2(segment->curve.end - at) < PATHOP_TOLERANCE * PATHOP_TOLERANCE) return segment;
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


//a shared run leaves no crossing behind, so both ends of it are made into one
static uint32_t _bridge(Workspace& ws, List<Contour>& lhs)
{
    uint32_t cnt = 0;

    INLIST_FOREACH(lhs, contour) {
        INLIST_FOREACH(contour->segments, segment) {
            if (!segment->coincident || !segment->twin) continue;

            Point ends[2] = {segment->curve.start, segment->curve.end};
            for (auto& at : ends) {
                //the piece that leaves the spot, or the one that arrives where none leaves
                auto ours = _leaves(contour, at);
                auto ourT = PATHOP_EPSILON;
                if (!ours) { ours = _arrives(contour, at); ourT = 1.0f - PATHOP_EPSILON; }

                auto theirs = _leaves(segment->twin->parent, at);
                auto theirT = PATHOP_EPSILON;
                if (!theirs) { theirs = _arrives(segment->twin->parent, at); theirT = 1.0f - PATHOP_EPSILON; }

                if (!ours || !theirs) continue;
                if (_noded(ours) || _noded(theirs)) continue;
                _pair(ws, ours, ourT, theirs, theirT);
                ++cnt;
            }
        }
    }
    return cnt;
}


//a point the walk passes right after the node, skipping the pieces too short to tell
static Point _ahead(const Intersection* hit)
{
    auto cur = hit;

    do {
        auto& piece = cur->nextCurve;
        if (length2(piece.end - piece.start) > PATHOP_TOLERANCE * PATHOP_TOLERANCE) return piece.at(0.5f);

        auto next = cur->next;
        if (!next) {
            auto segment = cur->segment->nextSegment();
            while (segment->intersections.empty()) segment = segment->nextSegment();
            next = segment->intersections.head;
        }
        cur = next;
    } while (cur != hit);

    return cur->nextCurve.at(0.5f);
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

        auto held = (_winding(other, _ahead(first)) != 0);
        first->inside = held;

        auto cur = first;
        while (true) {
            auto next = cur->next;
            if (!next) {
                auto segment = cur->segment->nextSegment();
                while (segment->intersections.empty()) segment = segment->nextSegment();
                next = segment->intersections.head;
            }
            if (next == first) break;

            next->inside = (_winding(other, _ahead(next)) != 0);
            next->crossing = (next->inside != held);
            held = next->inside;
            cur = next;
        }
        //the ring closes on the node it set out from
        first->crossing = (first->inside != held);
    }
}


/************************************************************************/
/* Walk                                                                 */
/************************************************************************/

//whether a run the two paths share is drawn at all
static bool _onward(const Segment* segment, PathOp op)
{
    return (segment->coincident > 0) ? (op != PathOp::Difference) : (op == PathOp::Difference);
}


//who draws a run the two paths share, the left hand operand always carrying it
static bool _owned(const Segment* segment, PathOp op)
{
    return _onward(segment, op) && !segment->parent->rhs;
}


//turns the walk onto the twin at the end it stands at
static Segment* _handover(const Segment* segment, bool& forward, PathOp op)
{
    auto twin = segment->twin;

    //the twin carries the run itself, so the walk takes it rather than stepping past
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
    if (_straight(bz)) out.lineTo(bz.end);   //if so, it can be emitted as a line
    else out.cubicTo(bz.ctrl1, bz.ctrl2, bz.end);
}


//emits the curve pieces
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

    //the ring the walk landed on carries no node to go on from
    if (segment->intersections.empty()) return nullptr;

    auto hit = forward ? segment->intersections.head : segment->intersections.tail;
    _emit(out, forward ? hit->prevCurve : hit->nextCurve, forward);
    return hit;
}


//the side of the other path a walk sets out from
static bool _entry(PathOp op, bool rhs)
{
    return rhs ? (op != PathOp::Union) : (op == PathOp::Intersect);
}


static bool _behind(const Intersection* hit)
{
    if (hit->prev) return hit->prev->inside;

    auto segment = hit->segment->prevSegment();
    while (segment->intersections.empty()) segment = segment->prevSegment();
    return segment->intersections.tail->inside;
}


static void _merge(List<Contour>& lhs, List<Contour>& rhs, PathOp op, RenderPath& out)
{
    List<Contour>* sides[2] = {&lhs, &rhs};

    for (auto side : sides) {
        auto& path = *side;
        auto entry = _entry(op, side == &rhs);

        INLIST_FOREACH(path, contour) {
            INLIST_FOREACH(contour->segments, segment) {
                INLIST_FOREACH(segment->intersections, head) {
                    if (head->visited || head->inside != entry) continue;

                    out.moveTo(head->segment->curve.at(head->t));

                    auto cur = head;
                    auto forward = true;
                    while (cur && !cur->visited) {
                        cur->visited = true;
                        auto next = _advance(out, cur, forward, op);
                        if (!next || next == head) break;

                        //a node the two paths only touch at is walked straight through
                        if (!next->crossing) { cur = next; continue; }

                        auto twin = next->pair;
                        auto wanted = _entry(op, twin->segment->parent->rhs);
                        if (twin->inside != wanted && _behind(twin) != wanted) { cur = next; continue; }

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


//draws a contour that only partly runs along the other path
static void _stitch(Segment* from, PathOp op, RenderPath& out)
{
    auto segment = from;
    auto forward = true;

    out.moveTo(segment->curve.start);

    do {
        //a loop ends at a piece already drawn, not only at the one it set out from
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

        //only a part of it runs along, so every loop the run cuts it into is stitched
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

        if (op == PathOp::Union) keep = !inside;
        else if (op == PathOp::Intersect) keep = inside;
        else {
            keep = (lhs != inside);
            flip = !lhs;
        }
        if (keep) _copy(contour, flip, out);
    }
}


/************************************************************************/
/* Operation                                                            */
/************************************************************************/

static bool _sound(const RenderPath& path)
{
    ARRAY_FOREACH(pt, path.pts) {
        if (!std::isfinite(pt->x) || !std::isfinite(pt->y)) return false;
    }
    return true;
}


static bool _op(const RenderPath& lhs, const RenderPath& rhs, RenderPath& out, PathOp op)
{
    //an infinity or a NaN would hang the walk, rather than answer it
    if (!_sound(lhs) || !_sound(rhs)) return false;

    auto lbox = _bounds(lhs);
    auto rbox = _bounds(rhs);

    //fast path: apart and winding alike
    if (!_overlap(lbox, rbox) && _area(lhs) * _area(rhs) > 0.0f) {
        if (op != PathOp::Intersect) {
            _copy(lhs, out);
            if (op == PathOp::Union) _copy(rhs, out);
        }
        return true;
    }

    Normalizer norm({{fminf(lbox.min.x, rbox.min.x), fminf(lbox.min.y, rbox.min.y)},
                     {fmaxf(lbox.max.x, rbox.max.x), fmaxf(lbox.max.y, rbox.max.y)}});

    auto& ws = _workspace();
    ws.reset();

    List<Contour> a, b;

    _contour(ws, lhs, false, norm, a);
    _contour(ws, rhs, true, norm, b);

    //a path that covers no ground is the empty operand
    if (a.empty() || b.empty()) {
        if (op == PathOp::Union) {
            if (!a.empty()) _copy(lhs, out);
            if (!b.empty()) _copy(rhs, out);
        } else if (op == PathOp::Difference && !a.empty()) _copy(lhs, out);
        return true;
    }

    auto mark = out.pts.count;

    //align the directions
    _orient(a);
    _orient(b);

    _slice(ws, a, b);
    _slice(ws, b, a);

    auto crossings = _intersect(ws, a, b);
    crossings += _bridge(ws, a);

    if (crossings > 0) {
        _mark(a, b);
        _mark(b, a);
        _merge(a, b, op, out);
    }

    _uncrossed(a, b, op, true, out);
    _uncrossed(b, a, op, false, out);

    for (auto i = mark; i < out.pts.count; ++i) out.pts[i] = norm.out(out.pts[i]);

    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool tvg::pathop(const RenderPath& lhs, const RenderPath& rhs, RenderPath& out, PathOp op)
{
    if (lhs.cmds.empty() || rhs.cmds.empty()) return false;

    auto cnt = out.cmds.count;

    //exclude intersections == (a - b) + (b - a)
    if (op == PathOp::Xor) {
        auto ret = _op(lhs, rhs, out, PathOp::Difference);
        if (!_op(rhs, lhs, out, PathOp::Difference) && !ret) return false;
    } else if (!_op(lhs, rhs, out, op)) return false;

    return out.cmds.count > cnt;
}
