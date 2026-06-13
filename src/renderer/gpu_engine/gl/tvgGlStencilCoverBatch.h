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

class GlRenderPass;
class GlRenderTask;
class GlStencilCoverTask;

class GlStencilCoverBatch
{
public:
    void clear();
    bool mergeable(const GlRenderPass* pass, GlStencilMode mode, const RenderRegion& bounds, const GlGeometryBuffer* stencilBuffer) const;
    void draw(GlRenderPass* pass, GlRenderTask* stencil, GlRenderTask* cover, bool merge, GlStencilMode mode, const RenderRegion& bounds, const GlGeometryBuffer* stencilBuffer, uint32_t* stencilIndices);

private:
    void emitSingle(GlRenderPass* pass, GlRenderTask* stencil, GlRenderTask* cover, GlStencilMode mode, const RenderRegion& bounds, const GlGeometryBuffer* stencilBuffer);
    void append(GlRenderTask* stencil, GlRenderTask* cover, const RenderRegion& bounds, const GlGeometryBuffer* stencilBuffer, uint32_t* stencilIndices);
    bool merge(GlRenderTask* stencil, const GlGeometryBuffer* stencilBuffer, uint32_t* stencilIndices);
    bool mergeCover(GlRenderTask* cover);
    bool intersects(const RenderRegion& bounds) const;
    void addBounds(const RenderRegion& bounds);

    GlRenderPass* pass = nullptr;
    GlStencilCoverTask* task = nullptr;
    GlRenderTask* stencilTask = nullptr;
    GlStencilMode mode = GlStencilMode::None;
    Array<RenderRegion> bounds = {};
    uint32_t vertexCount = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    bool ySorted = false;
    bool open = false;
};

#endif /* _TVG_GL_STENCIL_COVER_BATCH_H_ */
