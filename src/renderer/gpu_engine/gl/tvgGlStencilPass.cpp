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

#include "tvgGlStencilPass.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlProfiler.h"
#include "tvgGlRenderPass.h"
#include "tvgGlRenderTarget.h"
#include "tvgGlRenderTask.h"

static constexpr uint32_t RECT_INDEX_COUNT = 6;
static constexpr uint32_t RECT_INDEX[] = {0, 1, 2, 2, 1, 3};
static constexpr float COVER_VERTEX[] = {-1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f};

GlStencilAtlasPolicy::GlStencilAtlasPolicy(uint32_t gpuMaxRenderableSidePx, uint32_t sampleCount)
{
    if (gpuMaxRenderableSidePx == 0) gpuMaxRenderableSidePx = MaxPortableSide;
    if (sampleCount == 0) sampleCount = 4;

    this->sampleCount = sampleCount;
    maxPageSide = (gpuMaxRenderableSidePx < MaxPortableSide) ? gpuMaxRenderableSidePx : MaxPortableSide;

    auto bytesPerPixel = static_cast<uint64_t>(4) * sampleCount;
    auto budgetPixels = BudgetBytes / bytesPerPixel;
    auto sidePixels = static_cast<uint64_t>(maxPageSide) * maxPageSide;
    maxPagePixels = (budgetPixels < sidePixels) ? budgetPixels : sidePixels;
    veryLargeMaskPixels = maxPagePixels / 4;
}

bool GlStencilAtlasPolicy::maskFits(uint32_t width, uint32_t height) const
{
    if (width == 0 || height == 0 || width > maxPageSide || height > maxPageSide) return false;

    auto area = static_cast<uint64_t>(width) * height;
    if (area > maxPagePixels) return false;
    return area < veryLargeMaskPixels;
}

bool GlStencilAtlasPolicy::allocationFits(uint32_t width, uint32_t height) const
{
    if (width == 0 || height == 0 || width > maxPageSide || height > maxPageSide) return false;
    return static_cast<uint64_t>(width) * height <= maxPagePixels;
}

bool GlStencilAtlasPolicy::shouldUseSharedAtlas(uint32_t directPathDrawCalls, uint32_t atlasMaskBuildDrawCalls,
                                               uint32_t atlasBatchedContentDrawCalls) const
{
    auto atlasDrawCalls = static_cast<uint64_t>(atlasMaskBuildDrawCalls) + atlasBatchedContentDrawCalls;
    if (directPathDrawCalls < atlasDrawCalls) return false;
    return static_cast<uint64_t>(directPathDrawCalls) - atlasDrawCalls >= MinSavedDrawCalls;
}

bool GlStencilAtlasPolicy::shouldUseSharedAtlasForRecords(uint32_t recordCount, bool hasNonZero, bool hasEvenOdd) const
{
    auto atlasMaskBuildDrawCalls = 1u;
    if (hasNonZero) ++atlasMaskBuildDrawCalls;
    if (hasEvenOdd) ++atlasMaskBuildDrawCalls;
    return shouldUseSharedAtlas(recordCount * 2, atlasMaskBuildDrawCalls, recordCount);
}

static uint32_t _atlasSize(uint32_t value)
{
    if (value == 0) return 0;
    uint32_t ret = 1;
    while (ret < value) ret <<= 1;
    return ret;
}

static RenderRegion _atlasViewport(const GlStencilRecord& record, uint32_t atlasHeight)
{
    auto w = static_cast<int32_t>(record.screenBounds.w());
    auto h = static_cast<int32_t>(record.screenBounds.h());
    auto x = record.atlasX;
    auto y = static_cast<int32_t>(atlasHeight) - record.atlasY - h;
    return {{x, y}, {x + w, y + h}};
}

static RenderRegion _atlasViewport(const RenderRegion& bounds, uint32_t atlasHeight)
{
    auto y = static_cast<int32_t>(atlasHeight);
    return {{bounds.min.x, y - bounds.max.y}, {bounds.max.x, y - bounds.min.y}};
}

