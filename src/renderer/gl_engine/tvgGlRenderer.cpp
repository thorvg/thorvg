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

#include <atomic>
#include "tvgFill.h"
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

static atomic<int32_t> rendererCnt{-1};

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

    mRootTarget.reset();

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


GlRenderer::GlRenderer() : mEffect(GlEffect(&mGpuBuffer))
{
    ++rendererCnt;
}


GlRenderer::~GlRenderer()
{
    --rendererCnt;

    flush();

    ARRAY_FOREACH(p, mPrograms) delete(*p);
}


void GlRenderer::initShaders()
{
    mPrograms.reserve((int)RT_None);

#if 1  //for optimization
    #define LINEAR_TOTAL_LENGTH 2770
    #define RADIAL_TOTAL_LENGTH 5272
    #define BLEND_TOTAL_LENGTH 8192
#else
    #define COMMON_TOTAL_LENGTH strlen(STR_GRADIENT_FRAG_COMMON_VARIABLES) + strlen(STR_GRADIENT_FRAG_COMMON_FUNCTIONS) + 1
    #define LINEAR_TOTAL_LENGTH strlen(STR_LINEAR_GRADIENT_VARIABLES) + strlen(STR_LINEAR_GRADIENT_MAIN) + COMMON_TOTAL_LENGTH
    #define RADIAL_TOTAL_LENGTH strlen(STR_RADIAL_GRADIENT_VARIABLES) + strlen(STR_RADIAL_GRADIENT_MAIN) + COMMON_TOTAL_LENGTH
    #define BLEND_TOTAL_LENGTH strlen(BLEND_SOLID_FRAG_HEADER) + strlen(COLOR_BURN_BLEND_FRAG) + COMMON_TOTAL_LENGTH
#endif

    char linearGradientFragShader[LINEAR_TOTAL_LENGTH];
    snprintf(linearGradientFragShader, LINEAR_TOTAL_LENGTH, "%s%s%s%s",
        STR_GRADIENT_FRAG_COMMON_VARIABLES,
        STR_LINEAR_GRADIENT_VARIABLES,
        STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
        STR_LINEAR_GRADIENT_MAIN
    );

    char radialGradientFragShader[RADIAL_TOTAL_LENGTH];
    snprintf(radialGradientFragShader, RADIAL_TOTAL_LENGTH, "%s%s%s%s",
        STR_GRADIENT_FRAG_COMMON_VARIABLES,
        STR_RADIAL_GRADIENT_VARIABLES,
        STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
        STR_RADIAL_GRADIENT_MAIN
    );

    mPrograms.push(new GlProgram(COLOR_VERT_SHADER, COLOR_FRAG_SHADER));
    mPrograms.push(new GlProgram(GRADIENT_VERT_SHADER, linearGradientFragShader));
    mPrograms.push(new GlProgram(GRADIENT_VERT_SHADER, radialGradientFragShader));
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

    for (uint32_t i = 0; i < 17; i++) {
        mPrograms.push(nullptr); // slot for blend
        mPrograms.push(nullptr); // slot for gradient blend
        mPrograms.push(nullptr); // slot for image blend
        mPrograms.push(nullptr); // slot for scene blend
    }
}


