#include "tvgTessellator.h"
#include "thorvg.h"
#include "tvgArray.h"
#include "tvgRender.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace tvg
{

bool operator==(const Point &p1, const Point &p2)
{
    return p1.x == p2.x && p1.y == p2.y;
}

Point operator-(const Point &p1, const Point &p2)
{
    return Point{p1.x - p2.x, p1.y - p2.y};
}

Point operator+(const Point &p1, const Point &p2)
{
    return Point{p1.x + p2.x, p1.y + p2.y};
}

Point operator*(const Point &p1, const Point &p2)
{
    return Point{p1.x * p2.x, p1.y * p2.y};
}

namespace detail
{

template<typename T>
struct LinkedList
{
    T *head = nullptr;
    T *tail = nullptr;

    LinkedList() = default;
    LinkedList(T *head, T *tail) : head(head), tail(tail)
    {
    }

    template<T *T::*Prev, T *T::*Next>
    static void Insert(T *t, T *prev, T *next, T **head, T **tail)
    {
        t->*Prev = prev;
        t->*Next = next;

        if (prev)
        {
            prev->*Next = t;
        } else if (head)
        {
            *head = t;
        }

        if (next)
        {
            next->*Prev = t;
        } else if (tail)
        {
            *tail = t;
        }
    }

    template<T *T::*Prev, T *T::*Next>
    static void Remove(T *t, T **head, T **tail)
    {
        if (t->*Prev)
        {
            t->*Prev->*Next = t->*Next;
        } else if (head)
        {
            *head = t->*Next;
        }

        if (t->*Next)
        {
            t->*Next->*Prev = t->*Prev;
        } else if (tail)
        {
            *tail = t->*Prev;
        }

        t->*Prev = t->*Next = nullptr;
    }
};

// common obj for memory controle
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

        for (uint32_t i = 0; i < count; ++i)
        {
            delete static_cast<Object *>(first[i]);
        }
    }

    template<class T, class... Args>
    T *Allocate(Args &&...args)
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

    Point point = {};

    Vertex() = default;

    Vertex(const Point &p) : point(p)
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
        return Compare(v1->point, v2->point);
    }

    static bool Compare(const Point &a, const Point &b);
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
    double sideDist(const Point &p);

    bool isRightOf(const Point &p)
    {
        return sideDist(p) < 0.0;
    }

    bool isLeftOf(const Point &p)
    {
        return sideDist(p) > 0.0;
    }

    // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
    bool intersect(Edge *other, Point *point);

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
    if (e->top->point == e->bottom->point ||  // no edge
        VertexCompare::Compare(e->bottom->point, e->top->point))
    {  // not above
        return;
    }

    Edge *above_prev = nullptr;
    Edge *above_next = nullptr;

    // find insertion point
    for (above_next = this->edge_above.head; above_next; above_next = above_next->above_next)
    {
        if (above_next->isRightOf(e->top->point))
        {
            break;
        }

        above_prev = above_next;
    }

    LinkedList<Edge>::Insert<&Edge::above_prev, &Edge::above_next>(e, above_prev, above_next, &this->edge_above.head,
                                                                   &this->edge_above.tail);
}

void Vertex::insertBelow(Edge *e)
{
    if (e->top->point == e->bottom->point ||  // no edge
        VertexCompare::Compare(e->bottom->point, e->top->point))
    {  // not below
        return;
    }

    Edge *below_prev = nullptr;
    Edge *below_next = nullptr;

    // find insertion point
    for (below_next = this->edge_below.head; below_next; below_next = below_next->below_next)
    {
        if (below_next->isRightOf(e->bottom->point))
        {
            break;
        }

        below_prev = below_next;
    }

    LinkedList<Edge>::Insert<&Edge::below_prev, &Edge::below_next>(e, below_prev, below_next, &this->edge_below.head,
                                                                   &this->edge_below.tail);
}

