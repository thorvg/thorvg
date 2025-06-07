/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

/************************************************************************/
/* GlRenderTask Class Implementation                                    */
/************************************************************************/

GlRenderTask::GlRenderTask(GlProgram* program, GlRenderTask* other): mProgram(program)
{
    mVertexLayout.push(other->mVertexLayout);
    mViewport = other->mViewport;
    mIndexOffset = other->mIndexOffset;
    mIndexCount = other->mIndexCount;
}


void GlRenderTask::run()
{
    // bind shader
    mProgram->load();

    int32_t dLoc = mProgram->getUniformLocation("uDepth");
    if (dLoc >= 0) {
        // fixme: prevent compiler warning: macro expands to multiple statements [-Wmultistatement-macros]
        GL_CHECK(glUniform1f(dLoc, mDrawDepth));
    }

    // setup scissor rect
    GL_CHECK(glScissor(mViewport.sx(), mViewport.sy(), mViewport.sw(), mViewport.sh()));

    // setup attribute layout
    for (uint32_t i = 0; i < mVertexLayout.count; i++) {
        const auto &layout = mVertexLayout[i];
        GL_CHECK(glEnableVertexAttribArray(layout.index));
        GL_CHECK(glVertexAttribPointer(layout.index, layout.size, GL_FLOAT,
                                   GL_FALSE, layout.stride,
                                   reinterpret_cast<void *>(layout.offset)));
    }

    // binding uniforms
    for (uint32_t i = 0; i < mBindingResources.count; i++) {
        const auto& binding = mBindingResources[i];
        if (binding.type == GlBindingType::kTexture) {
            GL_CHECK(glActiveTexture(GL_TEXTURE0 + binding.bindPoint));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, binding.gBufferId));

            mProgram->setUniform1Value(binding.location, 1, (int32_t*)&binding.bindPoint);
        } else if (binding.type == GlBindingType::kUniformBuffer) {

            GL_CHECK(glUniformBlockBinding(mProgram->getProgramId(), binding.location, binding.bindPoint));
            GL_CHECK(glBindBufferRange(GL_UNIFORM_BUFFER, binding.bindPoint, binding.gBufferId,
                                       binding.bufferOffset, binding.bufferRange));
        }
    }

    GL_CHECK(glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, reinterpret_cast<void*>(mIndexOffset)));

    // setup attribute layout
    for (uint32_t i = 0; i < mVertexLayout.count; i++) {
        const auto &layout = mVertexLayout[i];
        GL_CHECK(glDisableVertexAttribArray(layout.index));
    }
}


void GlRenderTask::addVertexLayout(const GlVertexLayout &layout)
{
    mVertexLayout.push(layout);
}


void GlRenderTask::addBindResource(const GlBindingResource &binding)
{
    mBindingResources.push(binding);
}


void GlRenderTask::setDrawRange(uint32_t offset, uint32_t count)
{
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
 :GlRenderTask(nullptr), mStencilTask(stencil), mCoverTask(cover), mStencilMode(mode)
 {

 }


GlStencilCoverTask::~GlStencilCoverTask()
{
    delete mStencilTask;
    delete mCoverTask;
}


void GlStencilCoverTask::run()
{
    GL_CHECK(glEnable(GL_STENCIL_TEST));

    if (mStencilMode == GlStencilMode::Stroke) {
        GL_CHECK(glStencilFunc(GL_NOTEQUAL, 0x1, 0xFF));
        GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
    } else {
        GL_CHECK(glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF));
        GL_CHECK(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP));

        GL_CHECK(glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF));
        GL_CHECK(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP));
    }
    GL_CHECK(glColorMask(0, 0, 0, 0));

    mStencilTask->run();

    if (mStencilMode == GlStencilMode::FillEvenOdd) {
        GL_CHECK(glStencilFunc(GL_NOTEQUAL, 0x00, 0x01));
        GL_CHECK(glStencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE));
    } else {
        GL_CHECK(glStencilFunc(GL_NOTEQUAL, 0x0, 0xFF));
        GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
    }

    GL_CHECK(glColorMask(1, 1, 1, 1));

    mCoverTask->run();

    GL_CHECK(glDisable(GL_STENCIL_TEST));
}


