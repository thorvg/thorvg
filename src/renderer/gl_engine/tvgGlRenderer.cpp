/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgGlCommon.h"
#include "tvgGlRenderer.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlRenderTask.h"
#include "tvgGlProgram.h"
#include "tvgGlShaderSrc.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

#define NOISE_LEVEL 0.5f

static int32_t initEngineCnt = false;
static int32_t rendererCnt = 0;


static void _termEngine()
{
    if (rendererCnt > 0) return;

    //TODO: Clean up global resources
}


void GlRenderer::clearDisposes()
{
    if (mDisposed.textures.count > 0) {
        glDeleteTextures(mDisposed.textures.count, mDisposed.textures.data);
        mDisposed.textures.clear();
    }

    ARRAY_FOREACH(p, mRenderPassStack) delete(*p);
    mRenderPassStack.clear();
}


void GlRenderer::flush()
{
    clearDisposes();

    ARRAY_FOREACH(p, mComposePool) delete(*p);
    mComposePool.clear();

    ARRAY_FOREACH(p, mBlendPool) delete(*p);
    mBlendPool.clear();

    ARRAY_FOREACH(p, mComposeStack) delete(*p);
    mComposeStack.clear();
}


void GlRenderer::currentContext()
{
#ifdef __EMSCRIPTEN__
    auto targetContext = (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)mContext;
    if (emscripten_webgl_get_current_context() != targetContext) {
        emscripten_webgl_make_context_current(targetContext);
    }
#else
    TVGERR("GL_ENGINE", "Maybe missing MakeCurrent() Call?");
#endif
}


GlRenderer::GlRenderer()
{
}


GlRenderer::~GlRenderer()
{
    --rendererCnt;

    flush();

    ARRAY_FOREACH(p, mPrograms) delete(*p);

    if (rendererCnt == 0 && initEngineCnt == 0) _termEngine();
}


void GlRenderer::initShaders()
{
    mPrograms.reserve((int)RT_None);

    mPrograms.push(new GlProgram(COLOR_VERT_SHADER, COLOR_FRAG_SHADER));
    mPrograms.push(new GlProgram(GRADIENT_VERT_SHADER, LINEAR_GRADIENT_FRAG_SHADER));
    mPrograms.push(new GlProgram(GRADIENT_VERT_SHADER, RADIAL_GRADIENT_FRAG_SHADER));
    mPrograms.push(new GlProgram(IMAGE_VERT_SHADER, IMAGE_FRAG_SHADER));

    // compose Renderer
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_ALPHA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_INV_ALPHA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_LUMA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_INV_LUMA_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_ADD_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_SUB_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_INTERSECT_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_DIFF_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_LIGHTEN_FRAG_SHADER));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MASK_DARKEN_FRAG_SHADER));

    // stencil Renderer
    mPrograms.push(new GlProgram(STENCIL_VERT_SHADER, STENCIL_FRAG_SHADER));

    // blit Renderer
    mPrograms.push(new GlProgram(BLIT_VERT_SHADER, BLIT_FRAG_SHADER));

    // complex blending Renderer
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, MULTIPLY_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, SCREEN_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, OVERLAY_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, COLOR_DODGE_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, COLOR_BURN_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, HARD_LIGHT_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, SOFT_LIGHT_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, DIFFERENCE_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, EXCLUSION_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, LIGHTEN_BLEND_FRAG));
    mPrograms.push(new GlProgram(MASK_VERT_SHADER, DARKEN_BLEND_FRAG));
}


void GlRenderer::drawPrimitive(GlShape& sdata, const RenderColor& c, RenderUpdateFlag flag, int32_t depth)
{
    auto vp = currentPass()->getViewport();
    auto bbox = sdata.geometry.getViewport();

    bbox.intersect(vp);

    auto complexBlend = beginComplexBlending(mBlendMethod, bbox, sdata.geometry.getBounds());

    if (complexBlend) {
        vp = currentPass()->getViewport();
        bbox.intersect(vp);
    }

    auto x = bbox.x - vp.x;
    auto y = bbox.y - vp.y;
    auto w = bbox.w;
    auto h = bbox.h;

    GlRenderTask* task = nullptr;
    if (mBlendMethod != BlendMethod::Normal && !complexBlend) task = new GlSimpleBlendTask(mBlendMethod, mPrograms[RT_Color]);
    else task = new GlRenderTask(mPrograms[RT_Color]);

    task->setDrawDepth(depth);

    if (!sdata.geometry.draw(task, &mGpuBuffer, flag)) {
        delete task;
        return;
    }

    task->setViewport({x, vp.h - y - h, w, h});

    GlRenderTask* stencilTask = nullptr;

    GlStencilMode stencilMode = sdata.geometry.getStencilMode(flag);
    if (stencilMode != GlStencilMode::None) {
        stencilTask = new GlRenderTask(mPrograms[RT_Stencil], task);
        stencilTask->setDrawDepth(depth);
    }

    auto a = MULTIPLY(c.a, sdata.opacity);

    if (flag & RenderUpdateFlag::Stroke) {
        float strokeWidth = sdata.rshape->strokeWidth() * getScaleFactor(sdata.geometry.getTransformMatrix());
        if (strokeWidth < MIN_GL_STROKE_WIDTH) {
            float alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
            a = MULTIPLY(a, static_cast<uint8_t>(alpha * 255));
        }
    }

    // matrix buffer
    const auto& matrix = sdata.geometry.getTransformMatrix();

    float matrix44[16];
    currentPass()->getMatrix(matrix44, matrix);
    auto viewOffset = mGpuBuffer.push(matrix44, 16 * sizeof(float), true);

    task->addBindResource(GlBindingResource{
        0,
        task->getProgram()->getUniformBlockIndex("Matrix"),
        mGpuBuffer.getBufferId(),
        viewOffset,
        16 * sizeof(float),
    });

    if (stencilTask) {
        stencilTask->addBindResource(GlBindingResource{
            0,
            stencilTask->getProgram()->getUniformBlockIndex("Matrix"),
            mGpuBuffer.getBufferId(),
            viewOffset,
            16 * sizeof(float),
        });
    }

    // color
    float color[] = {c.r / 255.f, c.g / 255.f, c.b / 255.f, a / 255.f};

    task->addBindResource(GlBindingResource{
        1,
        task->getProgram()->getUniformBlockIndex("ColorInfo"),
        mGpuBuffer.getBufferId(),
        mGpuBuffer.push(color, 4 * sizeof(float), true),
         4 * sizeof(float),
    });

    if (stencilTask) currentPass()->addRenderTask(new GlStencilCoverTask(stencilTask, task, stencilMode));
    else currentPass()->addRenderTask(task);

    if (complexBlend) endBlendingCompose(mBlendMethod);
}