static bool _atlasEligible(const RenderRegion& bounds, const GlStencilAtlasPolicy& policy, GlStencilMode mode)
{
    if (mode == GlStencilMode::Stroke || bounds.invalid()) return false;
    return policy.maskFits(bounds.w(), bounds.h());
}

struct GlStencilBatchBuffer
{
    GlProgram* program = nullptr;
    Array<float> vertices = {};
    Array<uint32_t> indices = {};
};

struct GlStencilBatch
{
    GlStencilMode mode = GlStencilMode::None;
    GlRenderTask* task = nullptr;
};

struct GlStencilAtlasTarget
{
    GlRenderTarget target;
    bool leased = false;
};

struct GlStencilGpuTimer
{
    GLuint query = 0;
    const char* section = nullptr;
    uint64_t runId = 0;
    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;
    uint32_t samples = 0;
    uint32_t batches = 0;
    uint64_t clearSamples = 0;
    uint64_t blitPixels = 0;
    uint64_t coverPixels = 0;
};

static Array<GlStencilGpuTimer> _gpuTimerPending;

static bool _gpuTimerSupported()
{
#if defined(THORVG_GL_TARGET_GL)
    static int supported = -1;
    if (supported >= 0) return supported == 1;

    supported = (glGenQueries && glDeleteQueries && glBeginQuery && glEndQuery &&
                 glGetQueryObjectiv && glGetQueryObjectui64v) ? 1 : 0;
    if (!supported && tvgGlStencilAtlasProfileEnabled()) {
        tvgGlStencilAtlasProfileLog("atlas-gpu unavailable reason=timer-query-functions");
    }
    return supported == 1;
#else
    static bool reported = false;
    if (!reported && tvgGlStencilAtlasProfileEnabled()) {
        tvgGlStencilAtlasProfileLog("atlas-gpu unavailable reason=timer-query-target-not-core-gl");
        reported = true;
    }
    return false;
#endif
}

static void _drainGpuTimers()
{
    if (!_gpuTimerSupported()) return;

    for (uint32_t i = 0; i < _gpuTimerPending.count;) {
        auto& timer = _gpuTimerPending[i];
        GLint available = 0;
        GL_CHECK(glGetQueryObjectiv(timer.query, GL_QUERY_RESULT_AVAILABLE, &available));

        if (!available) {
            ++i;
            continue;
        }

        GLuint64 gpuNs = 0;
        GL_CHECK(glGetQueryObjectui64v(timer.query, GL_QUERY_RESULT, &gpuNs));
        GL_CHECK(glDeleteQueries(1, &timer.query));

        tvgGlStencilAtlasProfileLog("atlas-gpu section=%s run=%llu gpuNs=%llu gpuUs=%.3f atlas=%ux%u samples=%u clearSamples=%llu blitPixels=%llu coverPixels=%llu batches=%u",
                                   timer.section,
                                   static_cast<unsigned long long>(timer.runId),
                                   static_cast<unsigned long long>(gpuNs),
                                   static_cast<double>(gpuNs) / 1000.0,
                                   timer.atlasWidth, timer.atlasHeight, timer.samples,
                                   static_cast<unsigned long long>(timer.clearSamples),
                                   static_cast<unsigned long long>(timer.blitPixels),
                                   static_cast<unsigned long long>(timer.coverPixels),
                                   timer.batches);

        _gpuTimerPending[i] = _gpuTimerPending.last();
        _gpuTimerPending.pop();
    }
}

static GlStencilGpuTimer _beginGpuTimer(const char* section, uint64_t runId, uint32_t atlasWidth, uint32_t atlasHeight,
                                        uint32_t samples, uint32_t batches, uint64_t clearSamples,
                                        uint64_t blitPixels, uint64_t coverPixels)
{
    GlStencilGpuTimer timer;
    if (!tvgGlStencilAtlasProfileEnabled() || !_gpuTimerSupported()) return timer;

    timer.section = section;
    timer.runId = runId;
    timer.atlasWidth = atlasWidth;
    timer.atlasHeight = atlasHeight;
    timer.samples = samples;
    timer.batches = batches;
    timer.clearSamples = clearSamples;
    timer.blitPixels = blitPixels;
    timer.coverPixels = coverPixels;

    GL_CHECK(glGenQueries(1, &timer.query));
    if (timer.query == 0) return {};
    GL_CHECK(glBeginQuery(GL_TIME_ELAPSED, timer.query));
    return timer;
}

