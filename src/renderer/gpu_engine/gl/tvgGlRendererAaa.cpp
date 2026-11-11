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

#include "tvgGlRenderer.h"


static bool _axisAlignedRect(const RenderShape& shape, const Matrix& transform, const RenderRegion& viewport, GlCoverageMap& coverage)
{
    auto& path = shape.path;
    if (path.pts.count != 4 || path.cmds.count != 5 || shape.trimpath()) return false;
    if (path.cmds[0] != PathCommand::MoveTo) return false;
    for (uint32_t i = 1; i < 4; ++i) {
        if (path.cmds[i] != PathCommand::LineTo) return false;
    }
    if (path.cmds[4] != PathCommand::Close) return false;

    auto pt1 = path.pts[0] * transform;
    auto pt2 = path.pts[1] * transform;
    auto pt3 = path.pts[2] * transform;
    auto pt4 = path.pts[3] * transform;

    auto a = Point{pt1.x, pt3.y};
    auto b = Point{pt3.x, pt1.y};
    if (!((pt2.x == a.x && pt2.y == a.y && pt4.x == b.x && pt4.y == b.y) ||
          (pt2.x == b.x && pt2.y == b.y && pt4.x == a.x && pt4.y == a.y))) return false;

    // Clip in floating point before computing the integer pixel bounds.
    Point lo = {std::max(std::min(pt1.x, pt3.x), float(viewport.xMin)), std::max(std::min(pt1.y, pt3.y), float(viewport.yMin))};
    Point hi = {std::min(std::max(pt1.x, pt3.x), float(viewport.xMax)), std::min(std::max(pt1.y, pt3.y), float(viewport.yMax))};
    coverage.map.reset();
    coverage.status = CoverageStatus::Empty;
    if (hi.x > lo.x && hi.y > lo.y) {
        coverage.rect[0] = lo;
        coverage.rect[1] = hi;
        coverage.map.bounds = {int32_t(floorf(lo.x)), int32_t(floorf(lo.y)), int32_t(ceilf(hi.x)), int32_t(ceilf(hi.y))};
        coverage.format = GlCoverageFormat::Rect;
        coverage.status = CoverageStatus::Ready;
    }
    return true;
}


RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    auto shape = static_cast<GlShape*>(data);
    if (shape) {
        // A shared clip can still be read by earlier coverage tasks.
        if (clipper) doneCoverage();
        shape->done();
    } else {
        shape = new GlShape;
        shape->rshape = &rshape;
        flags = RenderUpdateFlag::All;
        shape->geometry = GlGeometry();
        coverageShapes.back(shape);
    }

    const auto& previous = shape->geometry.viewport;
    auto changed = !(previous == vport);
    auto update = changed || (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform | RenderUpdateFlag::Stroke | RenderUpdateFlag::Clip));
    auto updateFill = update || shape->geometry.fillRule != rshape.rule || shape->isClipper != clipper;
    shape->isClipper = clipper;
    // Keep stroke selection even when clipping removes its entire coverage.
    shape->clipStroke = rshape.stroke && !tvg::zero(rshape.stroke->width) && !tvg::zero(rshape.stroke->trim.begin - rshape.stroke->trim.end) &&
                        rshape.stroke->width * sqrtf(transform.e11 * transform.e11 + transform.e12 * transform.e12) > 0.0f;
    shape->geometry.transform(transform);
    shape->geometry.viewport = vport;
    shape->geometry.fillRule = rshape.rule;
    shape->geometry.fillWorld = true;
    shape->opacity = opacity;

    if (updateFill || update) {
        shape->clips = clips;
        shape->updateFill |= updateFill;
        shape->updateStroke |= update;
        ARRAY_FOREACH(p, clips) static_cast<GlShape*>(*p)->done();
        TaskScheduler::request(shape);
    }

    return shape;
}