void GlRenderer::drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag, int32_t depth)
{
    auto vp = currentPass()->getViewport();
    auto bbox = sdata.geometry.getViewport();

    bbox.intersect(vp);

    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = min(fill->colorStops(&stops), static_cast<uint32_t>(MAX_GRADIENT_STOPS));
    if (stopCnt < 2) return;

    GlRenderTask* task = nullptr;

    if (fill->type() == Type::LinearGradient) {
        task = new GlRenderTask(mPrograms[RT_LinGradient]);
    } else if (fill->type() == Type::RadialGradient) {
        task = new GlRenderTask(mPrograms[RT_RadGradient]);
    } else {
        return;
    }

    task->setDrawDepth(depth);

    if (!sdata.geometry.draw(task, &mGpuBuffer, flag)) {
        delete task;
        return;
    }

    auto complexBlend = beginComplexBlending(mBlendMethod, bbox, sdata.geometry.getBounds());

    if (complexBlend) vp = currentPass()->getViewport();

    auto x = bbox.x - vp.x;
    auto y = bbox.y - vp.y;

    task->setViewport({x, vp.h - y - bbox.h, bbox.w, bbox.h});

    GlRenderTask* stencilTask = nullptr;
    GlStencilMode stencilMode = sdata.geometry.getStencilMode(flag);
    if (stencilMode != GlStencilMode::None) {
        stencilTask = new GlRenderTask(mPrograms[RT_Stencil], task);
        stencilTask->setDrawDepth(depth);
    }

    // matrix buffer
    const auto& matrix = sdata.geometry.getTransformMatrix();
    float invMat4[16];
    Matrix inv;
    inverse(&fill->transform(), &inv);
    GET_MATRIX44(inv, invMat4);

    float matrix44[16];
    currentPass()->getMatrix(matrix44, matrix);

    auto viewOffset = mGpuBuffer.push(matrix44, 16 * sizeof(float), true);

    task->addBindResource(GlBindingResource{
        0,
        task->getProgram()->getUniformBlockIndex("Matrix"),
        mGpuBuffer.getBufferId(),
        viewOffset,
        16 * sizeof(float),
    });

    if (stencilTask) {
        stencilTask->addBindResource(GlBindingResource{
            0,
            stencilTask->getProgram()->getUniformBlockIndex("Matrix"),
            mGpuBuffer.getBufferId(),
            viewOffset,
            16 * sizeof(float),
        });
    }

    viewOffset = mGpuBuffer.push(invMat4, 16 * sizeof(float), true);

    task->addBindResource(GlBindingResource{
        1,
        task->getProgram()->getUniformBlockIndex("InvMatrix"),
        mGpuBuffer.getBufferId(),
        viewOffset,
        16 * sizeof(float),
    });

    auto alpha = sdata.opacity / 255.f;

    if (flag & RenderUpdateFlag::GradientStroke) {
        auto strokeWidth = sdata.rshape->strokeWidth();
        if (strokeWidth < MIN_GL_STROKE_WIDTH) {
            alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
        }
    }

    // gradient block
    GlBindingResource gradientBinding{};
    auto loc = task->getProgram()->getUniformBlockIndex("GradientInfo");

    if (fill->type() == Type::LinearGradient) {
        auto linearFill = static_cast<const LinearGradient*>(fill);

        GlLinearGradientBlock gradientBlock;

        gradientBlock.nStops[1] = NOISE_LEVEL;
        gradientBlock.nStops[2] = static_cast<int32_t>(fill->spread()) * 1.f;
        uint32_t nStops = 0;
        for (uint32_t i = 0; i < stopCnt; ++i) {
            if (i > 0 && gradientBlock.stopPoints[nStops - 1] > stops[i].offset) continue;

            gradientBlock.stopPoints[i] = stops[i].offset;
            gradientBlock.stopColors[i * 4 + 0] = stops[i].r / 255.f;
            gradientBlock.stopColors[i * 4 + 1] = stops[i].g / 255.f;
            gradientBlock.stopColors[i * 4 + 2] = stops[i].b / 255.f;
            gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f * alpha;
            nStops++;
        }
        gradientBlock.nStops[0] = nStops * 1.f;

        float x1, x2, y1, y2;
        linearFill->linear(&x1, &y1, &x2, &y2);

        gradientBlock.startPos[0] = x1;
        gradientBlock.startPos[1] = y1;
        gradientBlock.stopPos[0] = x2;
        gradientBlock.stopPos[1] = y2;

        gradientBinding = GlBindingResource{
            2,
            loc,
            mGpuBuffer.getBufferId(),
            mGpuBuffer.push(&gradientBlock, sizeof(GlLinearGradientBlock), true),
            sizeof(GlLinearGradientBlock),
        };
    } else {
        auto radialFill = static_cast<const RadialGradient*>(fill);

        GlRadialGradientBlock gradientBlock;

        gradientBlock.nStops[1] = NOISE_LEVEL;
        gradientBlock.nStops[2] = static_cast<int32_t>(fill->spread()) * 1.f;

        uint32_t nStops = 0;
        for (uint32_t i = 0; i < stopCnt; ++i) {
            if (i > 0 && gradientBlock.stopPoints[nStops - 1] > stops[i].offset) continue; 

            gradientBlock.stopPoints[i] = stops[i].offset;
            gradientBlock.stopColors[i * 4 + 0] = stops[i].r / 255.f;
            gradientBlock.stopColors[i * 4 + 1] = stops[i].g / 255.f;
            gradientBlock.stopColors[i * 4 + 2] = stops[i].b / 255.f;
            gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f * alpha;
            nStops++;
        }
        gradientBlock.nStops[0] = nStops * 1.f;

        float x, y, r, fx, fy, fr;
        radialFill->radial(&x, &y, &r, &fx, &fy, &fr);

        gradientBlock.centerPos[0] = fx;
        gradientBlock.centerPos[1] = fy;
        gradientBlock.centerPos[2] = x;
        gradientBlock.centerPos[3] = y;
        gradientBlock.radius[0] = fr;
        gradientBlock.radius[1] = r;

        gradientBinding = GlBindingResource{
            2,
            loc,
            mGpuBuffer.getBufferId(),
            mGpuBuffer.push(&gradientBlock, sizeof(GlRadialGradientBlock), true),
            sizeof(GlRadialGradientBlock),
        };
    }

    task->addBindResource(gradientBinding);

    if (stencilTask) {
        currentPass()->addRenderTask(new GlStencilCoverTask(stencilTask, task, stencilMode));
    } else {
        currentPass()->addRenderTask(task);
    }

    if (complexBlend) endBlendingCompose(mBlendMethod);
}


