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

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "tvgGlCommon.h"
#include "tvgGlTessellator.h"
#include "tvgRender.h"
#include "tvgGlList.h"

namespace tvg
{

// common obj for memory control
struct Object
{
    virtual ~Object() = default;
};

class ObjectHeap
{
public:
    ObjectHeap() = default;
    ~ObjectHeap()
    {
        auto count = pObjs.count;
        auto first = pObjs.data;

        for (uint32_t i = 0; i < count; ++i) {
            delete static_cast<Object *>(first[i]);
        }
    }

    template<class T, class... Args>
    T *allocate(Args &&...args)
    {
        pObjs.push(new T(std::forward<Args>(args)...));
        return static_cast<T *>(pObjs.data[pObjs.count - 1]);
    }

private:
    Array<Object *> pObjs = {};
};

struct Vertex : public Object
{
    // list
    Vertex *prev = nullptr;
    Vertex *next = nullptr;

    uint32_t index = 0xFFFFFFFF;
    /**
     * All edge above and end with this vertex
     *
     *          head           tail
     *              \   ...    /
     *                   v
     *
     */
    LinkedList<Edge> edge_above = {};

    /**
     *  All edge below this vertex
     *
     *
     *              v
     *             / \
     *          head  tail
     *          /      \
     *            ...
     *
     */
    LinkedList<Edge> edge_below = {};

    // left enclosing edge during sweep line
    Edge *left = nullptr;
    // right enclosing edge during sweep line
    Edge *right = nullptr;

    Point point = {0.0f, 0.0f};

    Vertex() = default;

    Vertex(const Point &p)
    {
        point = {ceilf(p.x * 100.f) / 100.f, ceilf(p.y * 100.f) / 100.f};
    }

    ~Vertex() override = default;

    bool isConnected() const
    {
        return edge_above.head || edge_below.head;
    }

    void insertAbove(Edge *e);
    void insertBelow(Edge *e);
};


// we sort point by top first then left
struct VertexCompare
{
    inline bool operator()(Vertex *v1, Vertex *v2)
    {
        return compare(v1->point, v2->point);
    }

    static bool compare(const Point &a, const Point &b);
};


// double linked list for all vertex in shape
struct VertexList : public LinkedList<Vertex>
{
    VertexList() = default;
    VertexList(Vertex *head, Vertex *tail) : LinkedList(head, tail)
    {
    }

    void insert(Vertex *v, Vertex *prev, Vertex *next);
    void remove(Vertex *v);
    void append(VertexList const &other);
    void append(Vertex *v);
    void prepend(Vertex *v);
    void close();
};


struct Edge : public Object
{
    Vertex *top = nullptr;
    Vertex *bottom = nullptr;

    Edge *abovePrev = nullptr;
    Edge *aboveNext = nullptr;
    Edge *belowPrev = nullptr;
    Edge *belowNext = nullptr;

    Edge *left = nullptr;     // left edge in active list during sweep line
    Edge *right = nullptr;    // right edge in active list during sweep line

    // edge list in polygon
    Edge *rightPolyPrev = nullptr;
    Edge *rightPolyNext = nullptr;
    Edge *leftPolyPrev = nullptr;
    Edge *leftPolyNext = nullptr;

    Polygon *leftPoly = nullptr;      // left polygon during sweep line
    Polygon *rightPoly = nullptr;     // right polygon during sweep line

    bool usedInLeft = false;
    bool usedInRight = false;

    int32_t winding = 1;

    Edge(Vertex *top, Vertex *bottom, int32_t winding);

    ~Edge() override = default;

    // https://stackoverflow.com/questions/1560492/how-to-tell-whether-a-point-is-to-the-right-or-left-side-of-a-line
    // return > 0 means point in left
    // return < 0 means point in right
    double sideDist(const Point& p);

    bool isRightOf(const Point& p)
    {
        return sideDist(p) < 0.0;
    }

    bool isLeftOf(const Point& p)
    {
        return sideDist(p) > 0.0;
    }

    // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
    bool intersect(Edge *other, Point* point);
    void recompute();
    void setBottom(Vertex *v);
    void setTop(Vertex *v);
    void disconnect();

private:
    double le_a;
    double le_b;
    double le_c;
};


// Active Edge List (AEL) or Active Edge Table (AET) during sweep line
struct ActiveEdgeList : public LinkedList<Edge>
{
    ActiveEdgeList() = default;
    ~ActiveEdgeList() = default;

    void insert(Edge *edge, Edge *prev, Edge *next);
    void insert(Edge *edge, Edge *prev);

    void append(Edge *edge);
    void remove(Edge *edge);

    bool contains(Edge *edge);

    // move event point from current to dst
    void rewind(Vertex **current, Vertex *dst);

    void findEnclosing(Vertex *v, Edge **left, Edge **right);

    bool valid();
};


enum class Side
{
    kLeft,
    kRight,
};


struct Polygon : public Object
{
    Vertex *first_vert = nullptr;
    int32_t winding;

    // vertex count
    int32_t  count = 0;
    Polygon *parent = nullptr;
    Polygon *next = nullptr;

    MonotonePolygon *head = nullptr;
    MonotonePolygon *tail = nullptr;

    Polygon(Vertex *first, int32_t winding) : first_vert(first), winding(winding)
    {
    }

    ~Polygon() override = default;

    Polygon *addEdge(Edge *e, Side side, ObjectHeap *heap);

    Vertex *lastVertex() const;
};


struct MonotonePolygon : public Object
{
    Side side;

    Edge *first = nullptr;
    Edge *last = nullptr;

    int32_t winding;

    MonotonePolygon *prev = nullptr;
    MonotonePolygon *next = nullptr;

    void addEdge(Edge *edge);

    MonotonePolygon(Edge *edge, Side side, int32_t winding) : side(side), winding(winding)
    {
        addEdge(edge);
    }

