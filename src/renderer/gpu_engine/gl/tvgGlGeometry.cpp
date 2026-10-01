/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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
#include "tvgGlRenderTask.h"
#include "tvgGlTessellator.h"

/************************************************************************/
/* GlIntersector                                                        */
/************************************************************************/

bool GlIntersector::intersect(const tvg::Array<tvg::RenderData>& clips, const Point& pt)
{
    ARRAY_FOREACH(c, clips) {
        auto clip = static_cast<const GlShape*>(*c);
        const auto& geometry = clip->geometry;
        if (clip->valid.fill) {
            auto p = geometry.fillWorld ? pt : pt * geometry.itransform();
            if (!geometry.fillBBox.inside(p) || !gpuPointInEvenOddMesh(p, geometry.fill.vertex.data, geometry.fill.index.data, geometry.fill.index.count)) return false;
        } else if (clip->valid.stroke) {
            auto p = pt * geometry.itransform();
            if (!geometry.strokeBBox.inside(p) || !gpuPointInAnyMesh(p, geometry.stroke.vertex.data, geometry.stroke.index.data, geometry.stroke.index.count)) return false;
        }
    }
    return true;
}

bool GlIntersector::intersect(const GlShape* shape, const RenderRegion& region)
{
    const auto& geometry = shape->geometry;
    auto validFill = shape->valid.fill && !geometry.fill.index.empty();
    auto validStroke = shape->valid.stroke && !geometry.stroke.index.empty();
    if (!validFill && !validStroke) return false;

    const auto& itransform = geometry.itransform();
    auto sizeX = region.sw();
    auto sizeY = region.sh();

    for (int32_t y = 0; y < sizeY; y++) {
        auto py = (y % 2 == 0) ? y : sizeY - y - sizeY % 2;
        for (int32_t x = 0; x < sizeX; x++) {
            Point pt{(float)x + region.min.x, (float)py + region.min.y};
            auto hit = false;
            if (validFill) {
                auto p = geometry.fillWorld ? pt : pt * itransform;
                hit = geometry.fillBBox.inside(p) && gpuPointInEvenOddMesh(p, geometry.fill.vertex.data, geometry.fill.index.data, geometry.fill.index.count);
            }
            if (!hit && validStroke) {
                auto p = pt * itransform;
                hit = geometry.strokeBBox.inside(p) && gpuPointInAnyMesh(p, geometry.stroke.vertex.data, geometry.stroke.index.data, geometry.stroke.index.count);
            }
            if (hit && intersect(shape->clips, pt)) return true;
        }
    }
    return false;
}

bool GlIntersector::intersect(const GlImage* image, const RenderRegion& region)
{
    if (image->geometry.fill.index.count < 6) return false;

    const auto& geometry = image->geometry;
    const auto& mesh = geometry.fill;
    Point triangle[6];

    for (uint32_t i = 0; i < 6; ++i) {
        auto idx = mesh.index[i] * 4;
        triangle[i] = Point{mesh.vertex[idx], mesh.vertex[idx + 1]};
    }

    auto sizeX = region.sw();
    auto sizeY = region.sh();
    for (int32_t y = 0; y < sizeY; y++) {
        auto py = (y % 2 == 0) ? y : sizeY - y - sizeY % 2;
        for (int32_t x = 0; x < sizeX; x++) {
            Point pt{(float)x + region.min.x, (float)py + region.min.y};
            if (gpuPointInQuad(pt, triangle) && intersect(image->clips, pt)) return true;
        }
    }
    return false;
}

/************************************************************************/
/* GlGeometry                                                           */
/************************************************************************/

