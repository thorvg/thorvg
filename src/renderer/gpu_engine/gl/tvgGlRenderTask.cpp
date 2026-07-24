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
#include "tvgGlProgram.h"
#include "tvgGlRenderPass.h"
#include "tvgGlStateCache.h"

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
    state.useProgram(mProgram->getProgramId());

    int32_t dLoc = mProgram->getUniformLocation(GlShaderUniform::Depth);
    if (dLoc >= 0) {
        // fixme: prevent compiler warning: macro expands to multiple statements [-Wmultistatement-macros]
        GL_CHECK(glUniform1f(dLoc, mDrawDepth));
    }

    int32_t vLoc = mProgram->getUniformLocation(GlShaderUniform::ViewMatrix);
    if (vLoc >= 0) {
        const auto& viewMatrix = mUseViewMatrix ? mViewMatrix : tvg::identity();
        float viewMat3[9];
        getMatrix3(viewMatrix, viewMat3);
        GL_CHECK(glUniformMatrix3fv(vLoc, 1, GL_FALSE, viewMat3));
    }

    // setup scissor rect
    state.scissor(mViewport.sx(), mViewport.sy(), mViewport.sw(), mViewport.sh());

    // setup attribute layout
    state.beginVertexLayout();
    bool hasVertexColorLayout = false;
    for (uint32_t i = 0; i < mVertexLayout.count; i++) {
        const auto &layout = mVertexLayout[i];
        assert(layout.arrayBufferId);
        if (layout.index == 1) hasVertexColorLayout = true;
        state.setVertexAttribPointer(layout.index, layout.size, layout.type, layout.normalized,
                                     layout.stride, layout.offset, layout.arrayBufferId);
    }
    if (mUseVertexColor && !hasVertexColorLayout) {
        state.setVertexAttrib4f(1, mVertexColor[0], mVertexColor[1], mVertexColor[2], mVertexColor[3]);
    }
    state.endVertexLayout();

    // binding uniforms
    for (uint32_t i = 0; i < mBindingResources.count; i++) {
        const auto& binding = mBindingResources[i];
        if (binding.type == GlBindingType::kTexture) {
            state.bindTexture2D(GL_TEXTURE0 + binding.bindPoint, binding.resourceId);
            mProgram->setSampler(binding.uniform, static_cast<int32_t>(binding.bindPoint));
        } else if (binding.type == GlBindingType::kUniformBuffer) {
            GL_CHECK(glBindBufferRange(GL_UNIFORM_BUFFER, binding.bindPoint, binding.resourceId,
                                       binding.bufferOffset, binding.bufferRange));
        }
    }

    if (mUseDrawArrays) {
        GL_CHECK(glDrawArrays(mArrayMode, mArrayOffset, mIndexCount));
    } else {
        GL_CHECK(glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(mIndexOffset)));
    }

}


void GlRenderTask::addVertexLayout(const GlVertexLayout &layout)
{
    assert(layout.arrayBufferId);
    mVertexLayout.push(layout);
}

void GlRenderTask::setVertexColor(float r, float g, float b, float a)
{
    mUseVertexColor = true;
    mVertexColor[0] = r;
    mVertexColor[1] = g;
    mVertexColor[2] = b;
    mVertexColor[3] = a;
}

void GlRenderTask::addBindResource(const GlBindingResource &binding)
{
    if (binding.type == GlBindingType::kUniformBuffer) {
        assert(mProgram);
        if (mProgram->getUniformBlockIndex(binding.uniformBlock) < 0) return;
        if (!mProgram->setUniformBlockBinding(binding.uniformBlock, binding.bindPoint)) {
            TVGERR("GL_ENGINE", "Conflicting uniform block binding for program %u", mProgram->getProgramId());
            return;
        }
    }
    mBindingResources.push(binding);
}


void GlRenderTask::setDrawRange(uint32_t offset, uint32_t count)
{
    mUseDrawArrays = false;
    mIndexOffset = offset;
    mIndexCount = count;
}


void GlRenderTask::setViewport(const RenderRegion &viewport)
{
    mViewport = viewport;
    if (mViewport.max.x < mViewport.min.x) mViewport.max.x = mViewport.min.x;
    if (mViewport.max.y < mViewport.min.y) mViewport.max.y = mViewport.min.y;
}


