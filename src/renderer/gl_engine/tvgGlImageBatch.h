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

#ifndef _TVG_GL_IMAGE_BATCH_H_
#define _TVG_GL_IMAGE_BATCH_H_

#include "tvgGlCommon.h"

struct GlRenderer;
class GlRenderPass;
class GlRenderTask;

struct GlImageBatchEntry
{
    int32_t colorInfo[4];
};

class GlImageBatch
{
public:
    static constexpr uint32_t MAX_DRAWS = GL_IMAGE_BATCH_MAX_DRAWS;

    void clear()
    {
        pass = nullptr;
        task = nullptr;
        texId = 0;
        drawCount = 0;
        vertexCount = 0;
        indexOffset = 0;
        indexCount = 0;
        entries.clear();
    }
    void draw(GlRenderer& renderer, GlShape& sdata, int32_t depth, const RenderRegion& viewRegion);
    void finalize(GlStageBuffer& gpuBuffer);

private:
    friend struct GlRenderer;
    bool appendable(const GlRenderer& renderer, const GlRenderPass* pass, const GlShape& sdata) const;
    void emitSingle(GlRenderer& renderer, GlRenderPass* pass, const GlShape& sdata, int32_t depth, const RenderRegion& viewRegion, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount);
    void append(GlRenderer& renderer, const GlShape& sdata, const GlGeometryBuffer* buffer, uint32_t vertexCount, uint32_t indexCount, int32_t depth, const RenderRegion& viewRegion);
    static void buildVertices(float* out, const GlGeometryBuffer* src, uint32_t drawId);
    static void buildIndices(uint32_t* out, const GlGeometryBuffer* src, uint32_t baseVertex);

    GlRenderPass* pass = nullptr;
    GlRenderTask* task = nullptr;
    GLuint texId = 0;
    uint32_t drawCount = 0;
    uint32_t vertexCount = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    Array<GlImageBatchEntry> entries = {};
};

#endif /* _TVG_GL_IMAGE_BATCH_H_ */