void GlRenderer::drawClip(Array<RenderData>& clips)
{
    Array<float> identityVertex(4 * 2);
    float left = -1.f;
    float top = 1.f;
    float right = 1.f;
    float bottom = -1.f;

    identityVertex.push(left);
    identityVertex.push(top);
    identityVertex.push(left);
    identityVertex.push(bottom);
    identityVertex.push(right);
    identityVertex.push(top);
    identityVertex.push(right);
    identityVertex.push(bottom);

    Array<uint32_t> identityIndex(6);
    identityIndex.push(0);
    identityIndex.push(1);
    identityIndex.push(2);
    identityIndex.push(2);
    identityIndex.push(1);
    identityIndex.push(3);

    float mat4[16];
    memset(mat4, 0, sizeof(float) * 16);
    mat4[0] = 1.f;
    mat4[5] = 1.f;
    mat4[10] = 1.f;
    mat4[15] = 1.f;

    auto identityVertexOffset = mGpuBuffer.push(identityVertex.data, 8 * sizeof(float));
    auto identityIndexOffset = mGpuBuffer.pushIndex(identityIndex.data, 6 * sizeof(uint32_t));
    auto mat4Offset = mGpuBuffer.push(mat4, 16 * sizeof(float), true);

    Array<int32_t> clipDepths(clips.count);
    clipDepths.count = clips.count;

    for (int32_t i = clips.count - 1; i >= 0; i--) {
        clipDepths[i] = currentPass()->nextDrawDepth();
    }

    const auto& vp = currentPass()->getViewport();

    for (uint32_t i = 0; i < clips.count; ++i) {
        auto sdata = static_cast<GlShape*>(clips[i]);

        auto clipTask = new GlRenderTask(mPrograms[RT_Stencil]);

        clipTask->setDrawDepth(clipDepths[i]);

        sdata->geometry.draw(clipTask, &mGpuBuffer, RenderUpdateFlag::Path);

        auto bbox = sdata->geometry.getViewport();

        bbox.intersect(vp);

        auto x = bbox.x - vp.x;
        auto y = bbox.y - vp.y;

        clipTask->setViewport({x, vp.h - y - bbox.h, bbox.w, bbox.h});

        const auto& matrix = sdata->geometry.getTransformMatrix();

        float matrix44[16];

        currentPass()->getMatrix(matrix44, matrix);

        auto loc = clipTask->getProgram()->getUniformBlockIndex("Matrix");
        auto viewOffset = mGpuBuffer.push(matrix44, 16 * sizeof(float), true);

        clipTask->addBindResource(GlBindingResource{0, loc, mGpuBuffer.getBufferId(), viewOffset, 16 * sizeof(float), });

        auto maskTask = new GlRenderTask(mPrograms[RT_Stencil]);

        maskTask->setDrawDepth(clipDepths[i]);
        maskTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), identityVertexOffset});
        maskTask->addBindResource(GlBindingResource{0, loc, mGpuBuffer.getBufferId(), mat4Offset, 16 * sizeof(float), });
        maskTask->setDrawRange(identityIndexOffset, 6);
        maskTask->setViewport({0, 0, static_cast<int32_t>(vp.w), static_cast<int32_t>(vp.h)});

        currentPass()->addRenderTask(new GlClipTask(clipTask, maskTask));
    }
}

