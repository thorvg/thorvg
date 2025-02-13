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

#ifndef _TVG_GL_TESSELLATOR_H_
#define _TVG_GL_TESSELLATOR_H_

#include <cstdint>

#include "tvgGlGeometry.h"
#include "tvgMath.h"

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
    Tessellator(Array<float>* points, Array<uint32_t>* indices);
    ~Tessellator();

    bool tessellate(const RenderShape *rshape, bool antialias = false);

    void tessellate(const Array<const RenderShape*> &shapes);

private:
    void visitShape(const PathCommand *cmds, uint32_t cmd_count, const Point *pts, uint32_t pts_count);

    void buildMesh();

    void mergeVertices();

    bool simplifyMesh();

    bool tessMesh();

    bool matchFillRule(int32_t winding);

    detail::Edge *makeEdge(detail::Vertex* p1, detail::Vertex* p2);

    bool checkIntersection(detail::Edge* left, detail::Edge* right, detail::ActiveEdgeList* ael,
                           detail::Vertex** current);

    bool splitEdge(detail::Edge* edge, detail::Vertex* v, detail::ActiveEdgeList* ael, detail::Vertex** current);

    bool intersectPairEdge(detail::Edge* left, detail::Edge* right, detail::ActiveEdgeList* ael,
                           detail::Vertex** current);

    detail::Polygon *makePoly(detail::Vertex* v, int32_t winding);

    void emitPoly(detail::MonotonePolygon* poly);

    void emitTriangle(detail::Vertex* p1, detail::Vertex* p2, detail::Vertex* p3);
private:
    FillRule fillRule = FillRule::Winding;
    std::unique_ptr<detail::ObjectHeap> pHeap;
    Array<detail::VertexList*> outlines;
    detail::VertexList* pMesh;
    detail::Polygon* pPolygon;
    Array<float>* resGlPoints;
    Array<uint32_t>* resIndices;
};

class Stroker final
{

    struct State
    {
        GlPoint firstPt = {};
        GlPoint firstPtDir = {};
        GlPoint prevPt = {};
        GlPoint prevPtDir = {};
    };
public:
    Stroker(Array<float>* points, Array<uint32_t>* indices, const Matrix& matrix);
    ~Stroker() = default;

    void stroke(const RenderShape *rshape);

    RenderRegion bounds() const;

private:
    void doTrimStroke(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count, bool simultaneous, float start, float end);
    void doStroke(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count);
    void doDashStroke(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count,
                      uint32_t dash_count, const float* dash_pattern, float dash_offset);

    float strokeRadius() const
    {
        return mStrokeWidth * 0.5f;
    }

    void strokeCap();

    void strokeLineTo(const GlPoint &curr);

    void strokeCubicTo(const GlPoint &cnt1, const GlPoint &cnt2, const GlPoint &end);

    void strokeClose();

    void strokeJoin(const GlPoint &dir);

    void strokeRound(const GlPoint &prev, const GlPoint &curr, const GlPoint &center);

    void strokeMiter(const GlPoint &prev, const GlPoint &curr, const GlPoint &center);

    void strokeBevel(const GlPoint &prev, const GlPoint &curr, const GlPoint &center);

    void strokeSquare(const GlPoint& p, const GlPoint& outDir);

    void strokeSquarePoint(const GlPoint& p);

    void strokeRound(const GlPoint& p, const GlPoint& outDir);

    void strokeRoundPoint(const GlPoint& p);
private:
    Array<float>* mResGlPoints;
    Array<uint32_t>* mResIndices;
    Matrix mMatrix;
    float mStrokeWidth = MIN_GL_STROKE_WIDTH;
    float mMiterLimit = 4.f;
    StrokeCap mStrokeCap = StrokeCap::Square;
    StrokeJoin mStrokeJoin = StrokeJoin::Bevel;
    State mStrokeState = {};
    GlPoint mLeftTop = {};
    GlPoint mRightBottom = {};
};

class DashStroke
{
public:
    DashStroke(Array<PathCommand>* cmds, Array<Point>* pts, uint32_t dash_count, const float* dash_pattern, float dash_offset);

    ~DashStroke() = default;

    void doStroke(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count);

private:
    void dashLineTo(const GlPoint &pt);
    void dashCubicTo(const GlPoint &pt1, const GlPoint &pt2, const GlPoint &pt3);

    void moveTo(const GlPoint &pt);
    void lineTo(const GlPoint &pt);
    void cubicTo(const GlPoint &pt1, const GlPoint &pt2, const GlPoint &pt3);

private:
    Array<PathCommand>* mCmds;
    Array<Point>* mPts;
    uint32_t mDashCount;
    const float* mDashPattern;
    float mDashOffset;
    float mCurrLen;
    int32_t mCurrIdx;
    bool mCurOpGap;
    GlPoint mPtStart;
    GlPoint mPtCur;
};

class PathTrim
{
public:
    PathTrim(): mCmds(), mPts() {}
    ~PathTrim() = default;
    bool trim(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count, float start, float end);
    const Array<PathCommand>& cmds() const { return mCmds; }
    const Array<Point>& pts() const { return mPts; }

private:
    float pathLength(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count);
    void trimPath(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count, float start, float end);

private:
    Array<PathCommand> mCmds;
    Array<Point> mPts;
};

class BWTessellator
{
public:
    BWTessellator(Array<float>* points, Array<uint32_t>* indices);
    ~BWTessellator() = default;

    void tessellate(const RenderShape *rshape, const Matrix& matrix);

    RenderRegion bounds() const;

private:
    uint32_t pushVertex(float x, float y);
    void pushTriangle(uint32_t a, uint32_t b, uint32_t c);

private:
    Array<float>* mResPoints;
    Array<uint32_t>* mResIndices;
    GlPoint mLeftTop = {};
    GlPoint mRightBottom = {};
};

}  // namespace tvg

#endif /* _TVG_GL_TESSELLATOR_H_ */
