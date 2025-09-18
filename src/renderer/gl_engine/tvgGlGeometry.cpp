/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgGlCommon.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlTessellator.h"
#include "tvgGlRenderTask.h"

bool GlIntersector::isPointInTriangle(const Point& p, const Point& a, const Point& b, const Point& c)
{
    auto d1 = tvg::cross(p - a, p - b);
    auto d2 = tvg::cross(p - b, p - c);
    auto d3 = tvg::cross(p - c, p - a);
    auto has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    auto has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

bool GlIntersector::isPointInImage(const Point& p, const GlGeometryBuffer& mesh, const Matrix& tr)
{
    for (uint32_t i = 0; i < mesh.index.count; i += 3) {
        auto p0 = Point{mesh.vertex[mesh.index[i+0]*4+0], mesh.vertex[mesh.index[i+0]*4+1]} * tr;
        auto p1 = Point{mesh.vertex[mesh.index[i+1]*4+0], mesh.vertex[mesh.index[i+1]*4+1]} * tr;
        auto p2 = Point{mesh.vertex[mesh.index[i+2]*4+0], mesh.vertex[mesh.index[i+2]*4+1]} * tr;
        if (isPointInTriangle(p, p0, p1, p2)) return true;
    }
    return false;
}


// triangle list
bool GlIntersector::isPointInTris(const Point& p, const GlGeometryBuffer& mesh, const Matrix& tr)
{
    for (uint32_t i = 0; i < mesh.index.count; i += 3) {
        auto p0 = Point{mesh.vertex[mesh.index[i+0]*2+0], mesh.vertex[mesh.index[i+0]*2+1]} * tr;
        auto p1 = Point{mesh.vertex[mesh.index[i+1]*2+0], mesh.vertex[mesh.index[i+1]*2+1]} * tr;
        auto p2 = Point{mesh.vertex[mesh.index[i+2]*2+0], mesh.vertex[mesh.index[i+2]*2+1]} * tr;
        if (isPointInTriangle(p, p0, p1, p2)) return true;
    }
    return false;
}


// even-odd triangle list
bool GlIntersector::isPointInMesh(const Point& p, const GlGeometryBuffer& mesh, const Matrix& tr)
{
    uint32_t crossings = 0;
    for (uint32_t i = 0; i < mesh.index.count; i += 3) {
        Point triangle[3] = {
            Point{mesh.vertex[mesh.index[i+0]*2+0], mesh.vertex[mesh.index[i+0]*2+1]} * tr,
            Point{mesh.vertex[mesh.index[i+1]*2+0], mesh.vertex[mesh.index[i+1]*2+1]} * tr,
            Point{mesh.vertex[mesh.index[i+2]*2+0], mesh.vertex[mesh.index[i+2]*2+1]} * tr
        };
        for (uint32_t j = 0; j < 3; j++) {
            auto p1 = triangle[j];
            auto p2 = triangle[(j + 1) % 3];
            if (p1.y == p2.y) continue;
            if (p1.y > p2.y) std::swap(p1, p2);
            if ((p.y > p1.y) && (p.y <= p2.y)) {
                auto intersectionX = (p2.x - p1.x) * (p.y - p1.y) / (p2.y - p1.y) + p1.x;
                if (intersectionX > p.x) crossings++;
            }
        }
    }
    return (crossings % 2) == 1;
}


bool GlIntersector::intersectClips(const Point& pt, const tvg::Array<tvg::RenderData>& clips)
{
    for (uint32_t i = 0; i < clips.count; i++) {
        auto clip = (GlShape*)clips[i];
        if (!isPointInMesh(pt, clip->geometry.fill, clip->geometry.matrix)) return false;
    }
    return true;
}


bool GlIntersector::intersectShape(const RenderRegion region, const GlShape* shape)
{
    if (!shape || ((shape->geometry.fill.index.count == 0) && (shape->geometry.stroke.index.count == 0))) return false;
    auto sizeX = region.sw();
    auto sizeY = region.sh();

    for (int32_t y = 0; y <= sizeY; y++) {
        for (int32_t x = 0; x <= sizeX; x++) {
            Point pt{(float)x + region.min.x, (float)y + region.min.y};
            if (y % 2 == 1) pt.y = (float)sizeY - y - sizeY % 2 + region.min.y;
            if (intersectClips(pt, shape->clips)) {
                if (shape->validFill && isPointInMesh(pt, shape->geometry.fill, shape->geometry.matrix)) return true;
                if (shape->validStroke && isPointInTris(pt, shape->geometry.stroke, shape->geometry.matrix)) return true;
            }
        }
    }
    return false;
}


bool GlIntersector::intersectImage(const RenderRegion region, const GlShape* image)
{
    if (image) {
        auto sizeX = region.sw();
        auto sizeY = region.sh();
        for (int32_t y = 0; y <= sizeY; y++) {
            for (int32_t x = 0; x <= sizeX; x++) {
                Point pt{(float) x + region.min.x, (float) y + region.min.y};
                if (y % 2 == 1) pt.y = (float) sizeY - y - sizeY % 2 + region.min.y;
                if (intersectClips(pt, image->clips)) {
                    if (isPointInImage(pt, image->geometry.fill, image->geometry.matrix)) return true;
                }
            }
        }
    }
    return false;
}


bool GlGeometry::tesselateShape(const RenderShape& rshape)
{
    fill.clear();
    BWTessellator bwTess{&fill};
    if (rshape.trimpath()) {
        RenderPath trimmedPath;
        if (rshape.stroke->trim.trim(rshape.path, trimmedPath)) {
            bwTess.tessellate(trimmedPath, matrix);
        }
    } else {
        bwTess.tessellate(rshape.path, matrix);
    }

    fillRule = rshape.rule;
    bounds = bwTess.bounds();

    return true;
}


bool GlGeometry::tesselateStroke(const RenderShape& rshape)
{
    stroke.clear();
    auto strokeWidth = 0.0f;
    if (isinf(matrix.e11)) {
        strokeWidth = rshape.strokeWidth() * scaling(matrix);
        if (strokeWidth <= MIN_GL_STROKE_WIDTH) strokeWidth = MIN_GL_STROKE_WIDTH;
        strokeWidth = strokeWidth / matrix.e11;
    } else {
        strokeWidth = rshape.strokeWidth();
    }
    //run stroking only if it's valid
    if (!tvg::zero(strokeWidth)) {
        Stroker stroker(&stroke, strokeWidth);
        stroker.run(rshape, matrix);
        bounds = stroker.bounds();
        return true;
    }
    return false;
}


void GlGeometry::tesselateImage(const RenderSurface* image)
{
    fill.clear();
    fill.vertex.reserve(5 * 4);
    fill.index.reserve(6);

    auto left = 0.f;
    auto top = 0.f;
    auto right = float(image->w);
    auto bottom = float(image->h);

    // left top point
    fill.vertex.push(left);
    fill.vertex.push(top);

    fill.vertex.push(0.f);
    fill.vertex.push(1.f);
    // left bottom point
    fill.vertex.push(left);
    fill.vertex.push(bottom);

    fill.vertex.push(0.f);
    fill.vertex.push(0.f);
    // right top point
    fill.vertex.push(right);
    fill.vertex.push(top);

    fill.vertex.push(1.f);
    fill.vertex.push(1.f);
    // right bottom point
    fill.vertex.push(right);
    fill.vertex.push(bottom);

    fill.vertex.push(1.f);
    fill.vertex.push(0.f);

    fill.index.push(0);
    fill.index.push(1);
    fill.index.push(2);

    fill.index.push(2);
    fill.index.push(1);
    fill.index.push(3);

    bounds = {{0, 0}, {int32_t(image->w), int32_t(image->h)}};
}


bool GlGeometry::draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag)
{
    if (flag == RenderUpdateFlag::None) return false;

    auto buffer = ((flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke)) ? &stroke : &fill;
    if (buffer->index.empty()) return false;

    auto vertexOffset = gpuBuffer->push(buffer->vertex.data, buffer->vertex.count * sizeof(float));
    auto indexOffset = gpuBuffer->pushIndex(buffer->index.data, buffer->index.count * sizeof(uint32_t));

    // vertex layout
    if (flag & RenderUpdateFlag::Image) {
        // image has two attribute: [pos, uv]
        task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset});
        task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float)});
    } else {
        task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), vertexOffset});
    }
    task->setDrawRange(indexOffset, buffer->index.count);
    return true;
}


