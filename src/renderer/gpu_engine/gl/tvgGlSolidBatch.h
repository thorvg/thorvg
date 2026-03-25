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

#ifndef _TVG_GL_SOLID_BATCH_H_
#define _TVG_GL_SOLID_BATCH_H_

#include "tvgGlCommon.h"
#include "tvgGlRenderPass.h"
#include "tvgGlRenderTask.h"

struct GlRenderer;

struct GlSolidBatch
{
    void clear() { *this = {}; }
    void draw(GlRenderer& renderer, GlShape& sdata, const RenderColor& color, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds);
    void draw(GlRenderer& renderer, GlImage& image, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds);

    struct DrawData
    {
        const GlGeometry* geometry;
        GlProgram* program;
        RenderUpdateFlag flag;
        RenderColor color;
        GLuint texId;
        uint32_t opacity;
        uint32_t vertexSize;
    };

    void draw(GlRenderer& renderer, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds);
    bool appendable(const GlRenderPass* pass, const DrawData& data, const RenderRegion& viewBounds) const;
    void emit(GlRenderer& renderer, GlRenderPass* pass, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, const RenderRegion& viewBounds, uint32_t vertexCount, uint32_t indexCount);
    void promote(GlRenderer& renderer, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount);
    void append(GlRenderer& renderer, const DrawData& data, int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount);
    void appendGeometry(GlRenderer& renderer, const DrawData& data);
    void commit(int32_t depth, const RenderRegion& viewRegion, uint32_t vertexCount, uint32_t indexCount);
    static void buildColors(tvg::RGBA* out, uint32_t count, const RenderColor& color);
    static void buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex);

    GlRenderTask* task = nullptr;
    RenderRegion viewBounds = {};
    RenderColor color = {};
    uint32_t vertexCount = 0;
    GLuint texId = 0;
    uint32_t opacity = 0;
};

#endif /* _TVG_GL_SOLID_BATCH_H_ */
