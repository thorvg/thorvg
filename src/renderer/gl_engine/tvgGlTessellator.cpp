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

#include "tvgGlTessellator.h"
#include "tvgRender.h"
#include "tvgGlList.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace tvg
{

namespace detail
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

    GlPoint point = {};

    Vertex() = default;

    Vertex(const GlPoint &p) : point(ceilf(p.x * 100.f) / 100.f, ceilf(p.y * 100.f) / 100.f)
    {
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

    static bool compare(const GlPoint &a, const GlPoint &b);
};


// double linked list for all vertex in shape
struct VertexList : public LinkedList<detail::Vertex>
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

    Edge *above_prev = nullptr;
    Edge *above_next = nullptr;
    Edge *below_prev = nullptr;
    Edge *below_next = nullptr;

    // left edge in active list during sweep line
    Edge *left = nullptr;
    // right edge in active list during sweep line
    Edge *right = nullptr;

    // edge list in polygon
    Edge *right_poly_prev = nullptr;
    Edge *right_poly_next = nullptr;
    Edge *left_poly_prev = nullptr;
    Edge *left_poly_next = nullptr;

    // left polygon during sweep line
    Polygon *left_poly = nullptr;
    // right polygon during sweep line
    Polygon *right_poly = nullptr;

    bool used_in_left = false;
    bool used_in_right = false;

    int32_t winding = 1;

    Edge(Vertex *top, Vertex *bottom, int32_t winding);

    ~Edge() override = default;

    // https://stackoverflow.com/questions/1560492/how-to-tell-whether-a-point-is-to-the-right-or-left-side-of-a-line
    // return > 0 means point in left
    // return < 0 means point in right
    double sideDist(const GlPoint &p);

    bool isRightOf(const GlPoint &p)
    {
        return sideDist(p) < 0.0;
    }

    bool isLeftOf(const GlPoint &p)
    {
        return sideDist(p) > 0.0;
    }

    // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
    bool intersect(Edge *other, GlPoint *point);

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

    if (LinkedList<Edge>::contains<&Edge::above_next>(e, &this->edge_above.head, &this->edge_above.tail)) {
        return;
    }

    Edge *above_prev = nullptr;
    Edge *above_next = nullptr;

    // find insertion point
    for (above_next = this->edge_above.head; above_next; above_next = above_next->above_next) {
        if (above_next->isRightOf(e->top->point)) {
            break;
        }

        above_prev = above_next;
    }

    LinkedList<Edge>::insert<&Edge::above_prev, &Edge::above_next>(e, above_prev, above_next, &this->edge_above.head,
                                                                   &this->edge_above.tail);
}


void Vertex::insertBelow(Edge *e)
{
    if (e->top->point == e->bottom->point ||                        // no edge
        VertexCompare::compare(e->bottom->point, e->top->point)) {  // not below
        return;
    }

    if (LinkedList<Edge>::contains<&Edge::below_next>(e, &this->edge_below.head, &this->edge_below.tail)) {
        return;
    }

    Edge *below_prev = nullptr;
    Edge *below_next = nullptr;

    // find insertion point
    for (below_next = this->edge_below.head; below_next; below_next = below_next->below_next) {
        if (below_next->isRightOf(e->bottom->point)) {
            break;
        }

        below_prev = below_next;
    }

    LinkedList<Edge>::insert<&Edge::below_prev, &Edge::below_next>(e, below_prev, below_next, &this->edge_below.head,
                                                                   &this->edge_below.tail);
}

bool VertexCompare::compare(const GlPoint &a, const GlPoint &b)
{
    return a.y < b.y || (a.y == b.y && a.x < b.x);
}

void VertexList::insert(Vertex *v, Vertex *prev, Vertex *next)
{
    LinkedList<detail::Vertex>::insert<&Vertex::prev, &Vertex::next>(v, prev, next, &head, &tail);
}

