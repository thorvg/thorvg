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

#include "tvgMath.h"
#include "tvgGlRenderer.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlGeometry.h"
#include "tvgGlRenderTask.h"
#include "tvgGlProgram.h"
#include "tvgGlShaderSrc.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
static int32_t initEngineCnt = false;
static int32_t rendererCnt = 0;


static void _termEngine()
{
    if (rendererCnt > 0) return;

    //TODO: Clean up global resources
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

#define NOISE_LEVEL 0.5f

bool GlRenderer::clear()
{
    mClearBuffer = true;
    return true;
}


bool GlRenderer::target(int32_t id, uint32_t w, uint32_t h)
{
    if (id == GL_INVALID_VALUE || w == 0 || h == 0) return false;

    surface.stride = w;
    surface.w = w;
    surface.h = h;

    mTargetFboId = static_cast<GLint>(id);

    mRootTarget = make_unique<GlRenderTarget>(surface.w, surface.h);
    mRootTarget->setViewport(RenderRegion{0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)});
    mRootTarget->init(mTargetFboId);

    mRenderPassStack.clear();
    mComposeStack.clear();

    for (uint32_t i = 0; i < mComposePool.count; i++) delete mComposePool[i];

    mComposePool.clear();

    return true;
}


bool GlRenderer::sync()
{
    //nothing to be done.
    if (mRenderPassStack.size() == 0) return true;

    // Blend function for straight alpha
    GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glEnable(GL_SCISSOR_TEST));
    GL_CHECK(glCullFace(GL_FRONT_AND_BACK));
    GL_CHECK(glFrontFace(GL_CCW));
    GL_CHECK(glEnable(GL_DEPTH_TEST));
    GL_CHECK(glDepthFunc(GL_GREATER));

    auto task = mRenderPassStack.front().endRenderPass<GlBlitTask>(mPrograms[RT_Blit].get(), mTargetFboId);

    prepareBlitTask(task);

    task->mClearBuffer = mClearBuffer;
    task->setTargetViewport(RenderRegion{0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)});

    mGpuBuffer->flushToGPU();
    mGpuBuffer->bind();

    task->run();

    mGpuBuffer->unbind();

    GL_CHECK(glDisable(GL_SCISSOR_TEST));

    mRenderPassStack.clear();

    delete task;

    return true;
}


RenderRegion GlRenderer::region(RenderData data)
{
    if (currentPass()->isEmpty()) return RenderRegion{0, 0, 0, 0};

    auto shape = reinterpret_cast<GlShape*>(data);
    auto bounds = shape->geometry->getBounds();

    auto const& vp = currentPass()->getViewport();
    bounds.intersect(vp);

    return bounds;
}


bool GlRenderer::preRender()
{
    if (mPrograms.size() == 0)
    {
        initShaders();
    }

    mRenderPassStack.emplace_back(GlRenderPass(mRootTarget.get()));

    return true;
}


bool GlRenderer::postRender()
{
    return true;
}


Compositor* GlRenderer::target(const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    auto vp = region;
    if (currentPass()->isEmpty()) return nullptr;

    vp.intersect(currentPass()->getViewport());

    mComposeStack.emplace_back(make_unique<GlCompositor>(vp));
    return mComposeStack.back().get();
}


bool GlRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity)
{
    if (!cmp) return false;

    cmp->method = method;
    cmp->opacity = opacity;

    uint32_t index = mRenderPassStack.size() - 1;

    if (index >= mComposePool.count) {
        mComposePool.push( new GlRenderTargetPool(surface.w, surface.h));
    }

    auto glCmp = static_cast<GlCompositor*>(cmp);

    if (glCmp->bbox.w > 0 && glCmp->bbox.h > 0) {
        auto renderTarget = mComposePool[index]->getRenderTarget(glCmp->bbox);
        mRenderPassStack.emplace_back(GlRenderPass(renderTarget));
    } else {
        // empty render pass
        mRenderPassStack.emplace_back(GlRenderPass(nullptr));
    }

    return true;
}


