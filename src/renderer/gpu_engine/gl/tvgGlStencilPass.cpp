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
#include "tvgGlRenderPass.h"
#include "tvgGlRenderTarget.h"
#include "tvgGlRenderTask.h"

static constexpr uint32_t RECT_INDEX_COUNT = 6;
static constexpr uint32_t RECT_INDEX[] = {0, 1, 2, 2, 1, 3};
static constexpr float COVER_VERTEX[] = {-1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f};

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

static bool _atlasEligible(const RenderRegion& bounds, uint32_t screenWidth, uint32_t screenHeight, GlStencilMode mode)
{
    if (mode == GlStencilMode::Stroke || bounds.invalid() || screenWidth == 0 || screenHeight == 0) return false;
    return (static_cast<uint64_t>(bounds.w()) * 4 < static_cast<uint64_t>(screenWidth) * 3) &&
           (static_cast<uint64_t>(bounds.h()) * 4 < static_cast<uint64_t>(screenHeight) * 3);
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

static GlRenderTargetDesc _stencilAtlasDesc()
{
    GlRenderTargetDesc desc;
    desc.colorInternalFormat = GL_R8;
    desc.colorFormat = GL_RED;
    desc.minFilter = GL_NEAREST;
    desc.magFilter = GL_NEAREST;
    desc.samples = 0;
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
                                uint32_t atlasWidth, uint32_t atlasHeight)
{
    if (!record.coverTask || textureId == 0 || atlasWidth == 0 || atlasHeight == 0) return;

    auto invAtlasW = 1.0f / static_cast<float>(atlasWidth);
    auto invAtlasH = 1.0f / static_cast<float>(atlasHeight);

    float transform[4] = {
        invAtlasW,
        invAtlasH,
        (static_cast<float>(atlasViewport.min.x) - static_cast<float>(record.screenBounds.min.x)) * invAtlasW,
        (static_cast<float>(atlasViewport.min.y) - static_cast<float>(record.screenBounds.min.y)) * invAtlasH
    };

    float bounds[4] = {
        static_cast<float>(atlasViewport.min.x) * invAtlasW,
        static_cast<float>(atlasViewport.min.y) * invAtlasH,
        static_cast<float>(atlasViewport.max.x) * invAtlasW,
        static_cast<float>(atlasViewport.max.y) * invAtlasH
    };

    record.coverTask->setStencilAtlas(textureId, transform, bounds);
}

struct GlStencilPassTask : GlRenderTask
{
    GlStencilPassTask(GlStencilAtlasTarget* atlasTarget, Array<GlStencilBatch>& batches, GlRenderTask* coverTask):
        GlRenderTask(nullptr), atlasTarget(atlasTarget),
        targetFbo(atlasTarget->target.fbo),
        width(atlasTarget->target.width), height(atlasTarget->target.height)
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
#if defined(THORVG_GL_TARGET_GL)
        GL_CHECK(glDisable(GL_MULTISAMPLE));
#endif
        GL_CHECK(glDisable(GL_DEPTH_TEST));
        GL_CHECK(glDisable(GL_BLEND));
        GL_CHECK(glColorMask(1, 1, 1, 1));
        GL_CHECK(glClearColor(0, 0, 0, 0));
        GL_CHECK(glClearStencil(0));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

        GL_CHECK(glEnable(GL_STENCIL_TEST));

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

        auto coverViewport = coverTask->getViewport();
        GL_CHECK(glViewport(coverViewport.sx(), coverViewport.sy(), coverViewport.sw(), coverViewport.sh()));
        GL_CHECK(glScissor(coverViewport.sx(), coverViewport.sy(), coverViewport.sw(), coverViewport.sh()));
        GL_CHECK(glStencilFunc(GL_NOTEQUAL, 0x0, 0xFF));
        GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
        GL_CHECK(glColorMask(1, 1, 1, 1));
        coverTask->run();

        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, restoreFbo));
        GL_CHECK(glViewport(restoreViewport[0], restoreViewport[1], restoreViewport[2], restoreViewport[3]));
        GL_CHECK(glScissor(restoreScissor[0], restoreScissor[1], restoreScissor[2], restoreScissor[3]));
        GL_CHECK(glEnable(GL_DEPTH_TEST));
        GL_CHECK(glEnable(GL_BLEND));
        GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        GL_CHECK(glDisable(GL_STENCIL_TEST));
#if defined(THORVG_GL_TARGET_GL)
        GL_CHECK(glEnable(GL_MULTISAMPLE));
#endif
        GL_CHECK(glColorMask(1, 1, 1, 1));
    }

    GlStencilAtlasTarget* atlasTarget = nullptr;
    GLuint targetFbo = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    Array<GlStencilBatch> batches = {};
    GlRenderTask* coverTask = nullptr;
};