static void _endGpuTimer(GlStencilGpuTimer& timer)
{
    if (timer.query == 0) return;
    GL_CHECK(glEndQuery(GL_TIME_ELAPSED));
    _gpuTimerPending.push(timer);
    timer.query = 0;
}

static GlRenderTargetDesc _stencilAtlasDesc()
{
    GlRenderTargetDesc desc;
    desc.colorInternalFormat = GL_R8;
    desc.colorFormat = GL_RED;
    desc.minFilter = GL_NEAREST;
    desc.magFilter = GL_NEAREST;
    return desc;
}

static GlStencilBatchBuffer& _batchBuffer(GlStencilBatchBuffer* buffers, GlStencilMode mode)
{
    return buffers[static_cast<uint32_t>(mode) - static_cast<uint32_t>(GlStencilMode::FillNonZero)];
}

static Point _targetPoint(const GlStencilRecord& record, float x, float y)
{
    auto ndc = Point{x, y} * record.viewMatrix;
    return {
        (ndc.x + 1.0f) * 0.5f * static_cast<float>(record.targetWidth),
        (ndc.y + 1.0f) * 0.5f * static_cast<float>(record.targetHeight)
    };
}

static void _appendBatch(GlStencilBatchBuffer& batch, const GlStencilRecord& record,
                         const RenderRegion& atlasViewport, uint32_t atlasWidth, uint32_t atlasHeight)
{
    auto buffer = record.buffer;
    auto baseVertex = batch.vertices.count / 2;
    auto invAtlasW = 2.0f / static_cast<float>(atlasWidth);
    auto invAtlasH = 2.0f / static_cast<float>(atlasHeight);
    auto atlasMinX = static_cast<float>(atlasViewport.min.x);
    auto atlasMinY = static_cast<float>(atlasViewport.min.y);
    auto screenMinX = static_cast<float>(record.screenBounds.min.x);
    auto screenMinY = static_cast<float>(record.screenBounds.min.y);

    for (uint32_t i = 0; i + 1 < buffer->vertex.count; i += 2) {
        auto target = _targetPoint(record, buffer->vertex[i], buffer->vertex[i + 1]);
        auto winX = atlasMinX + (target.x - screenMinX);
        auto winY = atlasMinY + (target.y - screenMinY);
        batch.vertices.push(winX * invAtlasW - 1.0f);
        batch.vertices.push(winY * invAtlasH - 1.0f);
    }

    for (uint32_t i = 0; i < buffer->index.count; ++i) {
        batch.indices.push(buffer->index[i] + baseVertex);
    }
}

static void _pushBatchTask(Array<GlStencilBatch>& batches, GlStencilMode mode, GlStencilBatchBuffer& batchBuffer,
                           GlStageBuffer& gpuBuffer, const RenderRegion& viewport)
{
    if (!batchBuffer.program || batchBuffer.vertices.empty() || batchBuffer.indices.empty()) return;

    auto task = new GlRenderTask(batchBuffer.program);
    task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), gpuBuffer.push(batchBuffer.vertices.data, batchBuffer.vertices.count * sizeof(float))});
    task->setDrawRange(gpuBuffer.pushIndex(batchBuffer.indices.data, batchBuffer.indices.count * sizeof(uint32_t)), batchBuffer.indices.count);
    task->setViewport(viewport);
    task->setDrawDepth(0);

    batches.push({mode, task});
}

