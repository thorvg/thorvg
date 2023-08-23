#include "sweep_line/slCommon.h"
#include <thorvg.h>

namespace tvg {
namespace sweep_line {

class OutlineExtractor {
public:
  OutlineExtractor() = default;
  ~OutlineExtractor() = default;

  void extractOutline(const Shape *src, Shape *dst);

private:
  void buildMesh();
  void mergeVertices();
  void simplifyMesh();
  void mergeMesh();

  Edge *makeEdge(Vertex *p1, Vertex *p2);

  bool checkIntersection(Edge *left, Edge *right, ActiveEdgeList *ael,
                         Vertex **current);

  bool splitEdge(Edge *edge, Vertex *v, ActiveEdgeList *ael, Vertex **current);

  bool intersectPairEdge(Edge *left, Edge *right, ActiveEdgeList *ael,
                         Vertex **current);

  void removeInnerEdges();

  void extractBoundary(Edge *edge);

  bool matchFillRule(int32_t winding);

private:
  ObjectHeap heap_ = {};
  FillRule fill_rule_ = FillRule::Winding;

  std::vector<VertexList> outlines_ = {};
  VertexList mesh_ = {};
  Shape* result_ = nullptr;
};

} // namespace sweep_line
} // namespace tvg
