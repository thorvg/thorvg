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

    void tessellate(const Array<const RenderShape *> &shapes);

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

    void emitTriangle(detail::Vertex *p1, detail::Vertex *p2, detail::Vertex *p3);
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
    void doDashStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count,
                      uint32_t dash_count, const float *dash_pattern);

    float strokeRadius() const
    {
        return mStrokeWidth * 0.5f;
    }

    void strokeCap();

    void strokeLineTo(const Point &curr);

    void strokeCubicTo(const Point &cnt1, const Point &cnt2, const Point &end);

    void strokeClose();

    void strokeJoin(const Point &dir);

    void strokeRound(const Point &prev, const Point &curr, const Point &center);

    void strokeMiter(const Point &prev, const Point &curr, const Point &center);

    void strokeBevel(const Point &prev, const Point &curr, const Point &center);
private:
    Array<float>    *mResPoints;
    Array<uint32_t> *mResIndices;
    float            mStrokeWidth = 1.f;
    float            mMiterLimit = 4.f;
    StrokeCap        mStrokeCap = StrokeCap::Square;
    StrokeJoin       mStrokeJoin = StrokeJoin::Bevel;
    State            mStrokeState = {};
};

class DashStroke
{
public:
    DashStroke(Array<PathCommand> *cmds, Array<Point> *pts, uint32_t dash_count, const float *dash_pattern);

    ~DashStroke() = default;

    void doStroke(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count);

private:
    void dashLineTo(const Point &pt);
    void dashCubicTo(const Point &pt1, const Point &pt2, const Point &pt3);

    void moveTo(const Point &pt);
    void lineTo(const Point &pt);
    void cubicTo(const Point &pt1, const Point &pt2, const Point &pt3);

private:
    Array<PathCommand> *mCmds;
    Array<Point>       *mPts;
    uint32_t            mDashCount;
    const float        *mDashPattern;
    float               mCurrLen;
    int32_t             mCurrIdx;
    bool                mCurOpGap;
    Point               mPtStart;
    Point               mPtCur;
};

}  // namespace tvg