    ~MonotonePolygon() override = default;
};


// ----------------------------- Impl -------------------------------
void Vertex::insertAbove(Edge *e)
{
    if (e->top->point == e->bottom->point ||                        // no edge
        VertexCompare::compare(e->bottom->point, e->top->point)) {  // not above
        return;
    }

    if (LinkedList<Edge>::contains<&Edge::aboveNext>(e, &this->edge_above.head, &this->edge_above.tail)) {
        return;
    }

    Edge *abovePrev = nullptr;
    Edge *aboveNext = nullptr;

    // find insertion point
    for (aboveNext = this->edge_above.head; aboveNext; aboveNext = aboveNext->aboveNext) {
        if (aboveNext->isRightOf(e->top->point)) {
            break;
        }

        abovePrev = aboveNext;
    }

    LinkedList<Edge>::insert<&Edge::abovePrev, &Edge::aboveNext>(e, abovePrev, aboveNext, &this->edge_above.head,
                                                                   &this->edge_above.tail);
}


void Vertex::insertBelow(Edge *e)
{
    if (e->top->point == e->bottom->point ||                        // no edge
        VertexCompare::compare(e->bottom->point, e->top->point)) {  // not below
        return;
    }

    if (LinkedList<Edge>::contains<&Edge::belowNext>(e, &this->edge_below.head, &this->edge_below.tail)) {
        return;
    }

    Edge *belowPrev = nullptr;
    Edge *belowNext = nullptr;

    // find insertion point
    for (belowNext = this->edge_below.head; belowNext; belowNext = belowNext->belowNext) {
        if (belowNext->isRightOf(e->bottom->point)) {
            break;
        }

        belowPrev = belowNext;
    }

    LinkedList<Edge>::insert<&Edge::belowPrev, &Edge::belowNext>(e, belowPrev, belowNext, &this->edge_below.head,
                                                                   &this->edge_below.tail);
}


bool VertexCompare::compare(const Point& a, const Point& b)
{
    return a.y < b.y || (a.y == b.y && a.x < b.x);
}


void VertexList::insert(Vertex *v, Vertex *prev, Vertex *next)
{
    LinkedList<Vertex>::insert<&Vertex::prev, &Vertex::next>(v, prev, next, &head, &tail);
}


void VertexList::remove(Vertex *v)
{
    LinkedList<Vertex>::remove<&Vertex::prev, &Vertex::next>(v, &head, &tail);
}


void VertexList::append(VertexList const &other)
{
    if (!other.head) {
        return;
    }

    if (tail) {
        tail->next = other.head;
        other.head->prev = tail;
    } else {
        head = other.head;
    }

    tail = other.tail;
}


void VertexList::append(Vertex *v)
{
    insert(v, tail, nullptr);
}


void VertexList::prepend(Vertex *v)
{
    insert(v, nullptr, head);
}


void VertexList::close()
{
    if (head && tail) {
        tail->next = head;
        head->prev = tail;
    }
}


Edge::Edge(Vertex *top, Vertex *bottom, int32_t winding)
    : top(top),
      bottom(bottom),
      winding(winding),
      le_a(static_cast<double>(bottom->point.y) - top->point.y),
      le_b(static_cast<double>(top->point.x) - bottom->point.x),
      le_c(static_cast<double>(top->point.y) * bottom->point.x - static_cast<double>(top->point.x) * bottom->point.y)
{
}


double Edge::sideDist(const Point& p)
{
    return le_a * p.x + le_b * p.y + le_c;
}


bool Edge::intersect(Edge *other, Point* point)
{
    if (top == other->top || bottom == other->bottom || top == other->bottom || bottom == other->top) return false;

    // check if two aabb bounds is intersect
    if (std::min(top->point.x, bottom->point.x) > std::max(other->top->point.x, other->bottom->point.x) ||
        std::max(top->point.x, bottom->point.x) < std::min(other->top->point.x, other->bottom->point.x) ||
        std::min(top->point.y, bottom->point.y) > std::max(other->top->point.y, other->bottom->point.y) ||
        std::max(top->point.y, bottom->point.y) < std::min(other->top->point.y, other->bottom->point.y)) {
        return false;
    }

    auto denom = le_a * other->le_b - le_b * other->le_a;
    if (tvg::zero(denom)) return false;

    auto dx = static_cast<double>(other->top->point.x) - top->point.x;
    auto dy = static_cast<double>(other->top->point.y) - top->point.y;
    auto s_number = dy * other->le_b + dx * other->le_a;
    auto t_number = dy * le_b + dx * le_a;

    if (denom > 0.0 ? (s_number < 0.0 || s_number > denom || t_number < 0.0 || t_number > denom) : (s_number > 0.0 || s_number < denom || t_number > 0.0 || t_number < denom)) return false;

    auto scale = 1.0 / denom;
    point->x = nearbyintf(static_cast<float>(top->point.x - s_number * le_b * scale));
    point->y = nearbyintf(static_cast<float>(top->point.y + s_number * le_a * scale));

    if (std::isinf(point->x) || std::isinf(point->y)) return false;
    if (std::abs(point->x - top->point.x) < 1e-6 && std::abs(point->y - top->point.y) < 1e-6) return false;
    if (std::abs(point->x - bottom->point.x) < 1e-6 && std::abs(point->y - bottom->point.y) < 1e-6) return false;
    if (std::abs(point->x - other->top->point.x) < 1e-6 && std::abs(point->y - other->top->point.y) < 1e-6) return false;
    if (std::abs(point->x - other->bottom->point.x) < 1e-6 && std::abs(point->y - other->bottom->point.y) < 1e-6) return false;

    return true;
}


void Edge::recompute()
{
    le_a = static_cast<double>(bottom->point.y) - top->point.y;
    le_b = static_cast<double>(top->point.x) - bottom->point.x;
    le_c = static_cast<double>(top->point.y) * bottom->point.x - static_cast<double>(top->point.x) * bottom->point.y;
}


void Edge::setBottom(Vertex *v)
{
    // remove this edge from bottom's above list
    LinkedList<Edge>::remove<&Edge::abovePrev, &Edge::aboveNext>(this, &bottom->edge_above.head,
                                                                   &bottom->edge_above.tail);
    // update bottom vertex
    bottom = v;
    // recompute line equation
    recompute();
    // insert self to new bottom's above list
    bottom->insertAbove(this);
}


void Edge::setTop(Vertex *v)
{
    // remove this edge from top's below list
    LinkedList<Edge>::remove<&Edge::belowPrev, &Edge::belowNext>(this, &top->edge_below.head, &top->edge_below.tail);
    // update top vertex
    top = v;
    // recompute line equation
    recompute();
    // insert self to new top's below list
    top->insertBelow(this);
}


static void remove_edge_above(Edge *edge)
{
    LinkedList<Edge>::remove<&Edge::abovePrev, &Edge::aboveNext>(edge, &edge->bottom->edge_above.head, &edge->bottom->edge_above.tail);
}


static void remove_edge_below(Edge *edge)
{
    LinkedList<Edge>::remove<&Edge::belowPrev, &Edge::belowNext>(edge, &edge->top->edge_below.head, &edge->top->edge_below.tail);
}


void Edge::disconnect()
{
    remove_edge_above(this);
    remove_edge_below(this);
}


void ActiveEdgeList::insert(Edge *e, Edge *prev, Edge *next)
{
    LinkedList<Edge>::insert<&Edge::left, &Edge::right>(e, prev, next, &head, &tail);
    if (!valid()) {
        return;
    }
}


void ActiveEdgeList::insert(Edge *e, Edge *prev)
{
    auto next = prev ? prev->right : head;

    insert(e, prev, next);
}


void ActiveEdgeList::append(Edge *e)
{
    insert(e, tail, nullptr);
}


void ActiveEdgeList::remove(Edge *e)
{
    LinkedList<Edge>::remove<&Edge::left, &Edge::right>(e, &head, &tail);
}


bool ActiveEdgeList::contains(Edge *edge)
{
    return edge->left || edge->right || head == edge;
}


void ActiveEdgeList::rewind(Vertex **current, Vertex *dst)
{
    if (!current || *current == dst || VertexCompare::compare((*current)->point, dst->point)) return;

    auto v = *current;

    while (v != dst) {
        v = v->prev;

        for (auto e = v->edge_below.head; e; e = e->belowNext) {
            this->remove(e);
        }

        auto left = v->left;

        for (auto e = v->edge_above.head; e; e = e->aboveNext) {
            this->insert(e, left);
            left = e;
            auto top = e->top;
            if (VertexCompare::compare(top->point, dst->point) &&
                ((top->left && !top->left->isLeftOf(e->top->point)) ||
                 (top->right && !top->right->isRightOf(e->top->point)))) {
                dst = top;
            }
        }
    }
    *current = v;
}


void ActiveEdgeList::findEnclosing(Vertex *v, Edge **left, Edge **right)
{
    if (v->edge_above.head && v->edge_above.tail) {
        *left = v->edge_above.head->left;
        *right = v->edge_above.tail->right;
        return;
    }

    Edge *prev = nullptr;
    Edge *next = nullptr;

    // walk through aet to get left most edge
    for (prev = tail; prev != nullptr; prev = prev->left) {
        if (prev->isLeftOf(v->point)) {
            break;
        }
        next = prev;
    }

    *left = prev;
    *right = next;
}


static bool _validEdgePair(Edge* left, Edge* right) {
    if (!left || !right) {
        return true;
    }

    if (left->top == right->top) {
        if (!left->isLeftOf(right->bottom->point)) {
            return false;
        }
        if (!right->isRightOf(left->bottom->point)) {
            return false;
        }
    } else if (VertexCompare::compare(left->top->point, right->top->point)) {
        if (!left->isLeftOf(right->top->point)) {
            return false;
        }
    } else {
        if (!right->isRightOf(left->top->point)) {
            return false;
        }
    }

    if (left->bottom == right->bottom) {
        if (!left->isLeftOf(right->top->point)) {
            return false;
        }
        if (!right->isRightOf(left->top->point)) {
            return false;
        }
    } else if (VertexCompare::compare(right->bottom->point, left->bottom->point)) {
        if (!left->isLeftOf(right->bottom->point)) {
            return false;
        }
    } else {
        if (!right->isRightOf(left->bottom->point)) {
            return false;
        }
    }

    return true;
}


bool ActiveEdgeList::valid()
{
    auto left = head;
    if (!left && !tail) {
        return true;
    } else if (!left || !tail) {
        return false;
    }

    for(auto right = left->right; right; right = right->right) {
        if (!_validEdgePair(left, right)) {
            return false;
        }

        left = right;
    }

    return true;
}


Polygon *Polygon::addEdge(Edge *e, Side side, ObjectHeap *heap)
{
    auto p_parent = this->parent;

    auto poly = this;

    if (side == Side::kRight) {
        if (e->usedInRight) {  // already in this polygon
            return this;
        }
    } else {
        if (e->usedInLeft) {  // already in this polygon
            return this;
        }
    }

    if (p_parent) {
        this->parent = p_parent->parent = nullptr;
    }

    if (!this->tail) {
        this->head = this->tail = heap->allocate<MonotonePolygon>(e, side, this->winding);
        this->count += 2;
    } else if (e->bottom == this->tail->last->bottom) {
        // close this polygon
        return poly;
    } else if (side == this->tail->side) {
        this->tail->addEdge(e);
        this->count++;
    } else {
        e = heap->allocate<Edge>(this->tail->last->bottom, e->bottom, 1);
        this->tail->addEdge(e);
        this->count++;

        if (p_parent) {
            p_parent->addEdge(e, side, heap);
            poly = p_parent;
        } else {
            auto m = heap->allocate<MonotonePolygon>(e, side, this->winding);
            m->prev = this->tail;

            this->tail->next = m;

            this->tail = m;
        }
    }

    return poly;
}


Vertex *Polygon::lastVertex() const
{
    if (tail) {
        return tail->last->bottom;
    }

    return first_vert;
}


void MonotonePolygon::addEdge(Edge *edge)
{
    if (this->side == Side::kRight) {
        LinkedList<Edge>::insert<&Edge::rightPolyPrev, &Edge::rightPolyNext>(edge, this->last, nullptr, &this->first, &this->last);
    } else {
        LinkedList<Edge>::insert<&Edge::leftPolyPrev, &Edge::leftPolyNext>(edge, last, nullptr, &this->first, &this->last);
    }
}


static bool _bezIsFlatten(const Bezier& bz)
{
    float diff1_x = fabs((bz.ctrl1.x * 3.f) - (bz.start.x * 2.f) - bz.end.x);
    float diff1_y = fabs((bz.ctrl1.y * 3.f) - (bz.start.y * 2.f) - bz.end.y);
    float diff2_x = fabs((bz.ctrl2.x * 3.f) - (bz.end.x * 2.f) - bz.start.x);
    float diff2_y = fabs((bz.ctrl2.y * 3.f) - (bz.end.y * 2.f) - bz.start.y);

    if (diff1_x < diff2_x) diff1_x = diff2_x;
    if (diff1_y < diff2_y) diff1_y = diff2_y;

    if (diff1_x + diff1_y <= 0.5f) return true;

    return false;
}


static int32_t _bezierCurveCount(const Bezier &curve)
{

    if (_bezIsFlatten(curve)) {
        return 1;
    }

    Bezier left{};
    Bezier right{};

    curve.split(left, right);

    return _bezierCurveCount(left) + _bezierCurveCount(right);
}


static Bezier _bezFromArc(const Point& start, const Point& end, float radius)
{
    // Calculate the angle between the start and end points
    auto angle = tvg::atan2(end.y - start.y, end.x - start.x);

    // Calculate the control points of the cubic bezier curve
    auto c = radius * 0.552284749831f;  // c = radius * (4/3) * tan(pi/8)

    Bezier bz;
    bz.start = {start.x, start.y};
    bz.ctrl1 = {start.x + radius * cos(angle), start.y + radius * sin(angle)};
    bz.ctrl2 = {end.x - c * cosf(angle), end.y - c * sinf(angle)};
    bz.end = {end.x, end.y};

    return bz;
}


static Point _upScalePoint(const Point& p)
{
    return Point{p.x * 1000.f, p.y * 1000.f};
}


static Point _downScalePoint(const Point& p)
{
    return Point {p.x / 1000.f, p.y / 1000.f};
}


static float _downScaleFloat(float v)
{
    return v / 1000.f;
}


static uint32_t _pushVertex(Array<float> *array, float x, float y)
{
    array->push(x);
    array->push(y);
    return (array->count - 2) / 2;
}


enum class Orientation
{
    Linear,
    Clockwise,
    CounterClockwise,
};


static Orientation _calcOrientation(const Point& p1, const Point& p2, const Point& p3)
{
    auto val = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
    if (std::abs(val) < 0.0001f) return Orientation::Linear;
    else return val > 0 ? Orientation::Clockwise : Orientation::CounterClockwise;
}


static Orientation _calcOrientation(const Point& dir1, const Point& dir2)
{
    auto val = (dir2.x - dir1.x) * (dir1.y + dir2.y);
    if (std::abs(val) < 0.0001f) return Orientation::Linear;
    return val > 0 ? Orientation::Clockwise : Orientation::CounterClockwise;
}


Tessellator::Tessellator(Array<float> *points, Array<uint32_t> *indices)
    : pHeap(new ObjectHeap),
      outlines(),
      pMesh(new VertexList),
      pPolygon(),
      resGlPoints(points),
      resIndices(indices)
{
}

Tessellator::~Tessellator()
{
    for (uint32_t i = 0; i < outlines.count; i++) {
        delete outlines[i];
    }

    delete pHeap;
    delete pMesh;
}


bool Tessellator::tessellate(const RenderShape *rshape, bool antialias)
{
    auto cmds = rshape->path.cmds.data;
    auto cmdCnt = rshape->path.cmds.count;
    auto pts = rshape->path.pts.data;
    auto ptsCnt = rshape->path.pts.count;

    this->fillRule = rshape->rule;

    this->visitShape(cmds, cmdCnt, pts, ptsCnt);
    this->buildMesh();
    this->mergeVertices();

    if (!this->simplifyMesh()) return false;
    if (!this->tessMesh()) return false;

    // output triangles
    for (auto poly = this->pPolygon; poly; poly = poly->next) {
        if (!this->matchFillRule(poly->winding)) continue;
        if (poly->count < 3) continue;
        for (auto m = poly->head; m; m = m->next) {
            this->emitPoly(m);
        }
    }

    if (antialias) {
        // TODO extract outline from current polygon list and generate aa edges
    }

    return true;
}


void Tessellator::tessellate(const Array<const RenderShape *> &shapes)
{
    this->fillRule = FillRule::NonZero;

    for (uint32_t i = 0; i < shapes.count; i++) {
        auto cmds = shapes[i]->path.cmds.data;
        auto cmdCnt = shapes[i]->path.cmds.count;
        auto pts = shapes[i]->path.pts.data;
        auto ptsCnt = shapes[i]->path.pts.count;

        this->visitShape(cmds, cmdCnt, pts, ptsCnt);
    }

    this->buildMesh();
    this->mergeVertices();
    this->simplifyMesh();
    this->tessMesh();

    // output triangles
    for (auto poly = this->pPolygon; poly; poly = poly->next) {
        if (!this->matchFillRule(poly->winding)) continue;
        if (poly->count < 3) continue;
        for (auto m = poly->head; m; m = m->next) {
            this->emitPoly(m);
        }
    }
}


void Tessellator::visitShape(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count)
{
    // all points at least need to be visit once
    // so the points count is at least the same as the count in shape
    resGlPoints->reserve(pts_count * 2);
    // triangle fans, the indices count is at least triangles number * 3
    resIndices->reserve((pts_count - 2) * 3);

    const Point *firstPt = nullptr;

    for (uint32_t i = 0; i < cmd_count; i++) {
        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                outlines.push(new VertexList);
                auto last = outlines.last();
                last->append(pHeap->allocate<Vertex>(_upScalePoint(*pts)));
                firstPt = pts;
                pts++;
                break;
            }
            case PathCommand::LineTo: {
                auto last = outlines.last();
                last->append(pHeap->allocate<Vertex>(_upScalePoint(*pts)));
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                // bezier curve needs to calculate how many segment to split
                // for now just break curve into 16 segments for convenient
                auto  last = outlines.last();
                Point start = _downScalePoint(Point{last->tail->point.x, last->tail->point.y});
                Point c1 = pts[0];
                Point c2 = pts[1];
                Point end = pts[2];
                Bezier curve{start, c1, c2, end};

                auto stepCount = _bezierCurveCount(curve);
                if (stepCount <= 1) stepCount = 2;
                auto step = 1.f / stepCount;

                for (uint32_t s = 1; s < static_cast<uint32_t>(stepCount); s++) {
                    last->append(pHeap->allocate<Vertex>(_upScalePoint(curve.at(step * s))));
                }

                last->append(pHeap->allocate<Vertex>(_upScalePoint(end)));
                pts += 3;
                break;
            }
            case PathCommand::Close: {
                if (firstPt && outlines.count > 0) {
                    auto last = outlines.last();
                    last->append(pHeap->allocate<Vertex>(_upScalePoint(*firstPt)));
                    firstPt = nullptr;
                }
                break;
            }
            default: break;
        }
    }
}


