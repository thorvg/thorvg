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


void GlRenderer::SolidBatch::draw(GlRenderer& renderer, GlShape& sdata, const RenderColor& color, int32_t depth, const RenderRegion& viewRegion)
{
    auto pass = renderer.currentPass();
    auto buffer = &sdata.geometry.fill;
    if (buffer->index.empty()) return;

    auto vertexCount = buffer->vertex.count / 2;
    auto indexCount = buffer->index.count;
    if (vertexCount == 0 || indexCount == 0) return;

    if (!appendable(renderer, pass, depth)) {
        emitSingle(renderer, pass, sdata, color, depth, viewRegion, vertexCount, indexCount);
        return;
    }

    auto batchColor = GlRenderer::solidColor(sdata, color, RenderUpdateFlag::Color);
    if (!promoted) {
        if (promote(renderer, pass, batchColor, depth, viewRegion, buffer, vertexCount, indexCount)) return;
        emitSingle(renderer, pass, sdata, color, depth, viewRegion, vertexCount, indexCount);
        return;
    }

    append(renderer, batchColor, viewRegion, buffer, vertexCount, indexCount);
}


bool GlRenderer::SolidBatch::appendable(const GlRenderer& renderer, const GlRenderPass* pass, int32_t depth) const
{
    if (!task) return false;
    if (this->pass != pass) return false;
    if (this->depth != depth) return false;
    if (pass->lastTask() != task) return false;
    return task->getProgram() == renderer.mPrograms[RT_Color];
}


void GlRenderer::SolidBatch::buildVertices(SolidBatchVertex* out, const GlGeometryBuffer* src, uint32_t count, const RenderColor& color)
{
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = {
            src->vertex[i * 2 + 0],
            src->vertex[i * 2 + 1],
            color.r, color.g, color.b, color.a
        };
    }
}


void GlRenderer::SolidBatch::buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex)
{
    for (uint32_t i = 0; i < src->index.count; ++i) out[i] = src->index[i] + baseVertex;
}


void GlRenderer::SolidBatch::emitSingle(GlRenderer& renderer, GlRenderPass* pass, GlShape& sdata, const RenderColor& color, int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount)
{
    auto drawTask = new GlRenderTask(renderer.mPrograms[RT_Color]);
    drawTask->setViewMatrix(pass->getViewMatrix());
    drawTask->setDrawDepth(depth);

    if (!sdata.geometry.draw(drawTask, &renderer.mGpuBuffer, RenderUpdateFlag::Color)) {
        delete drawTask;
        clear();
        return;
    }

    auto taskColor = GlRenderer::solidColor(sdata, color, RenderUpdateFlag::Color);
    drawTask->setVertexColor(taskColor.r / 255.f, taskColor.g / 255.f, taskColor.b / 255.f, taskColor.a / 255.f);
    drawTask->setViewport(viewRegion);
    pass->addRenderTask(drawTask);

    this->pass = pass;
    task = drawTask;
    shape = &sdata;
    this->color = color;
    flag = RenderUpdateFlag::Color;
    this->depth = depth;
    this->vertexCount = vertexCount;
    indexOffset = 0;
    this->indexCount = indexCount;
    promoted = false;
}


bool GlRenderer::SolidBatch::promote(GlRenderer& renderer, GlRenderPass* pass, const RenderColor& solidColor, int32_t depth, const RenderRegion& viewRegion, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount)
{
    if (!shape) return false;

    auto firstBuffer = &shape->geometry.fill;
    auto firstVertexCount = firstBuffer->vertex.count / 2;
    auto firstIndexCount = firstBuffer->index.count;
    if (firstVertexCount == 0 || firstIndexCount == 0) return false;

    auto firstColor = GlRenderer::solidColor(*shape, color, flag);
    auto totalVertexCount = firstVertexCount + vertexCount;
    auto totalIndexCount = firstIndexCount + indexCount;

    SolidBatchVertex* vertices = nullptr;
    uint32_t* indices = nullptr;
    auto vertexOffset = renderer.mGpuBuffer.reserve(totalVertexCount * sizeof(SolidBatchVertex), reinterpret_cast<void**>(&vertices));
    auto idxOffset = renderer.mGpuBuffer.reserveIndex(totalIndexCount * sizeof(uint32_t), reinterpret_cast<void**>(&indices));

    buildVertices(vertices, firstBuffer, firstVertexCount, firstColor);
    buildVertices(vertices + firstVertexCount, buffer, vertexCount, solidColor);
    buildIndices(indices, firstBuffer, 0);
    buildIndices(indices + firstIndexCount, buffer, firstVertexCount);

    auto drawTask = new GlRenderTask(renderer.mPrograms[RT_Color]);
    drawTask->setViewMatrix(pass->getViewMatrix());
    drawTask->setDrawDepth(depth);
    drawTask->addVertexLayout(GlVertexLayout{0, 2, sizeof(SolidBatchVertex), vertexOffset});
    drawTask->addVertexLayout(GlVertexLayout{1, 4, sizeof(SolidBatchVertex), vertexOffset + 2 * sizeof(float), GL_UNSIGNED_BYTE, GL_TRUE});
    drawTask->setDrawRange(idxOffset, totalIndexCount);

    auto merged = task->getViewport();
    merged.add(viewRegion);
    drawTask->setViewport(merged);

    auto oldTask = pass->takeLastTask();
    if (oldTask) delete oldTask;
    pass->addRenderTask(drawTask);

    task = drawTask;
    shape = nullptr;
    this->vertexCount = totalVertexCount;
    indexOffset = idxOffset;
    this->indexCount = totalIndexCount;
    promoted = true;
    return true;
}


void GlRenderer::SolidBatch::append(GlRenderer& renderer, const RenderColor& solidColor, const RenderRegion& viewRegion, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount)
{
    SolidBatchVertex* vertices = nullptr;
    uint32_t* indices = nullptr;
    renderer.mGpuBuffer.reserve(vertexCount * sizeof(SolidBatchVertex), reinterpret_cast<void**>(&vertices));
    renderer.mGpuBuffer.reserveIndex(indexCount * sizeof(uint32_t), reinterpret_cast<void**>(&indices));

    buildVertices(vertices, buffer, vertexCount, solidColor);
    buildIndices(indices, buffer, this->vertexCount);

    this->vertexCount += vertexCount;
    this->indexCount += indexCount;
    task->setDrawRange(indexOffset, this->indexCount);

    auto merged = task->getViewport();
    merged.add(viewRegion);
    task->setViewport(merged);
}
