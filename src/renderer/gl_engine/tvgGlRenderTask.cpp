/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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
/* External Class Implementation                                        */
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

    // setup scissor rect
    GL_CHECK(glScissor(mViewport.x, mViewport.y, mViewport.w, mViewport.h));

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
}

GlStencilCoverTask::GlStencilCoverTask(GlRenderTask* stencil, GlRenderTask* cover)
 :GlRenderTask(nullptr), mStencilTask(stencil), mCoverTask(cover) {}

GlStencilCoverTask::~GlStencilCoverTask()
{
    delete mStencilTask;
    delete mCoverTask;
}

void GlStencilCoverTask::run()
{
    GL_CHECK(glEnable(GL_STENCIL_TEST));

    GL_CHECK(glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x1, 0xFF));
    GL_CHECK(glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP));

    GL_CHECK(glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0x1, 0xFF));
    GL_CHECK(glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP));

    GL_CHECK(glColorMask(0, 0, 0, 0));

    mStencilTask->run();

    GL_CHECK(glStencilFunc(GL_NOTEQUAL, 0x0, 0xFF));
    GL_CHECK(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));

    GL_CHECK(glColorMask(1, 1, 1, 1));

    mCoverTask->run();

    GL_CHECK(glDisable(GL_STENCIL_TEST));
}

GlComposeTask::GlComposeTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 :GlRenderTask(program) ,mTargetFbo(target), mFbo(fbo), mTasks()
{
    mTasks.push(tasks);
    tasks.clear();
}

GlComposeTask::~GlComposeTask()
{
    for(uint32_t i = 0; i < mTasks.count; i++) {
        delete mTasks[i];
    }

    mTasks.clear();
}

void GlComposeTask::run()
{
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, getSelfFbo()));

    // clear this fbo
    if (mClearBuffer) {
        GL_CHECK(glClearColor(0, 0, 0, 0));
        GL_CHECK(glClearStencil(0));

        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }

    for(uint32_t i = 0; i < mTasks.count; i++) {
        mTasks[i]->run();
    }

    GLenum stencil_attachment = GL_STENCIL_ATTACHMENT;
    GL_CHECK(glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &stencil_attachment));

    onResolve();
}

GLuint GlComposeTask::getSelfFbo() { return mFbo->getFboId(); }

GLuint GlComposeTask::getResolveFboId() { return mFbo->getResolveFboId(); }

void GlComposeTask::onResolve() {
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, getSelfFbo()));
    GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, getResolveFboId()));

    GL_CHECK(glBlitFramebuffer(0, 0, mFbo->getWidth(), mFbo->getHeight(), 0, 0, mFbo->getWidth(), mFbo->getHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST));
}

GlBlitTask::GlBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, fbo, std::move(tasks))
{
}

void GlBlitTask::setSize(uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;
}

void GlBlitTask::run()
{
    GlComposeTask::run();

    GL_CHECK(glScissor(0, 0, mWidth, mHeight));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, getTargetFbo()));
    GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, getSelfFbo()));

    GL_CHECK(glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST));
}

GlDrawBlitTask::GlDrawBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, fbo, std::move(tasks))
{
}

void GlDrawBlitTask::run()
{
    GlComposeTask::run();

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, getTargetFbo()));

    GlRenderTask::run();
}