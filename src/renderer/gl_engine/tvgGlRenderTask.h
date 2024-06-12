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

#ifndef _TVG_GL_RENDER_TASK_H_
#define _TVG_GL_RENDER_TASK_H_

#include <memory>
#include <vector>

#include "tvgGlCommon.h"
#include "tvgGlProgram.h"


struct GlVertexLayout
{
    uint32_t index;
    uint32_t size;
    uint32_t stride;
    size_t   offset;
};

enum class GlBindingType
{
    kUniformBuffer,
    kTexture,
};


struct GlBindingResource
{
    GlBindingType type;
    /**
     * Binding point index.
     * Can be a uniform location for a texture
     * Can be a uniform buffer binding index for a uniform block
     */
    uint32_t        bindPoint = {};
    uint32_t        location = {};
    GLuint          gBufferId = {};
    uint32_t        bufferOffset = {};
    uint32_t        bufferRange = {};

    GlBindingResource() = default;

    GlBindingResource(uint32_t index, uint32_t location, uint32_t bufferId, uint32_t offset, uint32_t range)
        : type(GlBindingType::kUniformBuffer), bindPoint(index), location(location), gBufferId(bufferId), bufferOffset(offset), bufferRange(range)
    {
    }

    GlBindingResource(uint32_t bindPoint, uint32_t texId, uint32_t location)
        : type(GlBindingType::kTexture), bindPoint(bindPoint), location(location), gBufferId(texId)
    {
    }
};

class GlRenderTask
{
public:
    GlRenderTask(GlProgram* program): mProgram(program) {}
    GlRenderTask(GlProgram* program, GlRenderTask* other);

    virtual ~GlRenderTask() = default;

    virtual void run();

    void addVertexLayout(const GlVertexLayout& layout);
    void addBindResource(const GlBindingResource& binding);
    void setDrawRange(uint32_t offset, uint32_t count);
    void setViewport(const RenderRegion& viewport);
    void setDrawDepth(int32_t depth) { mDrawDepth = static_cast<float>(depth); }
    virtual void normalizeDrawDepth(int32_t maxDepth) { mDrawDepth /= static_cast<float>(maxDepth);  }

    GlProgram* getProgram() { return mProgram; }
    const RenderRegion& getViewport() const { return mViewport; }
private:
    GlProgram* mProgram;
    RenderRegion mViewport = {};
    uint32_t mIndexOffset = {};
    uint32_t mIndexCount = {};
    Array<GlVertexLayout> mVertexLayout = {};
    Array<GlBindingResource> mBindingResources = {};
    float mDrawDepth = 0.f;
};

class GlStencilCoverTask : public GlRenderTask
{
public:
    GlStencilCoverTask(GlRenderTask* stencil, GlRenderTask* cover, GlStencilMode mode);
    ~GlStencilCoverTask() override;

    void run() override;

    void normalizeDrawDepth(int32_t maxDepth) override;
private:
    GlRenderTask* mStencilTask;
    GlRenderTask* mCoverTask;
    GlStencilMode mStencilMode;
};

class GlRenderTarget;

class GlComposeTask : public GlRenderTask 
{
public:
    GlComposeTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);
    ~GlComposeTask() override;

    void run() override;

    bool mClearBuffer = true;

protected:
    GLuint getTargetFbo() { return mTargetFbo; }

    GLuint getSelfFbo();

    GLuint getResolveFboId();

    void onResolve();
private:
    GLuint mTargetFbo;
    GlRenderTarget* mFbo;
    Array<GlRenderTask*> mTasks;
};

class GlBlitTask : public GlComposeTask
{
public:
    GlBlitTask(GlProgram*, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);
    ~GlBlitTask() override = default;

    void run() override;

    GLuint getColorTextore() const { return mColorTex; }

    void setTargetViewport(const RenderRegion& vp) { mTargetViewport = vp; }
private:
    GLuint mColorTex = 0;
    RenderRegion mTargetViewport = {};
};

class GlDrawBlitTask : public GlComposeTask
{
public:
    GlDrawBlitTask(GlProgram*, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);
    ~GlDrawBlitTask() override = default;

    void run() override;
};

class GlClipTask : public GlRenderTask
{
public:
    GlClipTask(GlRenderTask* clip, GlRenderTask* mask);
    ~GlClipTask() override = default;

    void run() override;

    void normalizeDrawDepth(int32_t maxDepth) override;
private:
    GlRenderTask* mClipTask;
    GlRenderTask* mMaskTask;
};

#endif /* _TVG_GL_RENDER_TASK_H_ */
