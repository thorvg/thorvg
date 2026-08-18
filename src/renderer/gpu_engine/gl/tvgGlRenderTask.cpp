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

#include "tvgGlRenderTask.h"
#include "tvgGlRenderPass.h"

#if !defined(THORVG_GL_TARGET_GL)
static void clearColorTarget(GlStateCache& state, uint32_t width, uint32_t height)
{
    state.scissor(0, 0, width, height);
    state.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
}
#endif

/************************************************************************/
/* GlRenderTask Class Implementation                                    */
/************************************************************************/

void GlRenderTask::run(GlStateCache& state)
{
    // bind shader
    state.useProgram(program->getProgramId());

    int32_t dLoc = program->getUniformLocation(GlShaderUniform::Depth);
    if (dLoc >= 0) {
        // fixme: prevent compiler warning: macro expands to multiple statements [-Wmultistatement-macros]
        GL_CHECK(glUniform1f(dLoc, drawDepth));
    }

    int32_t vLoc = program->getUniformLocation(GlShaderUniform::ViewMatrix);
    if (vLoc >= 0) {
        const auto& matrix = useViewMatrix ? viewMatrix : tvg::identity();
        float viewMat3[9];
        getMatrix3(matrix, viewMat3);
        GL_CHECK(glUniformMatrix3fv(vLoc, 1, GL_FALSE, viewMat3));
    }

    if (useVertexColor) {
        int32_t cLoc = program->getUniformLocation(GlShaderUniform::Color);
        if (cLoc >= 0) GL_CHECK(glUniform4fv(cLoc, 1, vertexColor));
    }

    // setup scissor rect
    state.scissor(viewport.sx(), viewport.sy(), viewport.sw(), viewport.sh());

    // setup attribute layout
    state.beginVertexLayout();
    bool hasVertexColorLayout = false;
    for (uint32_t i = 0; i < vertexLayout.count; i++) {
        const auto& layout = vertexLayout[i];
        assert(layout.arrayBufferId);
        if (layout.index == 1) hasVertexColorLayout = true;
        state.setVertexAttribPointer(layout.index, layout.size, layout.type, layout.normalized, layout.stride, layout.offset, layout.arrayBufferId);
    }
    if (useVertexColor && !hasVertexColorLayout) state.setVertexAttrib4f(1, vertexColor[0], vertexColor[1], vertexColor[2], vertexColor[3]);
    state.endVertexLayout();

    // binding uniforms
    for (uint32_t i = 0; i < bindResources.count; i++) {
        const auto& binding = bindResources[i];
        if (binding.type == GlBindingType::kTexture) {
            state.bindTexture2D(GL_TEXTURE0 + binding.bindPoint, binding.resourceId);
            program->setSampler(binding.uniform, static_cast<int32_t>(binding.bindPoint));
        } else if (binding.type == GlBindingType::kUniformBuffer) {
            GL_CHECK(glBindBufferRange(GL_UNIFORM_BUFFER, binding.bindPoint, binding.resourceId, binding.bufferOffset, binding.bufferRange));
        }
    }

    if (useDrawArrays) {
        GL_CHECK(glDrawArrays(arrayMode, arrayOffset, indexCnt));
    } else {
        GL_CHECK(glDrawElements(GL_TRIANGLES, indexCnt, GL_UNSIGNED_INT, reinterpret_cast<void*>(indexOffset)));
    }

}

void GlRenderTask::addVertexLayout(const GlVertexLayout& layout)
{
    assert(layout.arrayBufferId);
    vertexLayout.push(layout);
}

void GlRenderTask::setVertexColor(float r, float g, float b, float a)
{
    useVertexColor = true;
    vertexColor[0] = r;
    vertexColor[1] = g;
    vertexColor[2] = b;
    vertexColor[3] = a;
}

