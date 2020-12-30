/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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

#include <float.h>
#include "tvgGlGpuBuffer.h"
#include "tvgGlGeometry.h"

#define NORMALIZED_TOP_3D			1.0f
#define NORMALIZED_BOTTOM_3D	   -1.0f
#define NORMALIZED_LEFT_3D		   -1.0f
#define NORMALIZED_RIGHT_3D			1.0f

uint32_t GlGeometry::getPrimitiveCount()
{
    return mPrimitives.size();
}


const GlSize GlGeometry::getPrimitiveSize(const uint32_t primitiveIndex) const
{
    if (primitiveIndex >= mPrimitives.size()) return GlSize();
    GlSize size = mPrimitives[primitiveIndex].mBottomRight - mPrimitives[primitiveIndex].mTopLeft;
    return size;
}


bool GlGeometry::decomposeOutline(const Shape& shape)
{
    const PathCommand* cmds = nullptr;
    auto cmdCnt = shape.pathCommands(&cmds);

    Point* pts = nullptr;
    auto ptsCnt = shape.pathCoords(const_cast<const Point**>(&pts));

    //No actual shape data
    if (cmdCnt == 0 || ptsCnt == 0) return false;

    GlPrimitive* curPrimitive = nullptr;
    GlPoint min = { FLT_MAX, FLT_MAX };
    GlPoint max = { 0.0f, 0.0f };

    for (unsigned i = 0; i < cmdCnt; ++i) {
        switch (*(cmds + i)) {
            case PathCommand::Close: {
                if (curPrimitive) {
                    if (curPrimitive->mAAPoints.size() > 0 && (curPrimitive->mAAPoints[0].orgPt != curPrimitive->mAAPoints.back().orgPt)) {
                        curPrimitive->mAAPoints.push_back(curPrimitive->mAAPoints[0].orgPt);
                    }
                    curPrimitive->mIsClosed = true;
                }
                break;
            }
            case PathCommand::MoveTo: {
                if (curPrimitive) {
                    curPrimitive->mTopLeft = min;
                    curPrimitive->mBottomRight = max;
                    if (curPrimitive->mAAPoints.size() > 2 && (curPrimitive->mAAPoints[0].orgPt == curPrimitive->mAAPoints.back().orgPt)) {
                        curPrimitive->mIsClosed = true;
                    }
                }
                mPrimitives.push_back(GlPrimitive());
                curPrimitive = &mPrimitives.back();
            }
            __attribute__ ((fallthrough));
            case PathCommand::LineTo: {
                if (curPrimitive) addPoint(*curPrimitive, pts[0], min, max);
                pts++;
                break;
            }
            case PathCommand::CubicTo: {
                if (curPrimitive) decomposeCubicCurve(*curPrimitive, curPrimitive->mAAPoints.back().orgPt, pts[0], pts[1], pts[2], min, max);
                pts += 3;
                break;
            }
        }
    }
    if (curPrimitive) {
        curPrimitive->mTopLeft = min;
        curPrimitive->mBottomRight = max;
    }

    return true;
}

bool GlGeometry::generateAAPoints(TVG_UNUSED const Shape& shape, float strokeWd, RenderUpdateFlag flag)
{
    for (auto& shapeGeometry : mPrimitives) {
        vector<PointNormals> normalInfo;
        constexpr float blurDir = -1.0f;
        float antiAliasWidth = 1.0f;
        vector<SmoothPoint>& aaPts = shapeGeometry.mAAPoints;

        const float stroke = (strokeWd > 1) ? strokeWd - antiAliasWidth : strokeWd;

        size_t nPoints = aaPts.size();
        if (nPoints < 2) return false;

        normalInfo.resize(nPoints);

        size_t fPoint = 0;
        size_t sPoint = 1;
        for (size_t i = 0; i < nPoints - 1; ++i) {
            fPoint = i;
            sPoint = i + 1;
            if (shapeGeometry.mIsClosed && sPoint == nPoints - 1) sPoint = 0;
            GlPoint normal = getNormal(aaPts[fPoint].orgPt, aaPts[sPoint].orgPt);
            normalInfo[fPoint].normal1 = normal;
            normalInfo[sPoint].normal2 = normal;
        }
        if (shapeGeometry.mIsClosed) {
            normalInfo[nPoints - 1].normal1 = normalInfo[0].normal1;
            normalInfo[nPoints - 1].normal2 = normalInfo[0].normal2;
        } else {
            normalInfo[nPoints - 1].normal1 = normalInfo[nPoints - 1].normal2;
            normalInfo[0].normal2 = normalInfo[0].normal1;
        }

        for (uint32_t i = 0; i < nPoints; ++i) {
            normalInfo[i].normalF = normalInfo[i].normal1 + normalInfo[i].normal2;
            normalInfo[i].normalF.normalize();

            auto angle = dotProduct(normalInfo[i].normal2, normalInfo[i].normalF);
            if (angle != 0) normalInfo[i].normalF = normalInfo[i].normalF / angle;
            else normalInfo[i].normalF = GlPoint(0, 0);

            if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
                aaPts[i].fillOuterBlur = extendEdge(aaPts[i].orgPt, normalInfo[i].normalF, blurDir * stroke);
                aaPts[i].fillOuter = extendEdge(aaPts[i].fillOuterBlur, normalInfo[i].normalF, blurDir*antiAliasWidth);
            }
            if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
                aaPts[i].strokeOuterBlur = aaPts[i].orgPt;
                aaPts[i].strokeOuter = extendEdge(aaPts[i].strokeOuterBlur, normalInfo[i].normalF, blurDir*antiAliasWidth);
                aaPts[i].strokeInner = extendEdge(aaPts[i].strokeOuter, normalInfo[i].normalF, blurDir * stroke);
                aaPts[i].strokeInnerBlur = extendEdge(aaPts[i].strokeInner, normalInfo[i].normalF, blurDir*antiAliasWidth);
            }
        }
    }

    return true;
}