GlRenderPass* GlRenderer::currentPass()
{
    if (mRenderPassStack.empty()) return nullptr;
    return mRenderPassStack.last();
}

bool GlRenderer::beginComplexBlending(BlendMethod blend, const RenderRegion& vp, RenderRegion bounds)
{
    if (vp.w == 0 || vp.h == 0) return false;

    bounds.intersect(vp);

    if (bounds.w == 0 || bounds.h == 0) return false;

    if (blend == BlendMethod::Normal || blend == BlendMethod::Add) return false;

    if (mBlendPool.empty()) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));

    auto blendFbo = mBlendPool[0]->getRenderTarget(bounds);

    mRenderPassStack.push(new GlRenderPass(blendFbo, bounds));

    return true;
}

void GlRenderer::endBlendingCompose(BlendMethod blend)
{
    auto blendPass = mRenderPassStack.last();
    mRenderPassStack.pop();
    
    blendPass->setDrawDepth(currentPass()->nextDrawDepth());

    auto composeTask = blendPass->endRenderPass<GlComposeTask>(nullptr, currentPass()->getFboId());

    const auto& vp = blendPass->getViewport();
    if (mBlendPool.count < 2) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));
    auto dstCopyFbo = mBlendPool[1]->getRenderTarget(vp);

    auto task = new GlComplexBlendTask(getBlendProgram(blend), currentPass()->getFbo(), dstCopyFbo, composeTask);

    task->setViewport(currentPass()->getViewport());
    task->setCopyViewport(blendPass->getViewport());

    prepareCmpTask(task, vp, blendPass->getFboWidth(), blendPass->getFboHeight());

    task->setDrawDepth(currentPass()->nextDrawDepth());

    // src and dst texture
    task->addBindResource(GlBindingResource{1, blendPass->getFbo()->getColorTexture(), task->getProgram()->getUniformLocation("uSrcTexture")});
    task->addBindResource(GlBindingResource{2, dstCopyFbo->getColorTexture(), task->getProgram()->getUniformLocation("uDstTexture")});

    currentPass()->addRenderTask(task);

    delete(blendPass);
}

GlProgram* GlRenderer::getBlendProgram(BlendMethod blend)
{
    switch (blend) {
        case BlendMethod::Multiply: return mPrograms[RT_MultiplyBlend];
        case BlendMethod::Screen: return mPrograms[RT_ScreenBlend];
        case BlendMethod::Overlay: return mPrograms[RT_OverlayBlend];
        case BlendMethod::ColorDodge: return mPrograms[RT_ColorDodgeBlend];
        case BlendMethod::ColorBurn: return mPrograms[RT_ColorBurnBlend];
        case BlendMethod::HardLight: return mPrograms[RT_HardLightBlend];
        case BlendMethod::SoftLight: return mPrograms[RT_SoftLightBlend];
        case BlendMethod::Difference: return mPrograms[RT_DifferenceBlend];
        case BlendMethod::Exclusion: return mPrograms[RT_ExclusionBlend];
        case BlendMethod::Lighten: return mPrograms[RT_LightenBlend];
        case BlendMethod::Darken: return mPrograms[RT_DarkenBlend];
        default: return nullptr;
    }
}


void GlRenderer::prepareBlitTask(GlBlitTask* task)
{
    RenderRegion region{0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)};
    prepareCmpTask(task, region, surface.w, surface.h);
    task->addBindResource(GlBindingResource{0, task->getColorTexture(), task->getProgram()->getUniformLocation("uSrcTexture")});
}


void GlRenderer::prepareCmpTask(GlRenderTask* task, const RenderRegion& vp, uint32_t cmpWidth, uint32_t cmpHeight)
{
    // we use 1:1 blit mapping since compositor fbo is same size as root fbo
    Array<float> vertices(4 * 4);

    const auto& passVp = currentPass()->getViewport();
    
    auto taskVp = vp;
    taskVp.intersect(passVp);

    auto x = taskVp.x - passVp.x;
    auto y = taskVp.y - passVp.y;
    auto w = taskVp.w;
    auto h = taskVp.h;

    float rw = static_cast<float>(passVp.w);
    float rh = static_cast<float>(passVp.h);

    float l = static_cast<float>(x);
    float t = static_cast<float>(rh - y);
    float r = static_cast<float>(x + w);
    float b = static_cast<float>(rh - y - h);

    // map vp ltrp to -1:1
    float left = (l / rw) * 2.f - 1.f;
    float top = (t / rh) * 2.f - 1.f;
    float right = (r / rw) * 2.f - 1.f;
    float bottom = (b / rh) * 2.f - 1.f;

    float uw = static_cast<float>(w) / static_cast<float>(cmpWidth);
    float uh = static_cast<float>(h) / static_cast<float>(cmpHeight);

    // left top point
    vertices.push(left);
    vertices.push(top);
    vertices.push(0.f);
    vertices.push(uh);
    // left bottom point
    vertices.push(left);
    vertices.push(bottom);
    vertices.push(0.f);
    vertices.push(0.f);
    // right top point
    vertices.push(right);
    vertices.push(top);
    vertices.push(uw);
    vertices.push(uh);
    // right bottom point
    vertices.push(right);
    vertices.push(bottom);
    vertices.push(uw);
    vertices.push(0.f);

    Array<uint32_t> indices(6);

    indices.push(0);
    indices.push(1);
    indices.push(2);
    indices.push(2);
    indices.push(1);
    indices.push(3);

    uint32_t vertexOffset = mGpuBuffer.push(vertices.data, vertices.count * sizeof(float));
    uint32_t indexOffset = mGpuBuffer.pushIndex(indices.data, indices.count * sizeof(uint32_t));

    task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset});
    task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float)});

    task->setDrawRange(indexOffset, indices.count);

    task->setViewport({x, static_cast<int32_t>((passVp.h - y - h)), w, h});
}