void GlStencilCoverTask::normalizeDrawDepth(int32_t maxDepth)
{
    mCoverTask->normalizeDrawDepth(maxDepth);
    mStencilTask->normalizeDrawDepth(maxDepth);
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


void GlComposeTask::run()
{
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, getSelfFbo()));

    // we must clear all area of fbo
    GL_CHECK(glViewport(0, 0, mFbo->getWidth(), mFbo->getHeight()));
    GL_CHECK(glScissor(0, 0, mFbo->getWidth(), mFbo->getHeight()));
    GL_CHECK(glClearColor(0, 0, 0, 0));
    GL_CHECK(glClearStencil(0));
#ifdef THORVG_GL_TARGET_GLES
    GL_CHECK(glClearDepthf(0.0));
#else
    GL_CHECK(glClearDepth(0.0));
#endif
    GL_CHECK(glDepthMask(1));

    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    GL_CHECK(glDepthMask(0));

    GL_CHECK(glViewport(0, 0, mRenderWidth, mRenderHeight));
    GL_CHECK(glScissor(0, 0, mRenderWidth, mRenderHeight));

    ARRAY_FOREACH(p, mTasks) {
        (*p)->run();
    }

#if defined(THORVG_GL_TARGET_GLES)
    // only OpenGLES has tiled base framebuffer and discard function
    GLenum attachments[2] = {GL_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT };
    GL_CHECK(glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, attachments));
#endif
    // reset scissor box
    GL_CHECK(glScissor(0, 0, mFbo->getWidth(), mFbo->getHeight()));
    onResolve();
}


GLuint GlComposeTask::getSelfFbo()
{
    return mFbo->getFboId();
}


GLuint GlComposeTask::getResolveFboId()
{
    return mFbo->getResolveFboId();
}


void GlComposeTask::onResolve()
{
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, getSelfFbo()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, getResolveFboId()));
    GL_CHECK(glBlitFramebuffer(0, 0, mRenderWidth, mRenderHeight, 0, 0, mRenderWidth, mRenderHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST));
}


/************************************************************************/
/* GlBlitTask Class Implementation                                      */
/************************************************************************/

GlBlitTask::GlBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, fbo, std::move(tasks)), mColorTex(fbo->getColorTexture())
{
}


void GlBlitTask::run()
{
    GlComposeTask::run();

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, getTargetFbo()));
    GL_CHECK(glViewport(mTargetViewport.x(), mTargetViewport.y(), mTargetViewport.w(), mTargetViewport.h()));

    if (mClearBuffer) {
        GL_CHECK(glClearColor(0, 0, 0, 0));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    }

    GL_CHECK(glDisable(GL_DEPTH_TEST));
    // make sure the blending is correct
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

    GlRenderTask::run();
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


void GlDrawBlitTask::run()
{
    if (mPrevTask) mPrevTask->run();

    GlComposeTask::run();

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, getTargetFbo()));

    GL_CHECK(glViewport(0, 0, mParentWidth, mParentHeight));
    GL_CHECK(glScissor(0, 0, mParentWidth, mParentWidth));
    GlRenderTask::run();
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


void GlClipTask::run()
{
    GL_CHECK(glEnable(GL_STENCIL_TEST));
    GL_CHECK(glColorMask(0, 0, 0, 0));
    // draw clip path as normal stencil mask
    GL_CHECK(glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x1, 0xFF));
    GL_CHECK(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP));

    GL_CHECK(glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x1, 0xFF));
    GL_CHECK(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP));

    mClipTask->run();


    // draw clip mask
    GL_CHECK(glDepthMask(1));
    GL_CHECK(glStencilFunc(GL_EQUAL, 0x0, 0xFF));
    GL_CHECK(glStencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE));

    mMaskTask->run();

    GL_CHECK(glColorMask(1, 1, 1, 1));
    GL_CHECK(glDepthMask(0));
    GL_CHECK(glDisable(GL_STENCIL_TEST));
}


