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

#include "tvgGlRenderer.h"

void GlImageBatch::draw(GlRenderer& renderer, GlShape& sdata, int32_t depth, const RenderRegion& viewRegion)
{
    auto pass = renderer.currentPass();
    auto buffer = &sdata.geometry.fill;

    auto vertexCount = buffer->vertex.count / 4;
    auto indexCount = buffer->index.count;
    if (vertexCount == 0 || indexCount == 0) return;

    if (!appendable(renderer, pass, sdata)) {
        if (task) clear();
        emitSingle(renderer, pass, sdata, depth, viewRegion, vertexCount, indexCount);
        return;
    }

    if (!promoted) {
        if (promote(renderer, pass, sdata, buffer, vertexCount, indexCount, depth, viewRegion)) return;
        emitSingle(renderer, pass, sdata, depth, viewRegion, vertexCount, indexCount);
        return;
    }

    append(renderer, sdata, buffer, vertexCount, indexCount, depth, viewRegion);
}


bool GlImageBatch::appendable(const GlRenderer& renderer, const GlRenderPass* pass, const GlShape& sdata) const
{
    if (this->pass != pass) return false;
    if (pass->lastTask() != task) return false;
    if (task->getProgram() != renderer.mPrograms[GlRenderer::RT_Image]) return false;
    // Images batched by the same retained texture do not vary texColorSpace today:
    // prepare() copies it from the shared RenderSurface, and texFlipY is currently
    // hardcoded to 1 for GL images. RT_Image still keeps opacity in ColorInfo, so
    // only same-texture, same-opacity draws can be merged onto the same task.
    if (texId != sdata.texId) return false;
    if (opacity != sdata.opacity) return false;
    if (drawCount >= MAX_DRAWS) return false;
    return true;
}


void GlImageBatch::buildVertices(float* out, const GlGeometryBuffer* src)
{
    for (uint32_t i = 0; i < src->vertex.count; ++i) {
        out[i] = src->vertex[i];
    }
}


void GlImageBatch::buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex)
{
    for (uint32_t i = 0; i < src->index.count; ++i) {
        out[i] = src->index[i] + baseVertex;
    }
}


void GlImageBatch::emitSingle(GlRenderer& renderer, GlRenderPass* pass, GlShape& sdata, int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount)
{
    auto drawTask = new GlRenderTask(renderer.mPrograms[GlRenderer::RT_Image]);
    drawTask->setViewMatrix(pass->getViewMatrix());
    drawTask->setDrawDepth(depth);

    if (!sdata.geometry.draw(drawTask, &renderer.mGpuBuffer, RenderUpdateFlag::Image)) {
        delete drawTask;
        clear();
        return;
    }

    uint32_t info[4] = {(uint32_t)sdata.texColorSpace, sdata.texFlipY, sdata.opacity, 0};
    drawTask->addBindResource(GlBindingResource{
        1,
        drawTask->getProgram()->getUniformBlockIndex("ColorInfo"),
        renderer.mGpuBuffer.getBufferId(),
        renderer.mGpuBuffer.push(info, 4 * sizeof(uint32_t), true),
        4 * sizeof(uint32_t),
    });

    drawTask->addBindResource(GlBindingResource{0, sdata.texId, drawTask->getProgram()->getUniformLocation("uTexture")});
    drawTask->setViewport(viewRegion);
    pass->addRenderTask(drawTask);

    this->pass = pass;
    task = drawTask;
    shape = &sdata;
    texId = sdata.texId;
    opacity = sdata.opacity;
    promoted = false;
    drawCount = 1;
    this->vertexCount = vertexCount;
    indexOffset = drawTask->getIndexOffset();
    this->indexCount = indexCount;
}