void Tessellator::buildMesh()
{
    Array<Vertex *> temp{};

    for (uint32_t i = 0; i < outlines.count; i++) {
        auto list = outlines[i];
        auto prev = list->tail;
        auto v = list->head;

        while (v) {
            auto next = v->next;
            auto edge = this->makeEdge(prev, v);

            if (edge) {
                edge->bottom->insertAbove(edge);
                edge->top->insertBelow(edge);
            }

            temp.push(v);

            prev = v;
            v = next;
        }
    }

    temp.sort<VertexCompare>();

    for (uint32_t i = 0; i < temp.count; i++) {
        this->pMesh->append(temp[i]);
    }
}


void Tessellator::mergeVertices()
{
    if (!pMesh->head) return;

    for (auto v = pMesh->head->next; v;) {
        auto next = v->next;

        // already sorted, this means these two points is same
        if (VertexCompare::compare(v->point, v->prev->point) || length(v->point - v->prev->point) <= 0.025f) {
            v->point = v->prev->point;
        }

        if (v->point == v->prev->point) {
            // merge v into v->prev
            while (auto e = v->edge_above.head) {
                e->setBottom(v->prev);
            }
            while (auto e = v->edge_below.head) {
                e->setTop(v->prev);
            }
            pMesh->remove(v);
        }
        v = next;
    }
}