static void _configureCoverTask(GlStencilRecord& record, GLuint textureId, const RenderRegion& atlasViewport,
                                uint32_t atlasWidth, uint32_t atlasHeight, GlStageBuffer& gpuBuffer)
{
    if (!record.coverTask || textureId == 0 || atlasWidth == 0 || atlasHeight == 0) return;

    auto invAtlasW = 1.0f / static_cast<float>(atlasWidth);
    auto invAtlasH = 1.0f / static_cast<float>(atlasHeight);
    auto atlasMinX = static_cast<float>(atlasViewport.min.x);
    auto atlasMinY = static_cast<float>(atlasViewport.min.y);
    auto screenMinX = static_cast<float>(record.screenBounds.min.x);
    auto screenMinY = static_cast<float>(record.screenBounds.min.y);

    // Bake atlas UVs into the cover quad once instead of updating per-cover
    // transform/bounds uniforms; the atlas mask is nearest-filtered and maps 1:1.
    const float coverVertex[] = {
        static_cast<float>(record.meshBounds.min.x), static_cast<float>(record.meshBounds.min.y),
        static_cast<float>(record.meshBounds.min.x), static_cast<float>(record.meshBounds.max.y),
        static_cast<float>(record.meshBounds.max.x), static_cast<float>(record.meshBounds.min.y),
        static_cast<float>(record.meshBounds.max.x), static_cast<float>(record.meshBounds.max.y)
    };
    float atlasUv[8];

    for (uint32_t i = 0; i < 4; ++i) {
        auto target = _targetPoint(record, coverVertex[i * 2], coverVertex[i * 2 + 1]);
        atlasUv[i * 2] = (atlasMinX + target.x - screenMinX) * invAtlasW;
        atlasUv[i * 2 + 1] = (atlasMinY + target.y - screenMinY) * invAtlasH;
    }

    record.coverTask->setStencilAtlas(textureId, gpuBuffer.push(atlasUv, sizeof(atlasUv)));
}

struct GlStencilPassTask : GlRenderTask
{
    GlStencilPassTask(GlStencilAtlasTarget* atlasTarget, Array<GlStencilBatch>& batches, GlRenderTask* coverTask):
        GlRenderTask(nullptr), atlasTarget(atlasTarget),
        targetFbo(atlasTarget->target.fbo), resolveFbo(atlasTarget->target.resolvedFbo),
        width(atlasTarget->target.width), height(atlasTarget->target.height),
        samples(_stencilAtlasDesc().samples)
    {
        batches.move(this->batches);
        this->coverTask = coverTask;
    }

    ~GlStencilPassTask() override
    {
        ARRAY_FOREACH(p, batches) delete(p->task);
        delete(coverTask);
        atlasTarget->leased = false;
    }

    void normalizeDrawDepth(int32_t maxDepth) override
    {
        ARRAY_FOREACH(p, batches) p->task->normalizeDrawDepth(maxDepth);
        coverTask->normalizeDrawDepth(maxDepth);
    }