bool GlRenderer::endComposite(Compositor* cmp)
{
    if (mComposeStack.empty()) return false;
    if (mComposeStack.back().get() != cmp) return false;

    // end current render pass;
    auto currCmp  = std::move(mComposeStack.back());
    mComposeStack.pop_back();

    assert(cmp == currCmp.get());

    endRenderPass(currCmp.get());

    return true;
}


ColorSpace GlRenderer::colorSpace()
{
    return ColorSpace::Unsupported;
}


const Surface* GlRenderer::mainSurface()
{
    return &surface;
}


bool GlRenderer::blend(TVG_UNUSED BlendMethod method)
{
    if (method != BlendMethod::Normal) {
        return true;
    }
    //TODO:
    return false;
}


bool GlRenderer::renderImage(void* data)
{
    auto sdata = static_cast<GlShape*>(data);

    if (!sdata) return false;

    if (currentPass()->isEmpty()) return true;

    if ((sdata->updateFlag & RenderUpdateFlag::Image) == 0) return true;
    const auto& vp = currentPass()->getViewport();

    auto bbox = sdata->geometry->getViewport();

    bbox.intersect(vp);

    if (bbox.w <= 0 || bbox.h <= 0) return true;

    auto x = bbox.x - vp.x;
    auto y = bbox.y - vp.y;


    int32_t drawDepth = currentPass()->nextDrawDepth();

    if (!sdata->clips.empty()) drawClip(sdata->clips);

    auto task = new GlRenderTask(mPrograms[RT_Image].get());
    task->setDrawDepth(drawDepth);

    if (!sdata->geometry->draw(task, mGpuBuffer.get(), RenderUpdateFlag::Image)) return true;

    // matrix buffer
    {
        const auto& matrix = sdata->geometry->getTransformMatrix();

        float matrix44[16];

        currentPass()->getMatrix(matrix44, matrix);

        uint32_t loc = task->getProgram()->getUniformBlockIndex("Matrix");

        task->addBindResource(GlBindingResource{
            0,
            loc,
            mGpuBuffer->getBufferId(),
            mGpuBuffer->push(matrix44, 16 * sizeof(float), true),
            16 * sizeof(float),
        });
    }
    // image info
    {
        uint32_t info[4] = {sdata->texColorSpace, sdata->texFlipY, sdata->opacity, 0};
        uint32_t loc = task->getProgram()->getUniformBlockIndex("ColorInfo");

        task->addBindResource(GlBindingResource{
            1,
            loc,
            mGpuBuffer->getBufferId(),
            mGpuBuffer->push(info, 4 * sizeof(uint32_t), true),
            4 * sizeof(uint32_t),
        });
    }
    // texture id
    {
        uint32_t loc = task->getProgram()->getUniformLocation("uTexture");
        task->addBindResource(GlBindingResource{0, sdata->texId, loc});
    }

    task->setViewport(RenderRegion{
        x,
        vp.h - y - bbox.h,
        bbox.w,
        bbox.h
    });

    currentPass()->addRenderTask(task);

    return true;
}


bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    if (currentPass()->isEmpty()) return true;

    if (sdata->updateFlag == RenderUpdateFlag::None) return true;

    const auto& vp = currentPass()->getViewport();

    auto bbox = sdata->geometry->getViewport();
    bbox.intersect(vp);

    if (bbox.w <= 0 || bbox.h <= 0) return true;

    uint8_t r = 0, g = 0, b = 0, a = 0;
    int32_t drawDepth1 = 0, drawDepth2 = 0, drawDepth3 = 0;

    size_t flags = static_cast<size_t>(sdata->updateFlag);

    if (flags == 0) return false;

    if ((flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) && sdata->rshape->fill) drawDepth1 = currentPass()->nextDrawDepth();
    if(flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Transform))
    {
        sdata->rshape->fillColor(&r, &g, &b, &a);
        if (a > 0) drawDepth2 = currentPass()->nextDrawDepth();
    }

    if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform))
    {
        sdata->rshape->strokeFill(&r, &g, &b, &a);
        if (sdata->rshape->strokeFill() || a > 0) drawDepth3 = currentPass()->nextDrawDepth();
    }

    if (!sdata->clips.empty()) drawClip(sdata->clips);

    if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform))
    {
        auto gradient = sdata->rshape->fill;
        if (gradient) drawPrimitive(*sdata, gradient, RenderUpdateFlag::Gradient, drawDepth1);
    }

    if(flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Transform))
    {
        sdata->rshape->fillColor(&r, &g, &b, &a);
        if (a > 0)
        {
            drawPrimitive(*sdata, r, g, b, a, RenderUpdateFlag::Color, drawDepth2);
        }
    }

    if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform))
    {
        auto gradient =  sdata->rshape->strokeFill();
        if (gradient) {
            drawPrimitive(*sdata, gradient, RenderUpdateFlag::GradientStroke, drawDepth3);
        } else {
            if (sdata->rshape->strokeFill(&r, &g, &b, &a) && a > 0)
            {
                drawPrimitive(*sdata, r, g, b, a, RenderUpdateFlag::Stroke, drawDepth3);
            }
        }
    }

    return true;
}