/************************************************************************/
/* GlStencilCoverTask Class Implementation                              */
/************************************************************************/

GlStencilCoverTask::GlStencilCoverTask(GlRenderTask* stencil, GlRenderTask* cover, GlStencilMode mode)
 :GlRenderTask(nullptr), mStencilMode(mode)
{
    mStencilTasks.push(stencil);
    mCoverTasks.push(cover);
}


GlStencilCoverTask::~GlStencilCoverTask()
{
    ARRAY_FOREACH(p, mStencilTasks) delete(*p);
    ARRAY_FOREACH(p, mCoverTasks) delete(*p);
    mStencilTasks.clear();
    mCoverTasks.clear();
}

void GlStencilCoverTask::run(GlStateCache& state)
{
    state.enable(GL_STENCIL_TEST);

    if (mStencilMode == GlStencilMode::Stroke) {
        state.stencilFunc(GL_NOTEQUAL, 0x1, 0xFF);
        state.stencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    } else {
        state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF);
        state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);

        state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF);
        state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
    }
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    ARRAY_FOREACH(p, mStencilTasks)(*p)->run(state);

    if (mStencilMode == GlStencilMode::FillEvenOdd) {
        state.stencilFunc(GL_NOTEQUAL, 0x00, 0x01);
        state.stencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE);
    } else {
        state.stencilFunc(GL_NOTEQUAL, 0x0, 0xFF);
        state.stencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    ARRAY_FOREACH(p, mCoverTasks)(*p)->run(state);

    state.disable(GL_STENCIL_TEST);
}


void GlStencilCoverTask::normalizeDrawDepth(int32_t maxDepth)
{
    ARRAY_FOREACH(p, mCoverTasks) (*p)->normalizeDrawDepth(maxDepth);
    ARRAY_FOREACH(p, mStencilTasks) (*p)->normalizeDrawDepth(maxDepth);
}


/************************************************************************/
/* GlComposeTask Class Implementation                                   */
/************************************************************************/

GlComposeTask::GlComposeTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 :GlRenderTask(program) ,mTargetFbo(target), mFbo(fbo), mTasks()
{
    mTasks.push(tasks);
    tasks.clear();
}


GlComposeTask::~GlComposeTask()
{
    ARRAY_FOREACH(p, mTasks) delete(*p);
    mTasks.clear();
}

void GlComposeTask::run(GlStateCache& state)
{
    state.bindFramebuffer(GL_FRAMEBUFFER, getSelfFbo());

    // we must clear all area of fbo
    state.viewport(0, 0, mFbo->width, mFbo->height);
    state.scissor(0, 0, mFbo->width, mFbo->height);
    state.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    state.clearStencil(0);
    state.clearDepth(0.0);
    state.depthMask(GL_TRUE);

    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    state.depthMask(GL_FALSE);

    state.viewport(0, 0, mRenderWidth, mRenderHeight);
    state.scissor(0, 0, mRenderWidth, mRenderHeight);

    ARRAY_FOREACH(p, mTasks) {
        (*p)->run(state);
    }

#if defined(THORVG_GL_TARGET_GLES)
    // only OpenGLES has tiled base framebuffer and discard function
    GLenum attachments[2] = {GL_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT };
    GL_CHECK(glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, attachments));
#endif
    // reset scissor box
    state.scissor(0, 0, mFbo->width, mFbo->height);
    onResolve(state);
}


GLuint GlComposeTask::getSelfFbo()
{
    return mFbo->fbo;
}


GLuint GlComposeTask::getResolveFboId()
{
    return mFbo->resolvedFbo;
}

void GlComposeTask::onResolve(GlStateCache& state)
{
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, getSelfFbo());
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, getResolveFboId());
    GL_CHECK(glBlitFramebuffer(0, 0, mRenderWidth, mRenderHeight, 0, 0, mRenderWidth, mRenderHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST));
}


/************************************************************************/
/* GlBlitTask Class Implementation                                      */
/************************************************************************/

GlBlitTask::GlBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, fbo, std::move(tasks)), mColorTex(fbo->colorTex)
{
}