void GlClipTask::normalizeDrawDepth(int32_t maxDepth)
{
    mClipTask->normalizeDrawDepth(maxDepth);
    mMaskTask->normalizeDrawDepth(maxDepth);
}

/************************************************************************/
/* GlSimpleBlendTask Class Implementation                               */
/************************************************************************/

GlSimpleBlendTask::GlSimpleBlendTask(BlendMethod method, GlProgram* program)
 : GlRenderTask(program), mBlendMethod(method)
 {
 }


void GlSimpleBlendTask::run()
{
    if (mBlendMethod == BlendMethod::Add) glBlendFunc(GL_ONE, GL_ONE);
    else if (mBlendMethod == BlendMethod::Darken) {
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_MIN);
    } else if (mBlendMethod == BlendMethod::Lighten) {
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_MAX);
    }
    else glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GlRenderTask::run();

    if (mBlendMethod == BlendMethod::Darken || mBlendMethod == BlendMethod::Lighten) glBlendEquation(GL_FUNC_ADD);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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


void GlComplexBlendTask::run()
{
    mComposeTask->run();

    // copy the current fbo to the dstCopyFbo
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->getFboId()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo->getResolveFboId()));

    GL_CHECK(glViewport(0, 0, mDstFbo->getViewport().w(), mDstFbo->getViewport().h()));
    GL_CHECK(glScissor(0, 0, mDstFbo->getViewport().w(), mDstFbo->getViewport().h()));
    
    const auto& vp = getViewport();
    GL_CHECK(glBlitFramebuffer(vp.min.x, vp.min.y, vp.max.x, vp.max.y, 0, 0, vp.w(), vp.h(), GL_COLOR_BUFFER_BIT, GL_LINEAR));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstFbo->getFboId()));

    GL_CHECK(glEnable(GL_STENCIL_TEST));
    GL_CHECK(glColorMask(0, 0, 0, 0));
    GL_CHECK(glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x0, 0xFF));
    GL_CHECK(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP));

    GL_CHECK(glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x0, 0xFF));
    GL_CHECK(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP));


    mStencilTask->run();

    GL_CHECK(glColorMask(1, 1, 1, 1));
    GL_CHECK(glStencilFunc(GL_NOTEQUAL, 0x0, 0xFF));
    GL_CHECK(glStencilOp(GL_REPLACE, GL_KEEP, GL_REPLACE));

    GL_CHECK(glBlendFunc(GL_ONE, GL_ZERO));

    GlRenderTask::run();

    GL_CHECK(glDisable(GL_STENCIL_TEST));
    GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
}


void GlComplexBlendTask::normalizeDrawDepth(int32_t maxDepth)
{
    mStencilTask->normalizeDrawDepth(maxDepth);
    GlRenderTask::normalizeDrawDepth(maxDepth);
}

/************************************************************************/
/* GlGaussianBlurTask Class Implementation                              */
/************************************************************************/

void GlGaussianBlurTask::run()
{
    const auto vp = getViewport();
    const auto width = mDstFbo->getWidth();
    const auto height = mDstFbo->getHeight();

    // get targets handles
    GLuint dstCopyTexId0 = mDstCopyFbo0->getColorTexture();
    GLuint dstCopyTexId1 = mDstCopyFbo1->getColorTexture();
    // get programs properties
    GlProgram* programHorz = horzTask->getProgram();
    GlProgram* programVert = vertTask->getProgram();
    GLint horzSrcTextureLoc = programHorz->getUniformLocation("uSrcTexture");
    GLint vertSrcTextureLoc = programVert->getUniformLocation("uSrcTexture");

    GL_CHECK(glViewport(0, 0, width, height));
    GL_CHECK(glScissor(0, 0, width, height));
    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->getFboId()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo0->getResolveFboId()));
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstFbo->getFboId()));

    GL_CHECK(glDisable(GL_BLEND));
    if (effect->direction == 0) {
        GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->getFboId()));
        GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo1->getResolveFboId()));
        GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
        // horizontal blur
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstCopyFbo1->getResolveFboId()));
        horzTask->setViewport(vp);
        horzTask->addBindResource({ 0, dstCopyTexId0, horzSrcTextureLoc });
        horzTask->run();
        // vertical blur
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstFbo->getFboId()));
        vertTask->setViewport(vp);
        vertTask->addBindResource({ 0, dstCopyTexId1, vertSrcTextureLoc });
        vertTask->run();
    } // horizontal
    else if (effect->direction == 1) {
        horzTask->setViewport(vp);
        horzTask->addBindResource({ 0, dstCopyTexId0, horzSrcTextureLoc });
        horzTask->run();
    } // vertical
    else if (effect->direction == 2) {
        vertTask->setViewport(vp);
        vertTask->addBindResource({ 0, dstCopyTexId0, vertSrcTextureLoc });
        vertTask->run();
    }
    GL_CHECK(glEnable(GL_BLEND));
}