void GlRenderTask::addBindResource(const GlBindingResource& binding)
{
    if (binding.type == GlBindingType::kUniformBuffer) {
        assert(program);
        if (program->getUniformBlockIndex(binding.uniformBlock) < 0) return;
        if (!program->setUniformBlockBinding(binding.uniformBlock, binding.bindPoint)) {
            TVGERR("GL_ENGINE", "Conflicting uniform block binding for program %u", program->getProgramId());
            return;
        }
    }
    bindResources.push(binding);
}

void GlRenderTask::setDrawRange(uint32_t offset, uint32_t count)
{
    useDrawArrays = false;
    indexOffset = offset;
    indexCnt = count;
}


void GlRenderTask::setViewport(const RenderRegion &viewport)
{
    this->viewport = viewport;
    this->viewport.max.x = std::max(viewport.min.x, viewport.max.x);
    this->viewport.max.y = std::max(viewport.min.y, viewport.max.y);
}


/************************************************************************/
/* GlStencilCoverTask Class Implementation                              */
/************************************************************************/

GlStencilCoverTask::GlStencilCoverTask(GlRenderTask* stencil, GlRenderTask* cover, GlStencilMode mode) :
    GlRenderTask(nullptr), stencilMode(mode)
{
    stencilTasks.push(stencil);
    coverTasks.push(cover);
}


GlStencilCoverTask::~GlStencilCoverTask()
{
    ARRAY_FOREACH(p, stencilTasks) delete (*p);
    ARRAY_FOREACH(p, coverTasks) delete (*p);
    stencilTasks.clear();
    coverTasks.clear();
}

void GlStencilCoverTask::run(GlStateCache& state)
{
    state.enable(GL_STENCIL_TEST);

    if (stencilMode == GlStencilMode::Stroke) {
        state.stencilFunc(GL_NOTEQUAL, 0x1, 0xFF);
        state.stencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    } else {
        state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF);
        state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);

        state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF);
        state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    }
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    ARRAY_FOREACH(p, stencilTasks) (*p)->run(state);

    if (stencilMode == GlStencilMode::FillEvenOdd) {
        state.stencilFunc(GL_NOTEQUAL, 0x00, 0x01);
        state.stencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE);
    } else {
        state.stencilFunc(GL_NOTEQUAL, 0x0, 0xFF);
        state.stencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    ARRAY_FOREACH(p, coverTasks) (*p)->run(state);

    state.disable(GL_STENCIL_TEST);
}


void GlStencilCoverTask::normalizeDrawDepth(int32_t maxDepth)
{
    ARRAY_FOREACH(p, coverTasks) (*p)->normalizeDrawDepth(maxDepth);
    ARRAY_FOREACH(p, stencilTasks) (*p)->normalizeDrawDepth(maxDepth);
}

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
/************************************************************************/
/* GlDirectAaTask Class Implementation                                  */
/************************************************************************/

GlDirectAaTask::GlDirectAaTask(GlRenderTarget* dstTarget,
                               GlRenderTask* stencilTask,
                               GlRenderTask* boundaryTask,
                               GlRenderTask* coverTask,
                               GlStencilMode mode,
                               const RenderRegion& shapeRegion,
                               uint32_t passWidth, uint32_t passHeight) : GlRenderTask(nullptr), mDstTarget(dstTarget), mStencilTask(stencilTask),
                                                                          mBoundaryTask(boundaryTask), mCoverTask(coverTask),
                                                                          mStencilMode(mode), mShapeRegion(shapeRegion),
                                                                          mPassWidth(passWidth), mPassHeight(passHeight)
{
}

GlDirectAaTask::~GlDirectAaTask()
{
    delete mStencilTask;
    delete mBoundaryTask;
    delete mCoverTask;
}