bool Tessellator::simplifyMesh()
{
    /// this is a basic sweep line algorithm
    /// https://www.youtube.com/watch?v=qkhUNzCGDt0&t=293s
    /// in this function, we walk through all edges from top to bottom, and find
    /// all intersections edge and break them into flat segments by adding
    /// intersection point

    ActiveEdgeList ael{};

    for (auto v = pMesh->head; v; v = v->next) {
        if (!v->isConnected()) continue;

        Edge *left_enclosing = nullptr;
        Edge *right_enclosing = nullptr;
        auto intersected = false;

        do {
            intersected = false;

            ael.findEnclosing(v, &left_enclosing, &right_enclosing);

            v->left = left_enclosing;
            v->right = right_enclosing;

            // If AEL is not valid, means we meet the problem caused by floating point precision
            if (!ael.valid()) return false;

            if (v->edge_below.head) {
                for (auto e = v->edge_below.head; e; e = e->belowNext) {
                    // check current edge is intersected by left or right neighbor edges
                    if (checkIntersection(left_enclosing, e, &ael, &v) ||
                        checkIntersection(e, right_enclosing, &ael, &v)) {
                        intersected = true;
                        // find intersection between current and it's neighbor
                        break;
                    }
                }
            } else {
                // check left and right intersection
                if (checkIntersection(left_enclosing, right_enclosing, &ael, &v)) {
                    intersected = true;
                }
            }
        } while (intersected);

        // If AEL is not valid, means we meet the problem caused by floating point precision
        if (!ael.valid()) return false;

        // we are done for all edge end with current point
        for (auto e = v->edge_above.head; e; e = e->aboveNext) {
            ael.remove(e);
        }

        auto left = left_enclosing;

        // insert all edge start from current point into ael
        for (auto e = v->edge_below.head; e; e = e->belowNext) {
            ael.insert(e, left);
            left = e;
        }
    }
    return true;
}


bool Tessellator::tessMesh()
{
    /// this function also use sweep line algorithm
    /// but during the process, we calculate the winding number of left and right
    /// polygon and add edge to them

    ActiveEdgeList ael{};

    for (auto v = pMesh->head; v; v = v->next) {
        if (!v->isConnected()) continue;
        if (!ael.valid()) return false;

        Edge *left_enclosing = nullptr;
        Edge *right_enclosing = nullptr;

        ael.findEnclosing(v, &left_enclosing, &right_enclosing);

        /**
         *
         *                   ...
         *                      \
         *      leftPoly      head
         *                          v
         *
         */
        Polygon *leftPoly = nullptr;
        /**
         *
         *              ...
         *         /
         *       tail     rightPoly
         *     v
         *
         */
        Polygon *rightPoly = nullptr;

        if (v->edge_above.head) {
            leftPoly = v->edge_above.head->leftPoly;
            rightPoly = v->edge_above.tail->rightPoly;
        } else {
            leftPoly = left_enclosing ? left_enclosing->rightPoly : nullptr;
            rightPoly = right_enclosing ? right_enclosing->leftPoly : nullptr;
        }

        if (v->edge_above.head) {
            // add above edge first
            if (leftPoly) {
                leftPoly = leftPoly->addEdge(v->edge_above.head, Side::kRight, pHeap);
            }

            if (rightPoly) {
                rightPoly = rightPoly->addEdge(v->edge_above.tail, Side::kLeft, pHeap);
            }

            // walk through all edges end with this vertex
            for (auto e = v->edge_above.head; e != v->edge_above.tail; e = e->aboveNext) {
                auto right_edge = e->aboveNext;

                ael.remove(e);

                if (e->rightPoly) {
                    e->rightPoly->addEdge(right_edge, Side::kLeft, pHeap);
                }

                // this means there is a new polygon between e and right_edge
                if (right_edge->leftPoly && right_edge->leftPoly != e->rightPoly) {
                    right_edge->leftPoly->addEdge(e, Side::kRight, pHeap);
                }
            }

            ael.remove(v->edge_above.tail);

            // there is no edge begin with this vertex
            if (!v->edge_below.head) {
                if (leftPoly && rightPoly && leftPoly != rightPoly) {
                    // polygon not closed at this point
                    // need to mark these two polygon each other, because they will be
                    // linked by a cross edge later

                    leftPoly->parent = rightPoly;
                    rightPoly->parent = leftPoly;
                }
            }
        }

        if (v->edge_below.head) {
            if (!v->edge_above.head) {
                // there is no edge end with this vertex
                if (leftPoly && rightPoly) {

                    if (leftPoly == rightPoly) {
                        /**
                         *   leftPoly      rightPoly
                         *
                         *              v
                         *             / \
                         *            /   \
                         *             ...
                         */
                        if (leftPoly->tail && leftPoly->tail->side == Side::kLeft) {
                            leftPoly = this->makePoly(leftPoly->lastVertex(), leftPoly->winding);

                            left_enclosing->rightPoly = leftPoly;
                        } else {
                            rightPoly = this->makePoly(rightPoly->lastVertex(), rightPoly->winding);

                            right_enclosing->leftPoly = rightPoly;
                        }
                    }

                    // need to link this vertex to above polygon
                    auto join = pHeap->allocate<Edge>(leftPoly->lastVertex(), v, 1);

                    leftPoly = leftPoly->addEdge(join, Side::kRight, pHeap);
                    rightPoly = rightPoly->addEdge(join, Side::kLeft, pHeap);
                }
            }

            auto left_edge = v->edge_below.head;
            left_edge->leftPoly = leftPoly;

            ael.insert(left_edge, left_enclosing);

            for (auto right_edge = left_edge->belowNext; right_edge; right_edge = right_edge->belowNext) {
                ael.insert(right_edge, left_edge);

                int32_t winding = left_edge->leftPoly ? left_edge->leftPoly->winding : 0;

                winding += left_edge->winding;

                if (winding != 0) {
                    auto poly = this->makePoly(v, winding);

                    left_edge->rightPoly = right_edge->leftPoly = poly;
                }

                left_edge = right_edge;
            }

            v->edge_below.tail->rightPoly = rightPoly;
        }
    }
    return true;
}


bool Tessellator::matchFillRule(int32_t winding)
{
    if (fillRule == FillRule::NonZero) {
        return winding != 0;
    } else {
        return (winding & 0x1) != 0;
    }
}


Edge *Tessellator::makeEdge(Vertex *a, Vertex *b)
{
    if (!a || !b || a->point == b->point) {
        return nullptr;
    }

    int32_t winding = 1;

    if (VertexCompare::compare(b->point, a->point)) {
        winding = -1;
        std::swap(a, b);
    }

    return pHeap->allocate<Edge>(a, b, winding);
}


bool Tessellator::checkIntersection(Edge *left, Edge *right, ActiveEdgeList *ael, Vertex **current)
{
    if (!left || !right) return false;

    Point p;

    if (left->intersect(right, &p) && !std::isinf(p.x) && !std::isinf(p.y)) {
        Vertex *v;
        Vertex *top = *current;

        // the vertex in mesh is sorted, so walk to prev can find latest top point
        while (top && VertexCompare::compare(p, top->point)) {
            top = top->prev;
        }

        if (p == left->top->point) {
            v = left->top;
        } else if (p == left->bottom->point) {
            v = left->bottom;
        } else if (p == right->top->point) {
            v = right->top;
        } else if (p == right->bottom->point) {
            v = right->bottom;
        } else {
            // intersect point is between start and end point
            // need to insert new vertex
            auto prev = top;
            while (prev && VertexCompare::compare(p, prev->point)) {
                prev = prev->prev;
            }

            auto next = prev ? prev->next : pMesh->head;
            while (next && VertexCompare::compare(next->point, p)) {
                prev = next;
                next = next->next;
            }

            // check if point is already in mesh
            if (prev && prev->point == p) {
                v = prev;
            } else if (next && next->point == p) {
                v = next;
            } else {
                v = pHeap->allocate<Vertex>(p);
                v->point = p;
                pMesh->insert(v, prev, next);
            }
        }

        ael->rewind(current, top ? top : v);
        this->splitEdge(left, v, ael, current);
        this->splitEdge(right, v, ael, current);

        return true;
    }
    return this->intersectPairEdge(left, right, ael, current);
}


