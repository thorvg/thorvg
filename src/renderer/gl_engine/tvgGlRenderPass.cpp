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

#include "tvgMath.h"
#include "tvgGlRenderPass.h"
#include "tvgGlRenderTask.h"
#include "tvgGlGeometry.h"

GlRenderPass::GlRenderPass(GlRenderTarget* fbo): mFbo(fbo), mTasks(), mDrawDepth(0) {}

GlRenderPass::GlRenderPass(GlRenderPass&& other): mFbo(other.mFbo), mTasks(), mDrawDepth(0)
{
    mTasks.push(other.mTasks);

    other.mTasks.clear();

    mDrawDepth = other.mDrawDepth;
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

void GlRenderPass::getMatrix(float *dst, const Matrix &matrix) const
{
    const auto& vp = getViewport();

    Matrix postMatrix{};
    mathIdentity(&postMatrix);
    mathTranslate(&postMatrix, -vp.x, -vp.y);

    auto m = postMatrix * matrix;

    float modelMatrix[16];
    GET_MATRIX44(m, modelMatrix);
    MVP_MATRIX(vp.w, vp.h);

    MULTIPLY_MATRIX(mvp, modelMatrix, dst);
}