bool GlGeometry::tesselatePrimitive(const RenderShape& rshape)
{
    if (rshape.primitive == RenderPrimitive::General || !tvg::zero(rshape.strokeWidth()) || rshape.trimpath()) return false;

    auto pts = rshape.path.pts.data;
    Point min, max;
    uint32_t count;

    if (rshape.primitive == RenderPrimitive::Rect) {
        // Appends retain the creation hint. Require the original rectangle
        // (MoveTo, three LineTo, Close) so the mesh cannot omit appended geometry.
        if (rshape.path.cmds.count != 5 || rshape.path.pts.count != 4) return false;
        Point corners[4];
        for (uint32_t i = 0; i < 4; ++i) corners[i] = pts[i] * matrix;
        auto u = corners[1] - corners[0];
        auto v = corners[3] - corners[0];
        auto area = cross(u, v);
        // Keep both altitudes safely above the optimizer's 0.25px tolerance.
        // This also excludes short edges, thin fills and degenerate transforms.
        if (!(area * area > 0.125f * std::max(dot(u, u), dot(v, v)))) return false;

        count = 4;
        fill.vertex.reserve(8);
        min = max = corners[0];
        for (uint32_t i = 0; i < 4; ++i) {
            fill.vertex.data[2 * i] = corners[i].x;
            fill.vertex.data[2 * i + 1] = corners[i].y;
            min = tvg::min(min, corners[i]);
            max = tvg::max(max, corners[i]);
        }
    } else {
        // Only the original ellipse (MoveTo, four CubicTo, Close; 13 points)
        // fits this mesh. Appended geometry must use general tessellation.
        if (rshape.path.cmds.count != 6 || rshape.path.pts.count != 13) return false;
        // Ordered cardinal points retain the winding, including affine loader transforms.
        auto center = (pts[0] + pts[6]) * 0.5f;
        auto u = pts[0] - center;
        auto v = pts[3] - center;
        u = {matrix.e11 * u.x + matrix.e12 * u.y, matrix.e21 * u.x + matrix.e22 * u.y};
        v = {matrix.e11 * v.x + matrix.e12 * v.y, matrix.e21 * v.x + matrix.e22 * v.y};
        center *= matrix;

        // Frobenius norm bounds the largest radius even under shear; |det| / R
        // bounds the smallest. Keep tiny/flat cubics on the optimizer's route.
        auto radius = sqrtf(dot(u, u) + dot(v, v));
        if (!(fabsf(cross(u, v)) > radius)) return false;

        count = (std::max(4u, gpuArcSegmentsCnt(MATH_2PI, radius)) + 3u) & ~3u;
        fill.vertex.reserve(count * 2);
        auto quadrant = count / 4;
        // Double precision keeps accumulated rotation error small for large ellipses.
        auto angle = 6.28318530717958647692 / count;
        auto cosStep = cos(angle);
        auto sinStep = sin(angle);
        auto x = 1.0;
        auto y = 0.0;
        for (uint32_t i = 0; i < quadrant; ++i) {
            auto a = u * x + v * y;
            auto b = v * x - u * y;
            fill.vertex.data[2 * i] = center.x + a.x;
            fill.vertex.data[2 * i + 1] = center.y + a.y;
            fill.vertex.data[2 * (quadrant + i)] = center.x + b.x;
            fill.vertex.data[2 * (quadrant + i) + 1] = center.y + b.y;
            fill.vertex.data[2 * (2 * quadrant + i)] = center.x - a.x;
            fill.vertex.data[2 * (2 * quadrant + i) + 1] = center.y - a.y;
            fill.vertex.data[2 * (3 * quadrant + i)] = center.x - b.x;
            fill.vertex.data[2 * (3 * quadrant + i) + 1] = center.y - b.y;
            auto nextX = x * cosStep - y * sinStep;
            y = x * sinStep + y * cosStep;
            x = nextX;
        }
        // Analytic bounds enclose the polygon without another vertex pass.
        Point extent{sqrtf(u.x * u.x + v.x * v.x), sqrtf(u.y * u.y + v.y * v.y)};
        min = center - extent;
        max = center + extent;
    }

    fill.vertex.count = count * 2;
    fill.index.reserve((count - 2) * 3);
    fill.index.count = 0;
    for (uint32_t i = 1; i < count - 1; ++i) {
        fill.index.data[fill.index.count++] = 0;
        fill.index.data[fill.index.count++] = i;
        fill.index.data[fill.index.count++] = i + 1;
    }
    fillBBox = {{int32_t(floorf(min.x)), int32_t(floorf(min.y))}, {int32_t(ceilf(max.x)), int32_t(ceilf(max.y))}};
    fillRule = rshape.rule;
    fillWorld = true;
    convex = true;
    optPath.clear();
    optStrokePath.clear();
    optPathThin = optPathSkipFill = false;
    stroke.clear();
    strokeBBox = {};
    strokeRenderWidth = 0.0f;
    return true;
}