static RenderRegion _packStencilRecords(Array<GlStencilRecord>& records, uint32_t maxAtlasWidth, uint32_t maxAtlasHeight)
{
    auto atlasW = static_cast<int32_t>(maxAtlasWidth);
    auto atlasH = static_cast<int32_t>(maxAtlasHeight);
    int32_t x = 0, y = 0, rowH = 0;
    RenderRegion packedBounds{};
    bool packed = false;

    for (uint32_t i = 0; i < records.count; ++i) {
        auto& record = records[i];
        record.atlasX = record.atlasY = -1;

        auto w = static_cast<int32_t>(record.screenBounds.w());
        auto h = static_cast<int32_t>(record.screenBounds.h());
        if (w <= 0 || h <= 0 || w > atlasW || h > atlasH) continue;

        if (x + w > atlasW) {
            x = 0;
            y += rowH;
            rowH = 0;
        }

        if (y + h > atlasH) continue;

        record.atlasX = x;
        record.atlasY = y;

        RenderRegion bounds = {{x, y}, {x + w, y + h}};
        if (packed) packedBounds.add(bounds);
        else {
            packedBounds = bounds;
            packed = true;
        }

        x += w;
        if (rowH < h) rowH = h;
    }

    return packedBounds;
}

GlStencilPass::GlStencilPass(uint32_t screenWidth, uint32_t screenHeight):
    maxAtlasWidth(_atlasSize(screenWidth)), maxAtlasHeight(_atlasSize(screenHeight))
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
        _configureCoverTask(record, atlasTarget->target.colorTex, _atlasViewport(record, atlasHeight), atlasWidth, atlasHeight);
    }

    auto coverTask = new GlRenderTask(coverProgram);
    coverTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), gpuBuffer.push((void*)COVER_VERTEX, sizeof(COVER_VERTEX))});
    coverTask->setDrawRange(gpuBuffer.pushIndex((void*)RECT_INDEX, sizeof(RECT_INDEX)), RECT_INDEX_COUNT);
    coverTask->setViewport(_atlasViewport(coverBounds, atlasHeight));
    coverTask->setDrawDepth(0);
    coverTask->setVertexColor(1.0f, 1.0f, 1.0f, 1.0f);

    return new GlStencilPassTask(atlasTarget, batches, coverTask);
}

GlRenderTask* GlStencilPass::prepare(Array<GlStencilRecord>& records, GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLint restoreId)
{
    if (!coverProgram) return nullptr;

    auto coverBounds = _packStencilRecords(records, maxAtlasWidth, maxAtlasHeight);
    if (coverBounds.invalid()) return nullptr;

    auto atlasTarget = acquireTarget(_atlasSize(coverBounds.w()), _atlasSize(coverBounds.h()), restoreId);
    return buildTask(records, coverBounds, gpuBuffer, coverProgram, atlasTarget);
}

GlStencilPassManager::GlStencilPassManager(uint32_t screenWidth, uint32_t screenHeight):
    mPass(screenWidth, screenHeight), mScreenWidth(screenWidth), mScreenHeight(screenHeight)
{
}

GlStencilPassManager::~GlStencilPassManager()
{
    clearRecords();
}

static RenderRegion _stencilTargetBounds(const RenderRegion& meshBounds, const Matrix& viewMatrix,
                                         const RenderRegion& viewRegion, uint32_t targetWidth, uint32_t targetHeight)
{
    auto w = static_cast<float>(targetWidth);
    auto h = static_cast<float>(targetHeight);
    Matrix ndcToTarget = {w * 0.5f, 0.0f, w * 0.5f,
                          0.0f, h * 0.5f, h * 0.5f,
                          0.0f, 0.0f, 1.0f};

    auto bounds = gpuTransformBounds(meshBounds, ndcToTarget * viewMatrix);
    bounds.intersect(viewRegion);
    return bounds;
}

void GlStencilPassManager::record(GlRenderPass* pass, GlStencilAtlasCoverTask* coverTask, GlRenderTask* task,
                                  const GlGeometryBuffer* buffer, const RenderRegion& meshBounds,
                                  const RenderRegion& viewRegion, const Matrix& viewMatrix, GlStencilMode mode)
{
    if (!pass || !coverTask) return;

    auto target = pass->getViewport();
    auto screenBounds = _stencilTargetBounds(meshBounds, viewMatrix, viewRegion, target.w(), target.h());
    if (!_atlasEligible(screenBounds, mScreenWidth, mScreenHeight, mode)) return;

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

    set->records.push({task, coverTask, buffer, screenBounds, viewMatrix,
                       static_cast<uint32_t>(target.w()), static_cast<uint32_t>(target.h()), mode});
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

    auto task = mPass.prepare(set->records, gpuBuffer, coverProgram, restoreId);
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
