#pragma once

#include <thorvg.h>

#include "tvgArray.h"

namespace tvg {
namespace detail {
class ObjectHeap;

struct Vertex;
struct Edge;
struct Polygon;
struct MonotonePolygon;
struct VertexList;
struct ActiveEdgeList;

} // namespace detail

struct RenderShape;

class Tessellator final {
  enum {
    TES_POINT_STRIDE = 3,
  };

public:
  Tessellator();
  ~Tessellator();

  void tessellate(const Shape *shape);

  void tessellate(const RenderShape *rshape, bool antialias = false);

  void decomposeOutline(const Shape *shape, Shape *dst);

  const Array<float> &getPoints() const { return resPoints; }

  const Array<uint32_t> &getIndices() const { return resIndices; }

private:
  void visitShape(const PathCommand *cmds, uint32_t cmd_count, const Point *pts,
                  uint32_t pts_count);

  void buildMesh();

  void mergeVertices();

  void simplifyMesh();

  void tessMesh();

  void mergeMesh();

  bool matchFillRule(int32_t winding);

  void removeInnerEdges();

  void extractBoundary(detail::Edge *edge);

  detail::Edge *makeEdge(detail::Vertex *p1, detail::Vertex *p2);

  bool checkIntersection(detail::Edge *left, detail::Edge *right,
                         detail::ActiveEdgeList *ael, detail::Vertex **current);

  bool splitEdge(detail::Edge *edge, detail::Vertex *v,
                 detail::ActiveEdgeList *ael, detail::Vertex **current);

  bool intersectPairEdge(detail::Edge *left, detail::Edge *right,
                         detail::ActiveEdgeList *ael, detail::Vertex **current);

  detail::Polygon *makePoly(detail::Vertex *v, int32_t winding);

  void emitPoly(detail::MonotonePolygon *poly);

  uint32_t pushVertex(float x, float y, float a);

private:
  FillRule fillRule = FillRule::Winding;
  std::unique_ptr<detail::ObjectHeap> pHeap;

  Array<detail::VertexList *> outlines;

  detail::VertexList *pMesh;
  detail::Polygon *pPolygon;

  Array<float> resPoints;
  Array<uint32_t> resIndices;

  Shape *outlineResult = nullptr;
};

} // namespace tvg