void GlDirectAaTask::run(GlStateCache& state)
{
    auto stencilMask = (mStencilMode == GlStencilMode::FillEvenOdd) ? 0x01 : 0xFF;

    state.bindFramebuffer(GL_FRAMEBUFFER, mDstTarget->fbo);
    state.viewport(0, 0, mPassWidth, mPassHeight);
    state.scissor(mShapeRegion.sx(), mShapeRegion.sy(),
                  mShapeRegion.sw(), mShapeRegion.sh());
    state.stencilMask(0xFF);
    state.depthMask(GL_TRUE);
    state.clearStencil(0);
    state.clearDepth(0.0);
    GL_CHECK(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    // Classify the complete fill before analytical boundary patches paint.
    state.disable(GL_DEPTH_TEST);
    state.disable(GL_BLEND);
    state.enable(GL_STENCIL_TEST);
    state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    mStencilTask->run(state);

    // A boundary fragment writes fractional color and binary depth occupancy.
    // The following solid cover is therefore kept out of the analytical band.
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.disable(GL_STENCIL_TEST);
    state.enable(GL_DEPTH_TEST);
    state.depthFunc(GL_ALWAYS);
    state.depthMask(GL_TRUE);
    state.enable(GL_BLEND);
    state.blendEquation(GL_FUNC_ADD);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    mBoundaryTask->run(state);

    state.enable(GL_STENCIL_TEST);
    state.stencilFunc(GL_NOTEQUAL, 0x0, stencilMask);
    state.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    state.depthFunc(GL_GREATER);
    state.depthMask(GL_FALSE);
    mCoverTask->run(state);

    state.disable(GL_STENCIL_TEST);
    // Direct AA uses depth only as temporary per-shape occupancy. Do not leak
    // that binary band (or the winding classification) into a later fallback.
    state.stencilMask(0xFF);
    state.depthMask(GL_TRUE);
    state.clearStencil(0);
    state.clearDepth(0.0);
    GL_CHECK(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    state.depthMask(GL_FALSE);
    state.depthFunc(GL_GREATER);
}

void GlDirectAaTask::normalizeDrawDepth(int32_t maxDepth)
{
    mStencilTask->normalizeDrawDepth(maxDepth);
    mBoundaryTask->normalizeDrawDepth(maxDepth);
    mCoverTask->normalizeDrawDepth(maxDepth);
}

/************************************************************************/
/* GlFlatMaskTask Class Implementation                                  */
/************************************************************************/

GlFlatMaskTask::GlFlatMaskTask(GlFlatMaskTarget* maskTarget, GlRenderTarget* dstTarget,
                               GlRenderTask* stencilTask, GlRenderTask* interiorTask,
                               GlRenderTask* insidePositiveTask, GlRenderTask* insideNegativeTask,
                               GlRenderTask* outsideTask,
                               GlRenderTask* compositeTask, GlStencilMode mode,
                               const RenderRegion& maskRegion, uint32_t passWidth,
                               uint32_t passHeight)
    : GlRenderTask(nullptr), mMaskTarget(maskTarget), mDstTarget(dstTarget),
      mStencilTask(stencilTask), mInteriorTask(interiorTask), mInsidePositiveTask(insidePositiveTask),
      mInsideNegativeTask(insideNegativeTask), mOutsideTask(outsideTask),
      mCompositeTask(compositeTask), mStencilMode(mode),
      mMaskRegion(maskRegion), mPassWidth(passWidth), mPassHeight(passHeight)
{
}

GlFlatMaskTask::~GlFlatMaskTask()
{
    delete mStencilTask;
    delete mInteriorTask;
    delete mInsidePositiveTask;
    delete mInsideNegativeTask;
    delete mOutsideTask;
    delete mCompositeTask;
}

void GlFlatMaskTask::run(GlStateCache& state)
{
    auto stencilMask = (mStencilMode == GlStencilMode::FillEvenOdd) ? 0x01 : 0xFF;

    state.bindFramebuffer(GL_FRAMEBUFFER, mMaskTarget->fbo);
    state.viewport(0, 0, mPassWidth, mPassHeight);
    state.scissor(mMaskRegion.sx(), mMaskRegion.sy(), mMaskRegion.sw(), mMaskRegion.sh());
    state.disable(GL_DEPTH_TEST);
    state.disable(GL_BLEND);
    state.enable(GL_STENCIL_TEST);
    state.stencilMask(0xFF);
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    state.clearStencil(0);
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    mStencilTask->run(state);

    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.stencilFunc(GL_NOTEQUAL, 0x0, stencilMask);
    state.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    mInteriorTask->run(state);

    // Inside starts at one and takes the nearest edge through MIN; outside
    // starts at zero and takes the nearest edge through MAX.
    state.enable(GL_BLEND);
    state.blendEquation(GL_MIN);
    if (mStencilMode == GlStencilMode::FillEvenOdd) {
        state.stencilFunc(GL_NOTEQUAL, 0x0, 0x01);
        mInsidePositiveTask->run(state);
        mInsideNegativeTask->run(state);
    } else {
        // A directed edge is a NonZero fill boundary only where its left side
        // has winding +1, or its right side has winding -1. Other nonzero
        // transitions are internal to the filled region.
        state.stencilFunc(GL_EQUAL, 0x01, 0xFF);
        mInsidePositiveTask->run(state);
        state.stencilFunc(GL_EQUAL, 0xFF, 0xFF);
        mInsideNegativeTask->run(state);
    }

    state.stencilFunc(GL_EQUAL, 0x0, stencilMask);
    state.blendEquation(GL_MAX);
    mOutsideTask->run(state);

    state.blendEquation(GL_FUNC_ADD);
    state.disable(GL_STENCIL_TEST);
    state.bindFramebuffer(GL_FRAMEBUFFER, mDstTarget->fbo);
    state.viewport(0, 0, mPassWidth, mPassHeight);
    state.scissor(mMaskRegion.sx(), mMaskRegion.sy(), mMaskRegion.sw(), mMaskRegion.sh());
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.depthMask(GL_FALSE);
    state.enable(GL_DEPTH_TEST);
    state.depthFunc(GL_GREATER);
    state.enable(GL_BLEND);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // Shape color and effective opacity are applied only in this one draw.
    mCompositeTask->run(state);
}

void GlFlatMaskTask::normalizeDrawDepth(int32_t maxDepth)
{
    mStencilTask->normalizeDrawDepth(maxDepth);
    mInteriorTask->normalizeDrawDepth(maxDepth);
    mInsidePositiveTask->normalizeDrawDepth(maxDepth);
    mInsideNegativeTask->normalizeDrawDepth(maxDepth);
    mOutsideTask->normalizeDrawDepth(maxDepth);
    mCompositeTask->normalizeDrawDepth(maxDepth);
}


/************************************************************************/
/* GlCurveMaskTask Class Implementation                                 */
/************************************************************************/

GlCurveMaskTask::GlCurveMaskTask(GlFlatMaskTarget* maskTarget, GlRenderTarget* dstTarget,
                                 GlRenderTask* stencilTask, GlRenderTask* interiorTask,
                                 GlRenderTask* boundaryTask, GlRenderTask* compositeTask,
                                 GlStencilMode mode, const RenderRegion& maskRegion,
                                 uint32_t passWidth, uint32_t passHeight)
    : GlRenderTask(nullptr), mMaskTarget(maskTarget), mDstTarget(dstTarget),
      mStencilTask(stencilTask), mInteriorTask(interiorTask),
      mBoundaryTask(boundaryTask), mCompositeTask(compositeTask),
      mStencilMode(mode), mMaskRegion(maskRegion),
      mPassWidth(passWidth), mPassHeight(passHeight)
{
}

GlCurveMaskTask::~GlCurveMaskTask()
{
    delete mStencilTask;
    delete mInteriorTask;
    delete mBoundaryTask;
    delete mCompositeTask;
}

void GlCurveMaskTask::run(GlStateCache& state)
{
    auto stencilMask = (mStencilMode == GlStencilMode::FillEvenOdd) ? 0x01 : 0xFF;

    state.bindFramebuffer(GL_FRAMEBUFFER, mMaskTarget->fbo);
    state.viewport(0, 0, mPassWidth, mPassHeight);
    state.scissor(mMaskRegion.sx(), mMaskRegion.sy(), mMaskRegion.sw(), mMaskRegion.sh());
    state.disable(GL_DEPTH_TEST);
    state.disable(GL_BLEND);
    state.enable(GL_STENCIL_TEST);
    state.stencilMask(0xFF);
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    state.clearStencil(0);
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

    // Reuse the renderer's regular tessellated fill to classify winding.
    state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    mStencilTask->run(state);

    // Red stores binary inside/outside classification.
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.stencilFunc(GL_NOTEQUAL, 0x0, stencilMask);
    state.stencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    mInteriorTask->run(state);

    // Green stores the strongest proximity, i.e. the nearest original path
    // segment. Overlapping patches merge coverage data, never shape color.
    state.disable(GL_STENCIL_TEST);
    state.enable(GL_BLEND);
    state.blendEquation(GL_MAX);
    mBoundaryTask->run(state);

    state.blendEquation(GL_FUNC_ADD);
    state.bindFramebuffer(GL_FRAMEBUFFER, mDstTarget->fbo);
    state.viewport(0, 0, mPassWidth, mPassHeight);
    state.scissor(mMaskRegion.sx(), mMaskRegion.sy(), mMaskRegion.sw(), mMaskRegion.sh());
    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.depthMask(GL_FALSE);
    state.enable(GL_DEPTH_TEST);
    state.depthFunc(GL_GREATER);
    state.enable(GL_BLEND);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // Resolve classification + nearest boundary and apply color exactly once.
    mCompositeTask->run(state);
}

void GlCurveMaskTask::normalizeDrawDepth(int32_t maxDepth)
{
    mStencilTask->normalizeDrawDepth(maxDepth);
    mInteriorTask->normalizeDrawDepth(maxDepth);
    mBoundaryTask->normalizeDrawDepth(maxDepth);
    mCompositeTask->normalizeDrawDepth(maxDepth);
}
#endif


/************************************************************************/
/* GlComposeTask Class Implementation                                   */
/************************************************************************/

GlComposeTask::GlComposeTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks) :
    GlRenderTask(program), targetFbo(target), fbo(fbo)
{
    this->tasks.push(tasks);
    tasks.clear();
}

GlComposeTask::~GlComposeTask()
{
    ARRAY_FOREACH(p, tasks) delete (*p);
    tasks.clear();
}

void GlComposeTask::run(GlStateCache& state)
{
    state.bindFramebuffer(GL_FRAMEBUFFER, getSelfFbo());

    // we must clear all area of fbo
    state.viewport(0, 0, fbo->width, fbo->height);
    state.scissor(0, 0, fbo->width, fbo->height);
    state.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    state.clearStencil(0);
    state.clearDepth(0.0);
    state.depthMask(GL_TRUE);

    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    state.depthMask(GL_FALSE);
    state.viewport(0, 0, renderWidth, renderHeight);
    state.scissor(0, 0, renderWidth, renderHeight);

    ARRAY_FOREACH(p, tasks) (*p)->run(state);

#if defined(THORVG_GL_TARGET_GLES)
    // only OpenGLES has tiled base framebuffer and discard function
    GLenum attachments[2] = {GL_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT };
    GL_CHECK(glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, attachments));
#endif
    // reset scissor box
    state.scissor(0, 0, fbo->width, fbo->height);
    onResolve(state);
}


