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

    if (!appendable(renderer, pass, sdata.picture)) {
        emitSingle(renderer, pass, sdata, color, depth, viewRegion, vertexCount, indexCount);
        return;
    }

    auto batchColor = SolidBatch::solidColor(sdata, color, RenderUpdateFlag::Color);
    if (!promoted) {
        if (promote(renderer, pass, batchColor, depth, viewRegion, buffer, vertexCount, indexCount)) return;
        emitSingle(renderer, pass, sdata, color, depth, viewRegion, vertexCount, indexCount);
        return;
    }

    append(renderer, batchColor, viewRegion, buffer, vertexCount, indexCount, depth);
}


bool GlRenderer::SolidBatch::appendable(const GlRenderer& renderer, const GlRenderPass* pass, const Paint* picture) const
{
    if (!task) return false;
    if (this->pass != pass) return false;
    if (this->picture != picture) return false;
    if (pass->lastTask() != task) return false;
    if (task->getProgram() != renderer.mPrograms[RT_Color]) return false;
    return true;
}


void GlRenderer::SolidBatch::buildPositions(float* out, const GlGeometryBuffer* src, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        out[i * 2 + 0] = src->vertex[i * 2 + 0];
        out[i * 2 + 1] = src->vertex[i * 2 + 1];
    }
}


void GlRenderer::SolidBatch::buildColors(SolidBatchColor* out, uint32_t count, const RenderColor& color)
{
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = { color.r, color.g, color.b, color.a };
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

    auto taskColor = SolidBatch::solidColor(sdata, color, RenderUpdateFlag::Color);
    drawTask->setVertexColor(taskColor.r / 255.f, taskColor.g / 255.f, taskColor.b / 255.f, taskColor.a / 255.f);
    drawTask->setViewport(viewRegion);
    pass->addRenderTask(drawTask);

    this->pass = pass;
    task = drawTask;
    shape = &sdata;
    picture = sdata.picture;
    this->color = color;
    flag = RenderUpdateFlag::Color;
    this->depth = depth;
    this->vertexCount = vertexCount;
    indexOffset = drawTask->getIndexOffset();
    this->indexCount = indexCount;
    promoted = false;
}


bool GlRenderer::SolidBatch::promote(GlRenderer& renderer, GlRenderPass* pass, const RenderColor& solidColor, int32_t depth, const RenderRegion& viewRegion, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount)
{
    if (!shape) return false;

    auto firstVertexCount = this->vertexCount;
    auto firstIndexCount = this->indexCount;
    if (firstVertexCount == 0 || firstIndexCount == 0) return false;

    auto firstColor = SolidBatch::solidColor(*shape, color, flag);
    auto totalVertexCount = firstVertexCount + vertexCount;
    auto totalIndexCount = firstIndexCount + indexCount;

    // Promotion starts from a plain solid task: position-only attribute.
    const auto& layouts = task->getVertexLayout();
    if (layouts.count != 1) return false;
    const auto& posLayout = layouts[0];
    if (posLayout.size != 2 || posLayout.stride != 2 * sizeof(float)) return false;

    float* newPositions = nullptr;
    SolidBatchColor* colors = nullptr;
    uint32_t* newIndices = nullptr;
    // New geometry must be appended contiguously to keep the existing draw range base.
    auto newPosOffset = renderer.mGpuBuffer.reserve(vertexCount * 2 * sizeof(float), reinterpret_cast<void**>(&newPositions));
    auto expectedPosOffset = posLayout.offset + firstVertexCount * 2 * sizeof(float);
    if (newPosOffset != expectedPosOffset) return false;

    auto newIdxOffset = renderer.mGpuBuffer.reserveIndex(indexCount * sizeof(uint32_t), reinterpret_cast<void**>(&newIndices));
    auto expectedIdxOffset = indexOffset + firstIndexCount * sizeof(uint32_t);
    if (newIdxOffset != expectedIdxOffset) return false;

    auto colorOffset = renderer.mGpuBuffer.reserveAux(totalVertexCount * sizeof(SolidBatchColor), reinterpret_cast<void**>(&colors));

    // Build full color stream: old vertices first, then the incoming shape.
    buildPositions(newPositions, buffer, vertexCount);
    buildColors(colors, firstVertexCount, firstColor);
    buildColors(colors + firstVertexCount, vertexCount, solidColor);
    buildIndices(newIndices, buffer, firstVertexCount);

    // Upgrade the same task to per-vertex color mode (no task replacement).
    task->setViewMatrix(pass->getViewMatrix());
    task->setDrawDepth(depth);
    task->addVertexLayout(GlVertexLayout{1, 4, sizeof(SolidBatchColor), colorOffset, GL_UNSIGNED_BYTE, GL_TRUE, renderer.mGpuBuffer.getAuxBufferId()});
    task->setDrawRange(indexOffset, totalIndexCount);

    auto merged = task->getViewport();
    merged.add(viewRegion);
    task->setViewport(merged);

    shape = nullptr;
    this->depth = depth;
    this->vertexCount = totalVertexCount;
    this->indexCount = totalIndexCount;
    promoted = true;
    return true;
}


void GlRenderer::SolidBatch::append(GlRenderer& renderer, const RenderColor& solidColor, const RenderRegion& viewRegion, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount, int32_t depth)
{
    float* positions = nullptr;
    SolidBatchColor* colors = nullptr;
    uint32_t* indices = nullptr;
    renderer.mGpuBuffer.reserve(vertexCount * 2 * sizeof(float), reinterpret_cast<void**>(&positions));
    renderer.mGpuBuffer.reserveAux(vertexCount * sizeof(SolidBatchColor), reinterpret_cast<void**>(&colors));
    renderer.mGpuBuffer.reserveIndex(indexCount * sizeof(uint32_t), reinterpret_cast<void**>(&indices));

    buildPositions(positions, buffer, vertexCount);
    buildColors(colors, vertexCount, solidColor);
    buildIndices(indices, buffer, this->vertexCount);

    this->vertexCount += vertexCount;
    this->indexCount += indexCount;
    task->setDrawRange(indexOffset, this->indexCount);
    task->setDrawDepth(depth);
    this->depth = depth;

    auto merged = task->getViewport();
    merged.add(viewRegion);
    task->setViewport(merged);
}


RenderColor GlRenderer::SolidBatch::solidColor(const GlShape& sdata, const RenderColor& color, RenderUpdateFlag flag)
{
    RenderColor out = color;
    auto a = MULTIPLY(color.a, sdata.opacity);

    if (flag & RenderUpdateFlag::Stroke) {
        auto strokeWidth = sdata.geometry.strokeRenderWidth;
        if (strokeWidth < MIN_GL_STROKE_WIDTH) {
            auto alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
            a = MULTIPLY(a, static_cast<uint8_t>(alpha * 255));
        }
    }

    out.a = a;
    return out;
}
