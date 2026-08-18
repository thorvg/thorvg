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

#include "tvgGlCommon.h"
#include "tvgGlProgram.h"
#include "tvgGlRenderTarget.h"
#include "tvgGlStateCache.h"

struct GlVertexLayout
{
    uint32_t index;
    uint32_t size;
    uint32_t stride;
    size_t   offset;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    // VBO supplying this attribute. ThorVG layouts must always provide it.
    GLuint arrayBufferId = 0;
};

enum struct GlBindingType
{
    kUniformBuffer,
    kTexture,
};

struct GlBindingResource
{
    GlBindingType type;
    GlShaderUniform uniform = GlShaderUniform::Count;
    GlShaderUniformBlock uniformBlock = GlShaderUniformBlock::Count;
    /**
     * Binding point index.
     * Can be a uniform buffer binding index for a uniform block
     */
    uint32_t        bindPoint = 0;
    // GL object id used by this binding: texture id for kTexture, UBO id for kUniformBuffer.
    GLuint resourceId = 0;
    uint32_t        bufferOffset = 0;
    uint32_t        bufferRange = 0;

    GlBindingResource() = default;

    GlBindingResource(uint32_t index, GlShaderUniformBlock block, GLuint uniformBufferId, uint32_t offset, uint32_t range) :
        type(GlBindingType::kUniformBuffer), uniformBlock(block), bindPoint(index), resourceId(uniformBufferId), bufferOffset(offset), bufferRange(range) {}

    GlBindingResource(uint32_t bindPoint, GLuint textureId, GlShaderUniform uniform) :
        type(GlBindingType::kTexture), uniform(uniform), bindPoint(bindPoint), resourceId(textureId) {}
};

struct GlRenderTask
{
    GlRenderTask(GlProgram* program) :
        program(program) {}

    virtual ~GlRenderTask() = default;
    virtual void run(GlStateCache& state);

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
    RenderRegion viewport = {};
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

    void run(GlStateCache& state) override;
    void normalizeDrawDepth(int32_t maxDepth) override;

    Array<GlRenderTask*> stencilTasks;
    Array<GlRenderTask*> coverTasks;
    GlStencilMode stencilMode;
};

#if defined(THORVG_GL_FLAT_MASK_SUPPORT)
class GlDirectAaTask : public GlRenderTask
{
public:
    GlDirectAaTask(GlRenderTarget* dstTarget, GlRenderTask* stencilTask,
                   GlRenderTask* boundaryTask, GlRenderTask* coverTask,
                   GlStencilMode mode, const RenderRegion& shapeRegion,
                   uint32_t passWidth, uint32_t passHeight);
    ~GlDirectAaTask() override;

    void run(GlStateCache& state) override;
    void normalizeDrawDepth(int32_t maxDepth) override;

private:
    GlRenderTarget* mDstTarget;
    GlRenderTask* mStencilTask;
    GlRenderTask* mBoundaryTask;
    GlRenderTask* mCoverTask;
    GlStencilMode mStencilMode;
    RenderRegion mShapeRegion;
    uint32_t mPassWidth;
    uint32_t mPassHeight;
};

class GlFlatMaskTask : public GlRenderTask
{
public:
    GlFlatMaskTask(GlFlatMaskTarget* maskTarget, GlRenderTarget* dstTarget,
                   GlRenderTask* stencilTask, GlRenderTask* interiorTask,
                   GlRenderTask* insidePositiveTask, GlRenderTask* insideNegativeTask,
                   GlRenderTask* outsideTask,
                   GlRenderTask* compositeTask, GlStencilMode mode,
                   const RenderRegion& maskRegion, uint32_t passWidth,
                   uint32_t passHeight);
    ~GlFlatMaskTask() override;

    void run(GlStateCache& state) override;
    void normalizeDrawDepth(int32_t maxDepth) override;

private:
    GlFlatMaskTarget* mMaskTarget;
    GlRenderTarget* mDstTarget;
    GlRenderTask* mStencilTask;
    GlRenderTask* mInteriorTask;
    GlRenderTask* mInsidePositiveTask;
    GlRenderTask* mInsideNegativeTask;
    GlRenderTask* mOutsideTask;
    GlRenderTask* mCompositeTask;
    GlStencilMode mStencilMode;
    RenderRegion mMaskRegion;
    uint32_t mPassWidth;
    uint32_t mPassHeight;
};