void GlRenderer::drawPrimitive(GlShape& sdata, const RenderColor& c, RenderUpdateFlag flag, int32_t depth)
{
    auto vp = currentPass()->getViewport();
    auto bbox = sdata.geometry.viewport;

    bbox.intersect(vp);

    auto complexBlend = beginComplexBlending(bbox, sdata.geometry.getBounds());

    if (complexBlend) {
        vp = currentPass()->getViewport();
        bbox.intersect(vp);
    }

    auto x = bbox.sx() - vp.sx();
    auto y = bbox.sy() - vp.sy();
    auto w = bbox.sw();
    auto h = bbox.sh();

    GlRenderTask* task = nullptr;
    if (mBlendMethod != BlendMethod::Normal && !complexBlend) task = new GlSimpleBlendTask(mBlendMethod, mPrograms[RT_Color]);
    else task = new GlRenderTask(mPrograms[RT_Color]);

    task->setDrawDepth(depth);

    if (!sdata.geometry.draw(task, &mGpuBuffer, flag)) {
        delete task;
        return;
    }

    y = vp.sh() - y - h;
    task->setViewport({{x, y}, {x + w, y + h}});

    GlRenderTask* stencilTask = nullptr;

    GlStencilMode stencilMode = sdata.geometry.getStencilMode(flag);
    if (stencilMode != GlStencilMode::None) {
        stencilTask = new GlRenderTask(mPrograms[RT_Stencil], task);
        stencilTask->setDrawDepth(depth);
    }

    auto a = MULTIPLY(c.a, sdata.opacity);

    if (flag & RenderUpdateFlag::Stroke) {
        float strokeWidth = sdata.rshape->strokeWidth() * scaling(sdata.geometry.matrix);
        if (strokeWidth < MIN_GL_STROKE_WIDTH) {
            float alpha = strokeWidth / MIN_GL_STROKE_WIDTH;
            a = MULTIPLY(a, static_cast<uint8_t>(alpha * 255));
        }
    }

    // matrix buffer
    float matrix44[16];
    currentPass()->getMatrix(matrix44, sdata.geometry.matrix);
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

    if (complexBlend) {
        auto task = new GlRenderTask(mPrograms[RT_Stencil]);
        sdata.geometry.draw(task, &mGpuBuffer, flag);
        endBlendingCompose(task, sdata.geometry.matrix, false, false);
    }
}


void GlRenderer::drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag, int32_t depth)
{
    auto vp = currentPass()->getViewport();
    auto bbox = sdata.geometry.viewport;
    bbox.intersect(vp);

    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = min(fill->colorStops(&stops), static_cast<uint32_t>(MAX_GRADIENT_STOPS));
    if (stopCnt < 2) return;

    GlRenderTask* task = nullptr;

    if (fill->type() == Type::LinearGradient) task = new GlRenderTask(mPrograms[RT_LinGradient]);
    else if (fill->type() == Type::RadialGradient) task = new GlRenderTask(mPrograms[RT_RadGradient]);
    else return;

    task->setDrawDepth(depth);

    if (!sdata.geometry.draw(task, &mGpuBuffer, flag)) {
        delete task;
        return;
    }

    auto complexBlend = beginComplexBlending(bbox, sdata.geometry.getBounds());
    if (complexBlend) vp = currentPass()->getViewport();

    auto x = bbox.sx() - vp.sx();
    auto y = vp.sh() - (bbox.sy() - vp.sy()) - bbox.sh();
    task->setViewport({{x, y}, {x + bbox.sw(), y + bbox.sh()}});

    GlRenderTask* stencilTask = nullptr;
    GlStencilMode stencilMode = sdata.geometry.getStencilMode(flag);
    if (stencilMode != GlStencilMode::None) {
        stencilTask = new GlRenderTask(mPrograms[RT_Stencil], task);
        stencilTask->setDrawDepth(depth);
    }

    // matrix buffer
    float invMat4[16];
    Matrix inv;
    inverse(&fill->transform(), &inv);
    GET_MATRIX44(inv, invMat4);

    float matrix44[16];
    currentPass()->getMatrix(matrix44, sdata.geometry.matrix);

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
        CONST_RADIAL(radialFill)->correct(fx, fy, fr);

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

    if (complexBlend) {
        auto task = new GlRenderTask(mPrograms[RT_Stencil]);
        sdata.geometry.draw(task, &mGpuBuffer, flag);
        endBlendingCompose(task, sdata.geometry.matrix, true, false);
    }
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

        auto flag = (sdata->geometry.stroke.vertex.count > 0) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Path;
        sdata->geometry.draw(clipTask, &mGpuBuffer, flag);

        auto bbox = sdata->geometry.viewport;
        bbox.intersect(vp);

        auto x = bbox.sx() - vp.sx();
        auto y = vp.sh() - (bbox.sy() - vp.sy()) - bbox.sh();
        clipTask->setViewport({{x, y}, {x + bbox.sw(), y + bbox.sh()}});

        float matrix44[16];
        currentPass()->getMatrix(matrix44, sdata->geometry.matrix);

        auto loc = clipTask->getProgram()->getUniformBlockIndex("Matrix");
        auto viewOffset = mGpuBuffer.push(matrix44, 16 * sizeof(float), true);

        clipTask->addBindResource(GlBindingResource{0, loc, mGpuBuffer.getBufferId(), viewOffset, 16 * sizeof(float), });

        auto maskTask = new GlRenderTask(mPrograms[RT_Stencil]);

        maskTask->setDrawDepth(clipDepths[i]);
        maskTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), identityVertexOffset});
        maskTask->addBindResource(GlBindingResource{0, loc, mGpuBuffer.getBufferId(), mat4Offset, 16 * sizeof(float), });
        maskTask->setDrawRange(identityIndexOffset, 6);
        maskTask->setViewport({{0, 0}, {vp.sw(), vp.sh()}});

        currentPass()->addRenderTask(new GlClipTask(clipTask, maskTask));
    }
}