bool Tessellator::splitEdge(Edge *edge, Vertex *v, ActiveEdgeList *ael, Vertex **current)
{
    if (!edge->top || !edge->bottom || v == edge->top || v == edge->bottom) return false;

    auto winding = edge->winding;
    Vertex *top;
    Vertex *bottom;

    if (VertexCompare::compare(v->point, edge->top->point)) {
        /**
         *
         *   v
         *    \
         *     \
         *     top
         *      \
         *       \
         *        \
         *       bottom
         */
        top = v;
        bottom = edge->top;
        winding *= -1;
        edge->setTop(v);
    } else if (VertexCompare::compare(edge->bottom->point, v->point)) {
        /**
         *
         *   top
         *    \
         *     \
         *    bottom
         *      \
         *       \
         *        \
         *         v
         */
        top = edge->bottom;
        bottom = v;
        winding *= -1;
        edge->setBottom(v);
    } else {
        /**
         *
         *   top
         *    \
         *     \
         *      v
         *      \
         *       \
         *        \
         *       bottom
         */
        top = v;
        bottom = edge->bottom;
        edge->setBottom(v);
    }

    auto new_edge = pHeap->allocate<Edge>(top, bottom, winding);

    bottom->insertAbove(new_edge);
    top->insertBelow(new_edge);

    if (new_edge->abovePrev == nullptr && new_edge->aboveNext == nullptr) return false;
    if (new_edge->belowPrev == nullptr && new_edge->belowNext == nullptr) return false;

    return true;
}


bool Tessellator::intersectPairEdge(Edge *left, Edge *right, ActiveEdgeList *ael,
                                    Vertex **current)
{
    if (!left->top || !left->bottom || !right->top || !right->bottom) return false;
    if (left->top == right->top || left->bottom == right->bottom) return false;

    if (_calcOrientation(left->bottom->point - left->top->point, right->bottom->point - right->top->point) ==  Orientation::Linear) return false;

    Edge *split = nullptr;
    Vertex *split_at = nullptr;

    // check if these two edge is intersected
    if (VertexCompare::compare(left->top->point, right->top->point)) {
        if (!left->isLeftOf(right->top->point)) {
            split = left;
            split_at = right->top;
        }
    } else {
        if (!right->isRightOf(left->top->point)) {
            split = right;
            split_at = left->top;
        }
    }

    if (VertexCompare::compare(right->bottom->point, left->bottom->point)) {
        if (!left->isLeftOf(right->bottom->point)) {
            split = left;
            split_at = right->bottom;
        }
    } else {
        if (!right->isRightOf(left->bottom->point)) {
            split = right;
            split_at = left->bottom;
        }
    }

    if (!split) return false;

    ael->rewind(current, split->top);

    return splitEdge(split, split_at, ael, current);
}


Polygon *Tessellator::makePoly(Vertex *v, int32_t winding)
{
    auto poly = pHeap->allocate<Polygon>(v, winding);
    poly->next = this->pPolygon;
    this->pPolygon = poly;
    return poly;
}


void Tessellator::emitPoly(MonotonePolygon *poly)
{
    auto e = poly->first;
    VertexList vertices;

    vertices.append(e->top);
    int32_t count = 1;
    while (e != nullptr) {
        if (poly->side == Side::kRight) {
            vertices.append(e->bottom);
            e = e->rightPolyNext;
        } else {
            vertices.prepend(e->bottom);
            e = e->leftPolyNext;
        }
        count += 1;
    }

    if (count < 3) return;

    auto first = vertices.head;
    auto v = first->next;

    while (v != vertices.tail) {
        auto prev = v->prev;
        auto curr = v;
        auto next = v->next;

        if (count == 3) {
            emitTriangle(prev, curr, next);
            return;
        }

        double ax = static_cast<double>(curr->point.x) - prev->point.x;
        double ay = static_cast<double>(curr->point.y) - prev->point.y;
        double bx = static_cast<double>(next->point.x) - curr->point.x;
        double by = static_cast<double>(next->point.y) - curr->point.y;

        if (ax * by - ay * bx >= 0.0) {
            emitTriangle(prev, curr, next);
            v->prev->next = v->next;
            v->next->prev = v->prev;

            count--;

            if (v->prev == first) v = v->next;
            else v = v->prev;

        } else {
            v = v->next;
        }
    }
}


void Tessellator::emitTriangle(Vertex *p1, Vertex *p2, Vertex *p3)
{
    // check if index is generated
    if (p1->index == 0xFFFFFFFF) p1->index = _pushVertex(resGlPoints, _downScaleFloat(p1->point.x), _downScaleFloat(p1->point.y));
    if (p2->index == 0xFFFFFFFF) p2->index = _pushVertex(resGlPoints, _downScaleFloat(p2->point.x), _downScaleFloat(p2->point.y));
    if (p3->index == 0xFFFFFFFF) p3->index = _pushVertex(resGlPoints, _downScaleFloat(p3->point.x), _downScaleFloat(p3->point.y));

    resIndices->push(p1->index);
    resIndices->push(p2->index);
    resIndices->push(p3->index);
}


Stroker::Stroker(Array<float> *points, Array<uint32_t> *indices, const Matrix& matrix) : mResGlPoints(points), mResIndices(indices), mMatrix(matrix)
{
}


void Stroker::stroke(const RenderShape *rshape)
{
    mMiterLimit = rshape->strokeMiterlimit();
    mStrokeCap = rshape->strokeCap();
    mStrokeJoin = rshape->strokeJoin();
    mStrokeWidth = rshape->strokeWidth();

    if (isinf(mMatrix.e11)) {
        auto strokeWidth = rshape->strokeWidth() * getScaleFactor(mMatrix);
        if (strokeWidth <= MIN_GL_STROKE_WIDTH) strokeWidth = MIN_GL_STROKE_WIDTH;
        mStrokeWidth = strokeWidth / mMatrix.e11;
    }

    auto cmds = rshape->path.cmds.data;
    auto cmdCnt = rshape->path.cmds.count;
    auto pts = rshape->path.pts.data;
    auto ptsCnt = rshape->path.pts.count;

    if (rshape->strokeTrim()) {
        auto begin = 0.0f;
        auto end = 0.0f;
        rshape->stroke->trim.get(begin, end);

        if (begin == end) return;

        if (begin > end) {
            doTrimStroke(cmds, cmdCnt, pts, ptsCnt, rshape->stroke->trim.simultaneous, begin, 1.0f);
            doTrimStroke(cmds, cmdCnt, pts, ptsCnt, rshape->stroke->trim.simultaneous, 0.0f, end);
        } else {
            doTrimStroke(cmds, cmdCnt, pts, ptsCnt, rshape->stroke->trim.simultaneous, begin,  end);
        }

        return;
    }

    const float *dash_pattern = nullptr;
    auto dashCnt = rshape->strokeDash(&dash_pattern, nullptr);

    if (dashCnt == 0) doStroke(cmds, cmdCnt, pts, ptsCnt);
    else doDashStroke(cmds, cmdCnt, pts, ptsCnt, dashCnt, dash_pattern);
}


RenderRegion Stroker::bounds() const
{
    return RenderRegion {
        static_cast<int32_t>(floor(mLeftTop.x)),
        static_cast<int32_t>(floor(mLeftTop.y)),
        static_cast<int32_t>(ceil(mRightBottom.x - floor(mLeftTop.x))),
        static_cast<int32_t>(ceil(mRightBottom.y - floor(mLeftTop.y))),
    };
}

void Stroker::doTrimStroke(const PathCommand* cmds, uint32_t cmdCnt, const Point* pts, uint32_t ptsCnt, bool simultaneous, float start, float end)
{
    if (simultaneous) {
        auto startCmds = cmds;
        auto currCmds = cmds;
        int ptsNum = 0;
        for (uint32_t i = 0; i < cmdCnt; i++) {
            switch (*currCmds) {
                case PathCommand::MoveTo: {
                    if (currCmds != startCmds) {
                        PathTrim trim{};
                        if (trim.trim(startCmds, currCmds - startCmds, pts, ptsNum, start, end)) {
                            const auto& sCmds = trim.cmds();
                            const auto& sPts = trim.pts();
                            doStroke(sCmds.data, sCmds.count, sPts.data, sPts.count);
                        }
                        startCmds = currCmds;
                        pts += ptsNum;
                        ptsNum = 0;
                    }
                    currCmds++;
                    ptsNum++;
                    break;
                }
                case PathCommand::LineTo:
                    currCmds++;
                    ptsNum++;
                    break;
                case PathCommand::CubicTo:
                    currCmds++;
                    ptsNum += 3;
                    break;
                case PathCommand::Close: {
                    PathTrim trim{};
                    currCmds++;
                    if (trim.trim(startCmds, currCmds - startCmds, pts, ptsNum, start, end)) {
                        const auto& sCmds = trim.cmds();
                        const auto& sPts = trim.pts();
                        doStroke(sCmds.data, sCmds.count, sPts.data, sPts.count);
                    }
                    startCmds = currCmds;
                    pts += ptsNum;
                    ptsNum = 0;
                    break;
                }
            }
        }

        if (startCmds != currCmds && ptsNum > 0) {
            PathTrim trim{};

            if (trim.trim(startCmds, currCmds - startCmds, pts, ptsNum, start, end)) {
                const auto& sCmds = trim.cmds();
                const auto& sPts = trim.pts();
                doStroke(sCmds.data, sCmds.count, sPts.data, sPts.count);
            }
            startCmds = currCmds;
            pts += ptsNum;
            ptsNum = 0;
        }
    } else {
        PathTrim trim{};
        if (trim.trim(cmds, cmdCnt, pts, ptsCnt, start, end)) {
            const auto& sCmds = trim.cmds();
            const auto& sPts = trim.pts();
            doStroke(sCmds.data, sCmds.count, sPts.data, sPts.count);
        }
    }
}