bool GlGeometry::tesselate(TVG_UNUSED const Shape& shape, float viewWd, float viewHt, RenderUpdateFlag flag)
{
    for (auto& shapeGeometry : mPrimitives) {
        constexpr float opaque = 1.0f;
        constexpr float transparent = 0.0f;
        vector<SmoothPoint>& aaPts = shapeGeometry.mAAPoints;
        VertexDataArray& fill = shapeGeometry.mFill;
        VertexDataArray& stroke = shapeGeometry.mStroke;

        if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
            uint32_t i = 0;
            for (size_t pt = 0; pt < aaPts.size(); ++pt) {
                addGeometryPoint(fill, aaPts[pt].fillOuter, viewWd, viewHt, opaque);
                if (i > 1) addTriangleFanIndices(i, fill.indices);
                ++i;
            }
            for (size_t pt = 1; pt < aaPts.size(); ++pt) {
                addGeometryPoint(fill, aaPts[pt - 1].fillOuterBlur, viewWd, viewHt, transparent);
                addGeometryPoint(fill, aaPts[pt - 1].fillOuter, viewWd, viewHt, opaque);
                addGeometryPoint(fill, aaPts[pt].fillOuterBlur, viewWd, viewHt, transparent);
                addGeometryPoint(fill, aaPts[pt].fillOuter, viewWd, viewHt, opaque);
                addQuadIndices(i, fill.indices);
            }
        }
        if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
            uint32_t i = 0;
            for (size_t pt = 1; pt < aaPts.size(); ++pt) {
                addGeometryPoint(stroke, aaPts[pt - 1].strokeOuter, viewWd, viewHt, opaque);
                addGeometryPoint(stroke, aaPts[pt - 1].strokeInner, viewWd, viewHt, opaque);
                addGeometryPoint(stroke, aaPts[pt].strokeOuter, viewWd, viewHt, opaque);
                addGeometryPoint(stroke, aaPts[pt].strokeInner, viewWd, viewHt, opaque);
                addQuadIndices(i, stroke.indices);
            }
            for (size_t pt = 1; pt < aaPts.size(); ++pt) {
                addGeometryPoint(stroke, aaPts[pt - 1].strokeOuterBlur, viewWd, viewHt, transparent);
                addGeometryPoint(stroke, aaPts[pt - 1].strokeOuter, viewWd, viewHt, opaque);
                addGeometryPoint(stroke, aaPts[pt].strokeOuterBlur, viewWd, viewHt, transparent);
                addGeometryPoint(stroke, aaPts[pt].strokeOuter, viewWd, viewHt, opaque);
                addQuadIndices(i, stroke.indices);
            }
            for (size_t pt = 1; pt < aaPts.size(); ++pt) {
                addGeometryPoint(stroke, aaPts[pt - 1].strokeInner, viewWd, viewHt, opaque);
                addGeometryPoint(stroke, aaPts[pt - 1].strokeInnerBlur, viewWd, viewHt, transparent);
                addGeometryPoint(stroke, aaPts[pt].strokeInner, viewWd, viewHt, opaque);
                addGeometryPoint(stroke, aaPts[pt].strokeInnerBlur, viewWd, viewHt, transparent);
                addQuadIndices(i, stroke.indices);
            }
        }
        aaPts.clear();
    }
    return true;
}


void GlGeometry::disableVertex(uint32_t location)
{
    GL_CHECK(glDisableVertexAttribArray(location));
    mGpuBuffer->unbind(GlGpuBuffer::Target::ARRAY_BUFFER);
}


void GlGeometry::draw(const uint32_t location, const uint32_t primitiveIndex, RenderUpdateFlag flag)
{
    if (primitiveIndex >= mPrimitives.size()) return;

    VertexDataArray& geometry = (flag == RenderUpdateFlag::Stroke) ? mPrimitives[primitiveIndex].mStroke : mPrimitives[primitiveIndex].mFill;

    updateBuffer(location, geometry);
    GL_CHECK(glDrawElements(GL_TRIANGLES, geometry.indices.size(), GL_UNSIGNED_INT, geometry.indices.data()));
}