/************************************************************************/
/* GlEffectDropShadowTask Class Implementation                          */
/************************************************************************/

void GlEffectDropShadowTask::run()
{
    const auto vp = getViewport();
    const auto width = mDstFbo->getWidth();
    const auto height = mDstFbo->getHeight();

    // get targets handles
    GLuint dstCopyTexId0 = mDstCopyFbo0->getColorTexture();
    GLuint dstCopyTexId1 = mDstCopyFbo1->getColorTexture();
    // get programs properties
    GlProgram* programHorz = horzTask->getProgram();
    GlProgram* programVert = vertTask->getProgram();
    GLint horzSrcTextureLoc = programHorz->getUniformLocation("uSrcTexture");
    GLint vertSrcTextureLoc = programVert->getUniformLocation("uSrcTexture");

    GLint srcTextureLoc = getProgram()->getUniformLocation("uSrcTexture");
    GLint blrTextureLoc = getProgram()->getUniformLocation("uBlrTexture");
    addBindResource({ 0, dstCopyTexId0, srcTextureLoc });
    addBindResource({ 1, dstCopyTexId1, blrTextureLoc });

    GL_CHECK(glViewport(0, 0, width, height));
    GL_CHECK(glScissor(0, 0, width, height));

    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->getFboId()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo0->getResolveFboId()));
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->getFboId()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo1->getResolveFboId()));
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    
    GL_CHECK(glDisable(GL_BLEND));
    // horizontal blur
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstCopyFbo0->getResolveFboId()));
    horzTask->setViewport(vp);
    horzTask->addBindResource({ 0, dstCopyTexId1, horzSrcTextureLoc });
    horzTask->run();
    // vertical blur
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstCopyFbo1->getResolveFboId()));
    vertTask->setViewport(vp);
    vertTask->addBindResource({ 0, dstCopyTexId0, vertSrcTextureLoc });
    vertTask->run();
    // copy original image to intermediate buffer
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->getFboId()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo0->getResolveFboId()));
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    // run drop shadow effect
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstFbo->getFboId()));
    GlRenderTask::run();
    GL_CHECK(glEnable(GL_BLEND));
}

/************************************************************************/
/* GlEffectColorTransformTask Class Implementation                      */
/************************************************************************/

void GlEffectColorTransformTask::run()
{
    const auto width = mDstFbo->getWidth();
    const auto height = mDstFbo->getHeight();
    // get targets handles and pass to shader
    GLuint dstCopyTexId = mDstCopyFbo->getColorTexture();
    GLint srcTextureLoc = getProgram()->getUniformLocation("uSrcTexture");
    addBindResource({ 0, dstCopyTexId, srcTextureLoc });

    GL_CHECK(glViewport(0, 0, width, height));
    GL_CHECK(glScissor(0, 0, width, height));
    // we need to make a full copy of dst to intermediate buffers to be sure that they don’t contain prev data.
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, mDstFbo->getFboId()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mDstCopyFbo->getResolveFboId()));
    GL_CHECK(glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mDstFbo->getFboId()));

    // run transform
    GL_CHECK(glDisable(GL_BLEND));
    GlRenderTask::run();
    GL_CHECK(glEnable(GL_BLEND));
}
