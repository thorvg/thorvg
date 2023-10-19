/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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
    virtual ~GlRenderTask() = default;

    virtual void run();

    void addVertexLayout(const GlVertexLayout& layout);
    void addBindResource(const GlBindingResource& binding);
    void setDrawRange(uint32_t offset, uint32_t count);
    void setViewport(const RenderRegion& viewport);

    GlProgram* getProgram() { return mProgram; }
private:
    GlProgram* mProgram;
    RenderRegion mViewport = {};
    uint32_t mIndexOffset = {};
    uint32_t mIndexCount = {};
    Array<GlVertexLayout> mVertexLayout = {};
    Array<GlBindingResource> mBindingResources = {};
};

class GlComposeTask : public GlRenderTask 
{
public:
    GlComposeTask(GlProgram* program, GLuint target, GLuint selfFbo, vector<unique_ptr<GlRenderTask>> tasks);
    ~GlComposeTask() override = default;

    void run() override;

protected:
    GLuint getTargetFbo() { return mTargetFbo; }

    GLuint getSelfFbo() { return mSelfFbo; }

private:
    GLuint mTargetFbo;
    GLuint mSelfFbo;
    vector<unique_ptr<GlRenderTask>> mTasks;
};

class GlBlitTask : public GlComposeTask
{
public:
    GlBlitTask(GlProgram*, GLuint target, GLuint compose, vector<unique_ptr<GlRenderTask>> tasks);
    ~GlBlitTask() override = default;

    void setSize(uint32_t width, uint32_t height);

    void run() override;

private:
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
};

class GlDrawBlitTask : public GlComposeTask
{
public:
    GlDrawBlitTask(GlProgram*, GLuint target, GLuint compose, vector<unique_ptr<GlRenderTask>> tasks);
    ~GlDrawBlitTask() override = default;

    void run() override;
};


#endif /* _TVG_GL_RENDER_TASK_H_ */