    void run() override
    {
        static uint64_t nextRunId = 0;
        auto profiling = tvgGlStencilAtlasProfileEnabled();
        auto profileStart = profiling ? tvgGlStencilAtlasProfileNowUs() : 0;
        auto runId = profiling ? ++nextRunId : 0;
        auto atlasPixels = static_cast<uint64_t>(width) * height;
        auto clearSamples = atlasPixels * samples;
        auto coverViewport = coverTask->getViewport();
        auto coverPixels = static_cast<uint64_t>(coverViewport.sw()) * coverViewport.sh();

        if (profileStart) {
            _drainGpuTimers();
            tvgGlStencilAtlasProfileLog("atlas-run begin run=%llu atlas=%ux%u samples=%u clearSamples=%llu blitPixels=%llu coverViewport=%dx%d coverPixels=%llu batches=%u",
                                       static_cast<unsigned long long>(runId),
                                       width, height, samples,
                                       static_cast<unsigned long long>(clearSamples),
                                       static_cast<unsigned long long>(atlasPixels),
                                       coverViewport.sw(), coverViewport.sh(),
                                       static_cast<unsigned long long>(coverPixels),
                                       batches.count);
        }

        GLint restoreFbo = 0;
        GLint restoreViewport[4] = {};
        GLint restoreScissor[4] = {};
        GL_CHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &restoreFbo));
        GL_CHECK(glGetIntegerv(GL_VIEWPORT, restoreViewport));
        GL_CHECK(glGetIntegerv(GL_SCISSOR_BOX, restoreScissor));

        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, targetFbo));
        GL_CHECK(glViewport(0, 0, width, height));
        GL_CHECK(glEnable(GL_SCISSOR_TEST));
        GL_CHECK(glScissor(0, 0, width, height));
        GL_CHECK(glDisable(GL_DEPTH_TEST));
        GL_CHECK(glDisable(GL_BLEND));
        GL_CHECK(glColorMask(1, 1, 1, 1));
        GL_CHECK(glClearColor(0, 0, 0, 0));
        GL_CHECK(glClearStencil(0));
        auto clearTimer = _beginGpuTimer("clear", runId, width, height, samples, batches.count, clearSamples, atlasPixels, coverPixels);
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
        _endGpuTimer(clearTimer);

        GL_CHECK(glEnable(GL_STENCIL_TEST));

        auto stencilTimer = _beginGpuTimer("stencil-batches", runId, width, height, samples, batches.count, clearSamples, atlasPixels, coverPixels);
        ARRAY_FOREACH(p, batches) {
            auto& batch = *p;

            GL_CHECK(glViewport(0, 0, width, height));
            GL_CHECK(glScissor(0, 0, width, height));

            if (batch.mode == GlStencilMode::FillEvenOdd) {
                GL_CHECK(glStencilFunc(GL_ALWAYS, 0x0, 0xFF));
                GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT));
            } else {
                GL_CHECK(glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF));
                GL_CHECK(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP));

                GL_CHECK(glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF));
                GL_CHECK(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP));
            }

            GL_CHECK(glColorMask(0, 0, 0, 0));
            batch.task->run();
        }
        _endGpuTimer(stencilTimer);

        GL_CHECK(glViewport(coverViewport.sx(), coverViewport.sy(), coverViewport.sw(), coverViewport.sh()));
        GL_CHECK(glScissor(coverViewport.sx(), coverViewport.sy(), coverViewport.sw(), coverViewport.sh()));
        GL_CHECK(glStencilFunc(GL_NOTEQUAL, 0x0, 0xFF));
        GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
        GL_CHECK(glColorMask(1, 1, 1, 1));
        auto coverTimer = _beginGpuTimer("cover", runId, width, height, samples, batches.count, clearSamples, atlasPixels, coverPixels);
        coverTask->run();
        _endGpuTimer(coverTimer);

        GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, targetFbo));
        GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo));
        auto blitTimer = _beginGpuTimer("blit", runId, width, height, samples, batches.count, clearSamples, atlasPixels, coverPixels);
        GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
        _endGpuTimer(blitTimer);

#if defined(THORVG_GL_TARGET_GLES)
        GLenum attachments[2] = {GL_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT};
        GL_CHECK(glInvalidateFramebuffer(GL_READ_FRAMEBUFFER, 2, attachments));
#endif

        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, restoreFbo));
        GL_CHECK(glViewport(restoreViewport[0], restoreViewport[1], restoreViewport[2], restoreViewport[3]));
        GL_CHECK(glScissor(restoreScissor[0], restoreScissor[1], restoreScissor[2], restoreScissor[3]));
        GL_CHECK(glEnable(GL_DEPTH_TEST));
        GL_CHECK(glEnable(GL_BLEND));
        GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        GL_CHECK(glDisable(GL_STENCIL_TEST));
        GL_CHECK(glColorMask(1, 1, 1, 1));

        if (profileStart) {
            tvgGlStencilAtlasProfileLog("atlas-run end run=%llu submitUs=%llu atlas=%ux%u clearSamples=%llu blitPixels=%llu",
                                       static_cast<unsigned long long>(runId),
                                       static_cast<unsigned long long>(tvgGlStencilAtlasProfileNowUs() - profileStart),
                                       width, height,
                                       static_cast<unsigned long long>(clearSamples),
                                       static_cast<unsigned long long>(atlasPixels));
            _drainGpuTimers();
        }
    }

    GlStencilAtlasTarget* atlasTarget = nullptr;
    GLuint targetFbo = 0;
    GLuint resolveFbo = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t samples = 0;
    Array<GlStencilBatch> batches = {};
    GlRenderTask* coverTask = nullptr;
};

struct GlStencilAtlasLayout
{
    RenderRegion bounds{};
    uint32_t recordCount = 0;
};

