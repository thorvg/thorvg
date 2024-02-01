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
    //TODO: (Request) to clear target
    // Will be adding glClearColor for input buffer
    return true;
}


bool GlRenderer::target(TVG_UNUSED uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h)
{
    assert(w > 0 && h > 0);

    surface.stride = stride;
    surface.w = w;
    surface.h = h;

    mViewport.x = 0;
    mViewport.y = 0;
    mViewport.w = surface.w;
    mViewport.h = surface.h;

    // get current binded framebuffer id
    // EFL seems has a seperate framebuffer for evagl view
    //TODO: introduce a new api to specify which fbo this canvas is binded
    GL_CHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mTargetFboId));

    mRootTarget = make_unique<GlRenderTarget>(surface.w, surface.h);
    mRootTarget->init(mTargetFboId);

    return true;
}


bool GlRenderer::sync()
{
    //nothing to be done.
    if (mRenderPassStack.size() == 0) return true;

    mGpuBuffer->flushToGPU();

    // Blend function for straight alpha
    GL_CHECK(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glEnable(GL_SCISSOR_TEST));

    mGpuBuffer->bind();

    auto task = mRenderPassStack.front().endRenderPass<GlBlitTask>(nullptr, mTargetFboId);

    task->setSize(surface.w, surface.h);

    task->run();

    mGpuBuffer->unbind();


    GL_CHECK(glDisable(GL_SCISSOR_TEST));

    mRenderPassStack.clear();

    delete task;

    return true;
}


RenderRegion GlRenderer::region(TVG_UNUSED RenderData data)
{
    return {0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)};
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


Compositor* GlRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    mComposeStack.emplace_back(make_unique<tvg::Compositor>());
    return mComposeStack.back().get();
}


bool GlRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity)
{
    if (!cmp) return false;

    cmp->method = method;
    cmp->opacity = opacity;

    uint32_t index = mRenderPassStack.size() - 1;

    if (index >= mComposePool.count) {
        mComposePool.push( new GlRenderTarget(surface.w, surface.h));
        mComposePool[index]->init(mTargetFboId);
    }
    mRenderPassStack.emplace_back(GlRenderPass(mComposePool[index]));

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


bool GlRenderer::blend(TVG_UNUSED BlendMethod method)
{
    //TODO:
    return false;
}


bool GlRenderer::renderImage(void* data)
{
    auto sdata = static_cast<GlShape*>(data);

    if (!sdata) return false;

    if ((sdata->updateFlag & RenderUpdateFlag::Image) == 0) return false;

    auto task = new GlRenderTask(mPrograms[RT_Image].get());

    if (!sdata->geometry->draw(task, mGpuBuffer.get(), RenderUpdateFlag::Image)) return false;

    // matrix buffer
    {
        auto matrix = sdata->geometry->getTransforMatrix();
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

    currentPass()->addRenderTask(task);

    return true;
}


bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    uint8_t r = 0, g = 0, b = 0, a = 0;
    size_t flags = static_cast<size_t>(sdata->updateFlag);

    if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform))
    {
        auto gradient = sdata->rshape->fill;
        if (gradient) drawPrimitive(*sdata, gradient, RenderUpdateFlag::Gradient);
    }

    if(flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Transform))
    {
        sdata->rshape->fillColor(&r, &g, &b, &a);
        if (a > 0)
        {
            drawPrimitive(*sdata, r, g, b, a, RenderUpdateFlag::Color);
        }
    }

    if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform))
    {
        sdata->rshape->strokeColor(&r, &g, &b, &a);
        if (a > 0)
        {
            drawPrimitive(*sdata, r, g, b, a, RenderUpdateFlag::Stroke);
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

RenderData GlRenderer::prepare(Surface* image, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    if (flags == RenderUpdateFlag::None) return nullptr;

    auto sdata = static_cast<GlShape*>(data);

    if (!sdata) sdata = new GlShape;

    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = flags;

    sdata->texId = _genTexture(image);
    sdata->opacity = opacity;
    sdata->texColorSpace = image->cs;
    sdata->texFlipY = (mesh && mesh->triangleCnt) ? 0 : 1;
    sdata->geometry = make_unique<GlGeometry>();

    sdata->geometry->updateTransform(transform, sdata->viewWd, sdata->viewHt);
    sdata->geometry->setViewport(RenderRegion{
        mViewport.x,
        static_cast<int32_t>((surface.h - mViewport.y - mViewport.h)),
        mViewport.w,
        mViewport.h,
    });

    sdata->geometry->tesselate(image, mesh, flags);

    return sdata;
}


RenderData GlRenderer::prepare(TVG_UNUSED const Array<RenderData>& scene, TVG_UNUSED RenderData data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, TVG_UNUSED RenderUpdateFlag flags)
{
    //TODO:
    return nullptr;
}


RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, TVG_UNUSED bool clipper)
{
    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = new GlShape;
        sdata->rshape = &rshape;
    }

    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = flags;

    if (sdata->updateFlag == RenderUpdateFlag::None) return sdata;

    sdata->geometry = make_unique<GlGeometry>();
    sdata->opacity = opacity;

    //invisible?
    uint8_t alphaF = 0, alphaS = 0;
    rshape.fillColor(nullptr, nullptr, nullptr, &alphaF);
    rshape.strokeColor(nullptr, nullptr, nullptr, &alphaS);

    if ( ((sdata->updateFlag & RenderUpdateFlag::Gradient) == 0) &&
         ((sdata->updateFlag & RenderUpdateFlag::Color) && alphaF == 0) &&
         ((sdata->updateFlag & RenderUpdateFlag::Stroke) && alphaS == 0) )
    {
        return sdata;
    }

    sdata->geometry->updateTransform(transform, sdata->viewWd, sdata->viewHt);
    sdata->geometry->setViewport(RenderRegion{
        mViewport.x,
        static_cast<int32_t>(surface.h - mViewport.y - mViewport.h),
        mViewport.w,
        mViewport.h,
    });

    if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform) )
    {
        if (!sdata->geometry->tesselate(rshape, sdata->updateFlag)) return sdata;
    }
    return sdata;
}


