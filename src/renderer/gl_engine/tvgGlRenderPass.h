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

#ifndef _TVG_GL_RENDER_PASS_H_
#define _TVG_GL_RENDER_PASS_H_

#include "tvgGlCommon.h"
#include "tvgGlRenderTask.h"
#include "tvgGlRenderTarget.h"

class GlProgram;

class GlRenderPass
{
public:
    GlRenderPass(GlRenderTarget* fbo, RenderRegion viewport);
    GlRenderPass(GlRenderPass&& other);

    ~GlRenderPass();

    bool isEmpty() const { return mFbo == nullptr; }

    void addRenderTask(GlRenderTask* task);

    GLuint getFboId() { return mFbo->getFboId(); }

    GLuint getTextureId() { return mFbo->getColorTexture(); }

    const RenderRegion& getViewport() const { return mViewport; }

    uint32_t getFboWidth() const { return mFbo->getWidth(); }

    uint32_t getFboHeight() const { return mFbo->getHeight(); }

    void getMatrix(float dst[16], const Matrix& matrix) const;

    template <class T>
    T* endRenderPass(GlProgram* program, GLuint targetFbo) {
        int32_t maxDepth = mDrawDepth + 1;

        for (uint32_t i = 0; i < mTasks.count; i++) {
            mTasks[i]->normalizeDrawDepth(maxDepth);
        }

        auto task = new T(program, targetFbo, mFbo, std::move(mTasks));

        const auto& vp = mViewport;

        task->setRenderSize(static_cast<uint32_t>(vp.w), static_cast<uint32_t>(vp.h));

        return task;
    }

    int nextDrawDepth() { return ++mDrawDepth; }

    void setDrawDepth(int32_t depth) { mDrawDepth = depth; }

    GlRenderTarget* getFbo() { return mFbo; }
private:
    GlRenderTarget* mFbo;
    RenderRegion mViewport;
    Array<GlRenderTask*> mTasks = {};
    int32_t mDrawDepth = 0;
};


#endif // _TVG_GL_RENDER_PASS_H_