#include "sweep_line/outlineExtractor.h"

#include <array>
#include <cmath>

namespace tvg {
namespace sweep_line {

/**
 * use for : eval(t) = A * t ^ 3 + B * t ^ 2 + C * t + D
 */
struct Cubic {
  Point A{};
  Point B{};
  Point C{};
  Point D{};

  explicit Cubic(std::array<Point, 4> const &src) {
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

  Point eval(float t) {
    Point tt{t, t};
    return ((A * tt + B) * tt + C) * tt + D;
  }
};

void OutlineExtractor::extractOutline(const Shape *src, Shape *dst) {
  fill_rule_ = src->fillRule();
  result_ = dst;

  const PathCommand *cmds = nullptr;
  const Point *pts = nullptr;

  auto cmd_count = src->pathCommands(&cmds);
  auto pts_count = src->pathCoords(&pts);

  for (uint32_t i = 0; i < cmd_count; i++) {
    switch (cmds[i]) {
    case PathCommand::MoveTo: {
      outlines_.emplace_back(VertexList{});

      outlines_.back().append(heap_.Allocate<Vertex>(*pts));
      pts++;
    } break;
    case PathCommand::LineTo: {
      outlines_.back().append(heap_.Allocate<Vertex>(*pts));
      pts++;
    } break;
    case PathCommand::CubicTo: {
      // bezier curve needs to calcluate how many segment to split
      // for now just break curve into 16 segments for convenient
      Point start = outlines_.back().tail->point;
      Point c1 = pts[0];
      Point c2 = pts[1];
      Point end = pts[2];

      Cubic cubic({start, c1, c2, end});

      float step = 1.f / 15.f;

      for (uint32_t s = 0; s < 16; s++) {
        outlines_.back().append(heap_.Allocate<Vertex>(cubic.eval(step * s)));
      }

      pts += 3;

    } break;
    case PathCommand::Close: // fall through
    default:
      break;
    }
  }

  // construct edge and mesh
  buildMesh();
  // merge parallel vertices
  mergeVertices();
  // break edge intersections
  simplifyMesh();
  // merge adjacent polygons
  mergeMesh();
}

void OutlineExtractor::buildMesh() {
  std::vector<Vertex *> temp{};

  for (auto const &list : outlines_) {
    auto prev = list.tail;
    auto v = list.head;

    while (v != nullptr) {
      auto next = v->next;

      auto edge = makeEdge(prev, v);

      if (edge) {
        edge->bottom->insertAbove(edge);
        edge->top->insertBelow(edge);
      }

      temp.emplace_back(v);

      prev = v;
      v = next;
    }
  }

  std::sort(temp.begin(), temp.end(), VertexCompare{});

  for (auto const &v : temp) {
    mesh_.append(v);
  }
}

void OutlineExtractor::mergeVertices() {
  if (!mesh_.head) {
    // mesh is empty
    return;
  }

  for (auto v = mesh_.head->next; v;) {
    auto next = v->next;

    if (VertexCompare::Compare(v->point, v->prev->point)) {
      // already sorted, this means this two points is same
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

      mesh_.remove(v);
    }

    v = next;
  }
}

void OutlineExtractor::simplifyMesh() {
  ActiveEdgeList ael;

  for (Vertex *v = mesh_.head; v != nullptr; v = v->next) {
    if (!v->isConnected()) {
      continue;
    }

    Edge *left_enclosing = nullptr;
    Edge *right_enclosing = nullptr;

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

void OutlineExtractor::mergeMesh() {
  removeInnerEdges();

  for (auto v = mesh_.head; v; v = v->next) {
    while (v->edge_below.head) {
      auto winding = v->edge_below.head->winding;

      if (winding != 0 && !matchFillRule(winding)) {
        break;
      }
      extractBoundary(v->edge_below.head);
    }
  }
}

Edge *OutlineExtractor::makeEdge(Vertex *t, Vertex *b) {
  if (!t || !b || t->point == b->point) {
    return nullptr;
  }

  int32_t winding = 1;

  if (VertexCompare::Compare(b->point, t->point)) {
    winding = -1;
    std::swap(t, b);
  }

  return heap_.Allocate<Edge>(t, b, winding);
}

bool OutlineExtractor::checkIntersection(Edge *left, Edge *right,
                                         ActiveEdgeList *ael,
                                         Vertex **current) {
  if (!left || !right) {
    return false;
  }

  Point p;

  if (left->intersect(right, &p) && !std::isinf(p.x) && !std::isinf(p.y)) {
    Vertex *v;
    Vertex *top = *current;

    // the vertex in mesh is sorted, so walk to prev can find latest top point
    while (top && VertexCompare::Compare(p, top->point)) {
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
      while (prev && VertexCompare::Compare(p, prev->point)) {
        prev = prev->prev;
      }

      auto next = prev ? prev->next : mesh_.head;
      while (next && VertexCompare::Compare(next->point, p)) {
        prev = next;
        next = next->next;
      }

      // check if point is already in mesh
      if (prev && prev->point == p) {
        v = prev;
      } else if (next && next->point == p) {
        v = next;
      } else {
        v = heap_.Allocate<Vertex>(p);
        v->point = p;

        mesh_.insert(v, prev, next);
      }
    }
    ael->rewind(current, top ? top : v);

    splitEdge(left, v, ael, current);
    splitEdge(right, v, ael, current);
    return true;
  }

  return intersectPairEdge(left, right, ael, current);
}

bool OutlineExtractor::splitEdge(Edge *edge, Vertex *v, ActiveEdgeList *ael,
                                 Vertex **current) {
  if (!edge->top || !edge->bottom || v == edge->top || v == edge->bottom) {
    return false;
  }

  int32_t winding = edge->winding;

  Vertex *top;
  Vertex *bottom;

  if (VertexCompare::Compare(v->point, edge->top->point)) {
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
  } else if (VertexCompare::Compare(edge->bottom->point, v->point)) {
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

  auto new_edge = heap_.Allocate<Edge>(top, bottom, winding);

  bottom->insertAbove(new_edge);
  top->insertBelow(new_edge);

  return true;
}

bool OutlineExtractor::intersectPairEdge(Edge *left, Edge *right,
                                         ActiveEdgeList *ael,
                                         Vertex **current) {
  if (!left->top || !left->bottom || !right->top || !right->bottom) {
    return false;
  }

  if (left->top == right->top || left->bottom == right->bottom) {
    return false;
  }

  Edge *split = nullptr;

  Vertex *split_at = nullptr;

  // check if these two edge is intersected
  if (VertexCompare::Compare(left->top->point, right->top->point)) {
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

  if (VertexCompare::Compare(right->bottom->point, left->bottom->point)) {
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

void OutlineExtractor::removeInnerEdges() {
  ActiveEdgeList ael{};

  for (auto v = mesh_.head; v; v = v->next) {
    if (!v->isConnected()) {
      continue;
    }

    Edge *left_enclosing = nullptr;
    Edge *right_enclosing = nullptr;

    ael.findEnclosing(v, &left_enclosing, &right_enclosing);

    bool prev_filled = left_enclosing && matchFillRule(left_enclosing->winding);

    for (auto *e = v->edge_above.head; e;) {
      auto next = e->above_next;
      ael.remove(e);

      bool filled = matchFillRule(e->winding);

      if (filled == prev_filled) {
        e->disconnect();
      } else if (next && next->top->point == e->top->point &&
                 next->bottom->point == e->bottom->point) {
        if (!filled) {
          e->disconnect();
          filled = true;
        }
      }

      prev_filled = filled;
      e = next;
    }

    auto prev = left_enclosing;

    for (auto e = v->edge_below.head; e; e = e->below_next) {
      if (prev) {
        e->winding += prev->winding;
      }

      ael.insert(e, prev);
      prev = e;
    }
  }
}

void OutlineExtractor::extractBoundary(Edge *e) {
  bool down = e->winding & 1;

  auto start = down ? e->top : e->bottom;
  result_->moveTo(start->point.x, start->point.y);

  do {
    e->winding = down ? 1 : -1;

    if (down) {
      result_->lineTo(e->bottom->point.x, e->bottom->point.y);
    } else {
      result_->lineTo(e->top->point.x, e->top->point.y);
    }

    Edge *next;

    if (down) {
      if ((next = e->above_next)) {
        down = false;
      } else if ((next = e->bottom->edge_below.tail)) {
        down = true;
      } else if ((next = e->above_prev)) {
        down = false;
      }
    } else {
      if ((next = e->below_prev)) {
        down = true;
      } else if ((next = e->top->edge_above.head)) {
        down = false;
      } else if ((next = e->below_next)) {
        down = true;
      }
    }

    e->disconnect();
    e = next;

  } while (e && (down ? e->top : e->bottom) != start);

  result_->close();
}

bool OutlineExtractor::matchFillRule(int32_t winding) {
  if (fill_rule_ == FillRule::Winding) {
    return winding != 0;
  } else {
    return winding & 1;
  }
}

} // namespace sweep_line
} // namespace tvg