GlStencilMode GlGeometry::getStencilMode(RenderUpdateFlag flag)
{
    if (flag & RenderUpdateFlag::Stroke) return GlStencilMode::Stroke;
    if (flag & RenderUpdateFlag::GradientStroke) return GlStencilMode::Stroke;
    if (flag & RenderUpdateFlag::Image) return GlStencilMode::None;

    if (fillRule == FillRule::NonZero) return GlStencilMode::FillNonZero;
    if (fillRule == FillRule::EvenOdd) return GlStencilMode::FillEvenOdd;

    return GlStencilMode::None;
}


RenderRegion GlGeometry::getBounds() const
{
    if (tvg::identity(&matrix)) return bounds;

    auto lt = Point{float(bounds.min.x), float(bounds.min.y)} * matrix;
    auto lb = Point{float(bounds.min.x), float(bounds.max.y)} * matrix;
    auto rt = Point{float(bounds.max.x), float(bounds.min.y)} * matrix;
    auto rb = Point{float(bounds.max.x), float(bounds.max.y)} * matrix;

    auto left = min(min(lt.x, lb.x), min(rt.x, rb.x));
    auto top = min(min(lt.y, lb.y), min(rt.y, rb.y));
    auto right = max(max(lt.x, lb.x), max(rt.x, rb.x));
    auto bottom = max(max(lt.y, lb.y), max(rt.y, rb.y));

    auto bounds = RenderRegion {{int32_t(floor(left)), int32_t(floor(top))}, {int32_t(ceil(right)), int32_t(ceil(bottom))}};
    if (bounds.valid()) return bounds;
    return this->bounds;

}