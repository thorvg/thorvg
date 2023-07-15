#pragma once

#include <thorvg.h>
#include <cstdint>

#include "tvgArray.h"
#include "tvgBezier.h"

namespace tvg
{

namespace detail
{

class ObjectHeap;

struct Vertex;
struct Edge;
struct Polygon;
struct MonotonePolygon;
struct VertexList;
struct ActiveEdgeList;

}  // namespace detail

struct RenderShape;

class Tessellator final
{
public:
    Tessellator(Array<float> *points, Array<uint32_t> *indices);
    ~Tessellator();

    void tessellate(const Shape *shape);

    void tessellate(const RenderShape *rshape, bool antialias = false);

    void decomposeOutline(const Shape *shape, Shape *dst);

private:
    void visitShape(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count);

    void buildMesh();

    void mergeVertices();

    void simplifyMesh();

    void tessMesh();

    void mergeMesh();

    bool matchFillRule(int32_t winding);

    void removeInnerEdges();

    void extractBoundary(detail::Edge *edge);

    detail::Edge *makeEdge(detail::Vertex *p1, detail::Vertex *p2);

    bool checkIntersection(detail::Edge *left, detail::Edge *right, detail::ActiveEdgeList *ael,
                           detail::Vertex **current);

    bool splitEdge(detail::Edge *edge, detail::Vertex *v, detail::ActiveEdgeList *ael, detail::Vertex **current);

    bool intersectPairEdge(detail::Edge *left, detail::Edge *right, detail::ActiveEdgeList *ael,
                           detail::Vertex **current);

    detail::Polygon *makePoly(detail::Vertex *v, int32_t winding);

    void emitPoly(detail::MonotonePolygon *poly);

private:
    FillRule                            fillRule = FillRule::Winding;
    std::unique_ptr<detail::ObjectHeap> pHeap;

    Array<detail::VertexList *> outlines;

    detail::VertexList *pMesh;
    detail::Polygon    *pPolygon;

    Array<float>    *resPoints;
    Array<uint32_t> *resIndices;

    Shape *outlineResult = nullptr;
};

class Stroker final
{

    struct State
    {
        Point firstPt = {};
        Point firstPtDir = {};
        Point prevPt = {};
        Point prevPtDir = {};
        bool  hasMove = false;
    };
public:
    Stroker(Array<float> *points, Array<uint32_t> *indices);
    ~Stroker() = default;

    void stroke(const RenderShape *rshape);

private:
    void doStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count);

    float strokeRadius() const
    {
        return mStrokeWidth * 0.5f;
    }

    void strokeCap();

    void strokeLineTo(const Point &curr);

    void strokeCubicTo(const Point &cnt1, const Point &cnt2, const Point &end);

    void strokeClose();

    void strokeJoin(const Point &dir);

    void strokeRound(const Bezier& curve, const Point& center);
private:
    Array<float>    *mResPoints;
    Array<uint32_t> *mResIndices;
    float            mStrokeWidth = 1.f;
    float            mMiterLimit = 4.f;
    StrokeCap        mStrokeCap = StrokeCap::Square;
    StrokeJoin       mStrokeJoin = StrokeJoin::Bevel;
    State            mStrokeState = {};
};

}  // namespace tvg