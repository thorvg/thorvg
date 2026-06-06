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

#ifndef _TVG_GL_STENCIL_PASS_H_
#define _TVG_GL_STENCIL_PASS_H_

#include "tvgGlCommon.h"
#include "tvgGlRenderTask.h"

class GlProgram;
class GlStageBuffer;
class GlRenderPass;
struct GlStencilAtlasTarget;

struct GlStencilRecord
{
    GlRenderTask* task = nullptr;
    GlStencilAtlasCoverTask* coverTask = nullptr;
    const GlGeometryBuffer* buffer = nullptr;
    RenderRegion screenBounds{};
    Matrix viewMatrix = {};
    uint32_t targetWidth = 0;
    uint32_t targetHeight = 0;
    GlStencilMode mode = GlStencilMode::None;
    int32_t atlasX = -1;
    int32_t atlasY = -1;
};

class GlStencilPass
{
public:
    GlStencilPass(uint32_t screenWidth, uint32_t screenHeight);
    ~GlStencilPass();

    void reset();
    GlRenderTask* prepare(Array<GlStencilRecord>& records, GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLint restoreId = 0);

    uint32_t maxAtlasWidth = 0, maxAtlasHeight = 0;

private:
    GlStencilAtlasTarget* acquireTarget(uint32_t width, uint32_t height, GLint restoreId);
    GlRenderTask* buildTask(Array<GlStencilRecord>& records, const RenderRegion& coverBounds, GlStageBuffer& gpuBuffer, GlProgram* coverProgram,
                            GlStencilAtlasTarget* atlasTarget) const;

    Array<GlStencilAtlasTarget*> targets = {};
};

struct GlStencilPassManager
{
    struct RecordSet
    {
        GlRenderPass* pass = nullptr;
        Array<GlStencilRecord> records = {};
    };

    GlStencilPassManager(uint32_t screenWidth, uint32_t screenHeight);
    ~GlStencilPassManager();

    void record(GlRenderPass* pass, GlStencilAtlasCoverTask* coverTask, GlRenderTask* task, const GlGeometryBuffer* buffer,
                const RenderRegion& meshBounds, const RenderRegion& viewRegion, const Matrix& viewMatrix, GlStencilMode mode);
    bool prepare(GlRenderPass* pass, GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLint restoreId = 0);
    void clearRecords();

    GlStencilPass mPass;
    Array<RecordSet*> mRecordSets = {};
    uint32_t mScreenWidth = 0, mScreenHeight = 0;
};

#endif //_TVG_GL_STENCIL_PASS_H_
