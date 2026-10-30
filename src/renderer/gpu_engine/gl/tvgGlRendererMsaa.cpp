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


static constexpr float IDENTITY_VERTEX[] = {-1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f};

static Matrix _viewMatrix(const GlGeometry& geometry, const Matrix& viewMatrix, RenderUpdateFlag flag)
{
    // Most GL meshes are already in world space; local strokes fold model into
    // the shader's view matrix so the draw path can stay shared.
    if ((flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke)) return viewMatrix * geometry.matrix;
    return viewMatrix;
}

bool GlRenderer::skipClips(const Array<RenderData>& clips)
{
    if (clips.empty()) return false;

    // Clip geometries are prepared during update(), before this paint renders.
    // Clip masks are intersected, so one empty clip makes the paint invisible.
    ARRAY_FOREACH(p, clips) {
        auto clip = static_cast<GlShape*>(*p);
        auto flag = (clip->geometry.stroke.vertex.count > 0) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Path;
        if (!clip->geometry.drawable(flag)) return true;
    }

    return false;
}

RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    auto shape = static_cast<GlShape*>(data);
    if (!shape) {
        shape = new GlShape;
        shape->rshape = &rshape;
        flags = RenderUpdateFlag::All;
    }

    if (shape->prepare(flags, {float(surface.w), float(surface.h)}, opacity, clipper)) return shape;

    if (flags & RenderUpdateFlag::Path) shape->geometry = GlGeometry();

    shape->geometry.transform(transform);
    shape->geometry.viewport = vport;

    auto updateStroke = (flags & RenderUpdateFlag::Stroke) && rshape.stroke && std::isfinite(rshape.strokeWidth()) && !tvg::zero(rshape.strokeWidth()) && shape->geometry.optStrokePath.empty();
    auto updatePath = (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) || updateStroke;

    if (updatePath) {
        shape->geometry.prepare(rshape);
        shape->valid.fill = false;
        if (shape->geometry.tesselateShape(*(shape->rshape), shape->multiplier)) {
            shape->valid.fill = true;
        }
        shape->valid.stroke = shape->geometry.tesselateStroke(*(shape->rshape));
    }

    shape->opacity = float(opacity) * shape->multiplier;

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
    if (skipClips(shape->clips)) return true;  // TODO: move this in prepare() stage?

    int32_t drawDepth1 = 0, drawDepth2 = 0;
    if (shape->valid.fill) drawDepth1 = currentPass()->nextDrawDepth();
    if (shape->valid.stroke) drawDepth2 = currentPass()->nextDrawDepth();

    if (!shape->clips.empty()) drawClip(shape->clips, bbox);

    drawShapePaint(shape, drawDepth1, drawDepth2);

    return true;
}

void GlRenderer::drawPrimitive(GlShape& shape, const RenderColor& c, RenderUpdateFlag flag, int32_t depth)
{
    if (!shape.geometry.drawable(flag)) return;

    auto blendShape = (mBlendMethod != BlendMethod::Normal);
    auto vp = currentPass()->getViewport();
    // geometry.viewport carries the fast-tracked clip bounds; the pass
    // viewport can still be the full framebuffer.
    auto viewBounds = shape.geometry.viewport;
    viewBounds.intersect(vp);
    if (viewBounds.invalid()) return;

    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    auto bbox = stroke ? gpuTransformBounds(shape.geometry.strokeBBox, shape.geometry.matrix) : shape.geometry.fillBBox;
    bbox.intersect(viewBounds);
    if (bbox.invalid()) return;

    auto viewRegion = viewportRegion(vp, bbox);
    auto stencilMode = shape.geometry.stencilMode(flag);

    if (!stroke && !blendShape && stencilMode == GlStencilMode::None && shape.clips.empty()) {
        mSolidBatch.draw(*this, shape, c, depth, viewRegion, viewportRegion(vp, viewBounds));
        return;
    }

    if (!shape.clips.empty()) mSolidBatch.clear();

    GlRenderTarget* dstCopyFbo = nullptr;
    auto task = createPrimitiveTask(RT_Color, BlendSource::Solid, viewRegion, dstCopyFbo);
    auto viewMatrix = _viewMatrix(shape.geometry, currentPass()->getViewMatrix(), flag);

    task->setViewMatrix(viewMatrix);
    task->setDrawDepth(depth);

    auto a = MULTIPLY(c.a, shape.opacity);
    if (flag & RenderUpdateFlag::Stroke) {
        auto strokeWidth = shape.geometry.strokeRenderWidth;
        if (strokeWidth < MIN_GL_STROKE_WIDTH) {
            auto alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
            a = MULTIPLY(a, static_cast<uint8_t>(alpha * 255));
        }
    }
    RenderColor color = {c.r, c.g, c.b, a};

    if (stroke && (blendShape || a == 255)) {
        stencilMode = GlStencilMode::None;
    }

    if (stencilMode == GlStencilMode::None) task->setVertexColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
    task->setViewport(viewRegion);

    auto preparation = mStencilCoverBatch.prepare(*this, task, shape, flag, stencilMode, &color, viewBounds);
    // Keep BlendRegion on the existing solid-shape blend UBO slot.
    bindBlendTarget(task, dstCopyFbo, viewRegion, 2);

    mStencilCoverBatch.draw(currentPass(), task, preparation);
}