void GlRenderer::dispose(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return;

    if (sdata->texId) glDeleteTextures(1, &sdata->texId);

    delete sdata;
}

static GLuint _genTexture(Surface* image)
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

RenderData GlRenderer::prepare(Surface* image, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
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
        sdata->texFlipY = (mesh && mesh->triangleCnt) ? 0 : 1;
        sdata->geometry = make_unique<GlGeometry>();
    }

    sdata->geometry->updateTransform(transform);
    sdata->geometry->setViewport(mViewport);

    sdata->geometry->tesselate(image, mesh, flags);

    if (!clips.empty()) sdata->clips.push(clips);

    return sdata;
}


RenderData GlRenderer::prepare(TVG_UNUSED const Array<RenderData>& scene, TVG_UNUSED RenderData data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, TVG_UNUSED RenderUpdateFlag flags)
{
    //TODO:
    return nullptr;
}


RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
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

    sdata->geometry = make_unique<GlGeometry>();
    sdata->opacity = opacity;

    //invisible?
    uint8_t alphaF = 0, alphaS = 0;
    rshape.fillColor(nullptr, nullptr, nullptr, &alphaF);
    rshape.strokeFill(nullptr, nullptr, nullptr, &alphaS);

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

    sdata->geometry->updateTransform(transform);
    sdata->geometry->setViewport(mViewport);

    if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke | RenderUpdateFlag::Gradient | RenderUpdateFlag::GradientStroke | RenderUpdateFlag::Transform | RenderUpdateFlag::Path))
    {
        if (!sdata->geometry->tesselate(rshape, sdata->updateFlag)) return sdata;
    }

    if (!clipper && !clips.empty()) sdata->clips.push(clips);

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


GlRenderer::GlRenderer() :mGpuBuffer(new GlStageBuffer), mPrograms(), mComposePool()
{
}


GlRenderer::~GlRenderer()
{
    for (uint32_t i = 0; i < mComposePool.count; i++) {
        if (mComposePool[i]) delete mComposePool[i];
    }

    --rendererCnt;

    if (rendererCnt == 0 && initEngineCnt == 0) _termEngine();
}


void GlRenderer::initShaders()
{
    // Solid Color Renderer
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(COLOR_VERT_SHADER, COLOR_FRAG_SHADER)));

    // Linear Gradient Renderer
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(GRADIENT_VERT_SHADER, LINEAR_GRADIENT_FRAG_SHADER)));

    // Radial Gradient Renderer
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(GRADIENT_VERT_SHADER, RADIAL_GRADIENT_FRAG_SHADER)));

    // image Renderer
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(IMAGE_VERT_SHADER, IMAGE_FRAG_SHADER)));

    // compose Renderer
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_ALPHA_FRAG_SHADER)));
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_INV_ALPHA_FRAG_SHADER)));
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_LUMA_FRAG_SHADER)));
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_INV_LUMA_FRAG_SHADER)));
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_ADD_FRAG_SHADER)));
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_SUB_FRAG_SHADER)));
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_INTERSECT_FRAG_SHADER)));
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(MASK_VERT_SHADER, MASK_DIFF_FRAG_SHADER)));
    // stencil Renderer
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(STENCIL_VERT_SHADER, STENCIL_FRAG_SHADER)));
    // blit Renderer
    mPrograms.push_back(make_unique<GlProgram>(GlShader::gen(BLIT_VERT_SHADER, BLIT_FRAG_SHADER)));
}


