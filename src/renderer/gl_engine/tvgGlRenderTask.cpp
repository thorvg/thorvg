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

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

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

GlComposeTask::GlComposeTask(GlProgram* program, GLuint target, GLuint selfFbo, Array<GlRenderTask*>&& tasks)
 :GlRenderTask(program) ,mTargetFbo(target), mSelfFbo(selfFbo), mTasks()
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
    GLenum color_buffer = GL_COLOR_ATTACHMENT0;
    const float transparent[] = {0.f, 0.f, 0.f, 0.f};

    GL_CHECK(glDrawBuffers(1, &color_buffer));
    GL_CHECK(glClearBufferfv(GL_COLOR, 0, transparent));

    for(uint32_t i = 0; i < mTasks.count; i++) {
        mTasks[i]->run();
    }
}

GlBlitTask::GlBlitTask(GlProgram* program, GLuint target, GLuint compose, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, compose, std::move(tasks))
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

GlDrawBlitTask::GlDrawBlitTask(GlProgram* program, GLuint target, GLuint compose, Array<GlRenderTask*>&& tasks)
 : GlComposeTask(program, target, compose, std::move(tasks))
{
}

void GlDrawBlitTask::run()
{
    GlComposeTask::run();

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, getTargetFbo()));

    GlRenderTask::run();
}