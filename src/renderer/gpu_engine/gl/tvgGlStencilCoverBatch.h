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

#ifndef _TVG_GL_STENCIL_COVER_BATCH_H_
#define _TVG_GL_STENCIL_COVER_BATCH_H_

#include "tvgGlCommon.h"

struct GlProgram;
struct GlRenderPass;
struct GlStencilCoverTask;

struct GlStencilCoverBatch
{
    void clear();
    // The renderer supplies the cover's bindings, view matrix, depth and viewport.
    void draw(GlRenderPass& pass, GlStageBuffer& gpuBuffer, GlProgram* program, GlRenderTask* cover,
              const GlShape& shape, RenderUpdateFlag flag, GlStencilMode mode,
              const RenderRegion& viewBounds, const RenderColor* color = nullptr);

private:
    bool appendable(const GlRenderPass& pass, GlStencilMode mode, bool clipped, const RenderRegion& bounds, const GlGeometryBuffer& buffer) const;
    void setStencilMergeTarget(GlRenderTask* stencil, const GlGeometryBuffer& buffer);
    bool merge(GlRenderTask* stencil, const RenderRegion& viewBounds, const GlGeometryBuffer& buffer, uint32_t* indices);
    bool mergeCover(GlRenderTask* cover, const RenderRegion& viewBounds);
    bool intersects(const RenderRegion& bounds) const;
    void addBounds(const RenderRegion& bounds);

    GlRenderPass* pass = nullptr;
    GlStencilCoverTask* task = nullptr;
    GlRenderTask* stencilTask = nullptr;
    Array<RenderRegion> bounds = {};
    RenderRegion viewBounds = {};
    uint32_t vertexCount = 0;
    bool clipped = false;
    bool ySorted = false;
};

#endif /* _TVG_GL_STENCIL_COVER_BATCH_H_ */
