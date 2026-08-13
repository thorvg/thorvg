/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#include "tvgGlProgram.h"
#include "tvgGlRenderTarget.h"

struct GlVertexLayout
{
    uint32_t index;
    uint32_t size;
    uint32_t stride;
    size_t   offset;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    // Optional VBO for this attribute. 0 means use the GL_ARRAY_BUFFER binding
    // captured at the start of GlRenderTask::run().
    GLuint arrayBufferId = 0;
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
    uint32_t        bindPoint = 0;
    GLint           location = 0;
    // GL object id used by this binding: texture id for kTexture, UBO id for kUniformBuffer.
    GLuint resourceId = 0;
    uint32_t        bufferOffset = 0;
    uint32_t        bufferRange = 0;

    GlBindingResource() = default;
    GlBindingResource(uint32_t index, GLint location, GLuint uniformBufferId, uint32_t offset, uint32_t range) :
        type(GlBindingType::kUniformBuffer), bindPoint(index), location(location), resourceId(uniformBufferId), bufferOffset(offset), bufferRange(range) {}
    GlBindingResource(uint32_t bindPoint, GLuint textureId, GLint location) :
        type(GlBindingType::kTexture), bindPoint(bindPoint), location(location), resourceId(textureId) {}
};

struct GlRenderTask
{
    GlRenderTask(GlProgram* program) :
        program(program) {}

    virtual ~GlRenderTask() = default;
    virtual void run();

    void addVertexLayout(const GlVertexLayout& layout);
    void setVertexColor(float r, float g, float b, float a);
    void addBindResource(const GlBindingResource& binding);
    void setDrawRange(uint32_t offset, uint32_t count);
    void setViewport(const RenderRegion& viewport);
    void setDrawDepth(int32_t depth) { drawDepth = static_cast<float>(depth); }
    void setViewMatrix(const Matrix& matrix)
    {
        viewMatrix = matrix;
        useViewMatrix = true;
    }
    virtual void normalizeDrawDepth(int32_t maxDepth) { drawDepth /= static_cast<float>(maxDepth); }

    GlProgram* program;
    float drawDepth = 0.f;
    RenderRegion viewport;
    uint32_t indexCnt = 0;
    uint32_t indexOffset = 0;
    Array<GlVertexLayout> vertexLayout;
    Array<GlBindingResource> bindResources;
    uint32_t arrayOffset = 0;
    GLenum arrayMode = GL_TRIANGLES;
    Matrix viewMatrix = {};
    float vertexColor[4] = {0.f, 0.f, 0.f, 0.f};
    bool useViewMatrix = false;
    bool useVertexColor = false;
    bool useDrawArrays = false;
};

struct GlStencilCoverTask : GlRenderTask
{
    GlStencilCoverTask(GlRenderTask* stencil, GlRenderTask* cover, GlStencilMode mode);
    ~GlStencilCoverTask();

    void run() override;
    void normalizeDrawDepth(int32_t maxDepth) override;

    Array<GlRenderTask*> stencilTasks;
    Array<GlRenderTask*> coverTasks;
    GlStencilMode stencilMode;
};

struct GlComposeTask : GlRenderTask
{
    GlComposeTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);
    ~GlComposeTask();

    void run() override;
    void onResolve();
    void setRenderSize(uint32_t width, uint32_t height)
    {
        renderWidth = width;
        renderHeight = height;
    }
    GLuint getSelfFbo() { return fbo->fbo; }
    GLuint getResolveFboId() { return fbo->resolvedFbo; }

    GLuint targetFbo;
    GlRenderTarget* fbo;
    Array<GlRenderTask*> tasks;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    bool clearBuffer = true;
};

struct GlBlitTask : GlComposeTask
{
    GlBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);

    void run() override;

    GLuint colorTex;
    RenderRegion targetViewport = {};
};

struct GlDrawBlitTask : GlComposeTask
{
    GlDrawBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);
    ~GlDrawBlitTask();

    void setParentSize(uint32_t width, uint32_t height)
    {
        parentWidth = width;
        parentHeight = height;
    }

    void run() override;

    GlRenderTask* prevTask = nullptr;
    uint32_t parentWidth = 0;
    uint32_t parentHeight = 0;
};

struct GlSceneBlendTask : GlComposeTask
{
    GlSceneBlendTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);

    void setParentSize(uint32_t width, uint32_t height)
    {
        parentWidth = width;
        parentHeight = height;
    }

    void run() override;

    GlRenderTarget* srcFbo = nullptr;
    GlRenderTarget* dstCopyFbo = nullptr;
    uint32_t parentWidth = 0;
    uint32_t parentHeight = 0;
};

struct GlClipTask : GlRenderTask
{
    GlClipTask(GlRenderTask* clip, GlRenderTask* mask);
    ~GlClipTask();

    void run() override;
    void normalizeDrawDepth(int32_t maxDepth) override;

    GlRenderTask* clipTask;
    GlRenderTask* maskTask;
};

struct GlDirectBlendTask : GlRenderTask
{
    GlDirectBlendTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo, const RenderRegion& copyRegion);

    void run() override;

    GlRenderTarget* dstFbo;
    GlRenderTarget* dstCopyFbo;
    RenderRegion copyRegion;
};

struct GlComplexBlendTask : GlRenderTask
{
    GlComplexBlendTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo, GlRenderTask* stencilTask, GlComposeTask* composeTask);
    ~GlComplexBlendTask();

    void run() override;
    void normalizeDrawDepth(int32_t maxDepth) override;

    GlRenderTarget* dstFbo;
    GlRenderTarget* dstCopyFbo;
    GlRenderTask* stencilTask;
    GlComposeTask* composeTask;
};

struct GlGaussianBlurTask : GlRenderTask
{
    GlGaussianBlurTask(GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo0, GlRenderTarget* dstCopyFbo1) :
        GlRenderTask(nullptr), dstFbo(dstFbo), dstCopyFbo0(dstCopyFbo0), dstCopyFbo1(dstCopyFbo1){};

    ~GlGaussianBlurTask()
    {
        delete (horzTask);
        delete (vertTask);
    };

    void run() override;

    GlRenderTask* horzTask;
    GlRenderTask* vertTask;
    RenderEffectGaussianBlur* effect;
    GlRenderTarget* dstFbo;
    GlRenderTarget* dstCopyFbo0;
    GlRenderTarget* dstCopyFbo1;
};

struct GlEffectDropShadowTask : GlRenderTask
{
    GlEffectDropShadowTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo0, GlRenderTarget* dstCopyFbo1) :
        GlRenderTask(program), dstFbo(dstFbo), dstCopyFbo0(dstCopyFbo0), dstCopyFbo1(dstCopyFbo1){};
    ~GlEffectDropShadowTask()
    {
        delete (horzTask);
        delete (vertTask);
    };

    void run() override;

    GlRenderTask* horzTask;
    GlRenderTask* vertTask;
    RenderEffectDropShadow* effect;
    GlRenderTarget* dstFbo;
    GlRenderTarget* dstCopyFbo0;
    GlRenderTarget* dstCopyFbo1;
};

struct GlEffectColorTransformTask : GlRenderTask
{
    GlEffectColorTransformTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo) :
        GlRenderTask(program), dstFbo(dstFbo), dstCopyFbo(dstCopyFbo){};

    void run() override;

    GlRenderTarget* dstFbo;
    GlRenderTarget* dstCopyFbo;
};

#endif /* _TVG_GL_RENDER_TASK_H_ */
