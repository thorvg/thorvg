/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgGlRenderTarget.h"

/************************************************************************/
/* GlRenderTarget Class Implementation                              */
/************************************************************************/

GlRenderTarget::GlRenderTarget() {}

GlRenderTarget::~GlRenderTarget()
{
    reset();
}

void GlRenderTarget::init(uint32_t width, uint32_t height, GLint resolveId)
{
    if (width == 0 || height == 0) return;

    mWidth = width;
    mHeight = height;

    //TODO: fbo is used. maybe we can consider the direct rendering with resolveId as well.
    GL_CHECK(glGenFramebuffers(1, &mFbo));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFbo));

    GL_CHECK(glGenRenderbuffers(1, &mColorBuffer));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, mColorBuffer));
    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, mWidth, mHeight));

    GL_CHECK(glGenRenderbuffers(1, &mDepthStencilBuffer));

    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, mDepthStencilBuffer));

    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, mWidth, mHeight));

    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mColorBuffer));
    GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mDepthStencilBuffer));

    // resolve target
    GL_CHECK(glGenTextures(1, &mColorTex));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, mColorTex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));

    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    GL_CHECK(glGenFramebuffers(1, &mResolveFbo));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mResolveFbo));
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTex, 0));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, resolveId));
}

void GlRenderTarget::reset()
{
    if (mFbo == 0) return;

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CHECK(glDeleteFramebuffers(1, &mFbo));
    GL_CHECK(glDeleteRenderbuffers(1, &mColorBuffer));
    GL_CHECK(glDeleteRenderbuffers(1, &mDepthStencilBuffer));
    GL_CHECK(glDeleteFramebuffers(1, &mResolveFbo));
    GL_CHECK(glDeleteTextures(1, &mColorTex));

    mFbo = mColorBuffer = mDepthStencilBuffer = mResolveFbo = mColorTex = 0;
}

/************************************************************************/
/* GlRenderTargetPool Class Implementation                              */
/************************************************************************/

GlRenderTargetPool::GlRenderTargetPool(){}


GlRenderTargetPool::~GlRenderTargetPool()
{
    reset();
}


void GlRenderTargetPool::init(uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;
}


void GlRenderTargetPool::reset()
{
    for (uint32_t i = 0; i < mPool.count; i++)
        delete mPool[i];
    mPool.clear();
    mWidth = 0;
    mHeight = 0;
}


GlRenderTarget* GlRenderTargetPool::getRenderTarget(GLuint resolveId)
{
    GlRenderTarget* renderTarget{};
    if (mPool.count > 0) {
        renderTarget = mPool.last();
        mPool.pop();
    } else {
        renderTarget = new GlRenderTarget;
        renderTarget->init(mWidth, mHeight, resolveId);
        renderTarget->setViewport({{0,0},{(int32_t)mWidth, (int32_t)mHeight}});
    }
    return renderTarget;
}


void GlRenderTargetPool::freeRenderTarget(GlRenderTarget* renderTarget)
{
    if (renderTarget) mPool.push(renderTarget);
}