void GlRenderer::drawPrimitive(GlShape& sdata, uint8_t r, uint8_t g, uint8_t b, uint8_t a, RenderUpdateFlag flag, int32_t depth)
{
    const auto& vp = currentPass()->getViewport();
    auto bbox = sdata.geometry->getViewport();

    bbox.intersect(vp);

    auto x = bbox.x - vp.x;
    auto y = bbox.y - vp.y;
    auto w = bbox.w;
    auto h = bbox.h;

    auto task = new GlRenderTask(mPrograms[RT_Color].get());
    task->setDrawDepth(depth);

    if (!sdata.geometry->draw(task, mGpuBuffer.get(), flag)) {
        delete task;
        return;
    }

    task->setViewport(RenderRegion {
        x,
        vp.h - y - h,
        w,
        h
    });

    GlRenderTask* stencilTask = nullptr;

    GlStencilMode stencilMode = sdata.geometry->getStencilMode(flag);
    if (stencilMode != GlStencilMode::None) {
        stencilTask = new GlRenderTask(mPrograms[RT_Stencil].get(), task);
        stencilTask->setDrawDepth(depth);
    }

    a = MULTIPLY(a, sdata.opacity);

    // matrix buffer
    {
        const auto& matrix = sdata.geometry->getTransformMatrix();

        float matrix44[16];

        currentPass()->getMatrix(matrix44, matrix);

        uint32_t loc = task->getProgram()->getUniformBlockIndex("Matrix");

        uint32_t viewOffset = mGpuBuffer->push(matrix44, 16 * sizeof(float), true);

        task->addBindResource(GlBindingResource{
            0,
            loc,
            mGpuBuffer->getBufferId(),
            viewOffset,
            16 * sizeof(float),
        });

        if (stencilTask) {
            stencilTask->addBindResource(GlBindingResource{
                0,
                static_cast<uint32_t>(stencilTask->getProgram()->getUniformBlockIndex("Matrix")),
                mGpuBuffer->getBufferId(),
                viewOffset,
                16 * sizeof(float),
            });
        }
    }
    // color
    {
        float color[4] = {r / 255.f, g / 255.f, b / 255.f, a / 255.f};

        uint32_t loc = task->getProgram()->getUniformBlockIndex("ColorInfo");

        task->addBindResource(GlBindingResource{
            1,
            loc,
            mGpuBuffer->getBufferId(),
            mGpuBuffer->push(color, 4 * sizeof(float), true),
            4 * sizeof(float),
        });
    }

    if (stencilTask) {
        currentPass()->addRenderTask(new GlStencilCoverTask(stencilTask, task, stencilMode));
    } else {
        currentPass()->addRenderTask(task);
    }
}