GlRenderPass* GlRenderer::currentPass()
{
    if (mRenderPassStack.empty()) return nullptr;
    return mRenderPassStack.last();
}

bool GlRenderer::beginComplexBlending(const RenderRegion& vp, RenderRegion bounds)
{
    if (vp.invalid()) return false;

    bounds.intersect(vp);
    if (bounds.invalid()) return false;

    if (mBlendMethod == BlendMethod::Normal) return false;

    if (mBlendPool.empty()) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));

    auto blendFbo = mBlendPool[0]->getRenderTarget(bounds);

    mRenderPassStack.push(new GlRenderPass(blendFbo));

    return true;
}

void GlRenderer::endBlendingCompose(GlRenderTask* stencilTask, const Matrix& matrix, bool gradient, bool image)
{
    auto blendPass = mRenderPassStack.last();
    mRenderPassStack.pop();
    
    blendPass->setDrawDepth(currentPass()->nextDrawDepth());

    auto composeTask = blendPass->endRenderPass<GlComposeTask>(nullptr, currentPass()->getFboId());

    const auto& vp = blendPass->getViewport();
    if (mBlendPool.count < 2) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));
    auto dstCopyFbo = mBlendPool[1]->getRenderTarget(vp);

    auto x = vp.sx();
    auto y = currentPass()->getViewport().sh() - vp.sy() - vp.sh();
    stencilTask->setViewport({{x, y}, {x + vp.sw(), y + vp.sh()}});

    stencilTask->setDrawDepth(currentPass()->nextDrawDepth());

    // set view matrix
    float matrix44[16];
    currentPass()->getMatrix(matrix44, matrix);
    uint32_t viewOffset = mGpuBuffer.push(matrix44, 16 * sizeof(float), true);
    stencilTask->addBindResource(GlBindingResource{
        0,
        stencilTask->getProgram()->getUniformBlockIndex("Matrix"),
        mGpuBuffer.getBufferId(),
        viewOffset,
        16 * sizeof(float),
    });
    
    auto program = getBlendProgram(mBlendMethod, gradient, image, false);
    auto task = new GlComplexBlendTask(program, currentPass()->getFbo(), dstCopyFbo, stencilTask, composeTask);
    prepareCmpTask(task, vp, blendPass->getFboWidth(), blendPass->getFboHeight());
    task->setDrawDepth(currentPass()->nextDrawDepth());

    // src and dst texture
    task->addBindResource(GlBindingResource{1, blendPass->getFbo()->getColorTexture(), task->getProgram()->getUniformLocation("uSrcTexture")});
    task->addBindResource(GlBindingResource{2, dstCopyFbo->getColorTexture(), task->getProgram()->getUniformLocation("uDstTexture")});

    currentPass()->addRenderTask(task);

    delete(blendPass);
}