void GlBlitTask::run(GlStateCache& state)
{
    GlComposeTask::run(state);

    state.bindFramebuffer(GL_FRAMEBUFFER, getTargetFbo());
    state.viewport(mTargetViewport.x(), mTargetViewport.y(), mTargetViewport.w(), mTargetViewport.h());

    if (mClearBuffer) {
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


GlDrawBlitTask::GlDrawBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, fbo, std::move(tasks))
{
}


GlDrawBlitTask::~GlDrawBlitTask()
{
    if (mPrevTask) delete mPrevTask;
}

void GlDrawBlitTask::run(GlStateCache& state)
{
    if (mPrevTask) mPrevTask->run(state);

    GlComposeTask::run(state);

    state.bindFramebuffer(GL_FRAMEBUFFER, getTargetFbo());

    state.viewport(0, 0, mParentWidth, mParentHeight);
    state.scissor(0, 0, mParentWidth, mParentHeight);
    GlRenderTask::run(state);
}


/************************************************************************/
/* GlSceneBlendTask Class Implementation                                  */
/************************************************************************/


GlSceneBlendTask::GlSceneBlendTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, fbo, std::move(tasks))
{
}


GlSceneBlendTask::~GlSceneBlendTask()
{
}

void GlSceneBlendTask::run(GlStateCache& state)
{
    GlComposeTask::run(state);

    const auto& vp = getViewport();
    const auto width = mSrcFbo->width;
    const auto height = mSrcFbo->height;
    if (width <= 0 || height <= 0) return;


#if defined(THORVG_GL_TARGET_GL)
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, getTargetFbo());
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo->resolvedFbo);
    state.viewport(0, 0, mDstCopyFbo->width, mDstCopyFbo->height);
    state.scissor(0, 0, mDstCopyFbo->width, mDstCopyFbo->height);
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, 0, 0, vp.w(), vp.h(), GL_COLOR_BUFFER_BIT, GL_LINEAR));
#else // TODO: create partial buffer when MSAA is disabled
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo->resolvedFbo);
    if (vp.min.x != 0 || vp.min.y != 0 || mDstCopyFbo->width != static_cast<uint32_t>(vp.w()) || mDstCopyFbo->height != static_cast<uint32_t>(vp.h())) clearColorTarget(state, mDstCopyFbo->width, mDstCopyFbo->height);
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, mSrcFbo->fbo);
    state.viewport(0, 0, width, height);
    state.scissor(vp.min.x, vp.min.y, vp.w(), vp.h());
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, vp.min.x, vp.min.y, vp.max.x, vp.max.y, GL_COLOR_BUFFER_BIT, GL_NEAREST));
#endif

    state.bindFramebuffer(GL_FRAMEBUFFER, getTargetFbo());
    state.viewport(0, 0, mParentWidth, mParentHeight);
    state.scissor(0, 0, mParentWidth, mParentHeight);

    state.disable(GL_DEPTH_TEST);
    state.blendFunc(GL_ONE, GL_ZERO);
    GlRenderTask::run(state);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    state.enable(GL_DEPTH_TEST);
}


/************************************************************************/
/* GlClipTask Class Implementation                                      */
/************************************************************************/

GlClipTask::GlClipTask(GlRenderTask* clip, GlRenderTask* mask)
 :GlRenderTask(nullptr), mClipTask(clip), mMaskTask(mask) {}


GlClipTask::~GlClipTask()
{
    delete mClipTask;
    delete mMaskTask;
}

void GlClipTask::run(GlStateCache& state)
{
    state.enable(GL_STENCIL_TEST);
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    // draw clip path as normal stencil mask
    state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x1, 0xFF);
    state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);

    state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x1, 0xFF);
    state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);

    mClipTask->run(state);

    // draw clip mask
    state.depthMask(GL_TRUE);
    state.stencilFunc(GL_EQUAL, 0x0, 0xFF);
    state.stencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE);

    mMaskTask->run(state);

    state.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    state.depthMask(GL_FALSE);
    state.disable(GL_STENCIL_TEST);
}


void GlClipTask::normalizeDrawDepth(int32_t maxDepth)
{
    mClipTask->normalizeDrawDepth(maxDepth);
    mMaskTask->normalizeDrawDepth(maxDepth);
}

/************************************************************************/
/* GlDirectBlendTask Class Implementation                               */
/************************************************************************/