void GlComposeTask::onResolve(GlStateCache& state)
{
    if (fbo->samples == 1) return;

    state.bindFramebuffer(GL_READ_FRAMEBUFFER, getSelfFbo());
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, getResolveFboId());
    GL_CHECK(glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, renderWidth, renderHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST));
}

/************************************************************************/
/* GlBlitTask Class Implementation                                      */
/************************************************************************/

void GlBlitTask::run(GlStateCache& state)
{
    GlComposeTask::run(state);

    state.bindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    state.viewport(targetViewport.x(), targetViewport.y(), targetViewport.w(), targetViewport.h());

    if (clearBuffer) {
        state.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    }

    state.disable(GL_DEPTH_TEST);
    // make sure the blending is correct
    state.enable(GL_BLEND);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GlRenderTask::run(state);
}


/************************************************************************/
/* GlDrawBlitTask Class Implementation                                  */
/************************************************************************/

void GlDrawBlitTask::run(GlStateCache& state)
{
    if (prevTask) prevTask->run(state);

    GlComposeTask::run(state);

    state.bindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    state.viewport(0, 0, parentWidth, parentHeight);
    state.scissor(0, 0, parentWidth, parentHeight);

    GlRenderTask::run(state);
}


/************************************************************************/
/* GlSceneBlendTask Class Implementation                                  */
/************************************************************************/

