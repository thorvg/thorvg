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


RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    auto shape = static_cast<GlShape*>(data);
    if (!shape) {
        shape = new GlShape;
        shape->rshape = &rshape;
        flags = RenderUpdateFlag::All;
        shape->geometry = GlGeometry();
        coverageShapes.back(shape);
    }

    const auto& previous = shape->geometry.viewport;
    auto changed = !(previous == vport);
    auto update = changed || (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform | RenderUpdateFlag::Stroke | RenderUpdateFlag::Clip));
    auto updateFill = update || shape->geometry.fillRule != rshape.rule;
    // Keep stroke selection even when clipping removes its entire coverage.
    shape->clipStroke = rshape.stroke && !tvg::zero(rshape.stroke->width) && !tvg::zero(rshape.stroke->trim.begin - rshape.stroke->trim.end) &&
                        rshape.stroke->width * sqrtf(transform.e11 * transform.e11 + transform.e12 * transform.e12) > 0.0f;
    // Generate and clip CPU maps before staging their encoded atlas data.
    if (updateFill) {
        shape->fillCoverage.status = generateFillCoverage(rshape, transform, vport, shape->fillCoverage.map);
        if (!clips.empty() && shape->fillCoverage.status == CoverageStatus::Ready) shape->fillCoverage.status = clipCoverage(shape->fillCoverage.map, clips);
    }
    if (update) {
        shape->strokeCoverage.status = generateStrokeCoverage(rshape, transform, vport, shape->strokeCoverage.map);
        if (!clips.empty() && shape->strokeCoverage.status == CoverageStatus::Ready) shape->strokeCoverage.status = clipCoverage(shape->strokeCoverage.map, clips);
    }
    if (updateFill || !shape->fillCoverage.region.texture) {
        if (!stageCoverage(shape->fillCoverage)) shape->fillCoverage.status = CoverageStatus::Error;
    }
    if (update || !shape->strokeCoverage.region.texture) {
        if (!stageCoverage(shape->strokeCoverage)) shape->strokeCoverage.status = CoverageStatus::Error;
    }
    shape->geometry.transform(transform);
    shape->geometry.viewport = vport;
    shape->geometry.fillRule = rshape.rule;
    shape->geometry.fillWorld = true;
    shape->geometry.fillBBox = shape->fillCoverage.map.bounds;
    shape->geometry.strokeBBox = shape->strokeCoverage.map.bounds;
    shape->valid.fill = shape->fillCoverage.status == CoverageStatus::Ready;
    shape->valid.stroke = shape->strokeCoverage.status == CoverageStatus::Ready;
    shape->opacity = opacity;

    if (flags & RenderUpdateFlag::Clip) shape->clips = clips;

    return shape;
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
    if (!coverage.region.texture) return;
    auto bbox = coverage.map.bounds;
    bbox.intersect(viewBounds);
    if (bbox.invalid()) return;

    auto viewRegion = viewportRegion(vp, bbox);
    if (mBlendMethod == BlendMethod::Normal) {
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
    if (!coverage.region.texture) return false;
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

static bool _intersectsCoverage(const CoverageMap& map, const RenderRegion& region)
{
    auto bounds = RenderRegion::intersect(map.bounds, region);
    if (bounds.invalid()) return false;

    for (auto y = bounds.sy(); y < bounds.yMax; ++y) {
        auto row = map.data + size_t(y - map.bounds.sy()) * map.stride + (bounds.sx() - map.bounds.sx());
        for (auto x = bounds.sx(); x < bounds.xMax; ++x) {
            if (*row++) return true;
        }
    }
    return false;
}

bool GlRenderer::intersectsShape(RenderData data, const RenderRegion& region)
{
    auto shape = static_cast<GlShape*>(data);
    if (shape->opacity == 0) return false;

    // Maps already include transforms and clipping, but are generated independently of paint.
    auto& rshape = *shape->rshape;
    if (shape->valid.fill && (rshape.fill || rshape.color.a > 0) && _intersectsCoverage(shape->fillCoverage.map, region)) return true;
    return shape->valid.stroke && (rshape.stroke->fill || rshape.stroke->color.a > 0) && _intersectsCoverage(shape->strokeCoverage.map, region);
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

    auto source = map.data;
    auto data = source ? source : tvg::malloc<uint8_t>(size_t(bounds.w()) * bounds.h());
    if (!data) return CoverageStatus::Error;
    auto out = data;
    bool visible = false;
    for (auto y = bounds.sy(); y < bounds.yMax; ++y) {
        for (auto x = bounds.sx(); x < bounds.xMax; ++x) {
            auto value = source ? source[size_t(y - map.bounds.sy()) * map.stride + x - map.bounds.sx()] : uint8_t(255);
            ARRAY_FOREACH(p, clips) {
                const auto& clip = _clipMap(*p).map;
                value = MULTIPLY(value, clip.data[size_t(y - clip.bounds.sy()) * clip.stride + x - clip.bounds.sx()]);
            }
            *out++ = value;
            visible |= value != 0;
        }
    }
    map.data = data;
    map.bounds = bounds;
    map.stride = bounds.w();
    if (!visible) map.reset();
    return visible ? CoverageStatus::Ready : CoverageStatus::Empty;
}

void GlShape::destroy(GlRenderer& renderer)
{
    renderer.coverageAtlas.retire(fillCoverage.region);
    renderer.coverageAtlas.retire(strokeCoverage.region);
    renderer.coverageShapes.remove(this);
}

bool GlRenderer::stageCoverage(GlCoverageMap& coverage)
{
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
    auto& bounds = coverage.map.bounds;
    auto& rect = coverage.region.rect;
    auto runs = coverage.format == GlCoverageFormat::Runs;
    struct {
        float bounds[4];
        int32_t atlas[4];
    } info = {{float(bounds.sx()), float(bounds.sy()), float(bounds.w()), float(bounds.h())},
              {rect.sx(), rect.sy(), rect.sw(), runs ? 1 : 0}};

    task->setViewMatrix(currentPass()->getViewMatrix());
    task->useDrawArrays = true;
    task->indexCnt = runs ? coverage.runCount * 6 : 6;
    task->addBindResource(GlBindingResource{4, GlShaderUniformBlock::CoverageInfo, mGpuBuffer.getBufferId(),
                                          mGpuBuffer.push(&info, sizeof(info), true), sizeof(info)});
    bindCoverageTextures(task, coverage.region.texture, runs);
}

#include "tvgGlRendererPaint.h"