void GlRenderer::prepareCmpStencilTask(GlRenderTask* task, RenderRegion& vp)
{
    auto x = 0;
    auto y = 0;
    auto w = vp.w;
    auto h = vp.h;

    Array<float> vertices(4 * 2);
    // left top
    vertices.push(x);
    vertices.push(y);
    // left bottom
    vertices.push(x);
    vertices.push(y + h);
    // right top
    vertices.push(x + w);
    vertices.push(y);
    // right bottom
    vertices.push(x + w);
    vertices.push(y + h);

    Array<uint32_t> indices(6);
    indices.push(0);
    indices.push(1);
    indices.push(2);
    indices.push(2);
    indices.push(1);
    indices.push(3);

    uint32_t vertexOffset = mGpuBuffer.push(vertices.data, vertices.count * sizeof(float));
    uint32_t indexOffset = mGpuBuffer.pushIndex(indices.data, indices.count * sizeof(uint32_t));
    task->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), vertexOffset});
    task->setDrawRange(indexOffset, indices.count);
}


void GlRenderer::endRenderPass(RenderCompositor* cmp)
{
    auto glCmp = static_cast<GlCompositor*>(cmp);
    if (cmp->method != MaskMethod::None) {
        auto selfPass = mRenderPassStack.last();
        mRenderPassStack.pop();

        // mask is pushed first
        auto maskPass = mRenderPassStack.last();
        mRenderPassStack.pop();

        bool complexBlend = beginComplexBlending(glCmp->blend, glCmp->bbox, glCmp->bbox);

        GlProgram* program = nullptr;
        switch(cmp->method) {
            case MaskMethod::Alpha: program = mPrograms[RT_MaskAlpha]; break;
            case MaskMethod::InvAlpha: program = mPrograms[RT_MaskAlphaInv]; break;
            case MaskMethod::Luma: program = mPrograms[RT_MaskLuma]; break;
            case MaskMethod::InvLuma: program = mPrograms[RT_MaskLumaInv]; break;
            case MaskMethod::Add: program = mPrograms[RT_MaskAdd]; break;
            case MaskMethod::Subtract: program = mPrograms[RT_MaskSub]; break;
            case MaskMethod::Intersect: program = mPrograms[RT_MaskIntersect]; break;
            case MaskMethod::Difference: program = mPrograms[RT_MaskDifference]; break;
            case MaskMethod::Lighten: program = mPrograms[RT_MaskLighten]; break;
            case MaskMethod::Darken: program = mPrograms[RT_MaskDarken]; break;
            default: break;
        }

        if (program && !selfPass->isEmpty() && !maskPass->isEmpty()) {
            auto prev_task = maskPass->endRenderPass<GlComposeTask>(nullptr, currentPass()->getFboId());
            prev_task->setDrawDepth(currentPass()->nextDrawDepth());
            prev_task->setRenderSize(static_cast<uint32_t>(glCmp->bbox.w), static_cast<uint32_t>(glCmp->bbox.h));
            prev_task->setViewport(glCmp->bbox);

            auto compose_task = selfPass->endRenderPass<GlDrawBlitTask>(program, currentPass()->getFboId());
            compose_task->setRenderSize(static_cast<uint32_t>(glCmp->bbox.w), static_cast<uint32_t>(glCmp->bbox.h));
            compose_task->setPrevTask(prev_task);

            prepareCmpTask(compose_task, glCmp->bbox, selfPass->getFboWidth(), selfPass->getFboHeight());

            compose_task->addBindResource(GlBindingResource{0, selfPass->getTextureId(), program->getUniformLocation("uSrcTexture")});
            compose_task->addBindResource(GlBindingResource{1, maskPass->getTextureId(), program->getUniformLocation("uMaskTexture")});

            compose_task->setDrawDepth(currentPass()->nextDrawDepth());
            compose_task->setParentSize(static_cast<uint32_t>(currentPass()->getViewport().w), static_cast<uint32_t>(currentPass()->getViewport().h));
            currentPass()->addRenderTask(compose_task);
        }

        if (complexBlend) endBlendingCompose(glCmp->blend);

        maskPass->getFbo()->setInUse(false);
        selfPass->getFbo()->setInUse(false);

        delete(selfPass);
        delete(maskPass);
    } else {

        auto renderPass = mRenderPassStack.last();
        mRenderPassStack.pop();

        if (!renderPass->isEmpty()) {
            bool complexBlend = beginComplexBlending(glCmp->blend, glCmp->bbox, glCmp->bbox);

            auto task = renderPass->endRenderPass<GlDrawBlitTask>(mPrograms[RT_Image], currentPass()->getFboId());
            task->setRenderSize(static_cast<uint32_t>(glCmp->bbox.w), static_cast<uint32_t>(glCmp->bbox.h));
            prepareCmpTask(task, glCmp->bbox, renderPass->getFboWidth(), renderPass->getFboHeight());
            task->setDrawDepth(currentPass()->nextDrawDepth());

            // matrix buffer
            float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

            task->addBindResource(GlBindingResource{
                0,
                task->getProgram()->getUniformBlockIndex("Matrix"),
                mGpuBuffer.getBufferId(),
                mGpuBuffer.push(matrix, 16 * sizeof(float), true),
                16 * sizeof(float),
            });

            // image info
            uint32_t info[4] = {(uint32_t)ColorSpace::ABGR8888, 0, cmp->opacity, 0};

            task->addBindResource(GlBindingResource{
                1,
                task->getProgram()->getUniformBlockIndex("ColorInfo"),
                mGpuBuffer.getBufferId(),
                mGpuBuffer.push(info, 4 * sizeof(uint32_t), true),
                4 * sizeof(uint32_t),
            });

            task->addBindResource(GlBindingResource{
                1,
                task->getProgram()->getUniformBlockIndex("ColorInfo"),
                mGpuBuffer.getBufferId(),
                mGpuBuffer.push(info, 4 * sizeof(uint32_t), true),
                4 * sizeof(uint32_t),
            });

            // texture id
            task->addBindResource(GlBindingResource{0, renderPass->getTextureId(), task->getProgram()->getUniformLocation("uTexture")});
            task->setParentSize(static_cast<uint32_t>(currentPass()->getViewport().w), static_cast<uint32_t>(currentPass()->getViewport().h));
            currentPass()->addRenderTask(std::move(task));

            if (complexBlend) endBlendingCompose(glCmp->blend);
            renderPass->getFbo()->setInUse(false);

        }
        delete(renderPass);
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool GlRenderer::clear()
{
    mClearBuffer = true;
    return true;
}


bool GlRenderer::target(void* context, int32_t id, uint32_t w, uint32_t h)
{
    //assume the context zero is invalid
    if (!context || id == GL_INVALID_VALUE || w == 0 || h == 0) return false;

    if (mContext) currentContext();

    flush();

    surface.stride = w;
    surface.w = w;
    surface.h = h;

    mContext = context;
    mTargetFboId = static_cast<GLint>(id);

    currentContext();

    mRootTarget = GlRenderTarget(surface.w, surface.h);
    mRootTarget.setViewport({0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)});
    mRootTarget.init(mTargetFboId);

    return true;
}


bool GlRenderer::sync()
{
    //nothing to be done.
    if (mRenderPassStack.empty()) return true;

    currentContext();

    // Blend function for straight alpha
    GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glEnable(GL_SCISSOR_TEST));
    GL_CHECK(glCullFace(GL_FRONT_AND_BACK));
    GL_CHECK(glFrontFace(GL_CCW));
    GL_CHECK(glEnable(GL_DEPTH_TEST));
    GL_CHECK(glDepthFunc(GL_GREATER));

    auto task = mRenderPassStack.first()->endRenderPass<GlBlitTask>(mPrograms[RT_Blit], mTargetFboId);

    prepareBlitTask(task);

    task->mClearBuffer = mClearBuffer;
    task->setTargetViewport({0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)});

    if (mGpuBuffer.flushToGPU()) {
        mGpuBuffer.bind();
        task->run();
    }

    mGpuBuffer.unbind();

    GL_CHECK(glDisable(GL_SCISSOR_TEST));

    clearDisposes();

    delete task;

    return true;
}