void GlSceneBlendTask::run(GlStateCache& state)
{
    GlComposeTask::run(state);

    const auto width = srcFbo->width;
    const auto height = srcFbo->height;
    if (width <= 0 || height <= 0) return;

    const auto& vp = viewport;

#if defined(THORVG_GL_TARGET_GL)
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, targetFbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo->textureFbo());
    state.viewport(0, 0, dstCopyFbo->width, dstCopyFbo->height);
    state.scissor(0, 0, dstCopyFbo->width, dstCopyFbo->height);
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, 0, 0, vp.w(), vp.h(), GL_COLOR_BUFFER_BIT, GL_LINEAR));
#else // TODO: create partial buffer when MSAA is disabled
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo->textureFbo());
    if (vp.min.x != 0 || vp.min.y != 0 || dstCopyFbo->width != static_cast<uint32_t>(vp.w()) || dstCopyFbo->height != static_cast<uint32_t>(vp.h())) clearColorTarget(state, dstCopyFbo->width, dstCopyFbo->height);
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo->fbo);
    state.viewport(0, 0, width, height);
    state.scissor(vp.min.x, vp.min.y, vp.w(), vp.h());
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, vp.min.x, vp.min.y, vp.max.x, vp.max.y, GL_COLOR_BUFFER_BIT, GL_NEAREST));
#endif

    state.bindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    state.viewport(0, 0, parentWidth, parentHeight);
    state.scissor(0, 0, parentWidth, parentHeight);

    state.disable(GL_DEPTH_TEST);
    state.blendFunc(GL_ONE, GL_ZERO);
    GlRenderTask::run(state);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    state.enable(GL_DEPTH_TEST);
}