void GlGeometry::prepare(const RenderShape& rshape)
{
    optPathThin = false;
    optPathSkipFill = false;
    optStrokePath.clear();

    auto strokeWidth = rshape.strokeWidth();
    auto localOut = (std::isfinite(strokeWidth) && !tvg::zero(strokeWidth)) ? &optStrokePath : nullptr;
    auto path = &rshape.path;

    if (rshape.trimpath()) {
        auto& trimmedPath = RenderPath::scratch();
        if (rshape.stroke->trim.trim(rshape.path, trimmedPath)) {
            path = &trimmedPath;
        } else {
            optPath.clear();
            return;
        }
    }

    GpuOptimizeResult result{&optPath, localOut};
    gpuOptimize(*path, result, matrix);
    optPathThin = result.thin;
    optPathSkipFill = result.skipFill;
}


bool GlGeometry::tesselateShape(const RenderShape& rshape, float& multiplier)
{
    fill.clear();
    fillBBox = {};
    fillWorld = true;
    convex = false;
    multiplier = 1.0f;

    // `skipFill` means the path stayed thin enough that even thin fallback should not draw a fill.
    if (optPathSkipFill) return false;

    // When the CTM scales a filled path so small that its device-space
    // World:  [========]     // normal-sized filled path
    // After CTM:  [.]        // thinner than 1 px in device space
    // Visible thin fills use stroke tessellation; sub-quantum fills are skipped earlier.
    if (optPathThin && tvg::zero(rshape.strokeWidth())) {
        if (tesselateThinFill(optPath)) {
            multiplier = MIN_GL_STROKE_ALPHA;
            fillRule = rshape.rule;
            return true;
        }
        return false;
    }

    // Handle normal shapes with more than 2 points
    BWTessellator bwTess{&fill};
    bwTess.tessellate(optPath);
    fillRule = rshape.rule;
    fillBBox = bwTess.bounds();
    convex = bwTess.convex;

    return true;
}


bool GlGeometry::tesselateThinFill(const RenderPath& path)
{
    stroke.clear();
    strokeBBox = {};
    if (path.pts.count < 2) return false;

    // Thin fills borrow stroke tessellation, but the generated stroke buffer is
    // temporary. It must be moved into fill before this function returns.
    Stroker stroker(&stroke, MIN_GL_STROKE_WIDTH, StrokeCap::Butt, StrokeJoin::Bevel);
    stroker.run(path); // path is already in world space.
    stroke.index.move(fill.index);
    stroke.vertex.move(fill.vertex);
    fillBBox = stroker.bounds();
    strokeRenderWidth = 0.0f;
    return true;
}


bool GlGeometry::tesselateStroke(const RenderShape& rshape)
{
    stroke.clear();
    strokeBBox = {};
    strokeRenderWidth = 0.0f;

    auto strokeWidth = rshape.strokeWidth();
    if (!std::isfinite(strokeWidth)) return false;
    if (tvg::zero(strokeWidth)) return false;

    auto qualityScale = scaling(matrix);
    if (!std::isfinite(qualityScale)) return false;
    if (tvg::zero(qualityScale)) return false;
    strokeRenderWidth = strokeWidth * qualityScale;
    if (!std::isfinite(strokeRenderWidth)) return false; // Invalid stroke render width when width and quality scale are finite but their product is not finite.

    // Keep stroke vertices local; GL applies model later through uViewMatrix.
    Stroker stroker(&stroke, strokeWidth, rshape.strokeCap(), rshape.strokeJoin(), rshape.strokeMiterlimit(), qualityScale);
    auto& dashed = RenderPath::scratch();
    if (gpuStrokeDash(rshape, dashed, nullptr)) stroker.run(dashed);
    else stroker.run(optStrokePath);
    strokeBBox = stroker.bounds();
    return true;
}