GlDirectBlendTask::GlDirectBlendTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo, const RenderRegion& copyRegion)
    : GlRenderTask(program), mDstFbo(dstFbo), mDstCopyFbo(dstCopyFbo), mCopyRegion(copyRegion)
{
}

void GlDirectBlendTask::run(GlStateCache& state)
{
    auto width = mCopyRegion.w();
    auto height = mCopyRegion.h();
    if (width <= 0 || height <= 0) return;
    auto x = mCopyRegion.sx();
    auto y = mCopyRegion.sy();
    const auto fboW = mDstFbo->width;
    const auto fboH = mDstFbo->height;
    if (fboW <= 0 || fboH <= 0) return;

    state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo->resolvedFbo);

#if defined(THORVG_GL_TARGET_GL)
    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);
    GL_CHECK(glBlitFramebuffer(x, y, x + width, y + height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR));
#else // TODO: create partial buffer when MSAA is disabled
    if (x != 0 || y != 0 || mDstCopyFbo->width != static_cast<uint32_t>(width) || mDstCopyFbo->height != static_cast<uint32_t>(height)) clearColorTarget(state, mDstCopyFbo->width, mDstCopyFbo->height);
    state.viewport(0, 0, fboW, fboH);
    state.scissor(x, y, width, height);
    GL_CHECK(glBlitFramebuffer(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
#endif
    state.bindFramebuffer(GL_FRAMEBUFFER, mDstFbo->fbo);
    const auto& dstVp = mDstFbo->viewport;
    state.viewport(0, 0, dstVp.w(), dstVp.h());

    state.blendFunc(GL_ONE, GL_ZERO);
    GlRenderTask::run(state);
    state.blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


/************************************************************************/
/* GlComplexBlendTask Class Implementation                              */
/************************************************************************/


GlComplexBlendTask::GlComplexBlendTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo, GlRenderTask* stencilTask, GlComposeTask* composeTask)
 : GlRenderTask(program), mDstFbo(dstFbo), mDstCopyFbo(dstCopyFbo), mStencilTask(stencilTask), mComposeTask(composeTask)
 {
 }


GlComplexBlendTask::~GlComplexBlendTask()
{
    delete mStencilTask;
    delete mComposeTask;
}

void GlComplexBlendTask::run(GlStateCache& state)
{
    mComposeTask->run(state);

    const auto& vp = getViewport();
    const auto width = mDstFbo->width;
    const auto height = mDstFbo->height;
    if (width <= 0 || height <= 0) return;

    state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo->resolvedFbo);

#if defined(THORVG_GL_TARGET_GL)
    const auto& dstVp = mDstFbo->viewport;
    // copy the current fbo to the dstCopyFbo
    state.viewport(0, 0, dstVp.w(), dstVp.h());
    state.scissor(0, 0, dstVp.w(), dstVp.h());
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, 0, 0, vp.w(), vp.h(), GL_COLOR_BUFFER_BIT, GL_LINEAR));
#else // TODO: create partial buffer when MSAA is disabled
    if (vp.min.x != 0 || vp.min.y != 0 || mDstCopyFbo->width != static_cast<uint32_t>(vp.w()) || mDstCopyFbo->height != static_cast<uint32_t>(vp.h())) clearColorTarget(state, mDstCopyFbo->width, mDstCopyFbo->height);
    state.viewport(0, 0, width, height);
    state.scissor(vp.min.x, vp.min.y, vp.w(), vp.h());
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, vp.min.x, vp.min.y, vp.max.x, vp.max.y, GL_COLOR_BUFFER_BIT, GL_NEAREST));
#endif

    state.bindFramebuffer(GL_FRAMEBUFFER, mDstFbo->fbo);

    state.enable(GL_STENCIL_TEST);
    state.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    state.stencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);

    state.stencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF);
    state.stencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);

    mStencilTask->run(state);

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
    mStencilTask->normalizeDrawDepth(maxDepth);
    GlRenderTask::normalizeDrawDepth(maxDepth);
}

/************************************************************************/
/* GlGaussianBlurTask Class Implementation                              */
/************************************************************************/