RenderRegion GlRenderer::region(RenderData data)
{
    if (currentPass()->isEmpty()) return {0, 0, 0, 0};

    auto shape = reinterpret_cast<GlShape*>(data);
    auto bounds = shape->geometry.getBounds();

    auto const& vp = currentPass()->getViewport();
    bounds.intersect(vp);

    return bounds;
}


bool GlRenderer::preRender()
{
    currentContext();
    if (mPrograms.empty()) initShaders();
    mRenderPassStack.push(new GlRenderPass(&mRootTarget, RenderRegion{0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)}));

    return true;
}


bool GlRenderer::postRender()
{
    return true;
}


RenderCompositor* GlRenderer::target(const RenderRegion& region, TVG_UNUSED ColorSpace cs, TVG_UNUSED CompositionFlag flags)
{
    auto vp = region;
    if (currentPass()->isEmpty()) return nullptr;

    vp.intersect(currentPass()->getViewport());

    mComposeStack.push(new GlCompositor(vp));
    return mComposeStack.last();
}


bool GlRenderer::beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity)
{
    if (!cmp) return false;

    cmp->method = method;
    cmp->opacity = opacity;

    uint32_t index = mRenderPassStack.count - 1;

    if (index >= mComposePool.count) {
        mComposePool.push( new GlRenderTargetPool(surface.w, surface.h));
    }

    auto glCmp = static_cast<GlCompositor*>(cmp);
    glCmp->blend = mBlendMethod;

    if (glCmp->bbox.w > 0 && glCmp->bbox.h > 0) {
        auto renderTarget = mComposePool[index]->getRenderTarget(glCmp->bbox);
        mRenderPassStack.push(new GlRenderPass(renderTarget, glCmp->bbox));
    } else {
        // empty render pass
        mRenderPassStack.push(new GlRenderPass(nullptr, glCmp->bbox));
    }

    return true;
}