GlProgram* GlRenderer::getBlendProgram(BlendMethod method, bool gradient, bool image, bool scene) {
    // custom blend shaders
    static const char* shaderFunc[17] {
        NORMAL_BLEND_FRAG,
        MULTIPLY_BLEND_FRAG,
        SCREEN_BLEND_FRAG,
        OVERLAY_BLEND_FRAG,
        DARKEN_BLEND_FRAG,
        LIGHTEN_BLEND_FRAG,
        COLOR_DODGE_BLEND_FRAG,
        COLOR_BURN_BLEND_FRAG,
        HARD_LIGHT_BLEND_FRAG,
        SOFT_LIGHT_BLEND_FRAG,
        DIFFERENCE_BLEND_FRAG,
        EXCLUSION_BLEND_FRAG,
        HUE_BLEND_FRAG,
        SATURATION_BLEND_FRAG,
        COLOR_BLEND_FRAG,
        LUMINOSITY_BLEND_FRAG,
        ADD_BLEND_FRAG
    };
    
    uint32_t methodInd = (uint32_t)method;
    uint32_t startInd = (uint32_t)RenderTypes::RT_Blend_Normal;
    uint32_t shaderInd = methodInd + startInd;

    const char* helpers = "";
    if ((method == BlendMethod::Hue) ||
        (method == BlendMethod::Saturation) ||
        (method == BlendMethod::Color) ||
        (method == BlendMethod::Luminosity))
        helpers = BLEND_FRAG_HSL;

    const char* vertShader = BLIT_VERT_SHADER;
    char fragShader[BLEND_TOTAL_LENGTH];
    if (gradient) {
        startInd = (uint32_t)RenderTypes::RT_Blend_Gradient_Normal;
        shaderInd = methodInd + startInd;
        strcat(strcat(strcpy(fragShader, BLEND_GRADIENT_FRAG_HEADER), helpers), shaderFunc[methodInd]);
    } else if (image) {
        startInd = (uint32_t)RenderTypes::RT_Blend_Image_Normal;
        shaderInd = methodInd + startInd;
        strcat(strcat(strcpy(fragShader, BLEND_IMAGE_FRAG_HEADER), helpers), shaderFunc[methodInd]);
    } else if (scene) {
        vertShader = BLEND_SCENE_VERT_SHADER;
        startInd = (uint32_t)RenderTypes::RT_Blend_Scene_Normal;
        shaderInd = methodInd + startInd;
        strcat(strcat(strcpy(fragShader, BLEND_SCENE_FRAG_HEADER), helpers), shaderFunc[methodInd]);
    } else {
        strcat(strcat(strcpy(fragShader, BLEND_SOLID_FRAG_HEADER), helpers), shaderFunc[methodInd]);
    }

    if (!mPrograms[shaderInd])
        mPrograms[shaderInd] = new GlProgram(vertShader, fragShader);
    return mPrograms[shaderInd];
}


void GlRenderer::prepareBlitTask(GlBlitTask* task)
{
    prepareCmpTask(task, {{0, 0}, {int32_t(surface.w), int32_t(surface.h)}}, surface.w, surface.h);
    task->addBindResource(GlBindingResource{0, task->getColorTexture(), task->getProgram()->getUniformLocation("uSrcTexture")});
}