void Stroker::doStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count)
{
    mResGlPoints->reserve(pts_count * 4 + 16);
    mResIndices->reserve(pts_count * 3);

    auto validStrokeCap = false;

    for (uint32_t i = 0; i < cmd_count; i++) {
        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                if (validStrokeCap) { // check this, so we can skip if path only contains move instruction
                    strokeCap();
                    validStrokeCap = false;
                }
                mStrokeState.firstPt = *pts;
                mStrokeState.firstPtDir = {0.0f, 0.0f};
                mStrokeState.prevPt = *pts;
                mStrokeState.prevPtDir = {0.0f, 0.0f};
                pts++;
                validStrokeCap = false;
            } break;
            case PathCommand::LineTo: {
                validStrokeCap = true;
                this->strokeLineTo(*pts);
                pts++;
            } break;
            case PathCommand::CubicTo: {
                validStrokeCap = true;
                this->strokeCubicTo(pts[0], pts[1], pts[2]);
                pts += 3;
            } break;
            case PathCommand::Close: {
                this->strokeClose();

                validStrokeCap = false;
            } break;
            default:
                break;
        }
    }
    if (validStrokeCap) strokeCap();
}


void Stroker::doDashStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count, uint32_t dash_count, const float *dash_pattern)
{
    Array<PathCommand> dash_cmds{};
    Array<Point> dash_pts{};

    dash_cmds.reserve(20 * cmd_count);
    dash_pts.reserve(20 * pts_count);

    DashStroke dash(&dash_cmds, &dash_pts, dash_count, dash_pattern);
    dash.doStroke(cmds, cmd_count, pts, pts_count);

    this->doStroke(dash_cmds.data, dash_cmds.count, dash_pts.data, dash_pts.count);
}


void Stroker::strokeCap()
{
    if (mStrokeCap == StrokeCap::Butt) return;

    if (mStrokeCap == StrokeCap::Square) {
        if (mStrokeState.firstPt == mStrokeState.prevPt) strokeSquarePoint(mStrokeState.firstPt);
        else {
            strokeSquare(mStrokeState.firstPt, {-mStrokeState.firstPtDir.x, -mStrokeState.firstPtDir.y});
            strokeSquare(mStrokeState.prevPt, mStrokeState.prevPtDir);
        }
    } else if (mStrokeCap == StrokeCap::Round) {
        if (mStrokeState.firstPt == mStrokeState.prevPt) strokeRoundPoint(mStrokeState.firstPt);
        else {
            strokeRound(mStrokeState.firstPt, {-mStrokeState.firstPtDir.x, -mStrokeState.firstPtDir.y});
            strokeRound(mStrokeState.prevPt, mStrokeState.prevPtDir);
        }
    }
}


void Stroker::strokeLineTo(const Point& curr)
{
    auto dir = (curr - mStrokeState.prevPt);
    normalize(dir);

    if (dir.x == 0.f && dir.y == 0.f) return;  //same point

    auto normal = Point{-dir.y, dir.x};
    auto a = mStrokeState.prevPt + normal * strokeRadius();
    auto b = mStrokeState.prevPt - normal * strokeRadius();
    auto c = curr + normal * strokeRadius();
    auto d = curr - normal * strokeRadius();

    auto ia = _pushVertex(mResGlPoints, a.x, a.y);
    auto ib = _pushVertex(mResGlPoints, b.x, b.y);
    auto ic = _pushVertex(mResGlPoints, c.x, c.y);
    auto id = _pushVertex(mResGlPoints, d.x, d.y);

    /**
     *   a --------- c
     *   |           |
     *   |           |
     *   b-----------d
     */

    this->mResIndices->push(ia);
    this->mResIndices->push(ib);
    this->mResIndices->push(ic);

    this->mResIndices->push(ib);
    this->mResIndices->push(id);
    this->mResIndices->push(ic);

    if (mStrokeState.prevPt == mStrokeState.firstPt) {
        // first point after moveTo
        mStrokeState.prevPt = curr;
        mStrokeState.prevPtDir = dir;
        mStrokeState.firstPtDir = dir;
    } else {
        this->strokeJoin(dir);
        mStrokeState.prevPtDir = dir;
        mStrokeState.prevPt = curr;
    }

    if (ia == 0) {
        mRightBottom.x = mLeftTop.x = curr.x;
        mRightBottom.y = mLeftTop.y = curr.y;
    }

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::strokeCubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    Bezier curve{};
    curve.start = {mStrokeState.prevPt.x, mStrokeState.prevPt.y};
    curve.ctrl1 = {cnt1.x, cnt1.y};
    curve.ctrl2 = {cnt2.x, cnt2.y};
    curve.end = {end.x, end.y};

    Bezier relCurve {curve.start, curve.ctrl1, curve.ctrl2, curve.end};
    relCurve.start *= mMatrix;
    relCurve.ctrl1 *= mMatrix;
    relCurve.ctrl2 *= mMatrix;
    relCurve.end *= mMatrix;

    auto count = _bezierCurveCount(relCurve);
    auto step = 1.f / count;

    for (int32_t i = 0; i <= count; i++) {
        strokeLineTo(curve.at(step * i));
    }
}


void Stroker::strokeClose()
{
    if (mStrokeState.prevPt != mStrokeState.firstPt) {
        this->strokeLineTo(mStrokeState.firstPt);
    }

    // join firstPt with prevPt
    this->strokeJoin(mStrokeState.firstPtDir);
}


void Stroker::strokeJoin(const Point& dir)
{
    auto orientation = _calcOrientation(mStrokeState.prevPt - mStrokeState.prevPtDir, mStrokeState.prevPt, mStrokeState.prevPt + dir);

    if (orientation == Orientation::Linear) {
        if (mStrokeState.prevPtDir == dir) return;      // check is same direction
        if (mStrokeJoin != StrokeJoin::Round) return;   // opposite direction

        auto normal = Point{-dir.y, dir.x};
        auto p1 = mStrokeState.prevPt + normal * strokeRadius();
        auto p2 = mStrokeState.prevPt - normal * strokeRadius();
        auto oc = mStrokeState.prevPt + dir * strokeRadius();

        this->strokeRound(p1, oc, mStrokeState.prevPt);
        this->strokeRound(oc, p2, mStrokeState.prevPt);

    } else {
        auto normal = Point{-dir.y, dir.x};
        auto prevNormal = Point{-mStrokeState.prevPtDir.y, mStrokeState.prevPtDir.x};
        Point prevJoin, currJoin;

        if (orientation == Orientation::CounterClockwise) {
            prevJoin = mStrokeState.prevPt + prevNormal * strokeRadius();
            currJoin = mStrokeState.prevPt + normal * strokeRadius();
        } else {
            prevJoin = mStrokeState.prevPt - prevNormal * strokeRadius();
            currJoin = mStrokeState.prevPt - normal * strokeRadius();
        }

        if (mStrokeJoin == StrokeJoin::Miter) strokeMiter(prevJoin, currJoin, mStrokeState.prevPt);
        else if (mStrokeJoin == StrokeJoin::Bevel) strokeBevel(prevJoin, currJoin, mStrokeState.prevPt);
        else this->strokeRound(prevJoin, currJoin, mStrokeState.prevPt);
    }
}


