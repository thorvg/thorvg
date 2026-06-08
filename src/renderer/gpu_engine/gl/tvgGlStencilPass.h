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

struct GlStencilAtlasPolicy
{
    static constexpr uint32_t MaxPortableSide = 4096;
    static constexpr uint64_t BudgetBytes = 32ull * 1024ull * 1024ull;
    static constexpr uint64_t MaxSharedAtlasSamples = 4ull * 1024ull * 1024ull;
    static constexpr uint32_t MinSavedDrawCalls = 64;

    GlStencilAtlasPolicy(uint32_t gpuMaxRenderableSidePx = MaxPortableSide, uint32_t sampleCount = 4);

    bool maskFits(uint32_t width, uint32_t height) const;
    bool allocationFits(uint32_t width, uint32_t height) const;
    bool shouldUseSharedAtlas(uint32_t directPathDrawCalls, uint32_t atlasMaskBuildDrawCalls,
                              uint32_t atlasBatchedContentDrawCalls) const;
    bool shouldUseSharedAtlasForRecords(uint32_t recordCount, bool hasNonZero, bool hasEvenOdd) const;

    uint32_t sampleCount = 4;
    uint32_t maxPageSide = MaxPortableSide;
    uint64_t maxPagePixels = 0;
    uint64_t maxAllocationPixels = 0;
    uint64_t veryLargeMaskPixels = 0;
};

static inline uint64_t glStencilAtlasActivePixels(const RenderRegion& region)
{
    if (region.invalid()) return 0;
    return static_cast<uint64_t>(region.sw()) * region.sh();
}

static inline uint64_t glStencilAtlasActiveSamples(const RenderRegion& region, uint32_t samples)
{
    return glStencilAtlasActivePixels(region) * (samples == 0 ? 1 : samples);
}

static inline RenderRegion glStencilAtlasRestoreViewport(const RenderRegion& passViewport)
{
    return {{0, 0}, {static_cast<int32_t>(passViewport.w()), static_cast<int32_t>(passViewport.h())}};
}

static inline uint32_t glStencilAtlasMaskBuildDrawCalls(bool hasNonZero, bool hasEvenOdd)
{
    auto modeDrawCalls = 0u;
    if (hasNonZero) ++modeDrawCalls;
    if (hasEvenOdd) ++modeDrawCalls;
    return modeDrawCalls == 0 ? 0 : modeDrawCalls + 1;
}

struct GlStencilRecord
{
    GlRenderTask* task = nullptr;
    GlStencilAtlasCoverTask* coverTask = nullptr;
    const GlGeometryBuffer* buffer = nullptr;
    RenderRegion meshBounds{};
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
    GlStencilPass(uint32_t screenWidth, uint32_t screenHeight,
                  uint32_t gpuMaxRenderableSidePx = GlStencilAtlasPolicy::MaxPortableSide);
    ~GlStencilPass();

    void reset();
    GlRenderTask* prepare(Array<GlStencilRecord>& records, uint32_t recordCount, bool hasNonZero, bool hasEvenOdd,
                          GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLuint restoreFbo,
                          const RenderRegion& restoreViewport, GLint targetRestoreId = 0);

    GlStencilAtlasPolicy policy;
    uint32_t maxAtlasWidth = 0, maxAtlasHeight = 0;

private:
    GlStencilAtlasTarget* acquireTarget(uint32_t width, uint32_t height, GLint restoreId);
    GlRenderTask* buildTask(Array<GlStencilRecord>& records, const RenderRegion& coverBounds, GlStageBuffer& gpuBuffer, GlProgram* coverProgram,
                            GlStencilAtlasTarget* atlasTarget, GLuint restoreFbo, const RenderRegion& restoreViewport) const;

    Array<GlStencilAtlasTarget*> targets = {};
};

struct GlStencilPassManager
{
    struct RecordSet
    {
        GlRenderPass* pass = nullptr;
        Array<GlStencilRecord> records = {};
        uint32_t recordCount = 0;
        bool hasNonZero = false;
        bool hasEvenOdd = false;
    };

    GlStencilPassManager(uint32_t screenWidth, uint32_t screenHeight,
                         uint32_t gpuMaxRenderableSidePx = GlStencilAtlasPolicy::MaxPortableSide);
    ~GlStencilPassManager();

    void record(GlRenderPass* pass, GlStencilAtlasCoverTask* coverTask, GlRenderTask* task, const GlGeometryBuffer* buffer,
                const RenderRegion& meshBounds, const Matrix& viewMatrix, GlStencilMode mode);
    bool prepare(GlRenderPass* pass, GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLint restoreId = 0);
    void clearRecords();

    GlStencilPass mPass;
    Array<RecordSet*> mRecordSets = {};
};

#endif //_TVG_GL_STENCIL_PASS_H_