void VertexList::remove(Vertex *v)
{
    LinkedList<detail::Vertex>::remove<&Vertex::prev, &Vertex::next>(v, &head, &tail);
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

double Edge::sideDist(const GlPoint &p)
{
    return le_a * p.x + le_b * p.y + le_c;
}

bool Edge::intersect(Edge *other, GlPoint *point)
{
    if (this->top == other->top || this->bottom == other->bottom || this->top == other->bottom ||
        this->bottom == other->top) {
        return false;
    }

    // check if two aabb bounds is intersect
    if (std::min(top->point.x, bottom->point.x) > std::max(other->top->point.x, other->bottom->point.x) ||
        std::max(top->point.x, bottom->point.x) < std::min(other->top->point.x, other->bottom->point.x) ||
        std::min(top->point.y, bottom->point.y) > std::max(other->top->point.y, other->bottom->point.y) ||
        std::max(top->point.y, bottom->point.y) < std::min(other->top->point.y, other->bottom->point.y)) {
        return false;
    }

    double denom = le_a * other->le_b - le_b * other->le_a;

    if (denom == 0.0) {
        return false;
    }

    double dx = static_cast<double>(other->top->point.x) - top->point.x;
    double dy = static_cast<double>(other->top->point.y) - top->point.y;

    double s_number = dy * other->le_b + dx * other->le_a;
    double t_number = dy * le_b + dx * le_a;

    if (denom > 0.0 ? (s_number < 0.0 || s_number > denom || t_number < 0.0 || t_number > denom)
                    : (s_number > 0.0 || s_number < denom || t_number > 0.0 || t_number < denom)) {
        return false;
    }

    double scale = 1.0 / denom;

    point->x = std::round(static_cast<float>(top->point.x - s_number * le_b * scale));
    point->y = std::round(static_cast<float>(top->point.y + s_number * le_a * scale));

    if (std::isinf(point->x) || std::isinf(point->y)) {
        return false;
    }

    if (std::abs(point->x - top->point.x) < 1e-6 && std::abs(point->y - top->point.y) < 1e-6) {
        return false;
    }

    if (std::abs(point->x - bottom->point.x) < 1e-6 && std::abs(point->y - bottom->point.y) < 1e-6) {
        return false;
    }

    if (std::abs(point->x - other->top->point.x) < 1e-6 && std::abs(point->y - other->top->point.y) < 1e-6) {
        return false;
    }

    if (std::abs(point->x - other->bottom->point.x) < 1e-6 && std::abs(point->y - other->bottom->point.y) < 1e-6) {
        return false;
    }

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
    LinkedList<Edge>::remove<&Edge::above_prev, &Edge::above_next>(this, &bottom->edge_above.head,
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
    LinkedList<Edge>::remove<&Edge::below_prev, &Edge::below_next>(this, &top->edge_below.head, &top->edge_below.tail);
    // update top vertex
    top = v;
    // recompute line equation
    recompute();
    // insert self to new top's below list
    top->insertBelow(this);
}

static void remove_edge_above(Edge *edge)
{
    LinkedList<Edge>::remove<&Edge::above_prev, &Edge::above_next>(edge, &edge->bottom->edge_above.head,
                                                                   &edge->bottom->edge_above.tail);
}

static void remove_edge_below(Edge *edge)
{
    LinkedList<Edge>::remove<&Edge::below_prev, &Edge::below_next>(edge, &edge->top->edge_below.head,
                                                                   &edge->top->edge_below.tail);
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
    if (!current || *current == dst || VertexCompare::compare((*current)->point, dst->point)) {
        return;
    }

    Vertex *v = *current;

    while (v != dst) {
        v = v->prev;

        for (auto e = v->edge_below.head; e; e = e->below_next) {
            this->remove(e);
        }

        auto left = v->left;

        for (auto e = v->edge_above.head; e; e = e->above_next) {
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
    if (!left) {
        return true;
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
        if (e->used_in_right) {  // already in this polygon
            return this;
        }
    } else {
        if (e->used_in_left) {  // already in this polygon
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
        LinkedList<Edge>::insert<&Edge::right_poly_prev, &Edge::right_poly_next>(edge, this->last, nullptr,
                                                                                 &this->first, &this->last);
    } else {
        LinkedList<Edge>::insert<&Edge::left_poly_prev, &Edge::left_poly_next>(edge, last, nullptr, &this->first,
                                                                               &this->last);
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

    bezSplit(curve, left, right);

    return _bezierCurveCount(left) + _bezierCurveCount(right);
}

static Bezier _bezFromArc(const GlPoint& start, const GlPoint& end, float radius) {
    // Calculate the angle between the start and end points
    float angle = atan2(end.y - start.y, end.x - start.x);

    // Calculate the control points of the cubic bezier curve
    float c = radius * 0.552284749831;  // c = radius * (4/3) * tan(pi/8)

    Bezier bz;

    bz.start = Point{start.x, start.y};
    bz.ctrl1 = Point{start.x + radius * cos(angle), start.y + radius * sin(angle)};
    bz.ctrl2 = Point{end.x - c * cos(angle), end.y - c * sin(angle)};
    bz.end = Point{end.x, end.y};

    return bz;
}

static float _pointLength(const GlPoint& point)
{
    return sqrtf((point.x * point.x) + (point.y * point.y));
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

static uint32_t _pushVertex(Array<float> *array, float x, float y, float z)
{
    array->push(x);
    array->push(y);
    array->push(z);
    return (array->count - 3) / 3;
}

enum class Orientation
{
    Linear,
    Clockwise,
    CounterClockwise,
};

static Orientation _calcOrientation(const GlPoint &p1, const GlPoint &p2, const GlPoint &p3)
{
    float val = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);

    if (std::abs(val) < 0.0001f) {
        return Orientation::Linear;
    } else {
        return val > 0 ? Orientation::Clockwise : Orientation::CounterClockwise;
    }
}

static Orientation _calcOrientation(const GlPoint &dir1, const GlPoint &dir2)
{
    float val = (dir2.x - dir1.x) * (dir1.y + dir2.y);

    if (std::abs(val) < 0.0001f) {
        return Orientation::Linear;
    }

    return val > 0 ? Orientation::Clockwise : Orientation::CounterClockwise;
}

struct Line
{
    GlPoint p1;
    GlPoint p2;
};

static void _lineSplitAt(const Line &line, float at, Line *left, Line *right)
{
    auto len = _pointLength(line.p2 - line.p1);
    auto dx = ((line.p2.x - line.p1.x) / len) * at;
    auto dy = ((line.p2.y - line.p1.y) / len) * at;

    left->p1 = line.p1;
    left->p2 = GlPoint{line.p1.x + dx, line.p1.y + dy};

    right->p1 = left->p2;
    right->p2 = line.p2;
}

}  // namespace detail

Tessellator::Tessellator(Array<float> *points, Array<uint32_t> *indices)
    : pHeap(new detail::ObjectHeap),
      outlines(),
      pMesh(new detail::VertexList),
      pPolygon(),
      resGlPoints(points),
      resIndices(indices)
{
}

Tessellator::~Tessellator()
{
    if (outlines.count) {
        auto count = outlines.count;

        for (uint32_t i = 0; i < count; i++) {
            delete outlines[i];
        }
    }

    if (pMesh) {
        delete pMesh;
    }
}


void Tessellator::tessellate(const RenderShape *rshape, bool antialias)
{
    auto cmds = rshape->path.cmds.data;
    auto cmdCnt = rshape->path.cmds.count;
    auto pts = rshape->path.pts.data;
    auto ptsCnt = rshape->path.pts.count;

    this->fillRule = rshape->rule;

    this->visitShape(cmds, cmdCnt, pts, ptsCnt);

    this->buildMesh();

    this->mergeVertices();

    this->simplifyMesh();

    this->tessMesh();

    // output triangles
    for (auto poly = this->pPolygon; poly; poly = poly->next) {
        if (!this->matchFillRule(poly->winding)) {
            continue;
        }

        if (poly->count < 3) {
            continue;
        }

        for (auto m = poly->head; m; m = m->next) {
            this->emitPoly(m);
        }
    }

    if (antialias) {
        // TODO extract outline from current polygon list and generate aa edges
    }
}

void Tessellator::tessellate(const Array<const RenderShape *> &shapes)
{
    this->fillRule = FillRule::Winding;

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
        if (!this->matchFillRule(poly->winding)) {
            continue;
        }

        if (poly->count < 3) {
            continue;
        }

        for (auto m = poly->head; m; m = m->next) {
            this->emitPoly(m);
        }
    }
}

void Tessellator::visitShape(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count)
{
    // all points at least need to be visit once
    // so the points cound is at least is the same as the count in shape
    resGlPoints->reserve(pts_count * 2);
    // triangle fans, the indices count is at least triangles number * 3
    resIndices->reserve((pts_count - 2) * 3);

    const Point *firstPt = nullptr;

    for (uint32_t i = 0; i < cmd_count; i++) {
        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                outlines.push(new detail::VertexList);

                auto last = outlines.last();

                last->append(pHeap->allocate<detail::Vertex>(detail::_upScalePoint(*pts)));
                firstPt = pts;
                pts++;
            } break;
            case PathCommand::LineTo: {
                auto last = outlines.last();
                last->append(pHeap->allocate<detail::Vertex>(detail::_upScalePoint(*pts)));
                pts++;
            } break;
            case PathCommand::CubicTo: {
                // bezier curve needs to calcluate how many segment to split
                // for now just break curve into 16 segments for convenient

                auto  last = outlines.last();
                Point start = detail::_downScalePoint(Point{last->tail->point.x, last->tail->point.y});
                Point c1 = pts[0];
                Point c2 = pts[1];
                Point end = pts[2];

                Bezier curve{start, c1, c2, end};

                auto stepCount = detail::_bezierCurveCount(curve);

                if (stepCount <= 1) {
                    stepCount = 2;
                }

                float step = 1.f / stepCount;

                for (uint32_t s = 1; s < static_cast<uint32_t>(stepCount); s++) {
                    last->append(pHeap->allocate<detail::Vertex>(detail::_upScalePoint(bezPointAt(curve, step * s))));
                }

                last->append(pHeap->allocate<detail::Vertex>(detail::_upScalePoint(end)));

                pts += 3;
            } break;
            case PathCommand::Close: {
                if (firstPt && outlines.count > 0) {
                    auto last = outlines.last();

                    last->append(pHeap->allocate<detail::Vertex>(detail::_upScalePoint(*firstPt)));
                    firstPt = nullptr;
                }
            }
            default:
                break;
        }
    }
}

void Tessellator::buildMesh()
{
    Array<detail::Vertex *> temp{};

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

    temp.sort<detail::VertexCompare>();

    for (uint32_t i = 0; i < temp.count; i++) {
        this->pMesh->append(temp[i]);
    }
}

void Tessellator::mergeVertices()
{
    if (!pMesh->head) {
        return;
    }

    for (auto v = pMesh->head->next; v;) {
        auto next = v->next;

        if (detail::VertexCompare::compare(v->point, v->prev->point)) {
            // already sorted, this means these two points is same
            v->point = v->prev->point;
        }

        if (v->point == v->prev->point) {
            // merve v into v->prev
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

void Tessellator::simplifyMesh()
{
    /// this is a basic sweep line algorithm
    /// https://www.youtube.com/watch?v=qkhUNzCGDt0&t=293s
    /// in this function, we walk through all edges from top to bottom, and find
    /// all intersections edge and break them into flat segments by adding
    /// intersection point

    detail::ActiveEdgeList ael{};

    for (auto v = pMesh->head; v; v = v->next) {
        if (!v->isConnected()) {
            continue;
        }

        detail::Edge *left_enclosing = nullptr;
        detail::Edge *right_enclosing = nullptr;

        bool intersected = false;
        do {
            intersected = false;

            ael.findEnclosing(v, &left_enclosing, &right_enclosing);

            v->left = left_enclosing;
            v->right = right_enclosing;

            if (v->edge_below.head) {
                for (auto e = v->edge_below.head; e; e = e->below_next) {
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

        if (!ael.valid()) {
            // If AEL is not valid, means we meet the problem caused by floating point precision
            return;
        }

        // we are done for all edge end with current point
        for (auto e = v->edge_above.head; e; e = e->above_next) {
            ael.remove(e);
        }

        auto left = left_enclosing;

        // insert all edge start from current point into ael
        for (auto e = v->edge_below.head; e; e = e->below_next) {
            ael.insert(e, left);
            left = e;
        }
    }
}

void Tessellator::tessMesh()
{
    /// this function also use sweep line algorithm
    /// but during the process, we calculate the winding number of left and right
    /// polygon and add edge to them

    detail::ActiveEdgeList ael{};

    for (auto v = pMesh->head; v; v = v->next) {
        if (!v->isConnected()) {
            continue;
        }

        detail::Edge *left_enclosing = nullptr;
        detail::Edge *right_enclosing = nullptr;

        ael.findEnclosing(v, &left_enclosing, &right_enclosing);

        /**
         *
         *                   ...
         *                      \
         *      left_poly      head
         *                          v
         *
         */
        detail::Polygon *left_poly = nullptr;
        /**
         *
         *              ...
         *         /
         *       tail     right_poly
         *     v
         *
         */
        detail::Polygon *right_poly = nullptr;

        if (v->edge_above.head) {
            left_poly = v->edge_above.head->left_poly;
            right_poly = v->edge_above.tail->right_poly;
        } else {
            left_poly = left_enclosing ? left_enclosing->right_poly : nullptr;
            right_poly = right_enclosing ? right_enclosing->left_poly : nullptr;
        }

        if (v->edge_above.head) {
            // add above edge first
            if (left_poly) {
                left_poly = left_poly->addEdge(v->edge_above.head, detail::Side::kRight, pHeap.get());
            }

            if (right_poly) {
                right_poly = right_poly->addEdge(v->edge_above.tail, detail::Side::kLeft, pHeap.get());
            }

            // walk through all edges end with this vertex
            for (auto e = v->edge_above.head; e != v->edge_above.tail; e = e->above_next) {
                auto right_edge = e->above_next;

                ael.remove(e);

                if (e->right_poly) {
                    e->right_poly->addEdge(right_edge, detail::Side::kLeft, pHeap.get());
                }

                // this means there is a new polygon between e and right_edge
                if (right_edge->left_poly && right_edge->left_poly != e->right_poly) {
                    right_edge->left_poly->addEdge(e, detail::Side::kRight, pHeap.get());
                }
            }

            ael.remove(v->edge_above.tail);

            // there is no edge begin with this vertex
            if (!v->edge_below.head) {
                if (left_poly && right_poly && left_poly != right_poly) {
                    // polygon not closed at this point
                    // need to mark these two polygon each other, because they will be
                    // linked by a cross edge later

                    left_poly->parent = right_poly;
                    right_poly->parent = left_poly;
                }
            }
        }

        if (v->edge_below.head) {
            if (!v->edge_above.head) {
                // there is no edge end with this vertex
                if (left_poly && right_poly) {

                    if (left_poly == right_poly) {
                        /**
                         *   left_poly      right_poly
                         *
                         *              v
                         *             / \
                         *            /   \
                         *             ...
                         */
                        if (left_poly->tail && left_poly->tail->side == detail::Side::kLeft) {
                            left_poly = this->makePoly(left_poly->lastVertex(), left_poly->winding);

                            left_enclosing->right_poly = left_poly;
                        } else {
                            right_poly = this->makePoly(right_poly->lastVertex(), right_poly->winding);

                            right_enclosing->left_poly = right_poly;
                        }
                    }

                    // need to link this vertex to above polygon
                    auto join = pHeap->allocate<detail::Edge>(left_poly->lastVertex(), v, 1);

                    left_poly = left_poly->addEdge(join, detail::Side::kRight, pHeap.get());
                    right_poly = right_poly->addEdge(join, detail::Side::kLeft, pHeap.get());
                }
            }

            auto left_edge = v->edge_below.head;
            left_edge->left_poly = left_poly;

            ael.insert(left_edge, left_enclosing);

            for (auto right_edge = left_edge->below_next; right_edge; right_edge = right_edge->below_next) {
                ael.insert(right_edge, left_edge);

                int32_t winding = left_edge->left_poly ? left_edge->left_poly->winding : 0;

                winding += left_edge->winding;

                if (winding != 0) {
                    auto poly = this->makePoly(v, winding);

                    left_edge->right_poly = right_edge->left_poly = poly;
                }

                left_edge = right_edge;
            }

            v->edge_below.tail->right_poly = right_poly;
        }
    }
}

bool Tessellator::matchFillRule(int32_t winding)
{
    if (fillRule == FillRule::Winding) {
        return winding != 0;
    } else {
        return (winding & 0x1) != 0;
    }
}

detail::Edge *Tessellator::makeEdge(detail::Vertex *a, detail::Vertex *b)
{
    if (!a || !b || a->point == b->point) {
        return nullptr;
    }

    int32_t winding = 1;

    if (detail::VertexCompare::compare(b->point, a->point)) {
        winding = -1;
        std::swap(a, b);
    }

    return pHeap->allocate<detail::Edge>(a, b, winding);
}

bool Tessellator::checkIntersection(detail::Edge *left, detail::Edge *right, detail::ActiveEdgeList *ael,
                                    detail::Vertex **current)
{
    if (!left || !right) {
        return false;
    }

    GlPoint p;

    if (left->intersect(right, &p) && !std::isinf(p.x) && !std::isinf(p.y)) {
        detail::Vertex *v;
        detail::Vertex *top = *current;

        // the vertex in mesh is sorted, so walk to prev can find latest top point
        while (top && detail::VertexCompare::compare(p, top->point)) {
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
            while (prev && detail::VertexCompare::compare(p, prev->point)) {
                prev = prev->prev;
            }

            auto next = prev ? prev->next : pMesh->head;
            while (next && detail::VertexCompare::compare(next->point, p)) {
                prev = next;
                next = next->next;
            }

            // check if point is already in mesh
            if (prev && prev->point == p) {
                v = prev;
            } else if (next && next->point == p) {
                v = next;
            } else {
                v = pHeap->allocate<detail::Vertex>(p);
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

bool Tessellator::splitEdge(detail::Edge *edge, detail::Vertex *v, detail::ActiveEdgeList *ael,
                            detail::Vertex **current)
{
    if (!edge->top || !edge->bottom || v == edge->top || v == edge->bottom) {
        return false;
    }

    int32_t winding = edge->winding;

    detail::Vertex *top;
    detail::Vertex *bottom;

    if (detail::VertexCompare::compare(v->point, edge->top->point)) {
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
    } else if (detail::VertexCompare::compare(edge->bottom->point, v->point)) {
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

    auto new_edge = pHeap->allocate<detail::Edge>(top, bottom, winding);

    bottom->insertAbove(new_edge);

    top->insertBelow(new_edge);

    if (new_edge->above_prev == nullptr && new_edge->above_next == nullptr) {
        return false;
    }

    if (new_edge->below_prev == nullptr && new_edge->below_next == nullptr) {
        return false;
    }

    return true;
}

bool Tessellator::intersectPairEdge(detail::Edge *left, detail::Edge *right, detail::ActiveEdgeList *ael,
                                    detail::Vertex **current)
{
    if (!left->top || !left->bottom || !right->top || !right->bottom) {
        return false;
    }

    if (left->top == right->top || left->bottom == right->bottom) {
        return false;
    }

    if (detail::_calcOrientation(left->bottom->point - left->top->point, right->bottom->point - right->top->point) ==
        detail::Orientation::Linear) {
        return false;
    }

    detail::Edge *split = nullptr;

    detail::Vertex *split_at = nullptr;

    // check if these two edge is intersected
    if (detail::VertexCompare::compare(left->top->point, right->top->point)) {
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

    if (detail::VertexCompare::compare(right->bottom->point, left->bottom->point)) {
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

    if (!split) {
        return false;
    }

    ael->rewind(current, split->top);

    return splitEdge(split, split_at, ael, current);
}

detail::Polygon *Tessellator::makePoly(detail::Vertex *v, int32_t winding)
{
    auto poly = pHeap->allocate<detail::Polygon>(v, winding);

    poly->next = this->pPolygon;
    this->pPolygon = poly;

    return poly;
}

void Tessellator::emitPoly(detail::MonotonePolygon *poly)
{
    auto e = poly->first;

    detail::VertexList vertices;

    vertices.append(e->top);
    int32_t count = 1;
    while (e != nullptr) {
        if (poly->side == detail::Side::kRight) {
            vertices.append(e->bottom);
            e = e->right_poly_next;
        } else {
            vertices.prepend(e->bottom);
            e = e->left_poly_next;
        }
        count += 1;
    }

    if (count < 3) {
        return;
    }

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

            if (v->prev == first) {
                v = v->next;
            } else {
                v = v->prev;
            }
        } else {
            v = v->next;
        }
    }
}

void Tessellator::emitTriangle(detail::Vertex *p1, detail::Vertex *p2, detail::Vertex *p3)
{
    // check if index is generated
    if (p1->index == 0xFFFFFFFF)
        p1->index = detail::_pushVertex(resGlPoints, detail::_downScaleFloat(p1->point.x), detail::_downScaleFloat(p1->point.y), 1.f);
    if (p2->index == 0xFFFFFFFF)
        p2->index = detail::_pushVertex(resGlPoints, detail::_downScaleFloat(p2->point.x), detail::_downScaleFloat(p2->point.y), 1.f);
    if (p3->index == 0xFFFFFFFF)
        p3->index = detail::_pushVertex(resGlPoints, detail::_downScaleFloat(p3->point.x), detail::_downScaleFloat(p3->point.y), 1.f);

    resIndices->push(p1->index);
    resIndices->push(p2->index);
    resIndices->push(p3->index);
}


Stroker::Stroker(Array<float> *points, Array<uint32_t> *indices) : mResGlPoints(points), mResIndices(indices)
{
}

void Stroker::stroke(const RenderShape *rshape)
{
    mMiterLimit = rshape->strokeMiterlimit() * 2.f;
    mStrokeWidth = std::max(mStrokeWidth, rshape->strokeWidth());
    mStrokeCap = rshape->strokeCap();
    mStrokeJoin = rshape->strokeJoin();


    auto cmds = rshape->path.cmds.data;
    auto cmdCnt = rshape->path.cmds.count;
    auto pts = rshape->path.pts.data;
    auto ptsCnt = rshape->path.pts.count;

    const float *dash_pattern = nullptr;
    auto dash_count = rshape->strokeDash(&dash_pattern, nullptr);

    if (dash_count == 0) {
        doStroke(cmds, cmdCnt, pts, ptsCnt);
    } else {
        doDashStroke(cmds, cmdCnt, pts, ptsCnt, dash_count, dash_pattern);
    }
}

void Stroker::doStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count)
{
    mResGlPoints->reserve(pts_count * 4 + 16);
    mResIndices->reserve(pts_count * 3);


    for (uint32_t i = 0; i < cmd_count; i++) {
        switch (cmds[i]) {
            case PathCommand::MoveTo: {
                if (mStrokeState.hasMove) {
                    strokeCap();
                    mStrokeState.hasMove = false;
                }
                mStrokeState.hasMove = true;
                mStrokeState.firstPt = *pts;
                mStrokeState.firstPtDir = GlPoint{};
                mStrokeState.prevPt = *pts;
                mStrokeState.prevPtDir = GlPoint{};
                pts++;
            } break;
            case PathCommand::LineTo: {
                this->strokeLineTo(*pts);
                pts++;
            } break;
            case PathCommand::CubicTo: {
                this->strokeCubicTo(pts[0], pts[1], pts[2]);
                pts += 3;
            } break;
            case PathCommand::Close: {
                this->strokeClose();

                mStrokeState.hasMove = false;
            } break;
            default:
                break;
        }
    }
}

void Stroker::doDashStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count,
                           uint32_t dast_count, const float *dash_pattern)
{
    Array<PathCommand> dash_cmds{};
    Array<Point>       dash_pts{};

    dash_cmds.reserve(20 * cmd_count);
    dash_pts.reserve(20 * pts_count);

    DashStroke dash(&dash_cmds, &dash_pts, dast_count, dash_pattern);

    dash.doStroke(cmds, cmd_count, pts, pts_count);

    this->doStroke(dash_cmds.data, dash_cmds.count, dash_pts.data, dash_pts.count);
}

void Stroker::strokeCap()
{
}

void Stroker::strokeLineTo(const GlPoint &curr)
{
    auto dir = (curr - mStrokeState.prevPt);
    dir.normalize();

    if (dir.x == 0.f && dir.y == 0.f) {
        // same point
        return;
    }

    auto normal = GlPoint{-dir.y, dir.x};

    auto a = mStrokeState.prevPt + normal * strokeRadius();
    auto b = mStrokeState.prevPt - normal * strokeRadius();
    auto c = curr + normal * strokeRadius();
    auto d = curr - normal * strokeRadius();

    auto ia = detail::_pushVertex(mResGlPoints, a.x, a.y, 1.f);
    auto ib = detail::_pushVertex(mResGlPoints, b.x, b.y, 1.f);
    auto ic = detail::_pushVertex(mResGlPoints, c.x, c.y, 1.f);
    auto id = detail::_pushVertex(mResGlPoints, d.x, d.y, 1.f);

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
}

void Stroker::strokeCubicTo(const GlPoint &cnt1, const GlPoint &cnt2, const GlPoint &end)
{
    Bezier curve{};
    curve.start = Point{mStrokeState.prevPt.x, mStrokeState.prevPt.y};
    curve.ctrl1 = Point{cnt1.x, cnt1.y};
    curve.ctrl2 = Point{cnt2.x, cnt2.y};
    curve.end = Point{end.x, end.y};

    auto count = detail::_bezierCurveCount(curve);

    float step = 1.f / count;

    for (int32_t i = 0; i <= count; i++) {
        strokeLineTo(bezPointAt(curve, step * i));
    }
}

void Stroker::strokeClose()
{
    if (mStrokeState.prevPt != mStrokeState.firstPt) {
        this->strokeLineTo(mStrokeState.firstPt);
    }

    // join firstPt with prevPt
    this->strokeJoin(mStrokeState.firstPtDir);

    mStrokeState.hasMove = false;
}

void Stroker::strokeJoin(const GlPoint &dir)
{
    auto orientation = detail::_calcOrientation(mStrokeState.prevPt - mStrokeState.prevPtDir, mStrokeState.prevPt,
                                                mStrokeState.prevPt + dir);

    if (orientation == detail::Orientation::Linear) {
        // check is same direction
        if (mStrokeState.prevPtDir == dir) {
            return;
        }

        // opposite direction
        if (mStrokeJoin != StrokeJoin::Round) {
            return;
        }

        auto normal = GlPoint{-dir.y, dir.x};

        auto p1 = mStrokeState.prevPt + normal * strokeRadius();
        auto p2 = mStrokeState.prevPt - normal * strokeRadius();
        auto oc = mStrokeState.prevPt + dir * strokeRadius();

        this->strokeRound(p1, oc, mStrokeState.prevPt);
        this->strokeRound(oc, p2, mStrokeState.prevPt);

    } else {
        auto normal = GlPoint{-dir.y, dir.x};
        auto prevNormal = GlPoint{-mStrokeState.prevPtDir.y, mStrokeState.prevPtDir.x};

        GlPoint prevJoin{};
        GlPoint currJoin{};

        if (orientation == detail::Orientation::CounterClockwise) {
            prevJoin = mStrokeState.prevPt + prevNormal * strokeRadius();
            currJoin = mStrokeState.prevPt + normal * strokeRadius();
        } else {
            prevJoin = mStrokeState.prevPt - prevNormal * strokeRadius();
            currJoin = mStrokeState.prevPt - normal * strokeRadius();
        }

        if (mStrokeJoin == StrokeJoin::Miter) {
            this->strokeMiter(prevJoin, currJoin, mStrokeState.prevPt);
        } else if (mStrokeJoin == StrokeJoin::Bevel) {
            this->strokeBevel(prevJoin, currJoin, mStrokeState.prevPt);
        } else {
            // round join
            this->strokeRound(prevJoin, currJoin, mStrokeState.prevPt);
        }
    }
}


void Stroker::strokeRound(const GlPoint &prev, const GlPoint &curr, const GlPoint &center)
{
    if (detail::_calcOrientation(prev, center, curr) == detail::Orientation::Linear) {
        return;
    }

    // Fixme: just use bezier curve to calculate step count
    auto count = detail::_bezierCurveCount(detail::_bezFromArc(prev, curr, strokeRadius()));

    auto c = detail::_pushVertex(mResGlPoints, center.x, center.y, 1.f);

    auto pi = detail::_pushVertex(mResGlPoints, prev.x, prev.y, 1.f);

    float step = 1.f / (count - 1);

    auto dir = curr - prev;
    for (uint32_t i = 1; i < static_cast<uint32_t>(count); i++) {
        float t = i * step;

        auto p = prev + dir * t;

        auto o_dir = p - center;
        o_dir.normalize();

        auto out = center + o_dir * strokeRadius();

        auto oi = detail::_pushVertex(mResGlPoints, out.x, out.y, 1.f);

        this->mResIndices->push(c);
        this->mResIndices->push(pi);
        this->mResIndices->push(oi);

        pi = oi;
    }
}

void Stroker::strokeMiter(const GlPoint &prev, const GlPoint &curr, const GlPoint &center)
{
    auto pp1 = prev - center;
    auto pp2 = curr - center;

    auto out = pp1 + pp2;

    float k = 2.f * strokeRadius() * strokeRadius() / (out.x * out.x + out.y * out.y);

    auto pe = out * k;

    if (detail::_pointLength(pe) >= mMiterLimit) {
        this->strokeBevel(prev, curr, center);
        return;
    }

    auto join = center + pe;

    auto c = detail::_pushVertex(mResGlPoints, center.x, center.y, 1.f);

    auto cp1 = detail::_pushVertex(mResGlPoints, prev.x, prev.y, 1.f);
    auto cp2 = detail::_pushVertex(mResGlPoints, curr.x, curr.y, 1.f);

    auto e = detail::_pushVertex(mResGlPoints, join.x, join.y, 1.f);

    this->mResIndices->push(c);
    this->mResIndices->push(cp1);
    this->mResIndices->push(e);

    this->mResIndices->push(e);
    this->mResIndices->push(cp2);
    this->mResIndices->push(c);
}

void Stroker::strokeBevel(const GlPoint &prev, const GlPoint &curr, const GlPoint &center)
{
    auto a = detail::_pushVertex(mResGlPoints, prev.x, prev.y, 1.f);
    auto b = detail::_pushVertex(mResGlPoints, curr.x, curr.y, 1.f);
    auto c = detail::_pushVertex(mResGlPoints, center.x, center.y, 1.f);

    mResIndices->push(a);
    mResIndices->push(b);
    mResIndices->push(c);
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
            default:
                break;
        }
        cmds++;
    }
}

void DashStroke::dashLineTo(const GlPoint &to)
{
    float len = detail::_pointLength(mPtCur - to);

    if (len < mCurrLen) {
        mCurrLen -= len;

        if (!mCurOpGap) {
            this->moveTo(mPtCur);
            this->lineTo(to);
        }
    } else {
        detail::Line curr{mPtCur, to};

        while (len > mCurrLen) {
            len -= mCurrLen;

            detail::Line left, right;

            detail::_lineSplitAt(curr, mCurrLen, &left, &right);

            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            if (!mCurOpGap) {
                this->moveTo(left.p1);
                this->lineTo(left.p2);
            }
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
            curr = right;
            mPtCur = curr.p1;
        }
        mCurrLen -= len;
        if (!mCurOpGap) {
            this->moveTo(curr.p1);
            this->lineTo(curr.p2);
        }

        if (mCurrLen < 1) {
            mCurrIdx = (mCurrIdx + 1) % mDashCount;
            mCurrLen = mDashPattern[mCurrIdx];
            mCurOpGap = !mCurOpGap;
        }
    }

    mPtCur = to;
}

void DashStroke::dashCubicTo(const GlPoint &cnt1, const GlPoint &cnt2, const GlPoint &end)
{
    Bezier cur;
    cur.start = Point{mPtCur.x, mPtCur.y};
    cur.ctrl1 = Point{cnt1.x, cnt1.y};
    cur.ctrl2 = Point{cnt2.x, cnt2.y};
    cur.end = Point{end.x, end.y};

    auto len = bezLength(cur);

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

            bezSplitAt(cur, mCurrLen, left, right);

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

void DashStroke::moveTo(const GlPoint &pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::MoveTo);
}

void DashStroke::lineTo(const GlPoint &pt)
{
    mPts->push(Point{pt.x, pt.y});
    mCmds->push(PathCommand::LineTo);
}

void DashStroke::cubicTo(const GlPoint &cnt1, const GlPoint &cnt2, const GlPoint &end)
{
    mPts->push(Point{cnt1.x, cnt1.y});
    mPts->push(Point{cnt2.x, cnt2.y});
    mPts->push(Point{end.x, end.y});
    mCmds->push(PathCommand::CubicTo);
}

}  // namespace tvg
