#pragma once

#include <thorvg.h>

#include <vector>

namespace tvg {
namespace sweep_line {

template <typename T> struct LinkedList {
  T *head = nullptr;
  T *tail = nullptr;

  LinkedList() = default;
  LinkedList(T *head, T *tail) : head(head), tail(tail) {}

  template <T *T::*Prev, T *T::*Next>
  static void Insert(T *t, T *prev, T *next, T **head, T **tail) {
    t->*Prev = prev;
    t->*Next = next;

    if (prev) {
      prev->*Next = t;
    } else if (head) {
      *head = t;
    }

    if (next) {
      next->*Prev = t;
    } else if (tail) {
      *tail = t;
    }
  }

  template <T *T::*Prev, T *T::*Next>
  static void Remove(T *t, T **head, T **tail) {
    if (t->*Prev) {
      t->*Prev->*Next = t->*Next;
    } else if (head) {
      *head = t->*Next;
    }

    if (t->*Next) {
      t->*Next->*Prev = t->*Prev;
    } else if (tail) {
      *tail = t->*Prev;
    }

    t->*Prev = t->*Next = nullptr;
  }
};

// common obj for memory controle
struct Object {
  virtual ~Object() = default;
};

struct Edge;

struct Vertex : public Object {
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

  Vertex(const Point &p) : point(p) {}

  ~Vertex() override = default;

  bool isConnected() const { return edge_above.head || edge_below.head; }

  void insertAbove(Edge* e);
  void insertBelow(Edge* e);
};

// we sort point by top first then left
struct VertexCompare {
  inline bool operator()(Vertex* v1, Vertex* v2) {
    return Compare(v1->point, v2->point);
  }

  static bool Compare(const Point& a, const Point& b);
};

// double linked list for all vertex in shape
struct VertexList : public LinkedList<Vertex> {
  VertexList() = default;
  VertexList(Vertex* head, Vertex* tail) : LinkedList(head, tail) {}

  void insert(Vertex* v, Vertex* prev, Vertex* next);
  void remove(Vertex* v);

  void append(VertexList const& other);
  void append(Vertex* v);
  void prepend(Vertex* v);

  void close();
};

struct Edge : public Object {
  Vertex* top = nullptr;
  Vertex* bottom = nullptr;

  Edge* above_prev = nullptr;
  Edge* above_next = nullptr;
  Edge* below_prev = nullptr;
  Edge* below_next = nullptr;

  // left edge in active list during sweep line
  Edge* left = nullptr;
  // right edge in active list during sweep line
  Edge* right = nullptr;

  int32_t winding = 1;

  Edge(Vertex* top, Vertex* bottom, int32_t winding);

  ~Edge() override = default;

  // https://stackoverflow.com/questions/1560492/how-to-tell-whether-a-point-is-to-the-right-or-left-side-of-a-line
  // return > 0 means point in left
  // return < 0 means point in right
  double sideDist(const Point& p);

  bool isRightOf(const Point& p) { return sideDist(p) < 0.0; }

  bool isLeftOf(const Point& p) { return sideDist(p) > 0.0; }

  // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection
  bool intersect(Edge* other, Point* point);

  void recompute();

  void setBottom(Vertex* v);

  void setTop(Vertex* v);

  void disconnect();

 private:
  double le_a;
  double le_b;
  double le_c;
};

// Active Edge List (AEL) or Active Edge Table (AET) during sweep line
struct ActiveEdgeList : public LinkedList<Edge> {
  ActiveEdgeList() = default;
  ~ActiveEdgeList() = default;

  void insert(Edge* edge, Edge* prev, Edge* next);
  void insert(Edge* edge, Edge* prev);

  void append(Edge* edge);
  void remove(Edge* edge);

  bool contains(Edge* edge);

  // move event point from current to dst
  void rewind(Vertex** current, Vertex* dst);

  void findEnclosing(Vertex* v, Edge** left, Edge** right);
};

class ObjectHeap {
 public:
  ObjectHeap() = default;
  ~ObjectHeap() = default;

  template <class T, class... Args>
  T* Allocate(Args&&... args) {
    objs_.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));

    return static_cast<T*>(objs_.back().get());
  }

 private:
  std::vector<std::unique_ptr<Object>> objs_ = {};
};


bool operator==(const Point &p1, const Point &p2);
Point operator-(const Point &p1, const Point &p2);
Point operator+(const Point &p1, const Point &p2);
Point operator*(const Point &p1, const Point &p2);

} // namespace sweep_line
} // namespace tvg