void Stroker::strokeRound(const Point &prev, const Point& curr, const Point& center)
{
    if (_calcOrientation(prev, center, curr) == Orientation::Linear) return;

    mLeftTop.x = std::min(mLeftTop.x, std::min(center.x, std::min(prev.x, curr.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(center.y, std::min(prev.y, curr.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(center.x, std::max(prev.x, curr.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(center.y, std::max(prev.y, curr.y)));

    // Fixme: just use bezier curve to calculate step count
    auto count = _bezierCurveCount(_bezFromArc(prev, curr, strokeRadius()));
    auto c = _pushVertex(mResGlPoints, center.x, center.y);
    auto pi = _pushVertex(mResGlPoints, prev.x, prev.y);
    auto step = 1.f / (count - 1);
    auto dir = curr - prev;

    for (uint32_t i = 1; i < static_cast<uint32_t>(count); i++) {
        auto t = i * step;
        auto p = prev + dir * t;
        auto o_dir = p - center;
        normalize(o_dir);

        auto out = center + o_dir * strokeRadius();
        auto oi = _pushVertex(mResGlPoints, out.x, out.y);

        mResIndices->push(c);
        mResIndices->push(pi);
        mResIndices->push(oi);

        pi = oi;

        mLeftTop.x = std::min(mLeftTop.x, out.x);
        mLeftTop.y = std::min(mLeftTop.y, out.y);
        mRightBottom.x = std::max(mRightBottom.x, out.x);
        mRightBottom.y = std::max(mRightBottom.y, out.y);
    }
}


void Stroker::strokeRoundPoint(const Point &p)
{
    // Fixme: just use bezier curve to calculate step count
    auto count = _bezierCurveCount(_bezFromArc(p, p, strokeRadius())) * 2;
    auto c = _pushVertex(mResGlPoints, p.x, p.y);
    auto step = 2 * M_PI / (count - 1);

    for (uint32_t i = 1; i <= static_cast<uint32_t>(count); i++) {
        float angle = i * step;
        Point dir = {cos(angle), sin(angle)};
        Point out = p + dir * strokeRadius();
        auto oi = _pushVertex(mResGlPoints, out.x, out.y);

        if (oi > 1) {
            mResIndices->push(c);
            mResIndices->push(oi);
            mResIndices->push(oi - 1);
        }
    }

    mLeftTop.x = std::min(mLeftTop.x, p.x - strokeRadius());
    mLeftTop.y = std::min(mLeftTop.y, p.y - strokeRadius());
    mRightBottom.x = std::max(mRightBottom.x, p.x + strokeRadius());
    mRightBottom.y = std::max(mRightBottom.y, p.y + strokeRadius());
}


void Stroker::strokeMiter(const Point& prev, const Point& curr, const Point& center)
{
    auto pp1 = prev - center;
    auto pp2 = curr - center;
    auto out = pp1 + pp2;
    auto k = 2.f * strokeRadius() * strokeRadius() / (out.x * out.x + out.y * out.y);
    auto pe = out * k;

    if (length(pe) >= mMiterLimit * strokeRadius()) {
        this->strokeBevel(prev, curr, center);
        return;
    }

    auto join = center + pe;
    auto c = _pushVertex(mResGlPoints, center.x, center.y);
    auto cp1 = _pushVertex(mResGlPoints, prev.x, prev.y);
    auto cp2 = _pushVertex(mResGlPoints, curr.x, curr.y);
    auto e = _pushVertex(mResGlPoints, join.x, join.y);

    mResIndices->push(c);
    mResIndices->push(cp1);
    mResIndices->push(e);

    mResIndices->push(e);
    mResIndices->push(cp2);
    mResIndices->push(c);

    mLeftTop.x = std::min(mLeftTop.x, join.x);
    mLeftTop.y = std::min(mLeftTop.y, join.y);

    mRightBottom.x = std::max(mRightBottom.x, join.x);
    mRightBottom.y = std::max(mRightBottom.y, join.y);
}


void Stroker::strokeBevel(const Point& prev, const Point& curr, const Point& center)
{
    auto a = _pushVertex(mResGlPoints, prev.x, prev.y);
    auto b = _pushVertex(mResGlPoints, curr.x, curr.y);
    auto c = _pushVertex(mResGlPoints, center.x, center.y);

    mResIndices->push(a);
    mResIndices->push(b);
    mResIndices->push(c);
}


void Stroker::strokeSquare(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};

    auto a = p + normal * strokeRadius();
    auto b = p - normal * strokeRadius();
    auto c = a + outDir * strokeRadius();
    auto d = b + outDir * strokeRadius();

    auto ai = _pushVertex(mResGlPoints, a.x, a.y);
    auto bi = _pushVertex(mResGlPoints, b.x, b.y);
    auto ci = _pushVertex(mResGlPoints, c.x, c.y);
    auto di = _pushVertex(mResGlPoints, d.x, d.y);

    mResIndices->push(ai);
    mResIndices->push(bi);
    mResIndices->push(ci);

    mResIndices->push(ci);
    mResIndices->push(bi);
    mResIndices->push(di);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::strokeSquarePoint(const Point& p)
{
    auto offsetX = Point{strokeRadius(), 0.0f};
    auto offsetY = Point{0.0f, strokeRadius()};

    auto a = p + offsetX + offsetY;
    auto b = p - offsetX + offsetY;
    auto c = p - offsetX - offsetY;
    auto d = p + offsetX - offsetY;

    auto ai = _pushVertex(mResGlPoints, a.x, a.y);
    auto bi = _pushVertex(mResGlPoints, b.x, b.y);
    auto ci = _pushVertex(mResGlPoints, c.x, c.y);
    auto di = _pushVertex(mResGlPoints, d.x, d.y);

    mResIndices->push(ai);
    mResIndices->push(bi);
    mResIndices->push(ci);

    mResIndices->push(ci);
    mResIndices->push(di);
    mResIndices->push(ai);

    mLeftTop.x = std::min(mLeftTop.x, std::min(std::min(a.x, b.x), std::min(c.x, d.x)));
    mLeftTop.y = std::min(mLeftTop.y, std::min(std::min(a.y, b.y), std::min(c.y, d.y)));
    mRightBottom.x = std::max(mRightBottom.x, std::max(std::max(a.x, b.x), std::max(c.x, d.x)));
    mRightBottom.y = std::max(mRightBottom.y, std::max(std::max(a.y, b.y), std::max(c.y, d.y)));
}


void Stroker::strokeRound(const Point& p, const Point& outDir)
{
    auto normal = Point{-outDir.y, outDir.x};
    auto a = p + normal * strokeRadius();
    auto b = p - normal * strokeRadius();
    auto c = p + outDir * strokeRadius();

    strokeRound(a, c, p);
    strokeRound(c, b, p);
}


DashStroke::DashStroke(Array<PathCommand> *cmds, Array<Point> *pts, uint32_t dash_count, const float *dash_pattern)
    : mCmds(cmds),
      mPts(pts),
      mDashCount(dash_count),
      mDashPattern(dash_pattern),
      mCurrLen(),
      mCurrIdx(),
      mCurOpGap(false),
      mPtStart(),
      mPtCur()
{
}


void DashStroke::doStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count)
{
    for (uint32_t i = 0; i < cmd_count; i++) {
        switch (*cmds) {
            case PathCommand::Close: {
                this->dashLineTo(mPtStart);
                break;
            }
            case PathCommand::MoveTo: {
                // reset the dash state
                mCurrIdx = 0;
                mCurrLen = 0.f;
                mCurOpGap = false;
                mPtStart = mPtCur = *pts;
                pts++;
                break;
            }
            case PathCommand::LineTo: {
                this->dashLineTo(*pts);
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                this->dashCubicTo(pts[0], pts[1], pts[2]);
                pts += 3;
                break;
            }
            default: break;
        }
        cmds++;
    }
}


void DashStroke::dashLineTo(const Point& to)
{
    auto len = length(mPtCur - to);

    if (len < mCurrLen) {
        mCurrLen -= len;
        if (!mCurOpGap) {
            this->moveTo(mPtCur);
            this->lineTo(to);
        }
    } else {
        Line curr = {mPtCur, to};

        while (len > mCurrLen) {
            len -= mCurrLen;

            Line left, right;
            curr.split(mCurrLen, left, right);

            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            if (!mCurOpGap) {
                this->moveTo(left.pt1);
                this->lineTo(left.pt2);
            }
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
            curr = right;
            mPtCur = curr.pt1;
        }
        mCurrLen -= len;
        if (!mCurOpGap) {
            this->moveTo(curr.pt1);
            this->lineTo(curr.pt2);
        }

        if (mCurrLen < 1) {
            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
        }
    }

    mPtCur = to;
}


void DashStroke::dashCubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    Bezier cur;
    cur.start = {mPtCur.x, mPtCur.y};
    cur.ctrl1 = {cnt1.x, cnt1.y};
    cur.ctrl2 = {cnt2.x, cnt2.y};
    cur.end = {end.x, end.y};

    auto len = cur.length();

    if (len < mCurrLen) {
        mCurrLen -= len;
        if (!mCurOpGap) {
            this->moveTo(mPtCur);
            this->cubicTo(cnt1, cnt2, end);
        }
    } else {
        while (len > mCurrLen) {
            len -= mCurrLen;

            Bezier left, right;
            cur.split(mCurrLen, left, right);

            if (mCurrIdx == 0) {
                this->moveTo(left.start);
                this->cubicTo(left.ctrl1, left.ctrl2, left.end);
            }

            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
            cur = right;
            mPtCur = cur.start;
        }

        mCurrLen -= len;
        if (!mCurOpGap) {
            this->moveTo(cur.start);
            this->cubicTo(cur.ctrl1, cur.ctrl2, cur.end);
        }

        if (mCurrLen < 1) {
            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
        }
    }
    mPtCur = end;
}


void DashStroke::moveTo(const Point& pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::MoveTo);
}


void DashStroke::lineTo(const Point& pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::LineTo);
}


void DashStroke::cubicTo(const Point& cnt1, const Point& cnt2, const Point& end)
{
    mPts->push({cnt1.x, cnt1.y});
    mPts->push({cnt2.x, cnt2.y});
    mPts->push({end.x, end.y});
    mCmds->push(PathCommand::CubicTo);
}


bool PathTrim::trim(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count, float start, float end)
{
    if (end - start < 0.0001f) return false;

    auto len = pathLength(cmds, cmd_count, pts, pts_count);
    if (len < 0.001f) return false;

    auto startLength = len * start;
    auto endLength = len * end;
    if (startLength >= endLength) return false;

    trimPath(cmds, cmd_count, pts, pts_count, startLength, endLength);

    return true;
}


float PathTrim::pathLength(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count)
{
    auto len = 0.0f;
    Point zero = {0.0f, 0.0f};
    const Point* prev = nullptr;
    const Point* begin = nullptr;

    for (uint32_t i = 0; i < cmd_count; i++) {
        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                prev = pts;
                begin = pts;
                pts++;
                break;
            }
            case PathCommand::LineTo: {
                if (prev != nullptr) len += length(prev, pts);
                if (begin == nullptr) begin = pts;
                prev = pts;
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                if (prev == nullptr || begin == nullptr) {
                    prev = begin = &zero;
                }
                Bezier b{ *prev, pts[0], pts[1], pts[2]};
                len += b.length();
                prev = pts + 2;
                pts += 3;
                break;
            }
            case PathCommand::Close: {
                if (prev != nullptr && begin != nullptr && prev != begin) {
                    len += length(prev, begin);
                }
                prev = begin = nullptr;
                break;
            }
        }
    }
    return len;
}