inline bool GlRenderer::primitiveBounds(const GlShape& shape, RenderUpdateFlag flag, RenderRegion& bounds)
{
    if (!shape.geometry.drawable(flag)) return false;
    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    bounds = stroke ? gpuTransformBounds(shape.geometry.strokeBBox, shape.geometry.matrix) : shape.geometry.fillBBox;
    return true;
}

inline float GlRenderer::bindGradientGeometry(GlRenderTask* task, const GlShape& shape, RenderUpdateFlag flag, Matrix& inverseFill)
{
    task->setViewMatrix(_viewMatrix(shape.geometry, currentPass()->getViewMatrix(), flag));
    // Gradient strokes use local mesh coordinates; fills use world coordinates.
    if (!(flag & RenderUpdateFlag::GradientStroke)) {
        Matrix inverseShape;
        inverse(&shape.geometry.matrix, &inverseShape);
        inverseFill = inverseFill * inverseShape;
    }
    auto alpha = shape.opacity / 255.f;
    if (flag & RenderUpdateFlag::GradientStroke) {
        auto strokeWidth = shape.geometry.strokeRenderWidth;
        if (strokeWidth < MIN_GL_STROKE_WIDTH) alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
    }
    return alpha;
}

inline void GlRenderer::submitGradient(GlRenderTask* task, GlShape& shape, RenderUpdateFlag flag, bool opaque, const RenderRegion& viewBounds)
{
    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    auto stencilMode = shape.geometry.stencilMode(flag);
    if (stroke && (mBlendMethod != BlendMethod::Normal || opaque)) stencilMode = GlStencilMode::None;
    auto preparation = mStencilCoverBatch.prepare(*this, task, shape, flag, stencilMode, nullptr, viewBounds);
    mStencilCoverBatch.draw(currentPass(), task, preparation);
}

bool GlRenderer::bounds(RenderData data, Point* pt4, const Matrix& m)
{
    if (!data) return false;

    auto shape = static_cast<GlShape*>(data);
    if (!shape->valid.stroke) return false;

    tvg::BBox bbox;
    bbox.init();
    auto& vertexes = shape->geometry.stroke.vertex;
    for (uint32_t i = 0; i < vertexes.count / 2; i++) {
        Point vert = {vertexes[i * 2 + 0], vertexes[i * 2 + 1]};
        vert *= m;
        bbox = {min(bbox.min, vert), max(bbox.max, vert)};
    }

    pt4[0] = bbox.min;
    pt4[1] = {bbox.max.x, bbox.min.y};
    pt4[2] = bbox.max;
    pt4[3] = {bbox.min.x, bbox.max.y};
    return true;
}

bool GlRenderer::intersectsShape(RenderData data, TVG_UNUSED const RenderRegion& region)
{
    if (!data) return false;
    auto shape = (GlShape*)data;
    if (shape->opacity == 0) return false;
    const auto& bbox = shape->geometry.bounds();
    if (region.intersected(bbox)) {
        if (region.contained(bbox)) return true;
        GlIntersector intersector;
        return intersector.intersect(shape, RenderRegion::intersect(region, bbox));
    }
    return false;
}

void GlRenderer::drawClip(Array<RenderData>& clips, const RenderRegion& viewBounds)
{
    if (viewBounds.invalid()) return;

    // Clip is a render boundary.
    mStencilCoverBatch.clear();

    // Clip masks must stay inside the target paint view bounds. Fast-tracked
    // Lottie clips narrow geometry.viewport while the pass viewport remains
    // full-size; using the pass viewport here can affect neighboring animations.
    auto identityVertexOffset = mGpuBuffer.push((void*)IDENTITY_VERTEX, sizeof(IDENTITY_VERTEX));
    auto identityIndexOffset = mGpuBuffer.pushIndex((void*)RECT_INDEX, sizeof(RECT_INDEX));

    Array<int32_t> clipDepths(clips.count);
    clipDepths.count = clips.count;

    for (int32_t i = clips.count - 1; i >= 0; i--) {
        clipDepths[i] = currentPass()->nextDrawDepth();
    }

    const auto& passViewport = currentPass()->getViewport();
    const auto& viewMatrix = currentPass()->getViewMatrix();
    const auto viewRegion = viewportRegion(passViewport, viewBounds);

    for (uint32_t i = 0; i < clips.count; ++i) {
        auto shape = static_cast<GlShape*>(clips[i]);
        auto flag = (shape->geometry.stroke.vertex.count > 0) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Path;

        auto clipTask = new GlRenderTask(mPrograms[RT_Stencil]);
        clipTask->setDrawDepth(clipDepths[i]);
        clipTask->setViewMatrix(_viewMatrix(shape->geometry, viewMatrix, flag));
        shape->geometry.draw(clipTask, &mGpuBuffer, flag);

        auto clipBounds = shape->geometry.bounds();
        clipBounds.intersect(viewBounds);
        clipTask->setViewport(viewportRegion(passViewport, clipBounds));

        auto maskTask = new GlRenderTask(mPrograms[RT_Stencil]);

        maskTask->setDrawDepth(clipDepths[i]);
        maskTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), identityVertexOffset, GL_FLOAT, GL_FALSE, mGpuBuffer.getBufferId()});
        maskTask->setDrawRange(identityIndexOffset, RECT_INDEX_COUNT);
        maskTask->setViewport(viewRegion);

        currentPass()->addRenderTask(new GlClipTask(clipTask, maskTask));
    }
}

#include "tvgGlRendererPaint.h"
