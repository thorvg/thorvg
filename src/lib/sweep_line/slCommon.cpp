#include "sweep_line/slCommon.h"
#include "thorvg.h"

#include <cmath>

namespace tvg {
namespace sweep_line {

bool operator==(const Point &p1, const Point &p2) {
  return p1.x == p2.x && p1.y == p2.y;
}

Point operator-(const Point &p1, const Point &p2) {
  return Point{p1.x - p2.x, p1.y - p2.y};
}

Point operator+(const Point &p1, const Point &p2) {
  return Point{p1.x + p2.x, p1.y + p2.y};
}

Point operator*(const Point &p1, const Point &p2) {
  return Point{p1.x * p2.x, p1.y * p2.y};
}

void Vertex::insertAbove(Edge *e) {
  if (e->top->point == e->bottom->point ||                    // no edge
      VertexCompare::Compare(e->bottom->point, e->top->point) // not above
  ) {
    return;
  }

  Edge *above_prev = nullptr;
  Edge *above_next = nullptr;

  // find insert point
  for (above_next = edge_above.head; above_next != nullptr;
       above_next = above_next->above_next) {
    if (above_next->isRightOf(e->top->point)) {
      break;
    }

    above_prev = above_next;
  }

  LinkedList<Edge>::Insert<&Edge::above_prev, &Edge::above_next>(
      e, above_prev, above_next, &edge_above.head, &edge_above.tail);
}

void Vertex::insertBelow(Edge *e) {
  if (e->top->point == e->bottom->point ||                    // no edge
      VertexCompare::Compare(e->bottom->point, e->top->point) // not below
  ) {
    return;
  }

  Edge *below_prev = nullptr;
  Edge *below_next = nullptr;

  for (below_next = edge_below.head; below_next != nullptr;
       below_next = below_next->below_next) {
    if (below_next->isRightOf(e->bottom->point)) {
      break;
    }

    below_prev = below_next;
  }

  LinkedList<Edge>::Insert<&Edge::below_prev, &Edge::below_next>(
      e, below_prev, below_next, &edge_below.head, &edge_below.tail);
}

bool VertexCompare::Compare(const Point &a, const Point &b) {
  return a.y < b.y || (a.y == b.y && a.x < b.x);
}

void VertexList::insert(Vertex *v, Vertex *prev, Vertex *next) {
  Insert<&Vertex::prev, &Vertex::next>(v, prev, next, &head, &tail);
}

void VertexList::remove(Vertex *v) {
  Remove<&Vertex::prev, &Vertex::next>(v, &head, &tail);
}

void VertexList::append(VertexList const &other) {
  if (other.head == nullptr) {
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

void VertexList::append(Vertex *v) { insert(v, tail, nullptr); }

void VertexList::prepend(Vertex *v) { insert(v, nullptr, head); }

void VertexList::close() {
  if (head && tail) {
    tail->next = head;
    head->prev = tail;
  }
}

Edge::Edge(Vertex *top, Vertex *bottom, int32_t winding)
    : top(top), bottom(bottom), winding(winding),
      le_a(static_cast<double>(bottom->point.y) - top->point.y),
      le_b(static_cast<double>(top->point.x) - bottom->point.x),
      le_c(static_cast<double>(top->point.y) * bottom->point.x -
           static_cast<double>(top->point.x) * bottom->point.y) {}

double Edge::sideDist(const Point &p) { return le_a * p.x + le_b * p.y + le_c; }

bool Edge::intersect(Edge *other, Point *point) {
  if (top == other->top || bottom == other->bottom || top == other->bottom ||
      bottom == other->top) {
    return false;
  }

  // check if two aabb bounds is intersect
  if (std::min(top->point.x, bottom->point.x) >
          std::max(other->top->point.x, other->bottom->point.x) ||
      std::max(top->point.x, bottom->point.x) <
          std::min(other->top->point.x, other->bottom->point.x) ||
      std::min(top->point.y, bottom->point.y) >
          std::max(other->top->point.y, other->bottom->point.y) ||
      std::max(top->point.y, bottom->point.y) <
          std::min(other->top->point.y, other->bottom->point.y)) {
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

  if (denom > 0.0 ? (s_number < 0.0 || s_number > denom || t_number < 0.0 ||
                     t_number > denom)
                  : (s_number > 0.0 || s_number < denom || t_number > 0.0 ||
                     t_number < denom)) {
    return false;
  }

  double scale = 1.0 / denom;

  point->x = std::round(
      static_cast<float>(top->point.x - s_number * le_b * scale));
  point->y = std::round(
      static_cast<float>(top->point.y + s_number * le_a * scale));

  if (std::isinf(point->x) || std::isinf(point->y)) {
    return false;
  }

  return true;
}

void Edge::recompute() {
  le_a = static_cast<double>(bottom->point.y) - top->point.y;
  le_b = static_cast<double>(top->point.x) - bottom->point.x;
  le_c = static_cast<double>(top->point.y) * bottom->point.x -
         static_cast<double>(top->point.x) * bottom->point.y;
}

void Edge::setTop(Vertex *v) {
  // remove this edge from top's below list
  LinkedList<Edge>::Remove<&Edge::below_prev, &Edge::below_next>(
      this, &top->edge_below.head, &top->edge_below.tail);
  // update top vertex
  top = v;
  // recompute line equation
  recompute();
  // insert self to new top's below list
  top->insertBelow(this);
}

static void remove_edge_above(Edge *edge) {
  LinkedList<Edge>::Remove<&Edge::above_prev, &Edge::above_next>(
      edge, &edge->bottom->edge_above.head, &edge->bottom->edge_above.tail);
}

static void remove_edge_below(Edge *edge) {
  LinkedList<Edge>::Remove<&Edge::below_prev, &Edge::below_next>(
      edge, &edge->top->edge_below.head, &edge->top->edge_below.tail);
}

void Edge::disconnect() {
  remove_edge_above(this);
  remove_edge_below(this);
}

void Edge::setBottom(Vertex *v) {
  // remove this edge from bottom's above list
  LinkedList<Edge>::Remove<&Edge::above_prev, &Edge::above_next>(
      this, &bottom->edge_above.head, &bottom->edge_above.tail);
  // update bottom vertex
  bottom = v;
  // recompute line equation
  recompute();
  // insert self to new bottom's above list
  bottom->insertAbove(this);
}

void ActiveEdgeList::insert(Edge *edge, Edge *prev, Edge *next) {
  LinkedList<Edge>::Insert<&Edge::left, &Edge::right>(edge, prev, next, &head,
                                                      &tail);
}

void ActiveEdgeList::insert(Edge *edge, Edge *prev) {
  auto next = prev ? prev->right : head;

  insert(edge, prev, next);
}

void ActiveEdgeList::append(Edge *edge) { insert(edge, tail, nullptr); }

void ActiveEdgeList::remove(Edge *edge) {
  LinkedList<Edge>::Remove<&Edge::left, &Edge::right>(edge, &head, &tail);
}

bool ActiveEdgeList::contains(Edge *edge) {
  return edge->left || edge->right || head == edge;
}

void ActiveEdgeList::rewind(Vertex **current, Vertex *dst) {
  if (!current || *current == dst ||
      VertexCompare::Compare((*current)->point, dst->point)) {
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
      if (VertexCompare::Compare(top->point, dst->point) &&
          ((top->left && !top->left->isLeftOf(e->top->point)) ||
           (top->right && !top->right->isRightOf(e->top->point)))) {
        dst = top;
      }
    }
  }

  *current = v;
}

void ActiveEdgeList::findEnclosing(Vertex *v, Edge **left, Edge **right) {
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

} // namespace sweep_line
} // namespace tvg