/************************************************************************/
/* GlClipTask Class Implementation                                      */
/************************************************************************/

void GlClipTask::run(GlStateCache& state)
{
    state.enable(GL_STENCIL_TEST);
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    // draw clip path as normal stencil mask
    state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x1, 0xFF);
    state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);

    state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x1, 0xFF);
    state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);

    clipTask->run(state);

    // draw clip mask
    state.depthMask(GL_TRUE);
    state.stencilFunc(GL_EQUAL, 0x0, 0xFF);
    state.stencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE);

    maskTask->run(state);

    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.depthMask(GL_FALSE);
    state.disable(GL_STENCIL_TEST);
}


void GlClipTask::normalizeDrawDepth(int32_t maxDepth)
{
    clipTask->normalizeDrawDepth(maxDepth);
    maskTask->normalizeDrawDepth(maxDepth);
}

/************************************************************************/
/* GlDirectBlendTask Class Implementation                               */
/************************************************************************/

void GlDirectBlendTask::run(GlStateCache& state)
{
    auto width = copyRegion.w();
    auto height = copyRegion.h();
    if (width <= 0 || height <= 0) return;

    auto x = copyRegion.sx();
    auto y = copyRegion.sy();
    const auto fboW = dstFbo->width;
    const auto fboH = dstFbo->height;
    if (fboW <= 0 || fboH <= 0) return;

    state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo->textureFbo());