bool VertexCompare::Compare(const Point &a, const Point &b)
{
    return a.y < b.y || (a.y == b.y && a.x < b.x);
}

void VertexList::insert(Vertex *v, Vertex *prev, Vertex *next)
{
    Insert<&Vertex::prev, &Vertex::next>(v, prev, next, &head, &tail);
}

void VertexList::remove(Vertex *v)
{
    Remove<&Vertex::prev, &Vertex::next>(v, &head, &tail);
}

void VertexList::append(VertexList const &other)
{
    if (!other.head)
    {
        return;
    }

    if (tail)
    {
        tail->next = other.head;
        other.head->prev = tail;
    } else
    {
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
    if (head && tail)
    {
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

double Edge::sideDist(const Point &p)
{
    return le_a * p.x + le_b * p.y + le_c;
}

bool Edge::intersect(Edge *other, Point *point)
{
    if (this->top == other->top || this->bottom == other->bottom || this->top == other->bottom ||
        this->bottom == other->top)
    {
        return false;
    }

    // check if two aabb bounds is intersect
    if (std::min(top->point.x, bottom->point.x) > std::max(other->top->point.x, other->bottom->point.x) ||
        std::max(top->point.x, bottom->point.x) < std::min(other->top->point.x, other->bottom->point.x) ||
        std::min(top->point.y, bottom->point.y) > std::max(other->top->point.y, other->bottom->point.y) ||
        std::max(top->point.y, bottom->point.y) < std::min(other->top->point.y, other->bottom->point.y))
    {
        return false;
    }

    double denom = le_a * other->le_b - le_b * other->le_a;

    if (denom == 0.0)
    {
        return false;
    }

    double dx = static_cast<double>(other->top->point.x) - top->point.x;
    double dy = static_cast<double>(other->top->point.y) - top->point.y;

    double s_number = dy * other->le_b + dx * other->le_a;
    double t_number = dy * le_b + dx * le_a;

    if (denom > 0.0 ? (s_number < 0.0 || s_number > denom || t_number < 0.0 || t_number > denom)
                    : (s_number > 0.0 || s_number < denom || t_number > 0.0 || t_number < denom))
    {
        return false;
    }

    double scale = 1.0 / denom;

    point->x = std::round(static_cast<float>(top->point.x - s_number * le_b * scale));
    point->y = std::round(static_cast<float>(top->point.y + s_number * le_a * scale));

    if (std::isinf(point->x) || std::isinf(point->y))
    {
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
    LinkedList<Edge>::Remove<&Edge::above_prev, &Edge::above_next>(this, &bottom->edge_above.head,
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
    LinkedList<Edge>::Remove<&Edge::below_prev, &Edge::below_next>(this, &top->edge_below.head, &top->edge_below.tail);
    // update top vertex
    top = v;
    // recompute line equation
    recompute();
    // insert self to new top's below list
    top->insertBelow(this);
}

static void remove_edge_above(Edge *edge)
{
    LinkedList<Edge>::Remove<&Edge::above_prev, &Edge::above_next>(edge, &edge->bottom->edge_above.head,
                                                                   &edge->bottom->edge_above.tail);
}

static void remove_edge_below(Edge *edge)
{
    LinkedList<Edge>::Remove<&Edge::below_prev, &Edge::below_next>(edge, &edge->top->edge_below.head,
                                                                   &edge->top->edge_below.tail);
}

void Edge::disconnect()
{
    remove_edge_above(this);
    remove_edge_below(this);
}

void ActiveEdgeList::insert(Edge *e, Edge *prev, Edge *next)
{
    LinkedList<Edge>::Insert<&Edge::left, &Edge::right>(e, prev, next, &head, &tail);
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
    LinkedList<Edge>::Remove<&Edge::left, &Edge::right>(e, &head, &tail);
}

bool ActiveEdgeList::contains(Edge *edge)
{
    return edge->left || edge->right || head == edge;
}

void ActiveEdgeList::rewind(Vertex **current, Vertex *dst)
{
    if (!current || *current == dst || VertexCompare::Compare((*current)->point, dst->point))
    {
        return;
    }

    Vertex *v = *current;

    while (v != dst)
    {
        v = v->prev;

        for (auto e = v->edge_below.head; e; e = e->below_next)
        {
            this->remove(e);
        }

        auto left = v->left;

        for (auto e = v->edge_above.head; e; e = e->above_next)
        {
            this->insert(e, left);
            left = e;

            auto top = e->top;
            if (VertexCompare::Compare(top->point, dst->point) &&
                ((top->left && !top->left->isLeftOf(e->top->point)) ||
                 (top->right && !top->right->isRightOf(e->top->point))))
            {
                dst = top;
            }
        }
    }

    *current = v;
}

void ActiveEdgeList::findEnclosing(Vertex *v, Edge **left, Edge **right)
{
    if (v->edge_above.head && v->edge_above.tail)
    {
        *left = v->edge_above.head->left;
        *right = v->edge_above.tail->right;
        return;
    }

    Edge *prev = nullptr;
    Edge *next = nullptr;

    // walk through aet to get left most edge
    for (prev = tail; prev != nullptr; prev = prev->left)
    {
        if (prev->isLeftOf(v->point))
        {
            break;
        }
        next = prev;
    }

    *left = prev;
    *right = next;
}

Polygon *Polygon::addEdge(Edge *e, Side side, ObjectHeap *heap)
{
    auto p_parent = this->parent;

    auto poly = this;

    if (side == Side::kRight)
    {
        if (e->used_in_right)
        {  // already in this polygon
            return this;
        }
    } else
    {
        if (e->used_in_left)
        {  // already in this polygon
            return this;
        }
    }

    if (p_parent)
    {
        this->parent = p_parent->parent = nullptr;
    }

    if (!this->tail)
    {
        this->head = this->tail = heap->Allocate<MonotonePolygon>(e, side, this->winding);
        this->count += 2;
    } else if (e->bottom == this->tail->last->bottom)
    {
        // close this polygon
        return poly;
    } else if (side == this->tail->side)
    {
        this->tail->addEdge(e);
        this->count++;
    } else
    {
        e = heap->Allocate<Edge>(this->tail->last->bottom, e->bottom, 1);
        this->tail->addEdge(e);
        this->count++;

        if (p_parent)
        {
            p_parent->addEdge(e, side, heap);
            poly = p_parent;
        } else
        {
            auto m = heap->Allocate<MonotonePolygon>(e, side, this->winding);
            m->prev = this->tail;

            this->tail->next = m;

            this->tail = m;
        }
    }

    return poly;
}

Vertex *Polygon::lastVertex() const
{
    if (tail)
    {
        return tail->last->bottom;
    }

    return first_vert;
}

void MonotonePolygon::addEdge(Edge *edge)
{
    if (this->side == Side::kRight)
    {
        LinkedList<Edge>::Insert<&Edge::right_poly_prev, &Edge::right_poly_next>(edge, this->last, nullptr,
                                                                                 &this->first, &this->last);
    } else
    {
        LinkedList<Edge>::Insert<&Edge::left_poly_prev, &Edge::left_poly_next>(edge, last, nullptr, &this->first,
                                                                               &this->last);
    }
}

/**
 * use for : eval(t) = A * t ^ 3 + B * t ^ 2 + C * t + D
 */
struct Cubic
{
    Point A{};
    Point B{};
    Point C{};
    Point D{};

    explicit Cubic(std::array<Point, 4> const &src)
    {
        auto p0 = src[0];
        auto p1 = src[1];
        auto p2 = src[2];
        auto p3 = src[3];

        Point three{3, 3};

        A = p3 + three * (p1 - p2) - p0;
        B = three * (p2 - (p1 + p1) + p0);
        C = three * (p1 - p0);
        D = p0;
    }

    Point eval(float t)
    {
        Point tt{t, t};
        return ((A * tt + B) * tt + C) * tt + D;
    }
};

}  // namespace detail

Tessellator::Tessellator(Array<float> *points, Array<uint32_t> *indices)
    : pHeap(new detail::ObjectHeap),
      outlines(),
      pMesh(new detail::VertexList),
      pPolygon(),
      resPoints(points),
      resIndices(indices)
{
}

Tessellator::~Tessellator()
{
    if (outlines.count)
    {
        auto count = outlines.count;

        for (uint32_t i = 0; i < count; i++)
        {
            delete outlines.data[i];
        }
    }

    if (pMesh)
    {
        delete pMesh;
    }
}

void Tessellator::tessellate(const Shape *shape)
{
    this->fillRule = shape->fillRule();

    const PathCommand *cmds = nullptr;
    const Point       *pts = nullptr;

    auto cmd_count = shape->pathCommands(&cmds);
    auto pts_count = shape->pathCoords(&pts);

    this->visitShape(cmds, cmd_count, pts, pts_count);

    this->buildMesh();

    this->mergeVertices();

    this->simplifyMesh();

    this->tessMesh();

    // output triangles
    for (auto poly = this->pPolygon; poly; poly = poly->next)
    {
        if (!this->matchFillRule(poly->winding))
        {
            continue;
        }

        if (poly->count < 3)
        {
            continue;
        }

        for (auto m = poly->head; m; m = m->next)
        {
            this->emitPoly(m);
        }
    }
}

void Tessellator::tessellate(const RenderShape *rshape, bool antialias)
{
    auto cmds = rshape->path.cmds;
    auto cmdCnt = rshape->path.cmdCnt;
    auto pts = rshape->path.pts;
    auto ptsCnt = rshape->path.ptsCnt;

    this->fillRule = rshape->rule;

    this->visitShape(cmds, cmdCnt, pts, ptsCnt);

    this->buildMesh();

    this->mergeVertices();

    this->simplifyMesh();

    this->tessMesh();

    // output triangles
    for (auto poly = this->pPolygon; poly; poly = poly->next)
    {
        if (!this->matchFillRule(poly->winding))
        {
            continue;
        }

        if (poly->count < 3)
        {
            continue;
        }

        for (auto m = poly->head; m; m = m->next)
        {
            this->emitPoly(m);
        }
    }

    if (antialias)
    {
        // TODO extract outline from current polygon list and generate aa edges
    }
}

void Tessellator::decomposeOutline(const Shape *shape, Shape *dst)
{
    this->fillRule = shape->fillRule();

    outlineResult = dst;

    const PathCommand *cmd = nullptr;
    const Point       *pts = nullptr;

    auto cmdCnt = shape->pathCommands(&cmd);
    auto ptsCnt = shape->pathCoords(&pts);

    this->visitShape(cmd, cmdCnt, pts, ptsCnt);

    this->buildMesh();

    this->mergeVertices();

    this->simplifyMesh();

    this->mergeMesh();
}

void Tessellator::visitShape(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count)
{
    // all points at least need to be visit once
    // so the points cound is at least is the same as the count in shape
    resPoints->reserve(pts_count * 2);
    // triangle fans, the indices count is at least triangles number * 3
    resIndices->reserve((pts_count - 2) * 3);

    for (uint32_t i = 0; i < cmd_count; i++)
    {
        switch (cmds[i])
        {
            case PathCommand::MoveTo: {
                outlines.push(new detail::VertexList);

                auto last = outlines.last();

                last->append(pHeap->Allocate<detail::Vertex>(*pts));
                pts++;
            }
            break;
            case PathCommand::LineTo: {
                auto last = outlines.last();
                last->append(pHeap->Allocate<detail::Vertex>(*pts));
                pts++;
            }
            break;
            case PathCommand::CubicTo: {
                // bezier curve needs to calcluate how many segment to split
                // for now just break curve into 16 segments for convenient

                auto  last = outlines.last();
                Point start = last->tail->point;
                Point c1 = pts[0];
                Point c2 = pts[1];
                Point end = pts[2];

                detail::Cubic cubic({start, c1, c2, end});

                float step = 1.f / 15.f;

                for (uint32_t s = 0; s < 16; s++)
                {
                    last->append(pHeap->Allocate<detail::Vertex>(cubic.eval(step * s)));
                }

                pts += 3;
            }
            break;
            case PathCommand::Close:  // fall through
            default:
                break;
        }
    }
}

void Tessellator::buildMesh()
{
    Array<detail::Vertex *> temp{};

    for (uint32_t i = 0; i < outlines.count; i++)
    {
        auto list = outlines.data[i];

        auto prev = list->tail;
        auto v = list->head;

        while (v)
        {
            auto next = v->next;

            auto edge = this->makeEdge(prev, v);

            if (edge)
            {
                edge->bottom->insertAbove(edge);
                edge->top->insertBelow(edge);
            }

            if (v->isConnected())
            {
                this->pMesh->append(v);
                temp.push(v);
            }

            prev = v;
            v = next;
        }
    }

    if (temp.count < 3)
    {
        return;
    }

    temp.sort<detail::VertexCompare>();

    for (uint32_t i = 0; i < temp.count; i++)
    {
        this->pMesh->append(temp.data[i]);
    }
}

void Tessellator::mergeVertices()
{
    if (!pMesh->head)
    {
        return;
    }

    for (auto v = pMesh->head->next; v;)
    {
        if (detail::VertexCompare::Compare(v->point, v->prev->point))
        {
            // already sorted, this means these two points is same
            v->point = v->prev->point;
        }

        if (v->point == v->prev->point)
        {
            // merve v into v->prev
            while (auto e = v->edge_above.head)
            {
                e->setBottom(v->prev);
            }

            while (auto e = v->edge_below.head)
            {
                e->setTop(v->prev);
            }

            pMesh->remove(v);
        }

        v = v->next;
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

    for (auto v = pMesh->head; v; v = v->next)
    {
        if (!v->isConnected())
        {
            continue;
        }

        detail::Edge *left_enclosing = nullptr;
        detail::Edge *right_enclosing = nullptr;

        bool intersected = false;
        do {

            ael.findEnclosing(v, &left_enclosing, &right_enclosing);

            v->left = left_enclosing;
            v->right = right_enclosing;

            if (v->edge_below.head)
            {
                for (auto e = v->edge_below.head; e; e = e->below_next)
                {
                    // check current edge is intersected by left or right neighbor edges
                    if (checkIntersection(left_enclosing, e, &ael, &v) ||
                        checkIntersection(e, right_enclosing, &ael, &v))
                    {
                        intersected = true;
                        // find intersection between current and it's neighbor
                        break;
                    }
                }
            } else
            {
                // check left and right intersection
                if (checkIntersection(left_enclosing, right_enclosing, &ael, &v))
                {
                    intersected = true;
                }
            }
        } while (intersected);

        // we are done for all edge end with current point
        for (auto e = v->edge_above.head; e; e = e->above_next)
        {
            ael.remove(e);
        }

        auto left = left_enclosing;

        // insert all edge start from current point into ael
        for (auto e = v->edge_below.head; e; e = e->below_next)
        {
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

    for (auto v = pMesh->head; v; v = v->next)
    {
        if (!v->isConnected())
        {
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

        if (v->edge_above.head)
        {
            left_poly = v->edge_above.head->left_poly;
            right_poly = v->edge_above.head->right_poly;
        } else
        {
            left_poly = left_enclosing ? left_enclosing->right_poly : nullptr;
            right_poly = right_enclosing ? right_enclosing->left_poly : nullptr;
        }

        if (v->edge_above.head)
        {
            // add above edge first
            if (left_poly)
            {
                left_poly = left_poly->addEdge(v->edge_above.head, detail::Side::kRight, pHeap.get());
            }

            if (right_poly)
            {
                right_poly = right_poly->addEdge(v->edge_above.tail, detail::Side::kLeft, pHeap.get());
            }

            // walk through all edges end with this vertex
            for (auto e = v->edge_above.head; e != v->edge_above.tail; e = e->above_next)
            {
                auto right_edge = e->above_next;

                ael.remove(e);

                if (e->right_poly)
                {
                    e->right_poly->addEdge(right_edge, detail::Side::kLeft, pHeap.get());
                }

                // this means there is a new polygon between e and right_edge
                if (right_edge->left_poly && right_edge->left_poly != e->right_poly)
                {
                    right_edge->left_poly->addEdge(e, detail::Side::kRight, pHeap.get());
                }
            }

            ael.remove(v->edge_above.tail);

            // there is no edge begin with this vertex
            if (!v->edge_below.head)
            {
                if (left_poly && right_poly && left_poly != right_poly)
                {
                    // polygon not closed at this point
                    // need to mark these two polygon each other, because they will be
                    // linked by a cross edge later

                    left_poly->parent = right_poly;
                    right_poly->parent = left_poly;
                }
            }
        }

        if (v->edge_below.head)
        {
            if (!v->edge_above.head)
            {
                // there is no edge end with this vertex
                if (left_poly && right_poly)
                {

                    if (left_poly == right_poly)
                    {
                        /**
                         *   left_poly      right_poly
                         *
                         *              v
                         *             / \
                         *            /   \
                         *             ...
                         */
                        if (left_poly->tail && left_poly->tail->side == detail::Side::kLeft)
                        {
                            left_poly = this->makePoly(left_poly->lastVertex(), left_poly->winding);

                            left_enclosing->right_poly = left_poly;
                        } else
                        {
                            right_poly = this->makePoly(right_poly->lastVertex(), right_poly->winding);
                        }
                    }

                    // need to link this vertex to above polygon
                    auto join = pHeap->Allocate<detail::Edge>(left_poly->lastVertex(), v, 1);

                    left_poly = left_poly->addEdge(join, detail::Side::kRight, pHeap.get());
                    right_poly = right_poly->addEdge(join, detail::Side::kLeft, pHeap.get());
                }
            }

            auto left_edge = v->edge_below.head;
            left_edge->left_poly = left_poly;

            ael.insert(left_edge, left_enclosing);

            for (auto right_edge = left_edge->below_next; right_edge; right_edge = right_edge->below_next)
            {
                ael.insert(right_edge, left_edge);

                int32_t winding = left_edge->left_poly ? left_edge->left_poly->winding : 0;

                winding += left_edge->winding;

                if (winding != 0)
                {
                    auto poly = this->makePoly(v, winding);

                    left_edge->right_poly = right_edge->left_poly = poly;
                }

                left_edge = right_edge;
            }

            v->edge_below.tail->right_poly = right_poly;
        }
    }
}

void Tessellator::mergeMesh()
{
    this->removeInnerEdges();

    for (auto v = pMesh->head; v; v = v->next)
    {
        while (v->edge_below.head)
        {
            auto winding = v->edge_below.head->winding;

            if (winding != 0 && !matchFillRule(winding))
            {
                break;
            }

            this->extractBoundary(v->edge_below.head);
        }
    }
}

bool Tessellator::matchFillRule(int32_t winding)
{
    if (fillRule == FillRule::Winding)
    {
        return winding != 0;
    } else
    {
        return (winding & 0x1) != 0;
    }
}

void Tessellator::removeInnerEdges()
{
    /// we use sweep line algorithm here to remove inner edges
    /// during sweep line, we calculate all edge's winding number
    /// and remove edges which not effects the winding rules
    detail::ActiveEdgeList ael{};

    for (auto v = pMesh->head; v; v = v->next)
    {
        if (!v->isConnected())
        {
            continue;
        }

        detail::Edge *left_enclosing = nullptr;
        detail::Edge *right_enclosing = nullptr;

        ael.findEnclosing(v, &left_enclosing, &right_enclosing);

        bool prev_filled = left_enclosing && this->matchFillRule(left_enclosing->winding);

        for (auto e = v->edge_above.head; e;)
        {
            auto next = e->above_next;

            ael.remove(e);

            bool filled = this->matchFillRule(e->winding);

            if (filled == prev_filled)
            {
                e->disconnect();
            } else if (next && next->top->point == e->top->point && next->bottom->point == e->bottom->point)
            {
                if (!filled)
                {
                    e->disconnect();
                    filled = true;
                }
            }

            prev_filled = filled;
            e = next;
        }

        auto prev = left_enclosing;

        for (auto e = v->edge_below.head; e; e = e->below_next)
        {
            if (prev)
            {
                e->winding += prev->winding;
            }

            ael.insert(e, prev);

            prev = e;
        }
    }
}

void Tessellator::extractBoundary(detail::Edge *e)
{
    bool down = e->winding & 1;

    auto start = down ? e->top : e->bottom;
    outlineResult->moveTo(start->point.x, start->point.y);

    do {
        e->winding = down ? 1 : -1;

        if (down)
        {
            outlineResult->lineTo(e->bottom->point.x, e->bottom->point.y);
        } else
        {
            outlineResult->lineTo(e->top->point.x, e->top->point.y);
        }

        detail::Edge *next;

        if (down)
        {
            if ((next = e->above_next))
            {
                down = false;
            } else if ((next = e->bottom->edge_below.tail))
            {
                down = true;
            } else if ((next = e->above_prev))
            {
                down = false;
            }
        } else
        {
            if ((next = e->below_prev))
            {
                down = true;
            } else if ((next = e->top->edge_above.head))
            {
                down = false;
            } else if ((next = e->below_next))
            {
                down = true;
            }
        }

        e->disconnect();
        e = next;

    } while (e && (down ? e->top : e->bottom) != start);

    outlineResult->close();
}

detail::Edge *Tessellator::makeEdge(detail::Vertex *a, detail::Vertex *b)
{
    if (!a || !b || a->point == b->point)
    {
        return nullptr;
    }

    int32_t winding = 1;

    if (detail::VertexCompare::Compare(b->point, a->point))
    {
        winding = -1;
        std::swap(a, b);
    }

    return pHeap->Allocate<detail::Edge>(a, b, winding);
}

bool Tessellator::checkIntersection(detail::Edge *left, detail::Edge *right, detail::ActiveEdgeList *ael,
                                    detail::Vertex **current)
{
    if (!left || !right)
    {
        return false;
    }

    Point p;

    if (left->intersect(right, &p) && !std::isinf(p.x) && !std::isinf(p.y))
    {
        detail::Vertex *v;
        detail::Vertex *top = *current;

        // the vertex in mesh is sorted, so walk to prev can find latest top point
        while (top && detail::VertexCompare::Compare(p, top->point))
        {
            top = top->prev;
        }

        if (p == left->top->point)
        {
            v = left->top;
        } else if (p == left->bottom->point)
        {
            v = left->bottom;
        } else if (p == right->top->point)
        {
            v = right->top;
        } else if (p == right->bottom->point)
        {
            v = right->bottom;
        } else
        {
            // intersect point is between start and end point
            // need to insert new vertex
            auto prev = top;
            while (prev && detail::VertexCompare::Compare(p, prev->point))
            {
                prev = prev->prev;
            }

            auto next = prev ? prev->next : pMesh->head;
            while (next && detail::VertexCompare::Compare(next->point, p))
            {
                prev = next;
                next = next->next;
            }

            // check if point is already in mesh
            if (prev && prev->point == p)
            {
                v = prev;
            } else if (next && next->point == p)
            {
                v = next;
            } else
            {
                v = pHeap->Allocate<detail::Vertex>(p);
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
    if (!edge->top || !edge->bottom || v == edge->top || v == edge->bottom)
    {
        return false;
    }

    int32_t winding = edge->winding;

    detail::Vertex *top;
    detail::Vertex *bottom;

    if (detail::VertexCompare::Compare(v->point, edge->top->point))
    {
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
    } else if (detail::VertexCompare::Compare(edge->bottom->point, v->point))
    {
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
    } else
    {
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

    auto new_edge = pHeap->Allocate<detail::Edge>(top, bottom, winding);

    bottom->insertAbove(new_edge);

    top->insertBelow(new_edge);

    return true;
}

bool Tessellator::intersectPairEdge(detail::Edge *left, detail::Edge *right, detail::ActiveEdgeList *ael,
                                    detail::Vertex **current)
{
    if (!left->top || !left->bottom || !right->top || !right->bottom)
    {
        return false;
    }

    if (left->top == right->top || left->bottom == right->bottom)
    {
        return false;
    }

    detail::Edge *split = nullptr;

    detail::Vertex *split_at = nullptr;

    // check if these two edge is intersected
    if (detail::VertexCompare::Compare(left->top->point, right->top->point))
    {
        if (!left->isLeftOf(right->top->point))
        {
            split = left;
            split_at = right->top;
        }
    } else
    {
        if (!right->isRightOf(left->top->point))
        {
            split = right;
            split_at = left->top;
        }
    }

    if (detail::VertexCompare::Compare(right->bottom->point, left->bottom->point))
    {
        if (!left->isLeftOf(right->bottom->point))
        {
            split = left;
            split_at = right->bottom;
        }
    } else
    {
        if (!right->isRightOf(left->bottom->point))
        {
            split = right;
            split_at = left->bottom;
        }
    }

    if (!split)
    {
        return false;
    }

    ael->rewind(current, split->top);

    return splitEdge(split, split_at, ael, current);
}

detail::Polygon *Tessellator::makePoly(detail::Vertex *v, int32_t winding)
{
    auto poly = pHeap->Allocate<detail::Polygon>(v, winding);

    poly->next = this->pPolygon;
    this->pPolygon = poly;

    return poly;
}

void Tessellator::emitPoly(detail::MonotonePolygon *poly)
{
    auto e = poly->first;

    Array<detail::Vertex *> vertices;

    vertices.push(e->top);

    while (e != nullptr)
    {
        vertices.push(e->bottom);
        if (poly->side == detail::Side::kLeft)
        {
            e = e->left_poly_next;
        } else
        {
            e = e->right_poly_next;
        }
    }

    if (vertices.count < 3)
    {
        return;
    }

    uint32_t first_index = this->pushVertex(vertices.first()->point.x, vertices.first()->point.y, 1.f);

    uint32_t prev_index = this->pushVertex(vertices.data[1]->point.x, vertices.data[1]->point.y, 1.f);

    for (uint32_t i = 2; i < vertices.count; i++)
    {
        uint32_t curr_index = this->pushVertex(vertices.data[i]->point.x, vertices.data[i]->point.y, 1.f);

        this->resIndices->push(first_index);
        this->resIndices->push(prev_index);
        this->resIndices->push(curr_index);

        prev_index = curr_index;
    }
}

uint32_t Tessellator::pushVertex(float x, float y, float a)
{
    auto index = this->resPoints->count / TES_POINT_STRIDE;

    this->resPoints->push(x);
    this->resPoints->push(y);
    this->resPoints->push(a);

    return index;
}

}  // namespace tvg