bool GlRenderer::endComposite(RenderCompositor* cmp)
{
    if (mComposeStack.empty()) return false;
    if (mComposeStack.last() != cmp) return false;

    // end current render pass;
    auto curCmp  = mComposeStack.last();
    mComposeStack.pop();

    assert(cmp == curCmp);

    endRenderPass(curCmp);

    delete(curCmp);

    return true;
}


void GlRenderer::prepare(TVG_UNUSED RenderEffect* effect, TVG_UNUSED const Matrix& transform)
{
    //TODO: prepare the effect
}


bool GlRenderer::region(TVG_UNUSED RenderEffect* effect)
{
    //TODO: Return if the current post effect requires the region expansion
    return false;
}


bool GlRenderer::render(TVG_UNUSED RenderCompositor* cmp, TVG_UNUSED const RenderEffect* effect, TVG_UNUSED bool direct)
{
    TVGLOG("GL_ENGINE", "SceneEffect(%d) is not supported", (int)effect->type);
    return false;
}


void GlRenderer::dispose(TVG_UNUSED RenderEffect* effect)
{
    //TODO: dispose the effect
}


ColorSpace GlRenderer::colorSpace()
{
    return ColorSpace::Unknown;
}


const RenderSurface* GlRenderer::mainSurface()
{
    return &surface;
}


bool GlRenderer::blend(BlendMethod method)
{
    if (method == mBlendMethod) return true;

    mBlendMethod = method;

    return true;
}


bool GlRenderer::renderImage(void* data)
{
    auto sdata = static_cast<GlShape*>(data);

    if (!sdata) return false;

    if (currentPass()->isEmpty()) return true;

    if ((sdata->updateFlag & RenderUpdateFlag::Image) == 0) return true;
    auto vp = currentPass()->getViewport();

    auto bbox = sdata->geometry.getViewport();

    bool complexBlend = beginComplexBlending(mBlendMethod, bbox, sdata->geometry.getBounds());

    if (complexBlend) vp = currentPass()->getViewport();

    bbox.intersect(vp);

    if (bbox.w <= 0 || bbox.h <= 0) return true;

    auto x = bbox.x - vp.x;
    auto y = bbox.y - vp.y;


    int32_t drawDepth = currentPass()->nextDrawDepth();

    if (!sdata->clips.empty()) drawClip(sdata->clips);

    GlRenderTask* task = nullptr;
    if (mBlendMethod != BlendMethod::Normal && !complexBlend) task = new GlSimpleBlendTask(mBlendMethod, mPrograms[RT_Image]);
    else task = new GlRenderTask(mPrograms[RT_Image]);

    task->setDrawDepth(drawDepth);

    if (!sdata->geometry.draw(task, &mGpuBuffer, RenderUpdateFlag::Image)) {
        delete task;
        return true;
    }

    // matrix buffer
    const auto& matrix = sdata->geometry.getTransformMatrix();
    float matrix44[16];
    currentPass()->getMatrix(matrix44, matrix);

    task->addBindResource(GlBindingResource{
        0,
        task->getProgram()->getUniformBlockIndex("Matrix"),
        mGpuBuffer.getBufferId(),
        mGpuBuffer.push(matrix44, 16 * sizeof(float), true),
        16 * sizeof(float),
    });

    // image info
    uint32_t info[4] = {(uint32_t)sdata->texColorSpace, sdata->texFlipY, sdata->opacity, 0};

    task->addBindResource(GlBindingResource{
        1,
        task->getProgram()->getUniformBlockIndex("ColorInfo"),
        mGpuBuffer.getBufferId(),
        mGpuBuffer.push(info, 4 * sizeof(uint32_t), true),
        4 * sizeof(uint32_t),
    });

    // texture id
    task->addBindResource(GlBindingResource{0, sdata->texId, task->getProgram()->getUniformLocation("uTexture")});

    task->setViewport({x, vp.h - y - bbox.h, bbox.w, bbox.h});

    currentPass()->addRenderTask(task);

    if (complexBlend) endBlendingCompose(mBlendMethod);

    return true;
}


bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);

    if (currentPass()->isEmpty()) return true;

    if (sdata->updateFlag == RenderUpdateFlag::None) return true;

    const auto& vp = currentPass()->getViewport();

    auto bbox = sdata->geometry.getViewport();
    bbox.intersect(vp);

    if (bbox.w <= 0 || bbox.h <= 0) return true;

    int32_t drawDepth1 = 0, drawDepth2 = 0;

    size_t flags = static_cast<size_t>(sdata->updateFlag);

    if (flags == 0) return false;

    if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Color)) drawDepth1 = currentPass()->nextDrawDepth();

    if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke)) drawDepth2 = currentPass()->nextDrawDepth();

    if (!sdata->clips.empty()) drawClip(sdata->clips);

    auto processFill = [&]() {
        if (flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient)) {
            if (const auto& gradient = sdata->rshape->fill) {
                drawPrimitive(*sdata, gradient, RenderUpdateFlag::Gradient, drawDepth1);
            } else if (sdata->rshape->color.a > 0) {
                drawPrimitive(*sdata, sdata->rshape->color, RenderUpdateFlag::Color, drawDepth1);
            }
        }
    };

    auto processStroke = [&]() {
        if (!sdata->rshape->stroke) return;
        if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke)) {
            if (const auto& gradient = sdata->rshape->strokeFill()) {
                drawPrimitive(*sdata, gradient, RenderUpdateFlag::GradientStroke, drawDepth2);
            } else if (sdata->rshape->stroke->color.a > 0) {
                drawPrimitive(*sdata, sdata->rshape->stroke->color, RenderUpdateFlag::Stroke, drawDepth2);
            }
        }
    };

    if (sdata->rshape->stroke && sdata->rshape->stroke->strokeFirst) {
        processStroke();
        processFill();
    } else {
        processFill();
        processStroke();
    }

    return true;
}