void GlRenderer::prepareCmpTask(GlRenderTask* task, const RenderRegion& vp, uint32_t cmpWidth, uint32_t cmpHeight)
{
    const auto& passVp = currentPass()->getViewport();
    
    auto taskVp = vp;
    taskVp.intersect(passVp);

    auto x = taskVp.sx() - passVp.sx();
    auto y = taskVp.sy() - passVp.sy();
    auto w = taskVp.sw();
    auto h = taskVp.sh();

    float rw = static_cast<float>(passVp.w());
    float rh = static_cast<float>(passVp.h());

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

    float vertices[4*4] {
        left, top,     0.f, uh,  // left top point
        left, bottom,  0.f, 0.f, // left bottom point
        right, top,    uw, uh,   // right top point
        right, bottom, uw, 0.f   // right bottom point
    };
    uint32_t indices[6]{0, 1, 2, 2, 1, 3};
    uint32_t vertexOffset = mGpuBuffer.push(vertices, sizeof(vertices));
    uint32_t indexOffset = mGpuBuffer.pushIndex(indices, sizeof(indices));

    task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset});
    task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float)});
    task->setDrawRange(indexOffset, 6);
    y = (passVp.sh() - y - h);
    task->setViewport({{x, y}, {x + w, y + h}});
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
            prev_task->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
            prev_task->setViewport(glCmp->bbox);

            auto compose_task = selfPass->endRenderPass<GlDrawBlitTask>(program, currentPass()->getFboId());
            compose_task->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
            compose_task->setPrevTask(prev_task);

            prepareCmpTask(compose_task, glCmp->bbox, selfPass->getFboWidth(), selfPass->getFboHeight());

            compose_task->addBindResource(GlBindingResource{0, selfPass->getTextureId(), program->getUniformLocation("uSrcTexture")});
            compose_task->addBindResource(GlBindingResource{1, maskPass->getTextureId(), program->getUniformLocation("uMaskTexture")});

            compose_task->setDrawDepth(currentPass()->nextDrawDepth());
            compose_task->setParentSize(currentPass()->getViewport().w(), currentPass()->getViewport().h());
            currentPass()->addRenderTask(compose_task);
        }

        delete(selfPass);
        delete(maskPass);
    } else
    if (glCmp->blendMethod != BlendMethod::Normal) {
        auto renderPass = mRenderPassStack.last();
        mRenderPassStack.pop();

        if (!renderPass->isEmpty()) {
            const auto& vp = currentPass()->getViewport();
            if (mBlendPool.count < 1) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));
            if (mBlendPool.count < 2) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));
            auto dstCopyFbo = mBlendPool[1]->getRenderTarget(vp);

            // image info
            uint32_t info[4] = {(uint32_t)ColorSpace::ABGR8888, 0, cmp->opacity, 0};

            auto program = getBlendProgram(glCmp->blendMethod, false, false, true);
            auto task = renderPass->endRenderPass<GlSceneBlendTask>(program, currentPass()->getFboId());
            task->setDstCopy(dstCopyFbo);
            task->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
            prepareCmpTask(task, glCmp->bbox, renderPass->getFboWidth(), renderPass->getFboHeight());
            task->setDrawDepth(currentPass()->nextDrawDepth());
            // info
            task->addBindResource(GlBindingResource{0, task->getProgram()->getUniformBlockIndex("ColorInfo"), mGpuBuffer.getBufferId(), mGpuBuffer.push(info, sizeof(info), true), sizeof(info)});
            // textures
            task->addBindResource(GlBindingResource{0, renderPass->getTextureId(), task->getProgram()->getUniformLocation("uSrcTexture")});
            task->addBindResource(GlBindingResource{1, dstCopyFbo->getColorTexture(), task->getProgram()->getUniformLocation("uDstTexture")});
            task->setParentSize(currentPass()->getViewport().w(), currentPass()->getViewport().h());
            currentPass()->addRenderTask(std::move(task));
        }
        delete(renderPass);
    } else
    {
        auto renderPass = mRenderPassStack.last();
        mRenderPassStack.pop();

        if (!renderPass->isEmpty()) {
            auto task = renderPass->endRenderPass<GlDrawBlitTask>(mPrograms[RT_Image], currentPass()->getFboId());
            task->setRenderSize(glCmp->bbox.w(), glCmp->bbox.h());
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

            // texture id
            task->addBindResource(GlBindingResource{0, renderPass->getTextureId(), task->getProgram()->getUniformLocation("uTexture")});
            task->setParentSize(currentPass()->getViewport().w(), currentPass()->getViewport().h());
            currentPass()->addRenderTask(std::move(task));
        }
        delete(renderPass);
    }
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool GlRenderer::clear()
{
    if (mRootTarget.invalid()) return false;

    mClearBuffer = true;
    return true;
}


bool GlRenderer::target(void* context, int32_t id, uint32_t w, uint32_t h)
{
    //assume the context zero is invalid
    if (!context || w == 0 || h == 0) return false;

    if (mContext) currentContext();

    flush();

    surface.stride = w;
    surface.w = w;
    surface.h = h;

    mContext = context;
    mTargetFboId = static_cast<GLint>(id);

    currentContext();

    mRootTarget.setViewport({{0, 0}, {int32_t(surface.w), int32_t(surface.h)}});
    mRootTarget.init(surface.w, surface.h, mTargetFboId);

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
    task->setTargetViewport({{0, 0}, {int32_t(surface.w), int32_t(surface.h)}});

    if (mGpuBuffer.flushToGPU()) {
        mGpuBuffer.bind();
        task->run();
    }

    mGpuBuffer.unbind();

    GL_CHECK(glDisable(GL_SCISSOR_TEST));

    clearDisposes();

    // Reset clear buffer flag to default (false) after use.    
    mClearBuffer = false; 

    delete task;

    return true;
}


bool GlRenderer::bounds(RenderData data, TVG_UNUSED Point* pt4, TVG_UNUSED const Matrix& m)
{
    if (data) {
        //TODO: stroking bounding box is required
        TVGLOG("GL_ENGINE", "bounds() is not supported!");
    }
    return false;
}


