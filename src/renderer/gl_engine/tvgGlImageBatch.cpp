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

#include <string.h>

#include "tvgGlRenderer.h"

void GlImageBatch::draw(GlRenderer& renderer, GlShape& sdata, int32_t depth, const RenderRegion& viewRegion)
{
    auto pass = renderer.currentPass();
    auto buffer = &sdata.geometry.fill;

    auto vertexCount = buffer->vertex.count / 4;
    auto indexCount = buffer->index.count;
    if (vertexCount == 0 || indexCount == 0) return;

    if (!appendable(renderer, pass, sdata)) {
        if (task) finalize(renderer.mGpuBuffer);
        emitSingle(renderer, pass, sdata, depth, viewRegion, buffer, vertexCount, indexCount);
        return;
    }

    append(renderer, sdata, buffer, vertexCount, indexCount, depth, viewRegion);
}

bool GlImageBatch::appendable(const GlRenderer& renderer, const GlRenderPass* pass, const GlShape& sdata) const
{
    if (this->pass != pass) return false;
    if (pass->lastTask() != task) return false;
    if (task->getProgram() != renderer.mPrograms[GlRenderer::RT_ImageBatch]) return false;
    if (texId != sdata.texId) return false;
    if (drawCount >= MAX_DRAWS) return false;
    return true;
}

void GlImageBatch::buildVertices(float* out, const GlGeometryBuffer* src, uint32_t drawId)
{
    auto vertexCount = src->vertex.count / 4;
    for (uint32_t i = 0; i < vertexCount; ++i) {
        auto srcOffset = i * 4;
        auto dstOffset = i * 5;
        out[dstOffset + 0] = src->vertex[srcOffset + 0];
        out[dstOffset + 1] = src->vertex[srcOffset + 1];
        out[dstOffset + 2] = src->vertex[srcOffset + 2];
        out[dstOffset + 3] = src->vertex[srcOffset + 3];
        out[dstOffset + 4] = static_cast<float>(drawId);
    }
}

void GlImageBatch::buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex)
{
    for (uint32_t i = 0; i < src->index.count; ++i) {
        out[i] = src->index[i] + baseVertex;
    }
}

void GlImageBatch::emitSingle(GlRenderer& renderer, GlRenderPass* pass, const GlShape& sdata, int32_t depth, const RenderRegion& viewRegion, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount)
{
    auto drawTask = new GlRenderTask(renderer.mPrograms[GlRenderer::RT_ImageBatch]);
    drawTask->setViewMatrix(pass->getViewMatrix());
    drawTask->setDrawDepth(depth);

    float* vertices = nullptr;
    uint32_t* indices = nullptr;
    auto vertexOffset = renderer.mGpuBuffer.reserveAux(vertexCount * 5 * sizeof(float), reinterpret_cast<void**>(&vertices));
    auto indexOffset = renderer.mGpuBuffer.reserveIndex(indexCount * sizeof(uint32_t), reinterpret_cast<void**>(&indices));
    buildVertices(vertices, buffer, 0);
    buildIndices(indices, buffer, 0);

    auto auxBufferId = renderer.mGpuBuffer.getAuxBufferId();
    drawTask->addVertexLayout(GlVertexLayout{0, 2, 5 * sizeof(float), vertexOffset, GL_FLOAT, GL_FALSE, auxBufferId});
    drawTask->addVertexLayout(GlVertexLayout{1, 2, 5 * sizeof(float), vertexOffset + 2 * sizeof(float), GL_FLOAT, GL_FALSE, auxBufferId});
    drawTask->addVertexLayout(GlVertexLayout{2, 1, 5 * sizeof(float), vertexOffset + 4 * sizeof(float), GL_FLOAT, GL_FALSE, auxBufferId});
    drawTask->setDrawRange(indexOffset, indexCount);

    drawTask->addBindResource(GlBindingResource{0, sdata.texId, drawTask->getProgram()->getUniformLocation("uTexture")});
    drawTask->setViewport(viewRegion);
    pass->addRenderTask(drawTask);

    this->pass = pass;
    task = drawTask;
    texId = sdata.texId;
    drawCount = 1;
    this->vertexCount = vertexCount;
    this->indexOffset = indexOffset;
    this->indexCount = indexCount;
    entries.push(GlImageBatchEntry{{static_cast<int32_t>(sdata.texColorSpace), static_cast<int32_t>(sdata.texFlipY), static_cast<int32_t>(sdata.opacity), 0}});
}

void GlImageBatch::append(GlRenderer& renderer, const GlShape& sdata, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount, int32_t depth, const RenderRegion& viewRegion)
{
    float* vertices = nullptr;
    uint32_t* indices = nullptr;
    renderer.mGpuBuffer.reserveAux(vertexCount * 5 * sizeof(float), reinterpret_cast<void**>(&vertices));
    renderer.mGpuBuffer.reserveIndex(indexCount * sizeof(uint32_t), reinterpret_cast<void**>(&indices));
    buildVertices(vertices, buffer, drawCount);
    buildIndices(indices, buffer, this->vertexCount);

    entries.push(GlImageBatchEntry{{static_cast<int32_t>(sdata.texColorSpace), static_cast<int32_t>(sdata.texFlipY), static_cast<int32_t>(sdata.opacity), 0}});
    drawCount++;
    this->vertexCount += vertexCount;
    this->indexCount += indexCount;
    task->setDrawRange(indexOffset, this->indexCount);
    task->setDrawDepth(depth);

    auto merged = task->getViewport();
    merged.add(viewRegion);
    task->setViewport(merged);
}

void GlImageBatch::finalize(GlStageBuffer& gpuBuffer)
{
    if (!task || drawCount == 0 || entries.empty()) return;

    auto bytes = static_cast<uint32_t>(entries.count * sizeof(GlImageBatchEntry));
    auto offset = gpuBuffer.push(entries.data, bytes, true);
    task->addBindResource(GlBindingResource{
        1,
        task->getProgram()->getUniformBlockIndex("ImageBatch"),
        gpuBuffer.getBufferId(),
        offset,
        bytes,
    });

    clear();
}