void GlRenderer::dispose(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);

    //dispose the non thread-safety resources on clearDisposes() call
    if (sdata->texId) {
        ScopedLock lock(mDisposed.key);
        mDisposed.textures.push(sdata->texId);
    }

    delete sdata;
}

static GLuint _genTexture(RenderSurface* image)
{
    GLuint tex = 0;

    GL_CHECK(glGenTextures(1, &tex));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->w, image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->data));

    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    return tex;
}


RenderData GlRenderer::prepare(RenderSurface* image, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    if (flags == RenderUpdateFlag::None) return data;

    auto sdata = static_cast<GlShape*>(data);

    if (!sdata) sdata = new GlShape;

    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = RenderUpdateFlag::Image;

    if (sdata->texId == 0) {
        sdata->texId = _genTexture(image);
        sdata->opacity = opacity;
        sdata->texColorSpace = image->cs;
        sdata->texFlipY = 1;
        sdata->geometry = GlGeometry();
    }

    sdata->geometry.updateTransform(transform);
    sdata->geometry.setViewport(mViewport);

    sdata->geometry.tesselate(image, flags);

    if (!clips.empty()) {
        sdata->clips.clear();
        sdata->clips.push(clips);
    }

    return sdata;
}


RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    // If prepare for clip, only path is meaningful.
    if (clipper) flags = RenderUpdateFlag::Path;

    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = new GlShape;
        sdata->rshape = &rshape;
    }

    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = RenderUpdateFlag::None;

    sdata->geometry = GlGeometry();
    sdata->opacity = opacity;

    //invisible?
    auto alphaF = rshape.color.a;
    auto alphaS = rshape.stroke ? rshape.stroke->color.a : 0;

    if ( ((flags & RenderUpdateFlag::Gradient) == 0) &&
         ((flags & RenderUpdateFlag::Color) && alphaF == 0) &&
         ((flags & RenderUpdateFlag::Stroke) && alphaS == 0) )
    {
        return sdata;
    }

    if (clipper) {
        sdata->updateFlag = RenderUpdateFlag::Path;
    } else {
        if (alphaF) sdata->updateFlag = static_cast<RenderUpdateFlag>(RenderUpdateFlag::Color | sdata->updateFlag);
        if (rshape.fill) sdata->updateFlag = static_cast<RenderUpdateFlag>(RenderUpdateFlag::Gradient | sdata->updateFlag);
        if (alphaS) sdata->updateFlag = static_cast<RenderUpdateFlag>(RenderUpdateFlag::Stroke | sdata->updateFlag);
        if (rshape.strokeFill()) sdata->updateFlag = static_cast<RenderUpdateFlag>(RenderUpdateFlag::GradientStroke | sdata->updateFlag);
    }

    if (sdata->updateFlag == RenderUpdateFlag::None) return sdata;

    sdata->geometry.updateTransform(transform);
    sdata->geometry.setViewport(mViewport);

    if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke | RenderUpdateFlag::Gradient | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform | RenderUpdateFlag::Path))
    {
        if (!sdata->geometry.tesselate(rshape, sdata->updateFlag)) return sdata;
    }

    if (!clipper && !clips.empty()) {
        sdata->clips.clear();
        sdata->clips.push(clips);
    }

    return sdata;
}


RenderRegion GlRenderer::viewport()
{
    return mViewport;
}


bool GlRenderer::viewport(const RenderRegion& vp)
{
    mViewport = vp;
    return true;
}


bool GlRenderer::preUpdate()
{
    currentContext();
    return true;
}


bool GlRenderer::postUpdate()
{
    return true;
}


int GlRenderer::init(uint32_t threads)
{
    if ((initEngineCnt++) > 0) return true;

    //TODO: runtime linking?

    return true;
}


int32_t GlRenderer::init()
{
    return initEngineCnt;
}


int GlRenderer::term()
{
    if ((--initEngineCnt) > 0) return true;

    initEngineCnt = 0;

   _termEngine();

    return true;
}


GlRenderer* GlRenderer::gen()
{
    //TODO: GL minimum version check, should be replaced with the runtime linking in GlRenderer::init()
    GLint vMajor, vMinor;
    glGetIntegerv(GL_MAJOR_VERSION, &vMajor);
    glGetIntegerv(GL_MINOR_VERSION, &vMinor);
    if (vMajor < TVG_REQUIRE_GL_MAJOR_VER || (vMajor ==  TVG_REQUIRE_GL_MAJOR_VER && vMinor <  TVG_REQUIRE_GL_MINOR_VER)) {
        TVGERR("GL_ENGINE", "OpenGL/ES version is not satisfied. Current: v%d.%d, Required: v%d.%d", vMajor, vMinor, TVG_REQUIRE_GL_MAJOR_VER, TVG_REQUIRE_GL_MINOR_VER);
        return nullptr;
    }
    TVGLOG("GL_ENGINE", "OpenGL/ES version = v%d.%d", vMajor, vMinor);

    return new GlRenderer();
}