RenderRegion GlRenderer::region(RenderData data)
{
    auto pass = currentPass();
    if (!data || !pass || pass->isEmpty()) return {};

    auto shape = reinterpret_cast<GlShape*>(data);
    auto bounds = shape->geometry.getBounds();

    auto const& vp = pass->getViewport();
    bounds.intersect(vp);

    return bounds;
}


bool GlRenderer::preRender()
{
    if (mRootTarget.invalid()) return false;

    currentContext();
    if (mPrograms.empty()) initShaders();
    mRenderPassStack.push(new GlRenderPass(&mRootTarget));

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

    auto glCmp = static_cast<GlCompositor*>(cmp);
    glCmp->method = method;
    glCmp->opacity = opacity;
    glCmp->blendMethod = mBlendMethod;

    uint32_t index = mRenderPassStack.count - 1;
    if (index >= mComposePool.count) mComposePool.push( new GlRenderTargetPool(surface.w, surface.h));
    
    if (glCmp->bbox.valid()) mRenderPassStack.push(new GlRenderPass(mComposePool[index]->getRenderTarget(glCmp->bbox)));
    else mRenderPassStack.push(new GlRenderPass(nullptr));

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


void GlRenderer::prepare(RenderEffect* effect, const Matrix& transform)
{
    // we must be sure, that we have intermidiate FBOs
    if (mBlendPool.count < 1) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));
    if (mBlendPool.count < 2) mBlendPool.push(new GlRenderTargetPool(surface.w, surface.h));

    mEffect.update(effect, transform);
}


bool GlRenderer::region(RenderEffect* effect)
{
    return mEffect.region(effect);
}


bool GlRenderer::render(TVG_UNUSED RenderCompositor* cmp, const RenderEffect* effect, TVG_UNUSED bool direct)
{
    return mEffect.render(const_cast<RenderEffect*>(effect), currentPass(), mBlendPool);
}


void GlRenderer::dispose(RenderEffect* effect)
{
    tvg::free(effect->rd);
    effect->rd = nullptr;
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

    mBlendMethod = (method == BlendMethod::Composition ? BlendMethod::Normal : method);

    return true;
}


bool GlRenderer::renderImage(void* data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    if (currentPass()->isEmpty() || !(sdata->updateFlag & RenderUpdateFlag::Image)) return true;

    auto vp = currentPass()->getViewport();
    auto bbox = sdata->geometry.viewport;
    bbox.intersect(vp);
    if (bbox.invalid()) return true;

    auto x = bbox.sx() - vp.sx();
    auto y = bbox.sy() - vp.sy();
    auto drawDepth = currentPass()->nextDrawDepth();

    if (!sdata->clips.empty()) drawClip(sdata->clips);

    auto task = new GlRenderTask(mPrograms[RT_Image]);
    task->setDrawDepth(drawDepth);

    if (!sdata->geometry.draw(task, &mGpuBuffer, RenderUpdateFlag::Image)) {
        delete task;
        return true;
    }

    bool complexBlend = beginComplexBlending(bbox, sdata->geometry.getBounds());
    if (complexBlend) vp = currentPass()->getViewport();

    // matrix buffer
    float matrix44[16];
    currentPass()->getMatrix(matrix44, sdata->geometry.matrix);

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

    y = vp.sh() - y - bbox.sh();
    auto x2 = x + bbox.sw();
    auto y2 = y + bbox.sh();

    task->setViewport({{x, y}, {x2, y2}});

    currentPass()->addRenderTask(task);

    if (complexBlend) {
        auto task = new GlRenderTask(mPrograms[RT_Stencil]);
        sdata->geometry.draw(task, &mGpuBuffer, RenderUpdateFlag::Image);
        endBlendingCompose(task, sdata->geometry.matrix, false, true);
    }

    return true;
}


