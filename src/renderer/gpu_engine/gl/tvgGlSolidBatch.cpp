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

void GlSolidBatch::draw(GlRenderer& renderer, GlShape& shape, const RenderColor& color, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds)
{
    auto buffer = &shape.geometry.fill;
    auto vertexCount = buffer->vertex.count / 2;
    auto indexCount = buffer->index.count;
    if (vertexCount == 0 || indexCount == 0) return;

    auto batchColor = color;
    batchColor.a = MULTIPLY(color.a, shape.opacity);
    DrawData data = {
        &shape.geometry,
        renderer.mPrograms[GlRenderer::RT_Color],
        RenderUpdateFlag::Color,
        batchColor,
        0,
        0,
        2,
    };
    draw(renderer, data, depth, viewRegion, viewBounds);
}

void GlSolidBatch::draw(GlRenderer& renderer, GlImage& image, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds)
{
    DrawData data = {
        &image.geometry,
        renderer.mPrograms[GlRenderer::RT_Image],
        RenderUpdateFlag::Image,
        {},
        image.texId,
        image.opacity,
        4,
    };
    draw(renderer, data, depth, viewRegion, viewBounds);
}

void GlSolidBatch::draw(GlRenderer& renderer, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds)
{
    auto pass = renderer.currentPass();
    auto buffer = &data.geometry->fill;
    auto vertexCount = buffer->vertex.count / data.vertexSize;
    auto indexCount = buffer->index.count;

    if (!appendable(pass, data, viewBounds)) {
        emit(renderer, pass, data, depth, viewRegion, viewBounds, vertexCount, indexCount);
        return;
    }

    if (task->vertexLayout.count == 1) {
        promote(renderer, data, depth, viewRegion, vertexCount, indexCount);
        return;
    }

    append(renderer, data, depth, viewRegion, vertexCount, indexCount);
}

bool GlSolidBatch::appendable(const GlRenderPass* pass, const DrawData& data, const RenderRegion& viewBounds) const
{
    if (!task || pass->lastTask() != task) return false;
    if (task->program != data.program) return false;
    if (!(this->viewBounds == viewBounds)) return false;
    if (data.flag == RenderUpdateFlag::Image && (texId != data.texId || opacity != data.opacity)) return false;
    return true;
}

void GlSolidBatch::emit(GlRenderer& renderer, GlRenderPass* pass, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds, uint32_t vertexCount, uint32_t indexCount)
{
    auto drawTask = new GlRenderTask(data.program);
    drawTask->setViewMatrix(pass->getViewMatrix());
    drawTask->setDrawDepth(depth);

    if (data.flag == RenderUpdateFlag::Image) {
        uint32_t info[4] = {data.opacity, 0, 0, 0};
        drawTask->addBindResource(GlBindingResource{
            1,
            GlShaderUniformBlock::ColorInfo,
            renderer.mGpuBuffer.getBufferId(),
            renderer.mGpuBuffer.push(info, sizeof(info), true),
            sizeof(info),
        });
    }

    data.geometry->draw(drawTask, &renderer.mGpuBuffer, data.flag);

    if (data.flag == RenderUpdateFlag::Image) {
        drawTask->addBindResource(GlBindingResource{0, data.texId, GlShaderUniform::Texture});
    } else {
        drawTask->setVertexColor(data.color.r / 255.f, data.color.g / 255.f, data.color.b / 255.f, data.color.a / 255.f);
    }

    auto viewport = viewRegion;
    viewport.intersect(viewBounds);
    drawTask->setViewport(viewport);
    pass->addRenderTask(drawTask);

    task = drawTask;
    this->viewBounds = viewBounds;
    color = data.color;
    this->vertexCount = vertexCount;
    texId = data.texId;
    opacity = data.opacity;
}

void GlSolidBatch::appendGeometry(GlRenderer& renderer, const DrawData& data)
{
    auto buffer = &data.geometry->fill;
    float* vertices = nullptr;
    uint32_t* indices = nullptr;
    renderer.mGpuBuffer.reserve(buffer->vertex.count * sizeof(float), reinterpret_cast<void**>(&vertices));
    renderer.mGpuBuffer.reserveIndex(buffer->index.count * sizeof(uint32_t), reinterpret_cast<void**>(&indices));
    memcpy(vertices, buffer->vertex.data, buffer->vertex.count * sizeof(float));
    buildIndices(indices, buffer, this->vertexCount);
}

void GlSolidBatch::promote(GlRenderer& renderer, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount)
{
    appendGeometry(renderer, data);

    tvg::RGBA* colors = nullptr;
    auto totalVertexCount = this->vertexCount + vertexCount;
    auto colorOffset = renderer.mGpuBuffer.reserveAux(totalVertexCount * sizeof(tvg::RGBA), reinterpret_cast<void**>(&colors));
    buildColors(colors, this->vertexCount, color);
    buildColors(colors + this->vertexCount, vertexCount, data.color);

    task->addVertexLayout(GlVertexLayout{1, 4, sizeof(tvg::RGBA), colorOffset, GL_UNSIGNED_BYTE, GL_TRUE, renderer.mGpuBuffer.getAuxBufferId()});
    commit(depth, viewRegion, vertexCount, indexCount);
}

void GlSolidBatch::append(GlRenderer& renderer, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount)
{
    appendGeometry(renderer, data);

    if (data.flag == RenderUpdateFlag::Color) {
        tvg::RGBA* colors = nullptr;
        renderer.mGpuBuffer.reserveAux(vertexCount * sizeof(tvg::RGBA), reinterpret_cast<void**>(&colors));
        buildColors(colors, vertexCount, data.color);
    }

    commit(depth, viewRegion, vertexCount, indexCount);
}

void GlSolidBatch::commit(int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount)
{
    this->vertexCount += vertexCount;
    task->setDrawRange(task->indexOffset, task->indexCnt + indexCount);
    task->setDrawDepth(depth);

    auto viewport = task->viewport;
    viewport.add(viewRegion);
    viewport.intersect(viewBounds);
    task->setViewport(viewport);
}

void GlSolidBatch::buildColors(tvg::RGBA* out, uint32_t count, const RenderColor& color)
{
    for (uint32_t i = 0; i < count; ++i)
        out[i] = {color.r, color.g, color.b, color.a};
}

void GlSolidBatch::buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex)
{
    for (uint32_t i = 0; i < src->index.count; ++i)
        out[i] = src->index[i] + baseVertex;
}