void GlShape::run(TVG_UNUSED unsigned tid)
{
    if (updateFill) {
        if (fillCoverage.format == GlCoverageFormat::Rect) fillCoverage.format = GlCoverageFormat::Dense;
        if (isClipper || !clips.empty() || !_axisAlignedRect(*rshape, geometry.matrix, geometry.viewport, fillCoverage)) {
            fillCoverage.status = generateFillCoverage(*rshape, geometry.matrix, geometry.viewport, fillCoverage.map);
            if (!clips.empty() && fillCoverage.status == CoverageStatus::Ready) fillCoverage.status = GlRenderer::clipCoverage(fillCoverage.map, clips);
        }
    }
    if (updateStroke) {
        strokeCoverage.status = generateStrokeCoverage(*rshape, geometry.matrix, geometry.viewport, strokeCoverage.map);
        if (!clips.empty() && strokeCoverage.status == CoverageStatus::Ready) strokeCoverage.status = GlRenderer::clipCoverage(strokeCoverage.map, clips);
    }
    geometry.fillBBox = fillCoverage.map.bounds;
    geometry.strokeBBox = strokeCoverage.map.bounds;
    valid.fill = fillCoverage.status == CoverageStatus::Ready;
    valid.stroke = strokeCoverage.status == CoverageStatus::Ready;
}

void GlRenderer::doneCoverage()
{
    INLIST_FOREACH(coverageShapes, shape) shape->done();
}

bool GlRenderer::renderShape(RenderData data)
{
    auto shape = static_cast<GlShape*>(data);
    if (currentPass()->isEmpty() || (!shape->valid.fill && !shape->valid.stroke)) return true;

    auto bbox = shape->geometry.viewport;
    bbox.intersect(currentPass()->getViewport());
    if (bbox.invalid()) return true;

    int32_t drawDepth1 = 0, drawDepth2 = 0;
    if (shape->valid.fill) drawDepth1 = currentPass()->nextDrawDepth();
    if (shape->valid.stroke) drawDepth2 = currentPass()->nextDrawDepth();


    drawShapePaint(shape, drawDepth1, drawDepth2);

    return true;
}

void GlRenderer::drawPrimitive(GlShape& shape, const RenderColor& c, RenderUpdateFlag flag, int32_t depth)
{
    auto vp = currentPass()->getViewport();
    // geometry.viewport carries the fast-tracked clip bounds; the pass
    // viewport can still be the full framebuffer.
    auto viewBounds = shape.geometry.viewport;
    viewBounds.intersect(vp);
    if (viewBounds.invalid()) return;

    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    auto& coverage = stroke ? shape.strokeCoverage : shape.fillCoverage;
    if (!coverage.drawable()) return;
    auto bbox = coverage.map.bounds;
    bbox.intersect(viewBounds);
    if (bbox.invalid()) return;

    auto viewRegion = viewportRegion(vp, bbox);
    if (nativeBlend(mBlendMethod)) {
        RenderColor color = {c.r, c.g, c.b, MULTIPLY(c.a, shape.opacity)};
        mSolidBatch.draw(*this, coverage, color, depth, viewRegion, viewportRegion(vp, viewBounds));
        return;
    }

    GlRenderTarget* dstCopyFbo = nullptr;
    auto task = createPrimitiveTask(RT_Color, BlendSource::Solid, viewRegion, dstCopyFbo);
    bindCoverage(task, coverage);
    task->setDrawDepth(depth);

    auto a = MULTIPLY(c.a, shape.opacity);
    task->setVertexColor(c.r / 255.f, c.g / 255.f, c.b / 255.f, a / 255.f);
    task->setViewport(viewRegion);

    // Keep BlendRegion on the existing solid-shape blend UBO slot.
    bindBlendTarget(task, dstCopyFbo, viewRegion, 2);

    currentPass()->addRenderTask(task);
}

inline bool GlRenderer::primitiveBounds(const GlShape& shape, RenderUpdateFlag flag, RenderRegion& bounds)
{
    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    const auto& coverage = stroke ? shape.strokeCoverage : shape.fillCoverage;
    if (!coverage.drawable()) return false;
    bounds = coverage.map.bounds;
    return true;
}

inline float GlRenderer::bindGradientGeometry(GlRenderTask* task, const GlShape& shape, RenderUpdateFlag flag, Matrix& inverseFill)
{
    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    bindCoverage(task, stroke ? shape.strokeCoverage : shape.fillCoverage);
    // Coverage uses world coordinates for both fills and strokes.
    Matrix inverseShape;
    inverse(&shape.geometry.matrix, &inverseShape);
    inverseFill = inverseFill * inverseShape;
    return shape.opacity / 255.f;
}

