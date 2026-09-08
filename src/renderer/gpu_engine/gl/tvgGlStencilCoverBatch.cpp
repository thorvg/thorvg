/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

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

#include "tvgGlStencilCoverBatch.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlRenderPass.h"

// WebGL uses smaller batch caps to limit wasm memory. Native GL keeps larger caps
// for fewer render tasks. BATCH_REGION_RESET_THRESHOLD only releases cached bounds.
#ifdef __EMSCRIPTEN__
    static constexpr uint32_t BATCH_REGION_MAX_COUNT = 256;
    static constexpr uint32_t BATCH_REGION_RESET_THRESHOLD = 64;
    static constexpr uint32_t BATCH_VERTEX_MAX_COUNT = 65536;
    static constexpr uint32_t BATCH_INDEX_MAX_COUNT = 262144;
#else
    static constexpr uint32_t BATCH_REGION_MAX_COUNT = 512;
    static constexpr uint32_t BATCH_REGION_RESET_THRESHOLD = 128;
    static constexpr uint32_t BATCH_VERTEX_MAX_COUNT = 262144;
    static constexpr uint32_t BATCH_INDEX_MAX_COUNT = 1048576;
#endif

static constexpr uint32_t COVER_VERTEX_COUNT = 6;

struct StencilCoverVertex
{
    float x, y;
    tvg::RGBA color;
};

static void addStencilCoverPositionLayout(GlRenderTask* task, GlStageBuffer* gpuBuffer, const RenderRegion& bbox)
{
    float vertex[] = {
        float(bbox.min.x), float(bbox.min.y),
        float(bbox.min.x), float(bbox.max.y),
        float(bbox.max.x), float(bbox.min.y),
        float(bbox.max.x), float(bbox.min.y),
        float(bbox.min.x), float(bbox.max.y),
        float(bbox.max.x), float(bbox.max.y)};
    task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), gpuBuffer->pushAux(vertex, sizeof(vertex)), GL_FLOAT, GL_FALSE, gpuBuffer->getAuxBufferId()});
}

static void addStencilCoverSolidLayout(GlRenderTask* task, GlStageBuffer* gpuBuffer, const RenderRegion& bbox, const RenderColor& color)
{
    StencilCoverVertex vertex[] = {
        {float(bbox.min.x), float(bbox.min.y), color},
        {float(bbox.min.x), float(bbox.max.y), color},
        {float(bbox.max.x), float(bbox.min.y), color},
        {float(bbox.max.x), float(bbox.min.y), color},
        {float(bbox.min.x), float(bbox.max.y), color},
        {float(bbox.max.x), float(bbox.max.y), color}};
    auto vertexOffset = gpuBuffer->pushAux(vertex, sizeof(vertex));
    auto auxBuffer = gpuBuffer->getAuxBufferId();
    task->addVertexLayout(GlVertexLayout{0, 2, sizeof(StencilCoverVertex), vertexOffset, GL_FLOAT, GL_FALSE, auxBuffer});
    task->addVertexLayout(GlVertexLayout{1, 4, sizeof(StencilCoverVertex), vertexOffset + 2 * sizeof(float), GL_UNSIGNED_BYTE, GL_TRUE, auxBuffer});
}

static uint32_t* drawStencilGeometry(GlRenderTask* task, GlStageBuffer* gpuBuffer, const GlGeometryBuffer* buffer)
{
    auto vertexOffset = gpuBuffer->push(buffer->vertex.data, buffer->vertex.count * sizeof(float));

    uint32_t* indices = nullptr;
    auto indexOffset = gpuBuffer->reserveIndex(buffer->index.count * sizeof(uint32_t), reinterpret_cast<void**>(&indices));
    memcpy(indices, buffer->index.data, buffer->index.count * sizeof(uint32_t));

    task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), vertexOffset, GL_FLOAT, GL_FALSE, gpuBuffer->getBufferId()});
    task->setDrawRange(indexOffset, buffer->index.count);
    return indices;
}

static void buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex)
{
    for (uint32_t i = 0; i < src->index.count; ++i) out[i] = src->index[i] + baseVertex;
}

void GlStencilCoverBatch::clear()
{
    task = nullptr;
    if (bounds.reserved > BATCH_REGION_RESET_THRESHOLD) bounds.reset();
    else bounds.clear();
}