#if defined(THORVG_GL_TARGET_GL)
    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);
    GL_CHECK(glBlitFramebuffer(x, y, x + width, y + height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR));
#else // TODO: create partial buffer when MSAA is disabled
    if (x != 0 || y != 0 || dstCopyFbo->width != static_cast<uint32_t>(width) || dstCopyFbo->height != static_cast<uint32_t>(height)) clearColorTarget(state, dstCopyFbo->width, dstCopyFbo->height);
    state.viewport(0, 0, fboW, fboH);
    state.scissor(x, y, width, height);
    GL_CHECK(glBlitFramebuffer(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
#endif
    state.bindFramebuffer(GL_FRAMEBUFFER, dstFbo->fbo);
    const auto& dstVp = dstFbo->viewport;
    state.viewport(0, 0, dstVp.w(), dstVp.h());

    state.blendFunc(GL_ONE, GL_ZERO);
    GlRenderTask::run(state);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


/************************************************************************/
/* GlComplexBlendTask Class Implementation                              */
/************************************************************************/

void GlComplexBlendTask::run(GlStateCache& state)
{
    composeTask->run(state);

    const auto width = dstFbo->width;
    const auto height = dstFbo->height;
    if (width <= 0 || height <= 0) return;

    state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo->textureFbo());

    const auto& vp = viewport;

#if defined(THORVG_GL_TARGET_GL)
    const auto& dstVp = dstFbo->viewport;
    // copy the current fbo to the dstCopyFbo
    state.viewport(0, 0, dstVp.w(), dstVp.h());
    state.scissor(0, 0, dstVp.w(), dstVp.h());
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, 0, 0, vp.w(), vp.h(), GL_COLOR_BUFFER_BIT, GL_LINEAR));
#else // TODO: create partial buffer when MSAA is disabled
    if (vp.min.x != 0 || vp.min.y != 0 || dstCopyFbo->width != static_cast<uint32_t>(vp.w()) || dstCopyFbo->height != static_cast<uint32_t>(vp.h())) clearColorTarget(state, dstCopyFbo->width, dstCopyFbo->height);
    state.viewport(0, 0, width, height);
    state.scissor(vp.min.x, vp.min.y, vp.w(), vp.h());
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, vp.min.x, vp.min.y, vp.max.x, vp.max.y, GL_COLOR_BUFFER_BIT, GL_NEAREST));
#endif

    state.bindFramebuffer(GL_FRAMEBUFFER, dstFbo->fbo);

    state.enable(GL_STENCIL_TEST);
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);

    state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);

    stencilTask->run(state);

    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.stencilFunc(GL_NOTEQUAL, 0x0, 0xFF);
    state.stencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE);

    state.blendFunc(GL_ONE, GL_ZERO);

    GlRenderTask::run(state);

    state.disable(GL_STENCIL_TEST);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


void GlComplexBlendTask::normalizeDrawDepth(int32_t maxDepth)
{
    stencilTask->normalizeDrawDepth(maxDepth);
    GlRenderTask::normalizeDrawDepth(maxDepth);
}

/************************************************************************/
/* GlGaussianBlurTask Class Implementation                              */
/************************************************************************/

