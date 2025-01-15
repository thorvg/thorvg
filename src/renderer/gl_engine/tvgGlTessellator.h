/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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
#include "tvgGlCommon.h"

namespace tvg
{

class ObjectHeap;
struct Vertex;
struct Edge;
struct Polygon;
struct MonotonePolygon;
struct VertexList;
struct ActiveEdgeList;
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
    Edge *makeEdge(Vertex* p1, Vertex* p2);
    bool checkIntersection(Edge* left, Edge* right, ActiveEdgeList* ael, Vertex** current);
    bool splitEdge(Edge* edge, Vertex* v, ActiveEdgeList* ael, Vertex** current);
    bool intersectPairEdge(Edge* left, Edge* right, ActiveEdgeList* ael, Vertex** current);
    Polygon *makePoly(Vertex* v, int32_t winding);
    void emitPoly(MonotonePolygon* poly);
    void emitTriangle(Vertex* p1, Vertex* p2, Vertex* p3);

    FillRule fillRule = FillRule::NonZero;
    ObjectHeap* pHeap;
    Array<VertexList*> outlines;
    VertexList* pMesh;
    Polygon* pPolygon;
    Array<float>* resGlPoints;
    Array<uint32_t>* resIndices;
};

class Stroker final
{
    struct State
    {
        Point firstPt;
        Point firstPtDir;
        Point prevPt;
        Point prevPtDir;
    };
public:
    Stroker(Array<float>* points, Array<uint32_t>* indices, const Matrix& matrix);
    ~Stroker() = default;
    void stroke(const RenderShape *rshape);
    RenderRegion bounds() const;

private:
    void doStroke(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count);
    void doDashStroke(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count, uint32_t dash_count, const float* dash_pattern);

    float strokeRadius() const
    {
        return mStrokeWidth * 0.5f;
    }

    void strokeCap();
    void strokeLineTo(const Point& curr);
    void strokeCubicTo(const Point& cnt1, const Point& cnt2, const Point& end);
    void strokeClose();
    void strokeJoin(const Point& dir);
    void strokeRound(const Point& prev, const Point& curr, const Point& center);
    void strokeMiter(const Point& prev, const Point& curr, const Point& center);
    void strokeBevel(const Point& prev, const Point& curr, const Point& center);
    void strokeSquare(const Point& p, const Point& outDir);
    void strokeSquarePoint(const Point& p);
    void strokeRound(const Point& p, const Point& outDir);
    void strokeRoundPoint(const Point& p);

    Array<float>* mResGlPoints;
    Array<uint32_t>* mResIndices;
    Matrix mMatrix;
    float mStrokeWidth = MIN_GL_STROKE_WIDTH;
    float mMiterLimit = 4.f;
    StrokeCap mStrokeCap = StrokeCap::Square;
    StrokeJoin mStrokeJoin = StrokeJoin::Bevel;
    State mStrokeState = {};
    Point mLeftTop = {0.0f, 0.0f};
    Point mRightBottom = {0.0f, 0.0f};
};

class DashStroke
{
public:
    DashStroke(Array<PathCommand>* cmds, Array<Point>* pts, uint32_t dash_count, const float* dash_pattern);
    ~DashStroke() = default;
    void doStroke(const PathCommand* cmds, uint32_t cmd_count, const Point* pts, uint32_t pts_count);

private:
    void dashLineTo(const Point& pt);
    void dashCubicTo(const Point& pt1, const Point& pt2, const Point& pt3);
    void moveTo(const Point& pt);
    void lineTo(const Point& pt);
    void cubicTo(const Point& pt1, const Point& pt2, const Point& pt3);

    Array<PathCommand>* mCmds;
    Array<Point>* mPts;
    uint32_t mDashCount;
    const float* mDashPattern;
    float mCurrLen;
    int32_t mCurrIdx;
    bool mCurOpGap;
    Point mPtStart;
    Point mPtCur;
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

    Array<float>* mResPoints;
    Array<uint32_t>* mResIndices;
    Point mLeftTop = {0.0f, 0.0f};
    Point mRightBottom = {0.0f, 0.0f};
};

}  // namespace tvg

#endif /* _TVG_GL_TESSELLATOR_H_ */
