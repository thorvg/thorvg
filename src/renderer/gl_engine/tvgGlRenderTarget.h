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

#ifndef _TVG_GL_RENDER_RENDER_TARGET_H_
#define _TVG_GL_RENDER_RENDER_TARGET_H_

#include "tvgGlCommon.h"

class GlRenderTarget
{
public:
    GlRenderTarget() = default;
    GlRenderTarget(uint32_t width, uint32_t height);
    ~GlRenderTarget();

    void init(GLint resolveId);

    GLuint getFboId() { return mFbo; }
    GLuint getResolveFboId() { return mResolveFbo; }
    GLuint getColorTexture() { return mColorTex; }

    uint32_t getWidth() const { return mWidth; }
    uint32_t getHeight() const { return mHeight; }

    void setViewport(const RenderRegion& vp) { mViewport = vp; }
    const RenderRegion& getViewport() const { return mViewport; }

    bool isInUse() const { return mInUse; }
    void setInUse(bool inUse) { mInUse = inUse; }

private:
    uint32_t mWidth;
    uint32_t mHeight;
    RenderRegion mViewport{};
    GLuint mFbo = 0;
    GLuint mColorBuffer = 0;
    GLuint mDepthStencilBuffer = 0;
    GLuint mResolveFbo = 0;
    GLuint mColorTex = 0;
    bool mInUse = false;
};

class GlRenderTargetPool {
public:
    GlRenderTargetPool(uint32_t maxWidth, uint32_t maxHeight);
    ~GlRenderTargetPool();

    GlRenderTarget* getRenderTarget(const RenderRegion& vp, GLuint resolveId = 0);
private:
    uint32_t mMaxWidth = 0;
    uint32_t mMaxHeight = 0;
    Array<GlRenderTarget*> mPool;
};

#endif //_TVG_GL_RENDER_RENDER_TARGET_H_