struct GlStencilAtlasCandidate
{
    bool valid = false;
    uint32_t packWidth = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t area = 0;
};

static GlStencilAtlasLayout _placeRecordsInRows(Array<GlStencilRecord>& records, uint32_t atlasWidth, uint32_t atlasHeight)
{
    auto atlasW = static_cast<int32_t>(atlasWidth);
    auto atlasH = static_cast<int32_t>(atlasHeight);
    int32_t x = 0, y = 0, rowH = 0;
    GlStencilAtlasLayout layout;
    bool packed = false;

    for (uint32_t i = 0; i < records.count; ++i) {
        auto& record = records[i];
        record.atlasX = record.atlasY = -1;

        auto w = static_cast<int32_t>(record.screenBounds.w());
        auto h = static_cast<int32_t>(record.screenBounds.h());
        if (w <= 0 || h <= 0 || w > atlasW || h > atlasH) return {};

        if (x + w > atlasW) {
            x = 0;
            y += rowH;
            rowH = 0;
        }

        if (y + h > atlasH) return {};

        record.atlasX = x;
        record.atlasY = y;

        RenderRegion bounds = {{x, y}, {x + w, y + h}};
        if (packed) layout.bounds.add(bounds);
        else {
            layout.bounds = bounds;
            packed = true;
        }

        x += w;
        if (rowH < h) rowH = h;
        ++layout.recordCount;
    }

    return layout;
}

static void _considerAtlasPackWidth(Array<GlStencilRecord>& records, const GlStencilAtlasPolicy& policy,
                                    uint32_t packWidth, GlStencilAtlasCandidate& best)
{
    if (packWidth == 0 || packWidth > policy.maxPageSide) return;

    auto layout = _placeRecordsInRows(records, packWidth, policy.maxPageSide);
    if (layout.bounds.invalid() || layout.recordCount != records.count) return;

    auto allocatedWidth = _atlasSize(layout.bounds.w());
    auto allocatedHeight = _atlasSize(layout.bounds.h());
    if (!policy.allocationFits(allocatedWidth, allocatedHeight)) return;

    auto allocatedArea = static_cast<uint64_t>(allocatedWidth) * allocatedHeight;
    if (!best.valid || allocatedArea < best.area ||
        (allocatedArea == best.area && allocatedWidth < best.width)) {
        best.valid = true;
        best.packWidth = packWidth;
        best.width = allocatedWidth;
        best.height = allocatedHeight;
        best.area = allocatedArea;
    }
}

static GlStencilAtlasLayout _findSmallestAtlasAllocation(Array<GlStencilRecord>& records, const GlStencilAtlasPolicy& policy,
                                                        uint32_t& atlasWidth, uint32_t& atlasHeight)
{
    atlasWidth = atlasHeight = 0;
    if (records.empty()) return {};

    uint32_t maxRecordWidth = 0;
    uint32_t maxRecordHeight = 0;
    for (uint32_t i = 0; i < records.count; ++i) {
        auto& bounds = records[i].screenBounds;
        if (bounds.invalid() || !policy.maskFits(bounds.w(), bounds.h())) return {};
        if (maxRecordWidth < bounds.w()) maxRecordWidth = bounds.w();
        if (maxRecordHeight < bounds.h()) maxRecordHeight = bounds.h();
    }

    auto minPackWidth = _atlasSize(maxRecordWidth);
    if (minPackWidth == 0 || minPackWidth > policy.maxPageSide || maxRecordHeight > policy.maxPageSide) return {};

    GlStencilAtlasCandidate best;

    for (auto packWidth = minPackWidth; packWidth <= policy.maxPageSide;) {
        _considerAtlasPackWidth(records, policy, packWidth, best);
        if (packWidth > policy.maxPageSide / 2) break;
        packWidth <<= 1;
    }
    _considerAtlasPackWidth(records, policy, policy.maxPageSide, best);

    if (!best.valid) return {};

    auto layout = _placeRecordsInRows(records, best.packWidth, policy.maxPageSide);
    atlasWidth = best.width;
    atlasHeight = best.height;
    return layout;
}

