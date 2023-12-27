/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tvgGlRenderPass.h"
#include "tvgGlRenderTask.h"

GlRenderTarget::GlRenderTarget(uint32_t width, uint32_t height): mWidth(width), mHeight(height) {}

GlRenderTarget::~GlRenderTarget()
{
    if (mFbo == 0) return;
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CHECK(glDeleteFramebuffers(1, &mFbo));

    if (mColorTex == 0) return;
    GL_CHECK(glDeleteTextures(1, &mColorTex));
}

void GlRenderTarget::init(GLint resolveId)
{
    if (mFbo != 0 || mWidth == 0 || mHeight == 0) return;

    GL_CHECK(glGenFramebuffers(1, &mFbo));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFbo));

    GL_CHECK(glGenTextures(1, &mColorTex));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, mColorTex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));

    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTex, 0));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, resolveId));
}

GlRenderPass::GlRenderPass(GlRenderTarget* fbo): mFbo(fbo), mTasks() {}

GlRenderPass::GlRenderPass(GlRenderPass&& other): mFbo(other.mFbo), mTasks()
{
    mTasks.push(other.mTasks);

    other.mTasks.clear();
}

GlRenderPass::~GlRenderPass()
{
    if (mTasks.empty()) return;

    for(uint32_t i = 0; i < mTasks.count; i++) {
        delete mTasks[i];
    }

    mTasks.clear();
}

void GlRenderPass::addRenderTask(GlRenderTask* task)
{
    mTasks.push(task);
}