void PathTrim::trimPath(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count, float start, float end)
{
    auto pos = 0.0f;
    Point zero = {0.0f, 0.0f};
    Point prev = {};
    Point begin = {};
    auto closed = true;
    auto pushedMoveTo = false;
    auto hasLineTo = false;

    auto handle_line_to = [&](const Point* p1, const Point* p2) {
        auto currLen = length(p1, p2);
        if (pos + currLen <= start) {
            pos += currLen;
            prev = *p2;
            return;
        }

        if (pos >= end)  {
            prev = *p2;
            return;
        }

        Line line{*p1, *p2};

        if (pos < start) {
            Line left, right;
            line.split(start - pos, left, right);

            pos += left.length();
            line = right;
        }

        if (pos + currLen > end) {
            Line left, right;
            line.split(end - pos, left, right);
            pos += left.length();
            line = left;
        }

        if (!pushedMoveTo) {
            mCmds.push(PathCommand::MoveTo);
            mPts.push(line.pt1);
            pushedMoveTo = true;
            begin = line.pt1;
        }

        pos += line.length();

        mCmds.push(PathCommand::LineTo);
        mPts.push(line.pt2);
        prev = line.pt2;
        hasLineTo = true;
        closed = false;
    };

    for (uint32_t i = 0; i < cmd_count; i++) {
        if (pos - end > 0.001f) return; // we are done

        if (pos >= end) return;

        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                prev = *pts;
                begin = *pts;
                pts++;
                closed = false;
                break;
            }
            case PathCommand::LineTo: {
                handle_line_to(&prev, pts);
                hasLineTo = true;
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                Bezier b{ prev, pts[0], pts[1], pts[2]};

                auto currLen = b.length();
                if (pos + currLen <= start) {
                    pos += currLen;
                    prev = pts[2];
                    pts += 3;
                    break;
                }

                if (pos < start) {
                    Bezier left, right;
                    b.split(start - pos, left, right);
                    pos += left.length();
                    b = right;
                }

                if (pos + currLen > end) {
                    Bezier left, right;
                    b.split(end - pos, left, right);
                    pos += left.length();
                    b = left;
                }

                if (!pushedMoveTo) {
                    mCmds.push(PathCommand::MoveTo);
                    mPts.push(b.start);
                    pushedMoveTo = true;
                }


                pos += b.length();
                mCmds.push(PathCommand::CubicTo);
                mPts.push(b.ctrl1);
                mPts.push(b.ctrl2);
                mPts.push(b.end);
                prev = b.end;
                hasLineTo = true;
                closed = false;
                pts += 3;
                break;
            }
            case PathCommand::Close: {
                if (closed) break;
                if (!hasLineTo) {
                    closed = true;
                    pushedMoveTo = false;
                    prev = begin = zero;
                    break;
                }
                if (prev == begin) {
                    prev = begin = zero;
                    closed = true;
                    pushedMoveTo = false;
                    break;
                }
                auto currLen = length(&prev, &begin);
                if (currLen + pos < start) {
                    pos += currLen;
                    break;
                }
                if (pos + currLen  <= end) {
                    mCmds.push(PathCommand::Close);
                    pos += currLen;
                    break;
                }
                // handle_line_to(&prev, &begin);
                closed = true;
                pushedMoveTo = false;
                prev = begin = zero;
                pos += currLen;
                break;
            }
        }
    }
}


BWTessellator::BWTessellator(Array<float>* points, Array<uint32_t>* indices): mResPoints(points), mResIndices(indices)
{
}


void BWTessellator::tessellate(const RenderShape *rshape, const Matrix& matrix)
{
    auto cmds = rshape->path.cmds.data;
    auto cmdCnt = rshape->path.cmds.count;
    auto pts = rshape->path.pts.data;
    auto ptsCnt = rshape->path.pts.count;

    if (ptsCnt <= 2) return;

    uint32_t firstIndex = 0;
    uint32_t prevIndex = 0;

    mResPoints->reserve(ptsCnt * 2);
    mResIndices->reserve((ptsCnt - 2) * 3);

    for (uint32_t i = 0; i < cmdCnt; i++) {
        switch(cmds[i]) {
            case PathCommand::MoveTo: {
                firstIndex = pushVertex(pts->x, pts->y);
                prevIndex = 0;
                pts++;
            } break;
            case PathCommand::LineTo: {
                if (prevIndex == 0) {
                    prevIndex = pushVertex(pts->x, pts->y);
                    pts++;
                } else {
                    auto currIndex = pushVertex(pts->x, pts->y);
                    pushTriangle(firstIndex, prevIndex, currIndex);
                    prevIndex = currIndex;
                    pts++;
                }
            } break;
            case PathCommand::CubicTo: {
                Bezier curve{pts[-1], pts[0], pts[1], pts[2]};
                Bezier relCurve {pts[-1], pts[0], pts[1], pts[2]};
                relCurve.start *= matrix;
                relCurve.ctrl1 *= matrix;
                relCurve.ctrl2 *= matrix;
                relCurve.end *= matrix;

                auto stepCount = _bezierCurveCount(relCurve);
                if (stepCount <= 1) stepCount = 2;

                float step = 1.f / stepCount;

                for (uint32_t s = 1; s <= static_cast<uint32_t>(stepCount); s++) {
                    auto pt = curve.at(step * s);
                    auto currIndex = pushVertex(pt.x, pt.y);

                    if (prevIndex == 0) {
                        prevIndex = currIndex;
                        continue;
                    }

                    pushTriangle(firstIndex, prevIndex, currIndex);
                    prevIndex = currIndex;
                }

                pts += 3;
            } break;
            case PathCommand::Close:
            default:
                break;
        }
    }
}


RenderRegion BWTessellator::bounds() const
{
    return RenderRegion {
        static_cast<int32_t>(floor(mLeftTop.x)),
        static_cast<int32_t>(floor(mLeftTop.y)),
        static_cast<int32_t>(ceil(mRightBottom.x - floor(mLeftTop.x))),
        static_cast<int32_t>(ceil(mRightBottom.y - floor(mLeftTop.y))),
    };
}


uint32_t BWTessellator::pushVertex(float x, float y)
{
    auto index = _pushVertex(mResPoints, x, y);

    if (index == 0) {
        mRightBottom.x = mLeftTop.x = x;
        mRightBottom.y = mLeftTop.y = y;
    } else {
        mLeftTop.x = std::min(mLeftTop.x, x);
        mLeftTop.y = std::min(mLeftTop.y, y);
        mRightBottom.x = std::max(mRightBottom.x, x);
        mRightBottom.y = std::max(mRightBottom.y , y);
    }

    return index;
}


void BWTessellator::pushTriangle(uint32_t a, uint32_t b, uint32_t c)
{
    mResIndices->push(a);
    mResIndices->push(b);
    mResIndices->push(c);
}

}  // namespace tvg
