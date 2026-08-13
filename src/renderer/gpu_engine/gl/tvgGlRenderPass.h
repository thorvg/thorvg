/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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
#include "tvgGlProgram.h"

struct GlRenderPass
{
    GlRenderPass(GlRenderTarget* fbo);
    GlRenderPass(GlRenderPass&& other);
    ~GlRenderPass();

    void addRenderTask(GlRenderTask* task);

    GlRenderTask* takeLastTask() { return mTasks.empty() ? nullptr : mTasks.pick(); }
    bool isEmpty() const { return fbo == nullptr; }
    GlRenderTask* lastTask() const { return mTasks.empty() ? nullptr : mTasks.last(); }
    GLuint getFboId() { return fbo->fbo; }
    GLuint getTextureId() { return fbo->colorTex; }
    const RenderRegion& getViewport() const { return fbo->viewport; }
    uint32_t getFboWidth() const { return fbo->width; }
    uint32_t getFboHeight() const { return fbo->height; }
    const Matrix& getViewMatrix() const { return mViewMatrix; }
    int nextDrawDepth() { return ++mDrawDepth; }
    void setDrawDepth(int32_t depth) { mDrawDepth = depth; }

    template<class T>
    T* endRenderPass(GlProgram* program, GLuint targetFbo)
    {
        int32_t maxDepth = mDrawDepth + 1;

        for (uint32_t i = 0; i < mTasks.count; i++) {
            mTasks[i]->normalizeDrawDepth(maxDepth);
        }

        auto task = new T(program, targetFbo, fbo, std::move(mTasks));
        task->setRenderSize(fbo->viewport.w(), fbo->viewport.h());

        return task;
    }

    GlRenderTarget* fbo;

private:
    Array<GlRenderTask*> mTasks = {};
    int32_t mDrawDepth = 0;
    Matrix mViewMatrix = {};
};


#endif // _TVG_GL_RENDER_PASS_H_