void GlRenderer::drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag, int32_t depth)
{

    const auto& vp = currentPass()->getViewport();
    auto bbox = sdata.geometry->getViewport();

    bbox.intersect(vp);

    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = min(fill->colorStops(&stops),
                       static_cast<uint32_t>(MAX_GRADIENT_STOPS));
    if (stopCnt < 2) return;

    GlRenderTask* task = nullptr;

    if (fill->identifier() == TVG_CLASS_ID_LINEAR) {
        task = new GlRenderTask(mPrograms[RT_LinGradient].get());
    } else if (fill->identifier() == TVG_CLASS_ID_RADIAL) {
        task = new GlRenderTask(mPrograms[RT_RadGradient].get());
    } else {
        return;
    }

    task->setDrawDepth(depth);

    if (!sdata.geometry->draw(task, mGpuBuffer.get(), flag)) {
        delete task;
        return;
    }

    auto x = bbox.x - vp.x;
    auto y = bbox.y - vp.y;

    task->setViewport(RenderRegion {
        x,
        vp.h - y - bbox.h,
        bbox.w,
        bbox.h
    });

    GlRenderTask* stencilTask = nullptr;
    GlStencilMode stencilMode = sdata.geometry->getStencilMode(flag);
    if (stencilMode != GlStencilMode::None) {
        stencilTask = new GlRenderTask(mPrograms[RT_Stencil].get(), task);
        stencilTask->setDrawDepth(depth);
    }

    // matrix buffer
    {
        const auto& matrix = sdata.geometry->getTransformMatrix();

        auto gradientTransform = fill->transform();
        float invMat4[16];
        if (!mathIdentity(const_cast<const Matrix*>(&gradientTransform))) {
            Matrix inv{};
            mathInverse(&gradientTransform  , &inv);

            GET_MATRIX44(inv, invMat4);
        } else {
            memset(invMat4, 0, 16 * sizeof(float));
            invMat4[0] = 1.f;
            invMat4[5] = 1.f;
            invMat4[10] = 1.f;
            invMat4[15] = 1.f;
        }

        float matrix44[16];

        currentPass()->getMatrix(matrix44, matrix);

        uint32_t loc = task->getProgram()->getUniformBlockIndex("Matrix");

        uint32_t viewOffset = mGpuBuffer->push(matrix44, 16 * sizeof(float), true);

        task->addBindResource(GlBindingResource{
            0,
            loc,
            mGpuBuffer->getBufferId(),
            viewOffset,
            16 * sizeof(float),
        });

        if (stencilTask) {
            stencilTask->addBindResource(GlBindingResource{
                0,
                static_cast<uint32_t>(stencilTask->getProgram()->getUniformBlockIndex("Matrix")),
                mGpuBuffer->getBufferId(),
                viewOffset,
                16 * sizeof(float),
            });
        }

        loc = task->getProgram()->getUniformBlockIndex("InvMatrix");
        viewOffset = mGpuBuffer->push(invMat4, 16 * sizeof(float), true);

        task->addBindResource(GlBindingResource{
            1,
            loc,
            mGpuBuffer->getBufferId(),
            viewOffset,
            16 * sizeof(float),
        });
    }

    // gradient block
    {
        GlBindingResource gradientBinding{};
        uint32_t loc = task->getProgram()->getUniformBlockIndex("GradientInfo");

        if (fill->identifier() == TVG_CLASS_ID_LINEAR) {
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
                gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f;
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
                mGpuBuffer->getBufferId(),
                mGpuBuffer->push(&gradientBlock, sizeof(GlLinearGradientBlock), true),
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
                gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f;
                nStops++;
            }
            gradientBlock.nStops[0] = nStops * 1.f;

            float x, y, r;
            radialFill->radial(&x, &y, &r);

            gradientBlock.centerPos[0] = x;
            gradientBlock.centerPos[1] = y;
            gradientBlock.radius[0] = r;

            gradientBinding = GlBindingResource{
                2,
                loc,
                mGpuBuffer->getBufferId(),
                mGpuBuffer->push(&gradientBlock, sizeof(GlRadialGradientBlock), true),
                sizeof(GlRadialGradientBlock),
            };
        }

        task->addBindResource(gradientBinding);
    }

    if (stencilTask) {
        currentPass()->addRenderTask(new GlStencilCoverTask(stencilTask, task, stencilMode));
    } else {
        currentPass()->addRenderTask(task);
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

    auto identityVertexOffset = mGpuBuffer->push(identityVertex.data, 8 * sizeof(float));
    auto identityIndexOffset = mGpuBuffer->push(identityIndex.data, 6 * sizeof(uint32_t));
    auto mat4Offset = mGpuBuffer->push(mat4, 16 * sizeof(float), true);

    Array<int32_t> clipDepths(clips.count);
    clipDepths.count = clips.count;

    for (int32_t i = clips.count - 1; i >= 0; i--) {
        clipDepths[i] = currentPass()->nextDrawDepth();
    }

    const auto& vp = currentPass()->getViewport();

    for (uint32_t i = 0; i < clips.count; ++i) {
        auto sdata = static_cast<GlShape*>(clips[i]);

        auto clipTask = new GlRenderTask(mPrograms[RT_Stencil].get());

        clipTask->setDrawDepth(clipDepths[i]);

        sdata->geometry->draw(clipTask, mGpuBuffer.get(), RenderUpdateFlag::Path);

        auto bbox = sdata->geometry->getViewport();

        bbox.intersect(vp);

        auto x = bbox.x - vp.x;
        auto y = bbox.y - vp.y;

        clipTask->setViewport(RenderRegion {
            x,
            vp.h - y - bbox.h,
            bbox.w,
            bbox.h,
        });

        const auto& matrix = sdata->geometry->getTransformMatrix();

        float matrix44[16];

        currentPass()->getMatrix(matrix44, matrix);

        uint32_t loc = clipTask->getProgram()->getUniformBlockIndex("Matrix");

        uint32_t viewOffset = mGpuBuffer->push(matrix44, 16 * sizeof(float), true);

        clipTask->addBindResource(GlBindingResource{
            0,
            loc,
            mGpuBuffer->getBufferId(),
            viewOffset,
            16 * sizeof(float),
        });

        auto maskTask = new GlRenderTask(mPrograms[RT_Stencil].get());

        maskTask->setDrawDepth(clipDepths[i]);

        maskTask->addVertexLayout(GlVertexLayout{0, 2, 2 * sizeof(float), identityVertexOffset});
        maskTask->addBindResource(GlBindingResource{
            0,
            loc,
            mGpuBuffer->getBufferId(),
            mat4Offset, 16 * sizeof(float),
        });

        maskTask->setDrawRange(identityIndexOffset, 6);
        maskTask->setViewport(RenderRegion{0, 0, static_cast<int32_t>(vp.w), static_cast<int32_t>(vp.h)});

        currentPass()->addRenderTask(new GlClipTask(clipTask, maskTask));
    }
}