RenderRegion GlRenderer::viewport()
{
    return {0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)};
}


bool GlRenderer::viewport(const RenderRegion& vp)
{
    mViewport = vp;
    return true;
}


int GlRenderer::init(uint32_t threads)
{
    if ((initEngineCnt++) > 0) return true;

    //TODO:

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
    return new GlRenderer();
}

GlRenderer::GlRenderer() :mViewport() ,mGpuBuffer(new GlStageBuffer), mPrograms(), mComposePool()
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
}


void GlRenderer::drawPrimitive(GlShape& sdata, uint8_t r, uint8_t g, uint8_t b, uint8_t a, RenderUpdateFlag flag)
{
    auto task = new GlRenderTask(mPrograms[RT_Color].get());

    if (!sdata.geometry->draw(task, mGpuBuffer.get(), flag)) return;

    a = MULTIPLY(a, sdata.opacity);
    
    // matrix buffer
    {
        auto matrix = sdata.geometry->getTransforMatrix();
        uint32_t loc = task->getProgram()->getUniformBlockIndex("Matrix");

        task->addBindResource(GlBindingResource{
            0,
            loc,
            mGpuBuffer->getBufferId(),
            mGpuBuffer->push(matrix, 16 * sizeof(float), true),
            16 * sizeof(float),
        });
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

    currentPass()->addRenderTask(task);
}


void GlRenderer::drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag)
{
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

    if (!sdata.geometry->draw(task, mGpuBuffer.get(), flag)) return;

    // matrix buffer
    {
        auto matrix = sdata.geometry->getTransforMatrix();
        uint32_t loc = task->getProgram()->getUniformBlockIndex("Matrix");

        task->addBindResource(GlBindingResource{
            0,
            loc,
            mGpuBuffer->getBufferId(),
            mGpuBuffer->push(matrix, 16 * sizeof(float), true),
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

            gradientBlock.nStops[0] = stopCnt * 1.f;
            gradientBlock.nStops[1] = NOISE_LEVEL;
            for (uint32_t i = 0; i < stopCnt; ++i) {
                gradientBlock.stopPoints[i] = stops[i].offset;
                gradientBlock.stopColors[i * 4 + 0] = stops[i].r / 255.f;
                gradientBlock.stopColors[i * 4 + 1] = stops[i].g / 255.f;
                gradientBlock.stopColors[i * 4 + 2] = stops[i].b / 255.f;
                gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f;
            }

            float x1, x2, y1, y2;
            linearFill->linear(&x1, &y1, &x2, &y2);

            auto transform = linearFill->transform();

            gradientBlock.startPos[0] = x1 * transform.e11 + transform.e13;
            gradientBlock.startPos[1] = y1 * transform.e22 + transform.e23;
            gradientBlock.stopPos[0] = x2 * transform.e11 + transform.e13;
            gradientBlock.stopPos[1] = y2 * transform.e22 + transform.e23;

            gradientBinding = GlBindingResource{
                1,
                loc,
                mGpuBuffer->getBufferId(),
                mGpuBuffer->push(&gradientBlock, sizeof(GlLinearGradientBlock), true),
                sizeof(GlLinearGradientBlock),
            };
        } else {
            auto radialFill = static_cast<const RadialGradient*>(fill);

            GlRadialGradientBlock gradientBlock;

            gradientBlock.nStops[0] = stopCnt * 1.f;
            gradientBlock.nStops[1] = NOISE_LEVEL;
            for (uint32_t i = 0; i < stopCnt; ++i) {
                gradientBlock.stopPoints[i] = stops[i].offset;
                gradientBlock.stopColors[i * 4 + 0] = stops[i].r / 255.f;
                gradientBlock.stopColors[i * 4 + 1] = stops[i].g / 255.f;
                gradientBlock.stopColors[i * 4 + 2] = stops[i].b / 255.f;
                gradientBlock.stopColors[i * 4 + 3] = stops[i].a / 255.f;
            }

            float x, y, r;
            radialFill->radial(&x, &y, &r);

            auto transform = radialFill->transform();

            gradientBlock.centerPos[0] = x * transform.e11 + transform.e13;
            gradientBlock.centerPos[1] = y * transform.e22 + transform.e23;
            gradientBlock.radius[0] = r * transform.e11;

            gradientBinding = GlBindingResource{
                1,
                loc,
                mGpuBuffer->getBufferId(),
                mGpuBuffer->push(&gradientBlock, sizeof(GlRadialGradientBlock), true),
                sizeof(GlRadialGradientBlock),
            };
        }

        task->addBindResource(gradientBinding);
    }

    currentPass()->addRenderTask(task);
}

GlRenderPass* GlRenderer::currentPass() 
{
    if (mRenderPassStack.empty()) return nullptr;

    return &mRenderPassStack.back();
}

void GlRenderer::prepareCmpTask(GlRenderTask* task, float opacity)
{
    // we use 1:1 blit mapping since compositor fbo is same size as root fbo
    Array<float> vertices(5 * 4);

    float left = -1.f;
    float top = 1.f;
    float right = 1.f;
    float bottom = -1.f;

    // left top point
    vertices.push(left);
    vertices.push(top);
    vertices.push(opacity);
    vertices.push(0.f);
    vertices.push(1.f);
    // left bottom point
    vertices.push(left);
    vertices.push(bottom);
    vertices.push(opacity);
    vertices.push(0.f);
    vertices.push(0.f);
    // right top point
    vertices.push(right);
    vertices.push(top);
    vertices.push(opacity);
    vertices.push(1.f);
    vertices.push(1.f);
    // right bottom point
    vertices.push(right);
    vertices.push(bottom);
    vertices.push(opacity);
    vertices.push(1.f);
    vertices.push(0.f);

    Array<uint32_t> indices(6);

    indices.push(0);
    indices.push(1);
    indices.push(2);
    indices.push(2);
    indices.push(1);
    indices.push(3);

    uint32_t vertexOffset = mGpuBuffer->push(vertices.data, vertices.count * sizeof(float));
    uint32_t indexOffset = mGpuBuffer->push(indices.data, vertices.count * sizeof(uint32_t));

    task->addVertexLayout(GlVertexLayout{0, 3, 5 * sizeof(float), vertexOffset});
    task->addVertexLayout(GlVertexLayout{1, 2, 5 * sizeof(float), vertexOffset + 3 * sizeof(float)});

    task->setDrawRange(indexOffset, indices.count);
    task->setViewport(RenderRegion{
        mViewport.x,
        static_cast<int32_t>((surface.h - mViewport.y - mViewport.h)),
        mViewport.w,
        mViewport.h,
    });
}

void GlRenderer::endRenderPass(Compositor* cmp)
{
    if (cmp->method != CompositeMethod::None) {
        auto self_pass = std::move(mRenderPassStack.back());
        mRenderPassStack.pop_back();

        // mask is pushed first
        auto mask_pass = std::move(mRenderPassStack.back());
        mRenderPassStack.pop_back();

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

        currentPass()->addRenderTask(prev_task);

        auto compose_task = self_pass.endRenderPass<GlDrawBlitTask>(program, currentPass()->getFboId());

        prepareCmpTask(compose_task, cmp->opacity / 255.f);

        {
            uint32_t loc = program->getUniformLocation("uSrcTexture");
            compose_task->addBindResource(GlBindingResource{0, self_pass.getTextureId(), loc});
        }

        {
            uint32_t loc = program->getUniformLocation("uMaskTexture");
            compose_task->addBindResource(GlBindingResource{1, mask_pass.getTextureId(), loc});
        }


        currentPass()->addRenderTask(compose_task);
    } else {

        auto renderPass = std::move(mRenderPassStack.back());
        mRenderPassStack.pop_back();

        auto task = renderPass.endRenderPass<GlDrawBlitTask>(
            mPrograms[RT_Image].get(), currentPass()->getFboId());

        prepareCmpTask(task, 1.f);

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

        currentPass()->addRenderTask(std::move(task));
    }
}