GlStencilPass::GlStencilPass(TVG_UNUSED uint32_t screenWidth, TVG_UNUSED uint32_t screenHeight,
                             uint32_t gpuMaxRenderableSidePx):
    policy(gpuMaxRenderableSidePx, _stencilAtlasDesc().samples),
    maxAtlasWidth(policy.maxPageSide), maxAtlasHeight(policy.maxPageSide)
{
}

GlStencilPass::~GlStencilPass()
{
    reset();
}

void GlStencilPass::reset()
{
    for (uint32_t i = 0; i < targets.count; ++i) delete targets[i];
    targets.clear();
}

GlStencilAtlasTarget* GlStencilPass::acquireTarget(uint32_t width, uint32_t height, GLint restoreId)
{
    if (width == 0 || height == 0) return nullptr;
    if (!policy.allocationFits(width, height)) return nullptr;

    GlStencilAtlasTarget* best = nullptr;
    for (uint32_t i = 0; i < targets.count; ++i) {
        auto target = targets[i];
        if (target->leased || target->target.width < width || target->target.height < height) continue;

        if (!best) {
            best = target;
            continue;
        }

        auto targetArea = static_cast<uint64_t>(target->target.width) * target->target.height;
        auto bestArea = static_cast<uint64_t>(best->target.width) * best->target.height;
        if (targetArea < bestArea) best = target;
    }

    if (!best) {
        best = new GlStencilAtlasTarget;
        best->target.init(width, height, restoreId, _stencilAtlasDesc());
        if (best->target.invalid()) {
            delete best;
            return nullptr;
        }
        targets.push(best);
    }

    best->leased = true;
    return best;
}

GlRenderTask* GlStencilPass::buildTask(Array<GlStencilRecord>& records, const RenderRegion& coverBounds,
                                       GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GlStencilAtlasTarget* atlasTarget) const
{
    if (!atlasTarget || !coverProgram || coverBounds.invalid()) return nullptr;

    auto atlasWidth = atlasTarget->target.width;
    auto atlasHeight = atlasTarget->target.height;
    RenderRegion atlasViewport = {{0, 0}, {static_cast<int32_t>(atlasWidth), static_cast<int32_t>(atlasHeight)}};

    GlStencilBatchBuffer batchBuffers[2];

    for (uint32_t i = 0; i < records.count; ++i) {
        auto& record = records[i];
        if (record.atlasX < 0 || !record.task || !record.buffer) continue;

        auto& batch = _batchBuffer(batchBuffers, record.mode);
        if (!batch.program) batch.program = record.task->getProgram();
        batch.vertices.reserve(batch.vertices.count + record.buffer->vertex.count);
        batch.indices.reserve(batch.indices.count + record.buffer->index.count);
        auto recordViewport = _atlasViewport(record, atlasHeight);
        _appendBatch(batch, record, recordViewport, atlasWidth, atlasHeight);
    }

    Array<GlStencilBatch> batches;
    _pushBatchTask(batches, GlStencilMode::FillNonZero, batchBuffers[0], gpuBuffer, atlasViewport);
    _pushBatchTask(batches, GlStencilMode::FillEvenOdd, batchBuffers[1], gpuBuffer, atlasViewport);

    if (batches.empty()) {
        atlasTarget->leased = false;
        return nullptr;
    }

    for (uint32_t i = 0; i < records.count; ++i) {
        auto& record = records[i];
        if (record.atlasX < 0) continue;
        _configureCoverTask(record, atlasTarget->target.colorTex, _atlasViewport(record, atlasHeight), atlasWidth, atlasHeight, gpuBuffer);
    }

    auto coverTask = new GlRenderTask(coverProgram);
    coverTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), gpuBuffer.push((void*)COVER_VERTEX, sizeof(COVER_VERTEX))});
    coverTask->setDrawRange(gpuBuffer.pushIndex((void*)RECT_INDEX, sizeof(RECT_INDEX)), RECT_INDEX_COUNT);
    coverTask->setViewport(_atlasViewport(coverBounds, atlasHeight));
    coverTask->setDrawDepth(0);
    coverTask->setVertexColor(1.0f, 1.0f, 1.0f, 1.0f);

    return new GlStencilPassTask(atlasTarget, batches, coverTask);
}