void GlGeometry::tesselateImage(const RenderSurface* image)
{
    fill.clear();
    fillWorld = true;
    strokeRenderWidth = 0.0f;
    fill.vertex.reserve(5 * 4);
    fill.index.reserve(6);

    auto leftTop = Point{0.f, 0.f} * matrix;
    auto leftBottom = Point{0.f, float(image->h)} * matrix;
    auto rightTop = Point{float(image->w), 0.f} * matrix;
    auto rightBottom = Point{float(image->w), float(image->h)} * matrix;

    auto appendVertex = [&](const Point& pt, float u, float v) {
        fill.vertex.push(pt.x);
        fill.vertex.push(pt.y);
        fill.vertex.push(u);
        fill.vertex.push(v);
    };

    appendVertex(leftTop, 0.f, 0.f);
    appendVertex(leftBottom, 0.f, 1.f);
    appendVertex(rightTop, 1.f, 0.f);
    appendVertex(rightBottom, 1.f, 1.f);

    fill.index.push(0);
    fill.index.push(1);
    fill.index.push(2);

    fill.index.push(2);
    fill.index.push(1);
    fill.index.push(3);

    fillBBox = gpuTransformBounds(RenderRegion{{0, 0}, {int32_t(image->w), int32_t(image->h)}}, matrix);
}

void GlGeometry::draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag) const
{
    auto buffer = ((flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke)) ? &stroke : &fill;
    auto vertexOffset = gpuBuffer->push(buffer->vertex.data, buffer->vertex.count * sizeof(float));
    auto indexOffset = gpuBuffer->pushIndex(buffer->index.data, buffer->index.count * sizeof(uint32_t));
    auto vertexBuffer = gpuBuffer->getBufferId();

    if (flag & RenderUpdateFlag::Image) {
        // image has two attribute: [pos, uv]
        task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset, GL_FLOAT, GL_FALSE, vertexBuffer});
        task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float), GL_FLOAT, GL_FALSE, vertexBuffer});
    } else {
        task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), vertexOffset, GL_FLOAT, GL_FALSE, vertexBuffer});
    }
    task->setDrawRange(indexOffset, buffer->index.count);
}

GlStencilMode GlGeometry::stencilMode(RenderUpdateFlag flag) const
{
    if (flag & RenderUpdateFlag::Stroke) return GlStencilMode::Stroke;
    if (flag & RenderUpdateFlag::GradientStroke) return GlStencilMode::Stroke;
    if (flag & RenderUpdateFlag::Image) return GlStencilMode::None;

    if (convex) return GlStencilMode::None;
    if (fillRule == FillRule::NonZero) return GlStencilMode::FillNonZero;
    if (fillRule == FillRule::EvenOdd) return GlStencilMode::FillEvenOdd;

    return GlStencilMode::None;
}

RenderRegion GlGeometry::bounds() const
{
    auto bbox = RenderRegion{};
    auto valid = false;

    if (!fill.index.empty()) {
        auto fill = fillWorld ? fillBBox : gpuTransformBounds(fillBBox, matrix);
        if (fill.valid()) {
            bbox = fill;
            valid = true;
        }
    }

    if (!stroke.index.empty()) {
        auto stroke = gpuTransformBounds(strokeBBox, matrix);
        if (stroke.valid()) {
            if (valid) bbox.add(stroke);
            else bbox = stroke;
        }
    }

    return bbox;
}
