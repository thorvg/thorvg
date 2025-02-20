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


GlGeometry::GlGeometry()
{
    fill = GlGeometryBufferPool::instance()->acquire();
    stroke = GlGeometryBufferPool::instance()->acquire();
}


GlGeometry::~GlGeometry()
{
    GlGeometryBufferPool::instance()->retrieve(fill);
    GlGeometryBufferPool::instance()->retrieve(stroke);
}


void GlGeometry::reset()
{
    fill->clear();
    stroke->clear();
    fillRule = FillRule::NonZero;
}


bool GlGeometry::tesselate(const RenderShape& rshape, RenderUpdateFlag flag)
{
    if (flag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform | RenderUpdateFlag::Path)) {
        fill->clear();

        BWTessellator bwTess{fill};
        bwTess.tessellate(&rshape, matrix);
        fillRule = rshape.rule;
        bounds = bwTess.bounds();
    }

    if (flag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform)) {
        stroke->clear();

        Stroker stroker{stroke, matrix};
        stroker.stroke(&rshape);
        bounds = stroker.bounds();
    }

    return true;
}

bool GlGeometry::tesselate(const RenderSurface* image, RenderUpdateFlag flag)
{
    if (!(flag & RenderUpdateFlag::Image)) return true;

    fill->clear();

    auto& vertex = fill->vertex;
    auto& index = fill->index;

    vertex.reserve(5 * 4);
    index.reserve(6);

    auto left = 0.0f;
    auto top = 0.0f;
    auto right = static_cast<float>(image->w);
    auto bottom = static_cast<float>(image->h);

    // left top point
    vertex.push(left);
    vertex.push(top);
    vertex.push(0.f);
    vertex.push(1.f);

    // left bottom point
    vertex.push(left);
    vertex.push(bottom);
    vertex.push(0.f);
    vertex.push(0.f);

    // right top point
    vertex.push(right);
    vertex.push(top);
    vertex.push(1.f);
    vertex.push(1.f);

    // right bottom point
    vertex.push(right);
    vertex.push(bottom);
    vertex.push(1.f);
    vertex.push(0.f);

    index.push(0);
    index.push(1);
    index.push(2);

    index.push(2);
    index.push(1);
    index.push(3);

    bounds = {0, 0, (int32_t)image->w, (int32_t)image->h};

    return true;
}


void GlGeometry::disableVertex(uint32_t location)
{
    GL_CHECK(glDisableVertexAttribArray(location));
}


bool GlGeometry::draw(GlRenderTask* task, GlStageBuffer* gpuBuffer, RenderUpdateFlag flag)
{
    if (flag == RenderUpdateFlag::None) return false;

    auto buffer = ((flag & RenderUpdateFlag::Stroke) || (flag & RenderUpdateFlag::GradientStroke)) ? stroke : fill;
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
    if (identity(&matrix)) {
        return bounds;
    } else {
        Point lt{static_cast<float>(bounds.x), static_cast<float>(bounds.y)};
        Point lb{static_cast<float>(bounds.x), static_cast<float>(bounds.y + bounds.h)};
        Point rt{static_cast<float>(bounds.x + bounds.w), static_cast<float>(bounds.y)};
        Point rb{static_cast<float>(bounds.x + bounds.w), static_cast<float>(bounds.y + bounds.h)};

        lt *= matrix;
        lb *= matrix;
        rt *= matrix;
        rb *= matrix;

        float left = min(min(lt.x, lb.x), min(rt.x, rb.x));
        float top = min(min(lt.y, lb.y), min(rt.y, rb.y));
        float right = max(max(lt.x, lb.x), max(rt.x, rb.x));
        float bottom = max(max(lt.y, lb.y), max(rt.y, rb.y));

        auto bounds = RenderRegion {
            static_cast<int32_t>(floor(left)),
            static_cast<int32_t>(floor(top)),
            static_cast<int32_t>(ceil(right - floor(left))),
            static_cast<int32_t>(ceil(bottom - floor(top))),
        };
        if (bounds.w < 0 || bounds.h < 0) return this->bounds;
        else return bounds;
    }
}


GlGeometryBuffer* GlGeometryBufferPool::acquire()
{
    bool empty;
    {
        ScopedLock lock(key);
        empty = pool.empty();
    }
    if (empty) {
        auto p = new GlGeometryBuffer;
        return p;
    }

    GlGeometryBuffer* ret;
    {
        ScopedLock lock(key);
        ret = pool.last();
        --pool.count;
    }
    return ret;
}


void GlGeometryBufferPool::retrieve(GlGeometryBuffer* p)
{
    p->clear();
    ScopedLock lock(key);
    pool.push(p);
}


GlGeometryBufferPool::~GlGeometryBufferPool()
{
    ARRAY_FOREACH(p, pool) delete(*p);
}


GlGeometryBufferPool* GlGeometryBufferPool::instance()
{
    static GlGeometryBufferPool pool;
    return &pool;
}