GlRenderTask* GlStencilPass::prepare(Array<GlStencilRecord>& records, uint32_t recordCount, bool hasNonZero, bool hasEvenOdd,
                                     GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLint restoreId)
{
    if (!coverProgram) return nullptr;

    if (!policy.shouldUseSharedAtlasForRecords(recordCount, hasNonZero, hasEvenOdd)) return nullptr;

    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;
    auto layout = _findSmallestAtlasAllocation(records, policy, atlasWidth, atlasHeight);
    if (layout.bounds.invalid()) return nullptr;

    auto atlasTarget = acquireTarget(atlasWidth, atlasHeight, restoreId);
    return buildTask(records, layout.bounds, gpuBuffer, coverProgram, atlasTarget);
}

GlStencilPassManager::GlStencilPassManager(uint32_t screenWidth, uint32_t screenHeight, uint32_t gpuMaxRenderableSidePx):
    mPass(screenWidth, screenHeight, gpuMaxRenderableSidePx)
{
}

GlStencilPassManager::~GlStencilPassManager()
{
    clearRecords();
}

static RenderRegion _stencilTargetBounds(const RenderRegion& meshBounds, const Matrix& viewMatrix,
                                         uint32_t targetWidth, uint32_t targetHeight)
{
    auto w = static_cast<float>(targetWidth);
    auto h = static_cast<float>(targetHeight);
    Matrix ndcToTarget = {w * 0.5f, 0.0f, w * 0.5f,
                          0.0f, h * 0.5f, h * 0.5f,
                          0.0f, 0.0f, 1.0f};

    return gpuTransformBounds(meshBounds, ndcToTarget * viewMatrix);
}

void GlStencilPassManager::record(GlRenderPass* pass, GlStencilAtlasCoverTask* coverTask, GlRenderTask* task,
                                  const GlGeometryBuffer* buffer, const RenderRegion& meshBounds,
                                  const Matrix& viewMatrix, GlStencilMode mode)
{
    if (!pass || !coverTask) return;

    auto target = pass->getViewport();
    // Batch draws use one atlas-wide scissor, so each packed rect must cover
    // the full stencil geometry rather than the final clipped cover region.
    auto screenBounds = _stencilTargetBounds(meshBounds, viewMatrix, target.w(), target.h());
    if (!_atlasEligible(screenBounds, mPass.policy, mode)) return;

    RecordSet* set = nullptr;
    for (uint32_t i = 0; i < mRecordSets.count; ++i) {
        if (mRecordSets[i]->pass == pass) {
            set = mRecordSets[i];
            break;
        }
    }

    if (!set) {
        set = new RecordSet;
        set->pass = pass;
        mRecordSets.push(set);
    }

    set->records.push({task, coverTask, buffer, meshBounds, screenBounds, viewMatrix,
                       static_cast<uint32_t>(target.w()), static_cast<uint32_t>(target.h()), mode});
    ++set->recordCount;
    if (mode == GlStencilMode::FillEvenOdd) set->hasEvenOdd = true;
    else if (mode == GlStencilMode::FillNonZero) set->hasNonZero = true;
}

bool GlStencilPassManager::prepare(GlRenderPass* pass, GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLint restoreId)
{
    RecordSet* set = nullptr;
    for (uint32_t i = 0; i < mRecordSets.count; ++i) {
        if (mRecordSets[i]->pass == pass) {
            set = mRecordSets[i];
            mRecordSets[i] = mRecordSets.last();
            mRecordSets.pop();
            break;
        }
    }

    if (!set) return false;
    if (set->records.empty()) {
        delete set;
        return false;
    }

    auto task = mPass.prepare(set->records, set->recordCount, set->hasNonZero, set->hasEvenOdd,
                              gpuBuffer, coverProgram, restoreId);
    delete set;

    if (!task) return false;
    pass->prependRenderTask(task);
    return true;
}

void GlStencilPassManager::clearRecords()
{
    for (uint32_t i = 0; i < mRecordSets.count; ++i) delete mRecordSets[i];
    mRecordSets.clear();
}
