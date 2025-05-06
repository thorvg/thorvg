/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgGlCommon.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlTessellator.h"
#include "tvgGlRenderTask.h"


bool GlGeometry::tesselate(const RenderShape& rshape, RenderUpdateFlag flag)
{
    const RenderPath* path = nullptr;
    RenderPath trimmedPath;
    if (rshape.trimpath()) {
        if (!rshape.stroke->trim.trim(rshape.path, trimmedPath)) return true;
        path = &trimmedPath;
    } else path = &rshape.path;

    if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform | RenderUpdateFlag::Path)) {
        fill.clear();

        BWTessellator bwTess{&fill};
        bwTess.tessellate(*path, matrix);
        fillRule = rshape.rule;
        bounds = bwTess.bounds();
    }

    if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform)) {
        stroke.clear();

        Stroker stroker{&stroke, matrix};
        stroker.stroke(&rshape, *path);
        bounds = stroker.bounds();
    }

    return true;
}

bool GlGeometry::tesselate(const RenderSurface* image, RenderUpdateFlag flag)
{
    if (!(flag & RenderUpdateFlag::Image)) return true;

    fill.clear();

    fill.vertex.reserve(5 * 4);
    fill.index.reserve(6);

    auto left = 0.f;
    auto top = 0.f;
    auto right = float(image->w);
    auto bottom = float(image->h);

    // left top point
    fill.vertex.push(left);
    fill.vertex.push(top);

    fill.vertex.push(0.f);
    fill.vertex.push(1.f);
    // left bottom point
    fill.vertex.push(left);
    fill.vertex.push(bottom);

    fill.vertex.push(0.f);
    fill.vertex.push(0.f);
    // right top point
    fill.vertex.push(right);
    fill.vertex.push(top);

    fill.vertex.push(1.f);
    fill.vertex.push(1.f);
    // right bottom point
    fill.vertex.push(right);
    fill.vertex.push(bottom);

    fill.vertex.push(1.f);
    fill.vertex.push(0.f);

    fill.index.push(0);
    fill.index.push(1);
    fill.index.push(2);

    fill.index.push(2);
    fill.index.push(1);
    fill.index.push(3);

    bounds = {{0, 0}, {int32_t(image->w), int32_t(image->h)}};

    return true;
}


void GlGeometry::disableVertex(uint32_t location)
{
    GL_CHECK(glDisableVertexAttribArray(location));
}


bool GlGeometry::draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag)
{
    if (flag == RenderUpdateFlag::None) return false;

    auto buffer = ((flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke)) ? &stroke : &fill;
    if (buffer->index.empty()) return false;

    auto vertexOffset = gpuBuffer->push(buffer->vertex.data, buffer->vertex.count * sizeof(float));
    auto indexOffset = gpuBuffer->pushIndex(buffer->index.data, buffer->index.count * sizeof(uint32_t));

    // vertex layout
    if (flag & RenderUpdateFlag::Image) {
        // image has two attribute: [pos, uv]
        task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset});
        task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float)});
    } else {
        task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), vertexOffset});
    }
    task->setDrawRange(indexOffset, buffer->index.count);
    return true;
}


GlStencilMode GlGeometry::getStencilMode(RenderUpdateFlag flag)
{
    if (flag & RenderUpdateFlag::Stroke) return GlStencilMode::Stroke;
    if (flag & RenderUpdateFlag::GradientStroke) return GlStencilMode::Stroke;
    if (flag & RenderUpdateFlag::Image) return GlStencilMode::None;

    if (fillRule == FillRule::NonZero) return GlStencilMode::FillNonZero;
    if (fillRule == FillRule::EvenOdd) return GlStencilMode::FillEvenOdd;

    return GlStencilMode::None;
}


RenderRegion GlGeometry::getBounds() const
{
    if (tvg::identity(&matrix)) return bounds;

    auto lt = Point{float(bounds.min.x), float(bounds.min.y)} * matrix;
    auto lb = Point{float(bounds.min.x), float(bounds.max.y)} * matrix;
    auto rt = Point{float(bounds.max.x), float(bounds.min.y)} * matrix;
    auto rb = Point{float(bounds.max.x), float(bounds.max.y)} * matrix;

    auto left = min(min(lt.x, lb.x), min(rt.x, rb.x));
    auto top = min(min(lt.y, lb.y), min(rt.y, rb.y));
    auto right = max(max(lt.x, lb.x), max(rt.x, rb.x));
    auto bottom = max(max(lt.y, lb.y), max(rt.y, rb.y));

    auto bounds = RenderRegion {{int32_t(floor(left)), int32_t(floor(top))}, {int32_t(ceil(right)), int32_t(ceil(bottom))}};
    if (bounds.valid()) return bounds;
    return this->bounds;

}