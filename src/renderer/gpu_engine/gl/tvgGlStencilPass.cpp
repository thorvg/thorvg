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

static GlStencilBatchBuffer& _batchBuffer(GlStencilBatchBuffer* buffers, GlStencilMode mode)
{
    return buffers[static_cast<uint32_t>(mode) - static_cast<uint32_t>(GlStencilMode::FillNonZero)];
}

static void _reserveBatch(GlStencilBatchBuffer& batch, const GlGeometryBuffer& buffer)
{
    batch.vertices.reserve(batch.vertices.count + buffer.vertex.count);
    batch.indices.reserve(batch.indices.count + buffer.index.count);
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

struct GlStencilPassTask : GlRenderTask
{
    GlStencilPassTask(GLuint targetFbo, GLuint resolveFbo, uint32_t width, uint32_t height, Array<GlStencilBatch>& batches, GlRenderTask* coverTask):
        GlRenderTask(nullptr), targetFbo(targetFbo), resolveFbo(resolveFbo), width(width), height(height)
    {
        batches.move(this->batches);
        this->coverTask = coverTask;
    }

    ~GlStencilPassTask() override
    {
        ARRAY_FOREACH(p, batches) delete(p->task);
        delete(coverTask);
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

        GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, targetFbo));
        GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo));
        GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, restoreFbo));
        GL_CHECK(glViewport(restoreViewport[0], restoreViewport[1], restoreViewport[2], restoreViewport[3]));
        GL_CHECK(glScissor(restoreScissor[0], restoreScissor[1], restoreScissor[2], restoreScissor[3]));
        GL_CHECK(glEnable(GL_DEPTH_TEST));
        GL_CHECK(glEnable(GL_BLEND));
        GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        GL_CHECK(glDisable(GL_STENCIL_TEST));
        GL_CHECK(glColorMask(1, 1, 1, 1));
    }

    GLuint targetFbo = 0;
    GLuint resolveFbo = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    Array<GlStencilBatch> batches = {};
    GlRenderTask* coverTask = nullptr;
};

static RenderRegion _packStencilRecords(Array<GlStencilRecord>& records, uint32_t width, uint32_t height)
{
    auto atlasW = static_cast<int32_t>(width);
    auto atlasH = static_cast<int32_t>(height);
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

GlStencilPass::GlStencilPass(uint32_t screenWidth, uint32_t screenHeight): width(_atlasSize(screenWidth)), height(_atlasSize(screenHeight))
{
    if (width == 0 || height == 0) return;
    viewport = {{0, 0}, {static_cast<int32_t>(width), static_cast<int32_t>(height)}};
}

GlStencilPass::~GlStencilPass()
{
    reset();
}

void GlStencilPass::init(GLint restoreId)
{
    if (fbo || width == 0 || height == 0) return;

    GL_CHECK(glGenFramebuffers(1, &fbo));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    GL_CHECK(glGenRenderbuffers(1, &colorBuffer));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer));
    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_R8, width, height));
    GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer));

    GL_CHECK(glGenRenderbuffers(1, &stencilBuffer));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, stencilBuffer));
    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, width, height));
    GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilBuffer));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    GL_CHECK(glGenTextures(1, &colorTex));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, colorTex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    GL_CHECK(glGenFramebuffers(1, &resolvedFbo));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, resolvedFbo));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, restoreId));
}

void GlStencilPass::reset()
{
    if (fbo == 0) return;

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CHECK(glDeleteFramebuffers(1, &fbo));
    GL_CHECK(glDeleteFramebuffers(1, &resolvedFbo));
    GL_CHECK(glDeleteRenderbuffers(1, &colorBuffer));
    GL_CHECK(glDeleteTextures(1, &colorTex));
    GL_CHECK(glDeleteRenderbuffers(1, &stencilBuffer));

    resolvedFbo = fbo = colorBuffer = colorTex = stencilBuffer = 0;
}

