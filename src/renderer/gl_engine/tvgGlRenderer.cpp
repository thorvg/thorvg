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

    return true;
}


bool GlRenderer::sync()
{
    mGpuBuffer->flushToGPU();

    // Blend function for straight alpha
    GL_CHECK(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));

    mGpuBuffer->bind();

    for(auto& task: mRenderTasks) {
        task->run();
    }

    mGpuBuffer->unbind();

    mRenderTasks.clear();

    return true;
}


RenderRegion GlRenderer::region(TVG_UNUSED RenderData data)
{
    return {0, 0, 0, 0};
}


bool GlRenderer::preRender()
{
    if (mPrograms.size() == 0)
    {
        initShaders();
    }

    return true;
}


bool GlRenderer::postRender()
{
    return true;
}


Compositor* GlRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    //TODO: Prepare frameBuffer & Setup render target for composition
    return nullptr;
}


bool GlRenderer::beginComposite(TVG_UNUSED Compositor* cmp, CompositeMethod method, uint8_t opacity)
{
    //TODO: delete the given compositor and restore the context
    return false;
}


bool GlRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
    //TODO: delete the given compositor and restore the context
    return false;
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

    auto task = make_unique<GlRenderTask>(mPrograms[RT_Image].get());

    if (!sdata->geometry->draw(task.get(), mGpuBuffer.get(), RenderUpdateFlag::Image)) return false;

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
        uint32_t info[4] = {sdata->texColorSpace, sdata->texFlipY, sdata->texOpacity, 0};
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

    mRenderTasks.emplace_back(std::move(task));

    return true;
}


bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    uint8_t r = 0, g = 0, b = 0, a = 0;
    size_t flags = static_cast<size_t>(sdata->updateFlag);

    GL_CHECK(glViewport(0, 0, (GLsizei)sdata->viewWd, (GLsizei)sdata->viewHt));

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


bool GlRenderer::dispose(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    if (sdata->texId) glDeleteTextures(1, &sdata->texId);

    delete sdata;
    return true;
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
    sdata->texOpacity = opacity;
    sdata->texColorSpace = image->cs;
    sdata->texFlipY = (mesh && mesh->triangleCnt) ? 0 : 1;
    sdata->geometry = make_unique<GlGeometry>();

    sdata->geometry->updateTransform(transform, sdata->viewWd, sdata->viewHt);

    sdata->geometry->tesselate(image, mesh, flags);

    return sdata;
}


RenderData GlRenderer::prepare(TVG_UNUSED const Array<RenderData>& scene, TVG_UNUSED RenderData data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, TVG_UNUSED RenderUpdateFlag flags)
{
    //TODO:
    return nullptr;
}


RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, RenderUpdateFlag flags, TVG_UNUSED bool clipper)
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

    if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform) )
    {
        if (!sdata->geometry->tesselate(rshape, sdata->updateFlag)) return sdata;
    }
    return sdata;
}


RenderRegion GlRenderer::viewport()
{
    return {0, 0, INT32_MAX, INT32_MAX};
}


bool GlRenderer::viewport(TVG_UNUSED const RenderRegion& vp)
{
    //TODO:
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

GlRenderer::GlRenderer() :mGpuBuffer(new GlStageBuffer), mPrograms(), mRenderTasks()
{
}

GlRenderer::~GlRenderer()
{
    mRenderTasks.clear();

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
}


void GlRenderer::drawPrimitive(GlShape& sdata, uint8_t r, uint8_t g, uint8_t b, uint8_t a, RenderUpdateFlag flag)
{
    auto task = make_unique<GlRenderTask>(mPrograms[RT_Color].get());

    if (!sdata.geometry->draw(task.get(), mGpuBuffer.get(), flag)) return;
    
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

    mRenderTasks.emplace_back(std::move(task));
}


void GlRenderer::drawPrimitive(GlShape& sdata, const Fill* fill, RenderUpdateFlag flag)
{
    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = fill->colorStops(&stops);
    if (stopCnt < 2) return;

    unique_ptr<GlRenderTask> task;

    if (fill->identifier() == TVG_CLASS_ID_LINEAR) {
        task = make_unique<GlRenderTask>(mPrograms[RT_LinGradient].get());
    } else if (fill->identifier() == TVG_CLASS_ID_RADIAL) {
        task = make_unique<GlRenderTask>(mPrograms[RT_RadGradient].get());
    } else {
        return;
    }

    if (!sdata.geometry->draw(task.get(), mGpuBuffer.get(), flag)) return;

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

            linearFill->linear(&gradientBlock.startPos[0], &gradientBlock.startPos[1], &gradientBlock.stopPos[0], &gradientBlock.stopPos[1]);

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

            radialFill->radial(&gradientBlock.centerPos[0], &gradientBlock.centerPos[1], &gradientBlock.radius[0]);

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

    mRenderTasks.emplace_back(std::move(task));
}