inline void GlRenderer::submitGradient(GlRenderTask* task, TVG_UNUSED GlShape& shape, TVG_UNUSED RenderUpdateFlag flag, TVG_UNUSED bool opaque, TVG_UNUSED const RenderRegion& viewBounds)
{
    currentPass()->addRenderTask(task);
}

bool GlRenderer::bounds(RenderData data, Point* pt4, const Matrix& m)
{
    if (!data) return false;

    auto shape = static_cast<GlShape*>(data);
    return false;  // Coverage has no stroke mesh; use the shape bounds fallback.
}

bool GlRenderer::intersectsShape(RenderData data, const RenderRegion& region)
{
    auto shape = static_cast<GlShape*>(data);
    shape->done();
    if (shape->opacity == 0) return false;

    // Maps already include transforms and clipping, but are generated independently of paint.
    auto& rshape = *shape->rshape;
    const auto& fill = shape->fillCoverage;
    if (shape->valid.fill && (rshape.fill || rshape.color.a > 0)) {
        if (fill.format == GlCoverageFormat::Rect ? RenderRegion::intersect(fill.map.bounds, region).valid() :
                                                  intersectsCoverage(fill.map, region)) return true;
    }
    return shape->valid.stroke && (rshape.stroke->fill || rshape.stroke->color.a > 0) && intersectsCoverage(shape->strokeCoverage.map, region);
}

static const GlCoverageMap& _clipMap(RenderData data)
{
    auto shape = static_cast<GlShape*>(data);
    return shape->clipStroke ? shape->strokeCoverage : shape->fillCoverage;
}

CoverageStatus GlRenderer::clipCoverage(CoverageMap& map, const Array<RenderData>& clips)
{
    auto bounds = map.bounds;
    ARRAY_FOREACH(p, clips) {
        const auto& clip = _clipMap(*p);
        if (clip.status != CoverageStatus::Ready) {
            map.reset();
            return clip.status;
        }
        bounds.intersect(clip.map.bounds);
    }
    if (bounds.invalid()) {
        map.reset();
        return CoverageStatus::Empty;
    }

    ARRAY_FOREACH(p, clips) {
        auto status = tvg::clipCoverage(map, _clipMap(*p).map);
        if (status != CoverageStatus::Ready) return status;
    }
    return CoverageStatus::Ready;
}

void GlShape::destroy(GlRenderer& renderer)
{
    renderer.coverageAtlas.retire(fillCoverage.region);
    renderer.coverageAtlas.retire(strokeCoverage.region);
    renderer.coverageShapes.remove(this);
}

bool GlRenderer::stageCoverage(GlCoverageMap& coverage)
{
    if (coverage.format == GlCoverageFormat::Rect) {
        if (coverage.region.texture) coverageAtlas.retire(coverage.region);
        return true;
    }
    GlCoverageData packed;
    if (coverage.status != CoverageStatus::Ready || !packGlCoverage(coverage.map, coverageAtlas.maxTextureSize(), packed)) {
        coverageAtlas.retire(coverage.region);
        coverage.runCount = 0;
        return coverage.status == CoverageStatus::Empty;
    }

    // Queued draws must retain the previous bytes until sync().
    if (!mRenderPassStack.empty()) coverageAtlas.retire(coverage.region);
    if (!coverageAtlas.update(packed, coverage.region)) return false;
    coverage.format = packed.format;
    coverage.runCount = packed.runCount;
    return true;
}

void GlRenderer::bindCoverage(GlRenderTask* task, const GlCoverageMap& coverage)
{
    auto& rect = coverage.region.rect;
    auto runs = coverage.format == GlCoverageFormat::Runs;
    struct {
        float bounds[4];
        int32_t atlas[4];
    } info = {{}, {rect.sx(), rect.sy(), rect.sw(), int32_t(coverage.format)}};
    coverage.shaderBounds(info.bounds);

    task->setViewMatrix(currentPass()->getViewMatrix());
    task->useDrawArrays = true;
    task->indexCnt = runs ? coverage.runCount * 6 : 6;
    task->addBindResource(GlBindingResource{4, GlShaderUniformBlock::CoverageInfo, mGpuBuffer.getBufferId(),
                                          mGpuBuffer.push(&info, sizeof(info), true), sizeof(info)});
    task->addBindResource(GlBindingResource{1, coverage.region.texture, GlShaderUniform::CoverageTexture});
}

#include "tvgGlRendererPaint.h"