GlRenderTask* GlStencilPass::buildTask(Array<GlStencilRecord>& records, const RenderRegion& coverBounds,
                                       GlStageBuffer& gpuBuffer, GlProgram* coverProgram) const
{
    if (fbo == 0 || !coverProgram || coverBounds.invalid()) return nullptr;

    GlStencilBatchBuffer batchBuffers[2];

    for (uint32_t i = 0; i < records.count; ++i) {
        auto& record = records[i];
        if (record.atlasX < 0) continue;

        auto& batch = _batchBuffer(batchBuffers, record.mode);
        if (!batch.program) batch.program = record.task->getProgram();
        _reserveBatch(batch, *record.buffer);
    }

    for (uint32_t i = 0; i < records.count; ++i) {
        auto& record = records[i];
        if (record.atlasX < 0) continue;

        auto& batch = _batchBuffer(batchBuffers, record.mode);
        auto atlasViewport = _atlasViewport(record, height);
        _appendBatch(batch, record, atlasViewport, width, height);
    }

    Array<GlStencilBatch> batches;
    _pushBatchTask(batches, GlStencilMode::FillNonZero, batchBuffers[0], gpuBuffer, viewport);
    _pushBatchTask(batches, GlStencilMode::FillEvenOdd, batchBuffers[1], gpuBuffer, viewport);

    if (batches.empty()) return nullptr;

    auto coverTask = new GlRenderTask(coverProgram);
    coverTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), gpuBuffer.push((void*)COVER_VERTEX, sizeof(COVER_VERTEX))});
    coverTask->setDrawRange(gpuBuffer.pushIndex((void*)RECT_INDEX, sizeof(RECT_INDEX)), RECT_INDEX_COUNT);
    coverTask->setViewport(_atlasViewport(coverBounds, height));
    coverTask->setDrawDepth(0);
    coverTask->setVertexColor(1.0f, 1.0f, 1.0f, 1.0f);

    return new GlStencilPassTask(fbo, resolvedFbo, width, height, batches, coverTask);
}

GlRenderTask* GlStencilPass::prepare(Array<GlStencilRecord>& records, GlStageBuffer& gpuBuffer, GlProgram* coverProgram)
{
    auto coverBounds = _packStencilRecords(records, width, height);
    return buildTask(records, coverBounds, gpuBuffer, coverProgram);
}

GlStencilPassManager::GlStencilPassManager(uint32_t screenWidth, uint32_t screenHeight): mPass(screenWidth, screenHeight)
{
}

GlStencilPassManager::~GlStencilPassManager()
{
    clearRecords();
}

void GlStencilPassManager::init(GLint restoreId)
{
    mPass.init(restoreId);
}

void GlStencilPassManager::reset()
{
    clearRecords();
    mPass.reset();
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

void GlStencilPassManager::record(GlRenderPass* pass, GlRenderTask* task, const GlGeometryBuffer* buffer,
                                  const RenderRegion& meshBounds, const RenderRegion& viewRegion,
                                  const Matrix& viewMatrix, GlStencilMode mode)
{
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

    if (set->prepared) return;
    auto target = pass->getViewport();
    auto screenBounds = _stencilTargetBounds(meshBounds, viewMatrix, viewRegion, target.w(), target.h());
    set->records.push({task, buffer, screenBounds, viewMatrix,
                       static_cast<uint32_t>(target.w()), static_cast<uint32_t>(target.h()), mode});
}

bool GlStencilPassManager::prepare(GlRenderPass* pass, GlStageBuffer& gpuBuffer, GlProgram* coverProgram, GLint restoreId)
{
    RecordSet* set = nullptr;
    for (uint32_t i = 0; i < mRecordSets.count; ++i) {
        if (mRecordSets[i]->pass == pass) {
            set = mRecordSets[i];
            break;
        }
    }

    if (!set || set->prepared || set->records.empty()) return false;

    if (mPass.fbo == 0) mPass.init(restoreId);

    auto task = mPass.prepare(set->records, gpuBuffer, coverProgram);
    set->records.clear();
    set->prepared = true;

    if (task) pass->prependRenderTask(task);
    return true;
}

void GlStencilPassManager::clearRecords()
{
    for (uint32_t i = 0; i < mRecordSets.count; ++i) delete mRecordSets[i];
    mRecordSets.clear();
}