bool GlRenderer::renderShape(RenderData data)
{
    if (currentPass()->isEmpty()) return true;

    auto sdata = static_cast<GlShape*>(data);
    if (sdata->updateFlag == RenderUpdateFlag::None) return true;

    auto bbox = sdata->geometry.viewport;
    bbox.intersect(currentPass()->getViewport());
    if (bbox.invalid()) return true;

    int32_t drawDepth1 = 0, drawDepth2 = 0;

    if (sdata->updateFlag == RenderUpdateFlag::None) return false;
    if (sdata->updateFlag & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Color)) drawDepth1 = currentPass()->nextDrawDepth();
    if (sdata->updateFlag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke)) drawDepth2 = currentPass()->nextDrawDepth();

    if (!sdata->clips.empty()) drawClip(sdata->clips);

    auto processFill = [&]() {
        if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient)) {
            if (const auto& gradient = sdata->rshape->fill) {
                drawPrimitive(*sdata, gradient, RenderUpdateFlag::Gradient, drawDepth1);
            } else if (sdata->rshape->color.a > 0) {
                drawPrimitive(*sdata, sdata->rshape->color, RenderUpdateFlag::Color, drawDepth1);
            }
        }
    };

    auto processStroke = [&]() {
        if (!sdata->rshape->stroke) return;
        if (sdata->updateFlag & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke)) {
            if (const auto& gradient = sdata->rshape->strokeFill()) {
                drawPrimitive(*sdata, gradient, RenderUpdateFlag::GradientStroke, drawDepth2);
            } else if (sdata->rshape->stroke->color.a > 0) {
                drawPrimitive(*sdata, sdata->rshape->stroke->color, RenderUpdateFlag::Stroke, drawDepth2);
            }
        }
    };

    if (sdata->rshape->strokeFirst()) {
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

    sdata->geometry.matrix = transform;
    sdata->geometry.viewport = vport;

    sdata->geometry.tesselate(image, flags);

    if (flags & RenderUpdateFlag::Clip) {
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

    if ((flags & RenderUpdateFlag::Gradient) == 0 && ((flags & RenderUpdateFlag::Color) && alphaF == 0) && ((flags & RenderUpdateFlag::Stroke) && alphaS == 0)) {
        return sdata;
    }

    if (clipper) {
        sdata->updateFlag = (rshape.stroke && (rshape.stroke->width > 0)) ? RenderUpdateFlag::Stroke : RenderUpdateFlag::Path;
    } else {
        if (alphaF) sdata->updateFlag = (RenderUpdateFlag::Color | sdata->updateFlag);
        if (rshape.fill) sdata->updateFlag = (RenderUpdateFlag::Gradient | sdata->updateFlag);
        if (alphaS) sdata->updateFlag = (RenderUpdateFlag::Stroke | sdata->updateFlag);
        if (rshape.strokeFill()) sdata->updateFlag = (RenderUpdateFlag::GradientStroke | sdata->updateFlag);
    }

    if (sdata->updateFlag == RenderUpdateFlag::None) return sdata;

    sdata->geometry.matrix = transform;
    sdata->geometry.viewport = vport;

    if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke | RenderUpdateFlag::Gradient | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform | RenderUpdateFlag::Path)) {
        if (!sdata->geometry.tesselate(rshape, sdata->updateFlag)) return sdata;
    }

    if (flags & RenderUpdateFlag::Clip) {
        sdata->clips.clear();
        sdata->clips.push(clips);
    }

    return sdata;
}


bool GlRenderer::preUpdate()
{
    if (mRootTarget.invalid()) return false;

    currentContext();
    return true;
}


bool GlRenderer::postUpdate()
{
    return true;
}


void GlRenderer::damage(TVG_UNUSED RenderData rd, TVG_UNUSED const RenderRegion& region)
{
    //TODO
}


bool GlRenderer::partial(bool disable)
{
    //TODO
    return false;
}


bool GlRenderer::intersectsShape(RenderData data, TVG_UNUSED const RenderRegion& region)
{
    if (!data) return false;
    TVGLOG("GL_ENGINE", "Paint::intersect() is not supported!");
    return false;
}


bool GlRenderer::intersectsImage(RenderData data, TVG_UNUSED const RenderRegion& region)
{
    if (!data) return false;
    TVGLOG("GL_ENGINE", "Paint::intersect() is not supported!");
    return false;
}


bool GlRenderer::term()
{
    if (rendererCnt > 0) return false;

    glTerm();

    rendererCnt = -1;

    return true;
}


GlRenderer* GlRenderer::gen(TVG_UNUSED uint32_t threads)
{
    //initialize engine
    if (rendererCnt == -1) {
        if (!glInit()) {
            TVGERR("GL_ENGINE", "Failed GL initialization!");
            return nullptr;
        }    
        rendererCnt = 0;
    }

    return new GlRenderer;
}