void GlGaussianBlurTask::run(GlStateCache& state)
{
    const auto vp = getViewport();
    const auto width = mDstFbo->width;
    const auto height = mDstFbo->height;

    // get targets handles
    GLuint dstCopyTexId0 = mDstCopyFbo0->colorTex;
    GLuint dstCopyTexId1 = mDstCopyFbo1->colorTex;
    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);
    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo0->resolvedFbo);
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    state.bindFramebuffer(GL_FRAMEBUFFER, mDstFbo->fbo);

    state.disable(GL_BLEND);
    state.depthFunc(GL_ALWAYS);
    if (effect->direction == 0) {
        state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
        state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo1->resolvedFbo);
        GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
        // horizontal blur
        state.bindFramebuffer(GL_FRAMEBUFFER, mDstCopyFbo1->resolvedFbo);
        horzTask->setViewport(vp);
        horzTask->addBindResource({0, dstCopyTexId0, GlShaderUniform::SourceTexture});
        horzTask->run(state);
        // vertical blur
        state.bindFramebuffer(GL_FRAMEBUFFER, mDstFbo->fbo);
        vertTask->setViewport(vp);
        vertTask->addBindResource({0, dstCopyTexId1, GlShaderUniform::SourceTexture});
        vertTask->run(state);
    } // horizontal
    else if (effect->direction == 1) {
        horzTask->setViewport(vp);
        horzTask->addBindResource({0, dstCopyTexId0, GlShaderUniform::SourceTexture});
        horzTask->run(state);
    } // vertical
    else if (effect->direction == 2) {
        vertTask->setViewport(vp);
        vertTask->addBindResource({0, dstCopyTexId0, GlShaderUniform::SourceTexture});
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
    const auto vp = getViewport();
    const auto width = mDstFbo->width;
    const auto height = mDstFbo->height;

    // get targets handles
    GLuint dstCopyTexId0 = mDstCopyFbo0->colorTex;
    GLuint dstCopyTexId1 = mDstCopyFbo1->colorTex;
    addBindResource({0, dstCopyTexId0, GlShaderUniform::SourceTexture});
    addBindResource({1, dstCopyTexId1, GlShaderUniform::BlurTexture});

    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);

    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo0->resolvedFbo);
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo1->resolvedFbo);
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

    state.disable(GL_BLEND);
    state.depthFunc(GL_ALWAYS);
    // when sigma is 0, no blur is applied, and the original image is used directly as the shadow.
    if (!tvg::zero(effect->sigma)) {
        // horizontal blur
        state.bindFramebuffer(GL_FRAMEBUFFER, mDstCopyFbo0->resolvedFbo);
        horzTask->setViewport(vp);
        horzTask->addBindResource({0, dstCopyTexId1, GlShaderUniform::SourceTexture});
        horzTask->run(state);
        // vertical blur
        state.bindFramebuffer(GL_FRAMEBUFFER, mDstCopyFbo1->resolvedFbo);
        vertTask->setViewport(vp);
        vertTask->addBindResource({0, dstCopyTexId0, GlShaderUniform::SourceTexture});
        vertTask->run(state);
        // copy original image to intermediate buffer
        state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
        state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo0->resolvedFbo);
        GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    }
    // run drop shadow effect
    state.bindFramebuffer(GL_FRAMEBUFFER, mDstFbo->fbo);
    GlRenderTask::run(state);
    state.depthFunc(GL_GREATER);
    state.enable(GL_BLEND);
}

/************************************************************************/
/* GlEffectColorTransformTask Class Implementation                      */
/************************************************************************/

void GlEffectColorTransformTask::run(GlStateCache& state)
{
    const auto width = mDstFbo->width;
    const auto height = mDstFbo->height;
    // get targets handles and pass to shader
    GLuint dstCopyTexId = mDstCopyFbo->colorTex;
    addBindResource({0, dstCopyTexId, GlShaderUniform::SourceTexture});

    state.viewport(0, 0, width, height);
    state.scissor(0, 0, width, height);
    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    state.bindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->fbo);
    state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo->resolvedFbo);
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    state.bindFramebuffer(GL_FRAMEBUFFER, mDstFbo->fbo);

    // run transform
    state.disable(GL_BLEND);
    state.depthFunc(GL_ALWAYS);
    GlRenderTask::run(state);
    state.depthFunc(GL_GREATER);
    state.enable(GL_BLEND);
}