void GlStencilCoverBatch::draw(GlRenderPass& pass, GlStageBuffer& gpuBuffer, GlProgram* program, GlRenderTask* cover,
                              const GlShape& shape, RenderUpdateFlag flag, GlStencilMode mode,
                              const RenderRegion& viewBounds, const RenderColor* color)
{
    const auto& geometry = shape.geometry;
    auto stroke = (flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke);
    auto bbox = stroke ? geometry.strokeBBox : geometry.fillBBox;
    auto geometryBounds = stroke ? gpuTransformBounds(bbox, geometry.matrix) : bbox;
    geometryBounds.intersect(viewBounds);
    const auto& buffer = stroke ? geometry.stroke : geometry.fill;
    auto clipped = !shape.clips.empty();
    auto append = appendable(pass, mode, clipped, geometryBounds, buffer);

    if (color) addStencilCoverSolidLayout(cover, &gpuBuffer, bbox, *color);
    else addStencilCoverPositionLayout(cover, &gpuBuffer, bbox);
    cover->useDrawArrays = true;
    cover->indexCnt = COVER_VERTEX_COUNT;

    auto stencil = new GlRenderTask(program);
    stencil->setViewMatrix(cover->viewMatrix);
    stencil->drawDepth = cover->drawDepth;
    auto indices = drawStencilGeometry(stencil, &gpuBuffer, &buffer);
    stencil->setViewport(cover->viewport);

    if (append) {
        if (!mergeCover(cover, viewBounds)) task->coverTasks.push(cover);
        if (!merge(stencil, viewBounds, buffer, indices)) {
            task->stencilTasks.push(stencil);
            setStencilMergeTarget(stencil, buffer);
        }
    } else {
        clear();
        task = new GlStencilCoverTask(stencil, cover, mode);
        pass.addRenderTask(task);
        this->pass = &pass;
        this->clipped = clipped;
        const auto& viewport = pass.getViewport();
        ySorted = viewport.sh() > viewport.sw();
        setStencilMergeTarget(stencil, buffer);
    }
    this->viewBounds = viewBounds;
    addBounds(geometryBounds);
}

bool GlStencilCoverBatch::intersects(const RenderRegion& bounds) const
{
    auto min = ySorted ? bounds.min.y : bounds.min.x;
    auto max = ySorted ? bounds.max.y : bounds.max.x;
    ARRAY_FOREACH(p, this->bounds) {
        auto pMin = ySorted ? (*p).min.y : (*p).min.x;
        if ((ySorted ? (*p).max.y : (*p).max.x) <= min) continue;
        if (pMin >= max) break;
        if ((*p).intersected(bounds)) return true;
    }

    return false;
}

void GlStencilCoverBatch::addBounds(const RenderRegion& bounds)
{
    auto p = this->bounds.count;
    auto min = ySorted ? bounds.min.y : bounds.min.x;
    this->bounds.grow(1);
    while (p > 0 && (ySorted ? this->bounds[p - 1].min.y : this->bounds[p - 1].min.x) > min) {
        this->bounds[p] = this->bounds[p - 1];
        --p;
    }
    this->bounds[p] = bounds;
    ++this->bounds.count;
}

bool GlStencilCoverBatch::appendable(const GlRenderPass& pass, GlStencilMode mode, bool clipped, const RenderRegion& bounds, const GlGeometryBuffer& buffer) const
{
    if (!task || this->pass != &pass || pass.lastTask() != task) return false;
    if (task->stencilMode != mode || this->clipped != clipped) return false;
    if (this->bounds.count >= BATCH_REGION_MAX_COUNT) return false;
    auto incomingVertexCount = buffer.vertex.count / 2;
    auto incomingIndexCount = buffer.index.count;
    if (incomingVertexCount > BATCH_VERTEX_MAX_COUNT || vertexCount > BATCH_VERTEX_MAX_COUNT - incomingVertexCount) return false;
    if (incomingIndexCount > BATCH_INDEX_MAX_COUNT || stencilTask->indexCnt > BATCH_INDEX_MAX_COUNT - incomingIndexCount) return false;
    return !intersects(bounds);
}

void GlStencilCoverBatch::setStencilMergeTarget(GlRenderTask* stencil, const GlGeometryBuffer& buffer)
{
    stencilTask = stencil;
    vertexCount = buffer.vertex.count / 2;
}

bool GlStencilCoverBatch::merge(GlRenderTask* stencil, const RenderRegion& viewBounds, const GlGeometryBuffer& buffer, uint32_t* indices)
{
    if (!(this->viewBounds == viewBounds)) return false;
    if (stencilTask->program != stencil->program) return false;
    if (!(stencilTask->viewMatrix == stencil->viewMatrix)) return false;

    const auto& layout = stencilTask->vertexLayout[0];
    const auto& appendLayout = stencil->vertexLayout[0];
    if (layout.offset + vertexCount * layout.stride != appendLayout.offset) return false;
    auto expectedIndexOffset = stencilTask->indexOffset + stencilTask->indexCnt * sizeof(uint32_t);
    if (stencil->indexOffset != expectedIndexOffset) return false;

    buildIndices(indices, &buffer, vertexCount);
    vertexCount += buffer.vertex.count / 2;
    stencilTask->indexCnt += stencil->indexCnt;
    stencilTask->drawDepth = stencil->drawDepth;
    stencilTask->viewport.add(stencil->viewport);

    delete stencil;
    return true;
}

bool GlStencilCoverBatch::mergeCover(GlRenderTask* cover, const RenderRegion& viewBounds)
{
    if (!(this->viewBounds == viewBounds)) return false;

    auto dst = task->coverTasks.last();
    if (dst->program != cover->program) return false;
    if (!dst->bindResources.empty() || !cover->bindResources.empty()) return false;
    if (!(dst->viewMatrix == cover->viewMatrix)) return false;
    auto dstEnd = dst->vertexLayout[0].offset + dst->indexCnt * sizeof(StencilCoverVertex);
    if (dstEnd != cover->vertexLayout[0].offset) return false;

    dst->indexCnt += cover->indexCnt;
    dst->viewport.add(cover->viewport);
    dst->drawDepth = cover->drawDepth;

    delete cover;
    return true;
}