class GlCurveMaskTask : public GlRenderTask
{
public:
    GlCurveMaskTask(GlFlatMaskTarget* maskTarget, GlRenderTarget* dstTarget,
                    GlRenderTask* stencilTask, GlRenderTask* interiorTask,
                    GlRenderTask* boundaryTask, GlRenderTask* compositeTask,
                    GlStencilMode mode, const RenderRegion& maskRegion,
                    uint32_t passWidth, uint32_t passHeight);
    ~GlCurveMaskTask() override;

    void run(GlStateCache& state) override;
    void normalizeDrawDepth(int32_t maxDepth) override;

private:
    GlFlatMaskTarget* mMaskTarget;
    GlRenderTarget* mDstTarget;
    GlRenderTask* mStencilTask;
    GlRenderTask* mInteriorTask;
    GlRenderTask* mBoundaryTask;
    GlRenderTask* mCompositeTask;
    GlStencilMode mStencilMode;
    RenderRegion mMaskRegion;
    uint32_t mPassWidth;
    uint32_t mPassHeight;
};
#endif

struct GlComposeTask : GlRenderTask
{
    GlComposeTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks);
    ~GlComposeTask();

    void run(GlStateCache& state) override;
    void onResolve(GlStateCache& state);
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
    GlBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks) :
        GlComposeTask(program, target, fbo, std::move(tasks)), colorTex(fbo->colorTex) {}

    void run(GlStateCache& state) override;

    GLuint colorTex;
    RenderRegion targetViewport = {};
};

struct GlDrawBlitTask : GlComposeTask
{

    GlDrawBlitTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks) :
        GlComposeTask(program, target, fbo, std::move(tasks)) {}
    ~GlDrawBlitTask() { delete (prevTask); }

    void setParentSize(uint32_t width, uint32_t height)
    {
        parentWidth = width;
        parentHeight = height;
    }

    void run(GlStateCache& state) override;

    GlRenderTask* prevTask = nullptr;
    uint32_t parentWidth = 0;
    uint32_t parentHeight = 0;
};

struct GlSceneBlendTask : GlComposeTask
{
    GlSceneBlendTask(GlProgram* program, GLuint target, GlRenderTarget* fbo, Array<GlRenderTask*>&& tasks) :
        GlComposeTask(program, target, fbo, std::move(tasks)) {}

    void setParentSize(uint32_t width, uint32_t height)
    {
        parentWidth = width;
        parentHeight = height;
    }

    void run(GlStateCache& state) override;

    GlRenderTarget* srcFbo = nullptr;
    GlRenderTarget* dstCopyFbo = nullptr;
    uint32_t parentWidth = 0;
    uint32_t parentHeight = 0;
};

struct GlClipTask : GlRenderTask
{
    GlClipTask(GlRenderTask* clip, GlRenderTask* mask) :
        GlRenderTask(nullptr), clipTask(clip), maskTask(mask) {}
    ~GlClipTask()
    {
        delete (clipTask);
        delete (maskTask);
    }

    void run(GlStateCache& state) override;
    void normalizeDrawDepth(int32_t maxDepth) override;

    GlRenderTask* clipTask;
    GlRenderTask* maskTask;
};

struct GlDirectBlendTask : GlRenderTask
{
    GlDirectBlendTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo, const RenderRegion& copyRegion) :
        GlRenderTask(program), dstFbo(dstFbo), dstCopyFbo(dstCopyFbo), copyRegion(copyRegion) {}

    void run(GlStateCache& state) override;

    GlRenderTarget* dstFbo;
    GlRenderTarget* dstCopyFbo;
    RenderRegion copyRegion;
};

struct GlComplexBlendTask : GlRenderTask
{
    GlComplexBlendTask(GlProgram* program, GlRenderTarget* dstFbo, GlRenderTarget* dstCopyFbo, GlRenderTask* stencilTask, GlComposeTask* composeTask) :
        GlRenderTask(program), dstFbo(dstFbo), dstCopyFbo(dstCopyFbo), stencilTask(stencilTask), composeTask(composeTask) {}
    ~GlComplexBlendTask()
    {
        delete (stencilTask);
        delete (composeTask);
    }

    void run(GlStateCache& state) override;
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

    void run(GlStateCache& state) override;

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

    void run(GlStateCache& state) override;

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

    void run(GlStateCache& state) override;

    GlRenderTarget* dstFbo;
    GlRenderTarget* dstCopyFbo;
};

#endif /* _TVG_GL_RENDER_TASK_H_ */