GlRenderPass* GlRenderer::currentPass()
{
    if (mRenderPassStack.empty()) return nullptr;

    return &mRenderPassStack.back();
}

void GlRenderer::prepareBlitTask(GlBlitTask* task)
{
    RenderRegion region{0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)};
    prepareCmpTask(task, region, surface.w, surface.h);

    {
        uint32_t loc = task->getProgram()->getUniformLocation("uSrcTexture");
        task->addBindResource(GlBindingResource{0, task->getColorTexture(), loc});
    }
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

    uint32_t vertexOffset = mGpuBuffer->push(vertices.data, vertices.count * sizeof(float));
    uint32_t indexOffset = mGpuBuffer->push(indices.data, indices.count * sizeof(uint32_t));

    task->addVertexLayout(GlVertexLayout{0, 2, 4 * sizeof(float), vertexOffset});
    task->addVertexLayout(GlVertexLayout{1, 2, 4 * sizeof(float), vertexOffset + 2 * sizeof(float)});

    task->setDrawRange(indexOffset, indices.count);

    task->setViewport(RenderRegion{
        x,
        static_cast<int32_t>((passVp.h - y - h)),
        w,
        h,
    });
}

void GlRenderer::endRenderPass(Compositor* cmp)
{
    auto gl_cmp = static_cast<GlCompositor*>(cmp);
    if (cmp->method != CompositeMethod::None) {
        auto self_pass = std::move(mRenderPassStack.back());
        mRenderPassStack.pop_back();

        // mask is pushed first
        auto mask_pass = std::move(mRenderPassStack.back());
        mRenderPassStack.pop_back();

        if (self_pass.isEmpty() || mask_pass.isEmpty()) return;

        GlProgram* program = nullptr;
        switch(cmp->method) {
            case CompositeMethod::ClipPath:
            case CompositeMethod::AlphaMask:
                program = mPrograms[RT_MaskAlpha].get();
                break;
            case CompositeMethod::InvAlphaMask:
                program = mPrograms[RT_MaskAlphaInv].get();
                break;
            case CompositeMethod::LumaMask:
                program = mPrograms[RT_MaskLuma].get();
                break;
            case CompositeMethod::InvLumaMask:
                program = mPrograms[RT_MaskLumaInv].get();
                break;
            case CompositeMethod::AddMask:
                program = mPrograms[RT_MaskAdd].get();
                break;
            case CompositeMethod::SubtractMask:
                program = mPrograms[RT_MaskSub].get();
                break;
            case CompositeMethod::IntersectMask:
                program = mPrograms[RT_MaskIntersect].get();
                break;
            case CompositeMethod::DifferenceMask:
                program = mPrograms[RT_MaskDifference].get();
                break;
            default:
                break;
        }

        if (program == nullptr) {
            return;
        }

        auto prev_task = mask_pass.endRenderPass<GlComposeTask>(nullptr, currentPass()->getFboId());
        prev_task->setDrawDepth(currentPass()->nextDrawDepth());
        prev_task->setRenderSize(static_cast<uint32_t>(gl_cmp->bbox.w), static_cast<uint32_t>(gl_cmp->bbox.h));
        prev_task->setViewport(gl_cmp->bbox);

        auto compose_task = self_pass.endRenderPass<GlDrawBlitTask>(program, currentPass()->getFboId());
        compose_task->setRenderSize(static_cast<uint32_t>(gl_cmp->bbox.w), static_cast<uint32_t>(gl_cmp->bbox.h));
        compose_task->setPrevTask(prev_task);

        prepareCmpTask(compose_task, gl_cmp->bbox, self_pass.getFboWidth(), self_pass.getFboHeight());

        {
            uint32_t loc = program->getUniformLocation("uSrcTexture");
            compose_task->addBindResource(GlBindingResource{0, self_pass.getTextureId(), loc});
        }

        {
            uint32_t loc = program->getUniformLocation("uMaskTexture");
            compose_task->addBindResource(GlBindingResource{1, mask_pass.getTextureId(), loc});
        }

        compose_task->setDrawDepth(currentPass()->nextDrawDepth());
        compose_task->setParentSize(static_cast<uint32_t>(currentPass()->getViewport().w), static_cast<uint32_t>(currentPass()->getViewport().h));
        currentPass()->addRenderTask(compose_task);
    } else {

        auto renderPass = std::move(mRenderPassStack.back());
        mRenderPassStack.pop_back();

        if (renderPass.isEmpty()) return;

        auto task = renderPass.endRenderPass<GlDrawBlitTask>(
            mPrograms[RT_Image].get(), currentPass()->getFboId());
        task->setRenderSize(static_cast<uint32_t>(gl_cmp->bbox.w), static_cast<uint32_t>(gl_cmp->bbox.h));
        prepareCmpTask(task, gl_cmp->bbox, renderPass.getFboWidth(), renderPass.getFboHeight());
        task->setDrawDepth(currentPass()->nextDrawDepth());

        // matrix buffer
        {
            float matrix[16];
            memset(matrix, 0, 16 * sizeof(float));
            matrix[0] = 1.f;
            matrix[5] = 1.f;
            matrix[10] = 1.f;
            matrix[15] = 1.f;
            uint32_t loc = task->getProgram()->getUniformBlockIndex("Matrix");

            task->addBindResource(GlBindingResource{
                0,
                loc,
                mGpuBuffer->getBufferId(),
                mGpuBuffer->push(matrix, 16 * sizeof(float), true),
                16 * sizeof(float),
            });
        }
        // image info
        {
            uint32_t info[4] = {ABGR8888, 0, cmp->opacity, 0};
            uint32_t loc = task->getProgram()->getUniformBlockIndex("ColorInfo");

            task->addBindResource(GlBindingResource{
                1,
                loc,
                mGpuBuffer->getBufferId(),
                mGpuBuffer->push(info, 4 * sizeof(uint32_t), true),
                4 * sizeof(uint32_t),
            });
        }
        // texture id
        {
            uint32_t loc = task->getProgram()->getUniformLocation("uTexture");
            task->addBindResource(GlBindingResource{0, renderPass.getTextureId(), loc});
        }
        task->setParentSize(static_cast<uint32_t>(currentPass()->getViewport().w), static_cast<uint32_t>(currentPass()->getViewport().h));
        currentPass()->addRenderTask(std::move(task));
    }
}