void GlGeometry::updateBuffer(uint32_t location, const VertexDataArray& vertexArray)
{
    if (mGpuBuffer.get() == nullptr) mGpuBuffer = make_unique<GlGpuBuffer>();

    mGpuBuffer->updateBufferData(GlGpuBuffer::Target::ARRAY_BUFFER, vertexArray.vertices.size() * sizeof(VertexData), vertexArray.vertices.data());
    GL_CHECK(glVertexAttribPointer(location, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), 0));
    GL_CHECK(glEnableVertexAttribArray(location));
}


GlPoint GlGeometry::normalizePoint(const GlPoint& pt, float viewWd, float viewHt)
{
    GlPoint p;
    p.x = (pt.x * 2.0f / viewWd) - 1.0f;
    p.y = -1.0f * ((pt.y * 2.0f / viewHt) - 1.0f);
    return p;
}

void GlGeometry::addGeometryPoint(VertexDataArray& geometry, const GlPoint &pt, float viewWd, float viewHt, float opacity)
{
    VertexData tv = { normalizePoint(pt, viewWd, viewHt), opacity};
    geometry.vertices.push_back(tv);
}

GlPoint GlGeometry::getNormal(const GlPoint& p1, const GlPoint& p2)
{
    GlPoint normal = p1 - p2;
    normal.normalize();
    return GlPoint(-normal.y, normal.x);
}

float GlGeometry::dotProduct(const GlPoint& p1, const GlPoint& p2)
{
    return (p1.x * p2.x + p1.y * p2.y);
}

GlPoint GlGeometry::extendEdge(const GlPoint& pt, const GlPoint& normal, float scalar)
{
    GlPoint tmp = (normal * scalar);
    return (pt + tmp);
}

void GlGeometry::addPoint(GlPrimitive& primitve, const GlPoint& pt, GlPoint& min, GlPoint& max)
{
    if (pt.x < min.x) min.x = pt.x;
    if (pt.y < min.y) min.y = pt.y;
    if (pt.x > max.x) max.x = pt.x;
    if (pt.y > max.y) max.y = pt.y;

    primitve.mAAPoints.push_back(GlPoint(pt.x, pt.y));
}

void GlGeometry::addTriangleFanIndices(uint32_t& curPt, vector<uint32_t>& indices)
{
    indices.push_back(0);
    indices.push_back(curPt - 1);
    indices.push_back(curPt);
}

void GlGeometry::addQuadIndices(uint32_t& curPt, vector<uint32_t>& indices)
{
    indices.push_back(curPt);
    indices.push_back(curPt + 1);
    indices.push_back(curPt + 2);
    indices.push_back(curPt + 1);
    indices.push_back(curPt + 3);
    indices.push_back(curPt + 2);
    curPt += 4;
}

bool GlGeometry::isBezierFlat(const GlPoint& p1, const GlPoint& c1, const GlPoint& c2, const GlPoint& p2)
{
    GlPoint diff1 = (c1 * 3.0f) - (p1 * 2.0f) - p2;
    GlPoint diff2 = (c2 * 3.0f) - (p2 * 2.0f) - p1;

    diff1.mod();
    diff2.mod();

    if (diff1.x < diff2.x) diff1.x = diff2.x;
    if (diff1.y < diff2.y) diff1.y = diff2.y;
    if (diff1.x + diff1.y <= 0.5f) return true;
    return false;
}

void GlGeometry::decomposeCubicCurve(GlPrimitive& primitve, const GlPoint& pt1, const GlPoint& cpt1, const GlPoint& cpt2, const GlPoint& pt2, GlPoint& min, GlPoint& max)
{
    if (isBezierFlat(pt1, cpt1, cpt2, pt2)) {
        addPoint(primitve, pt2, min, max);
        return;
    }

    GlPoint p12 = (pt1 + cpt1) * 0.5f;
    GlPoint p23 = (cpt1 + cpt2) * 0.5f;
    GlPoint p34 = (cpt2 + pt2) * 0.5f;
    GlPoint p123 = (p12 + p23) * 0.5f;
    GlPoint p234 = (p23 + p34) * 0.5f;
    GlPoint p1234 = (p123 + p234) * 0.5f;

    decomposeCubicCurve(primitve, pt1, p12, p123, p1234, min, max);
    decomposeCubicCurve(primitve, p1234, p234, p34, pt2, min, max);
}


void GlGeometry::updateTransform(const RenderTransform* transform, float w, float h)
{
    if (transform) {
        mTransform.x = transform->x;
        mTransform.y = transform->y;
        mTransform.angle = transform->degree;
        mTransform.scale = transform->scale;
    }

    mTransform.w = w;
    mTransform.h = h;
    GET_TRANSFORMATION(NORMALIZED_LEFT_3D, NORMALIZED_TOP_3D, mTransform.matrix);
}

float* GlGeometry::getTransforMatrix()
{
    return mTransform.matrix;
}