void GlGaussianBlurTask::run(GlStateCache& state)
{
    const auto width = dstFbo->width;
    const auto height = dstFbo->height;

    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);
    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo0->textureFbo());
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    state.bindFramebuffer(GL_FRAMEBUFFER, dstFbo->fbo);

    state.disable(GL_BLEND);
    state.depthFunc(GL_ALWAYS);
    if (effect->direction == 0) {
        state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
        state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo1->textureFbo());
        GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
        // horizontal blur
        state.bindFramebuffer(GL_FRAMEBUFFER, dstCopyFbo1->textureFbo());
        horzTask->setViewport(viewport);
        horzTask->addBindResource({0, dstCopyFbo0->colorTex, GlShaderUniform::SourceTexture});
        horzTask->run(state);
        // vertical blur
        state.bindFramebuffer(GL_FRAMEBUFFER, dstFbo->fbo);
        vertTask->setViewport(viewport);
        vertTask->addBindResource({0, dstCopyFbo1->colorTex, GlShaderUniform::SourceTexture});
        vertTask->run(state);
    } // horizontal
    else if (effect->direction == 1) {
        horzTask->setViewport(viewport);
        horzTask->addBindResource({0, dstCopyFbo0->colorTex, GlShaderUniform::SourceTexture});
        horzTask->run(state);
    } // vertical
    else if (effect->direction == 2) {
        vertTask->setViewport(viewport);
        vertTask->addBindResource({0, dstCopyFbo0->colorTex, GlShaderUniform::SourceTexture});
        vertTask->run(state);
    }
    state.depthFunc(GL_GREATER);
    state.enable(GL_BLEND);
}

/************************************************************************/
/* GlEffectDropShadowTask Class Implementation                          */
/************************************************************************/

void GlEffectDropShadowTask::run(GlStateCache& state)
{
    const auto width = dstFbo->width;
    const auto height = dstFbo->height;
;
    addBindResource({0, dstCopyFbo0->colorTex, GlShaderUniform::SourceTexture});
    addBindResource({1, dstCopyFbo1->colorTex, GlShaderUniform::BlurTexture});

    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);

    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo0->textureFbo());
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo1->textureFbo());
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

    state.disable(GL_BLEND);
    state.depthFunc(GL_ALWAYS);
    // when sigma is 0, no blur is applied, and the original image is used directly as the shadow.
    if (!tvg::zero(effect->sigma)) {
        // horizontal blur
        state.bindFramebuffer(GL_FRAMEBUFFER, dstCopyFbo0->textureFbo());
        horzTask->setViewport(viewport);
        horzTask->addBindResource({0, dstCopyFbo1->colorTex, GlShaderUniform::SourceTexture});
        horzTask->run(state);
        // vertical blur
        state.bindFramebuffer(GL_FRAMEBUFFER, dstCopyFbo1->textureFbo());
        vertTask->setViewport(viewport);
        vertTask->addBindResource({0, dstCopyFbo0->colorTex, GlShaderUniform::SourceTexture});
        vertTask->run(state);
        // copy original image to intermediate buffer
        state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
        state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo0->textureFbo());
        GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    }
    // run drop shadow effect
    state.bindFramebuffer(GL_FRAMEBUFFER, dstFbo->fbo);
    GlRenderTask::run(state);
    state.depthFunc(GL_GREATER);
    state.enable(GL_BLEND);
}

/************************************************************************/
/* GlEffectColorTransformTask Class Implementation                      */
/************************************************************************/

void GlEffectColorTransformTask::run(GlStateCache& state)
{
    const auto width = dstFbo->width;
    const auto height = dstFbo->height;
    // get targets handles and pass to shader
    addBindResource({0, dstCopyFbo->colorTex, GlShaderUniform::SourceTexture});

    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);
    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, dstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, dstCopyFbo->textureFbo());
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    state.bindFramebuffer(GL_FRAMEBUFFER, dstFbo->fbo);

    // run transform
    state.disable(GL_BLEND);
    state.depthFunc(GL_ALWAYS);
    GlRenderTask::run(state);
    state.depthFunc(GL_GREATER);
    state.enable(GL_BLEND);
}
