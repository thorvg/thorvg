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

#ifdef THORVG_ANALYTIC_AA_SUPPORT
void GlSolidBatch::draw(GlRenderer& renderer, const GlCoverageMap& coverage, const RenderColor& color, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds)
{
    // Matches CoverageInfo in COVERAGE_VERT_SHADER. One map can own many run instances.
    struct MapInfo
    {
        float bounds[4];
        int32_t atlas[4];
        float color[4];
    };
    struct BatchInfo
    {
        int32_t format[4];
        MapInfo maps[64];
    };

    auto pass = renderer.currentPass();
    auto program = renderer.mPrograms[GlRenderer::RT_Color];
    auto runs = coverage.format == GlCoverageFormat::Runs;
    if (!task || pass->lastTask() != task || task->program != program || texId != coverage.region.texture ||
        coverageFormat != coverage.format || !(this->viewBounds == viewBounds) || coverageCount == 64) {
        task = new GlRenderTask(program);
        task->setViewMatrix(pass->getViewMatrix());
        task->useDrawArrays = true;
        task->indexCnt = 6;
        task->setViewport(viewRegion);

        BatchInfo info = {};
        info.format[0] = runs ? 1 : 0;
        coverageOffset = renderer.mGpuBuffer.push(&info, sizeof(info), true);
        task->addBindResource(GlBindingResource{4, GlShaderUniformBlock::CoverageInfo, renderer.mGpuBuffer.getBufferId(), coverageOffset, sizeof(info)});
        GlRenderer::bindCoverageTextures(task, coverage.region.texture, runs);
        pass->addRenderTask(task);
        this->viewBounds = viewBounds;
        texId = coverage.region.texture;
        coverageFormat = coverage.format;
        coverageCount = 0;
    }

    auto& bounds = coverage.map.bounds;
    auto& rect = coverage.region.rect;
    MapInfo info = {{float(bounds.sx()), float(bounds.sy()), float(bounds.w()), float(bounds.h())},
                    {rect.sx(), rect.sy(), rect.sw(), int32_t(task->instanceCount)},
                    {color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f}};
    renderer.mGpuBuffer.update(coverageOffset + sizeof(int32_t) * 4 + coverageCount * sizeof(MapInfo), &info, sizeof(info));

    auto count = runs ? coverage.runCount : 1u;
    void* indices = nullptr;
    auto offset = renderer.mGpuBuffer.reserveAux(count, &indices);
    memset(indices, coverageCount, count);
    if (coverageCount == 0) {
        task->addVertexLayout(GlVertexLayout{0, 1, 1, offset, GL_UNSIGNED_BYTE, GL_FALSE, renderer.mGpuBuffer.getAuxBufferId(), 1});
    }
    ++coverageCount;
    task->instanceCount += count;
    task->setDrawDepth(depth);
    auto viewport = task->viewport;
    viewport.add(viewRegion);
    viewport.intersect(viewBounds);
    task->setViewport(viewport);
}
#else
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
#endif

void GlSolidBatch::draw(GlRenderer& renderer, GlImage& image, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds)
{
    DrawData data = {
        &image.geometry,
        renderer.mPrograms[GlRenderer::RT_Image],
        RenderUpdateFlag::Image,
#ifndef THORVG_ANALYTIC_AA_SUPPORT
        {},
#endif
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

#ifndef THORVG_ANALYTIC_AA_SUPPORT
    if (task->vertexLayout.count == 1) {
        promote(renderer, data, depth, viewRegion, vertexCount, indexCount);
        return;
    }
#endif

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
#ifndef THORVG_ANALYTIC_AA_SUPPORT
    } else {
        drawTask->setVertexColor(data.color.r / 255.f, data.color.g / 255.f, data.color.b / 255.f, data.color.a / 255.f);
#endif
    }

    auto viewport = viewRegion;
    viewport.intersect(viewBounds);
    drawTask->setViewport(viewport);
    pass->addRenderTask(drawTask);

    task = drawTask;
    this->viewBounds = viewBounds;
#ifndef THORVG_ANALYTIC_AA_SUPPORT
    color = data.color;
#endif
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

#ifndef THORVG_ANALYTIC_AA_SUPPORT
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
#endif

void GlSolidBatch::append(GlRenderer& renderer, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount)
{
    appendGeometry(renderer, data);

#ifndef THORVG_ANALYTIC_AA_SUPPORT
    if (data.flag == RenderUpdateFlag::Color) {
        tvg::RGBA* colors = nullptr;
        renderer.mGpuBuffer.reserveAux(vertexCount * sizeof(tvg::RGBA), reinterpret_cast<void**>(&colors));
        buildColors(colors, vertexCount, data.color);
    }
#endif

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

#ifndef THORVG_ANALYTIC_AA_SUPPORT
void GlSolidBatch::buildColors(tvg::RGBA* out, uint32_t count, const RenderColor& color)
{
    for (uint32_t i = 0; i < count; ++i)
        out[i] = {color.r, color.g, color.b, color.a};
}
#endif

void GlSolidBatch::buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex)
{
    for (uint32_t i = 0; i < src->index.count; ++i)
        out[i] = src->index[i] + baseVertex;
}