bool GlImageBatch::promote(GlRenderer& renderer, GlRenderPass* pass, const GlShape& sdata, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount, int32_t depth, const RenderRegion& viewRegion)
{
    auto firstShape = shape;
    if (!firstShape) return false;
    if (firstShape->opacity != sdata.opacity) return false;

    auto firstBuffer = &firstShape->geometry.fill;
    auto firstVertexCount = firstBuffer->vertex.count / 4;
    auto firstIndexCount = firstBuffer->index.count;
    if (firstVertexCount == 0 || firstIndexCount == 0) return false;

    auto firstTask = pass->takeLastTask();
    assert(firstTask == task);
    if (firstTask != task) {
        if (firstTask) pass->addRenderTask(firstTask);
        return false;
    }

    float* vertices = nullptr;
    uint32_t* indices = nullptr;
    auto totalVertexCount = firstVertexCount + vertexCount;
    auto totalIndexCount = firstIndexCount + indexCount;

    auto drawTask = new GlRenderTask(renderer.mPrograms[GlRenderer::RT_Image]);
    drawTask->setViewMatrix(pass->getViewMatrix());
    drawTask->setDrawDepth(depth);

    // Push batch-wide image info before geometry so appended vertices stay contiguous.
    uint32_t info[4] = {(uint32_t)firstShape->texColorSpace, firstShape->texFlipY, firstShape->opacity, 0};
    drawTask->addBindResource(GlBindingResource{
        1,
        drawTask->getProgram()->getUniformBlockIndex("ColorInfo"),
        renderer.mGpuBuffer.getBufferId(),
        renderer.mGpuBuffer.push(info, 4 * sizeof(uint32_t), true),
        4 * sizeof(uint32_t),
    });

    auto vertexOffset = renderer.mGpuBuffer.reserve(totalVertexCount * 4 * sizeof(float), reinterpret_cast<void**>(&vertices));
    auto newIndexOffset = renderer.mGpuBuffer.reserveIndex(totalIndexCount * sizeof(uint32_t), reinterpret_cast<void**>(&indices));
    buildVertices(vertices, firstBuffer);
    buildVertices(vertices + firstBuffer->vertex.count, buffer);
    buildIndices(indices, firstBuffer, 0);
    buildIndices(indices + firstIndexCount, buffer, firstVertexCount);

    drawTask->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset});
    drawTask->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float)});
    drawTask->setDrawRange(newIndexOffset, totalIndexCount);
    drawTask->addBindResource(GlBindingResource{0, texId, drawTask->getProgram()->getUniformLocation("uTexture")});

    auto merged = firstTask->getViewport();
    merged.add(viewRegion);
    drawTask->setViewport(merged);
    pass->addRenderTask(drawTask);
    delete firstTask;

    task = drawTask;
    shape = nullptr;
    promoted = true;
    drawCount = 2;
    this->vertexCount = totalVertexCount;
    indexOffset = newIndexOffset;
    this->indexCount = totalIndexCount;
    return true;
}


void GlImageBatch::append(GlRenderer& renderer, const GlShape&, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount, int32_t depth, const RenderRegion& viewRegion)
{
    const auto& layouts = task->getVertexLayout();
    if (layouts.count != 2) return;
    const auto& posLayout = layouts[0];
    const auto& uvLayout = layouts[1];
    if (posLayout.size != 2 || uvLayout.size != 2) return;
    if (posLayout.stride != 4 * sizeof(float) || uvLayout.stride != 4 * sizeof(float)) return;
    if (posLayout.offset + 2 * sizeof(float) != uvLayout.offset) return;

    float* vertices = nullptr;
    uint32_t* indices = nullptr;
    auto vertexOffset = renderer.mGpuBuffer.reserve(vertexCount * 4 * sizeof(float), reinterpret_cast<void**>(&vertices));
    auto expectedVertexOffset = posLayout.offset + this->vertexCount * 4 * sizeof(float);
    auto newIndexOffset = renderer.mGpuBuffer.reserveIndex(indexCount * sizeof(uint32_t), reinterpret_cast<void**>(&indices));
    auto expectedIndexOffset = indexOffset + this->indexCount * sizeof(uint32_t);
    assert(vertexOffset == expectedVertexOffset);
    assert(newIndexOffset == expectedIndexOffset);
    if (vertexOffset != expectedVertexOffset || newIndexOffset != expectedIndexOffset) return;

    buildVertices(vertices, buffer);
    buildIndices(indices, buffer, this->vertexCount);

    drawCount++;
    this->vertexCount += vertexCount;
    this->indexCount += indexCount;
    task->setDrawRange(indexOffset, this->indexCount);
    task->setDrawDepth(depth);

    auto merged = task->getViewport();
    merged.add(viewRegion);
    task->setViewport(merged);
}
