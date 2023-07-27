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

#include "tvgGlGeometry.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlPropertyInterface.h"

#include "gl_shader_source.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
static int32_t initEngineCnt = false;
static int32_t rendererCnt = 0;

static void _termEngine()
{
    if (rendererCnt > 0) return;

    // TODO: Clean up global resources
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

#define NOISE_LEVEL 0.5f

bool GlRenderer::clear()
{
    // TODO: (Request) to clear target
    // Will be adding glClearColor for input buffer
    return true;
}

bool GlRenderer::target(TVG_UNUSED uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h)
{
    assert(w > 0 && h > 0);

    surface.stride = stride;
    surface.w = w;
    surface.h = h;

    if (mShaders.empty()) {
        initShaders();
    }

    mViewPort.x = 0;
    mViewPort.y = 0;
    mViewPort.w = w;
    mViewPort.h = h;

    return true;
}

bool GlRenderer::sync()
{
    GL_CHECK(glFinish());
    GlRenderTask::unload();
    return true;
}

RenderRegion GlRenderer::region(TVG_UNUSED RenderData data)
{
    return {0, 0, 0, 0};
}

bool GlRenderer::preRender()
{
    // Blend function for straight alpha
    GL_CHECK(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glEnable(GL_SCISSOR_TEST));
    GL_CHECK(glScissor(mViewPort.x, mViewPort.y, mViewPort.w, mViewPort.h));

    mVertexBuffer.copyToGPU();
    mIndexBuffer.copyToGPU();
    mUniformBuffer.copyToGPU();

    return true;
}

bool GlRenderer::postRender()
{
    // TODO: called just after render()

    return true;
}

Compositor* GlRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    // TODO: Prepare frameBuffer & Setup render target for composition
    return nullptr;
}

bool GlRenderer::beginComposite(TVG_UNUSED Compositor* cmp, CompositeMethod method, uint8_t opacity)
{
    // TODO: delete the given compositor and restore the context
    return false;
}

bool GlRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
    // TODO: delete the given compositor and restore the context
    return false;
}

ColorSpace GlRenderer::colorSpace()
{
    return ColorSpace::ABGR8888;
}

bool GlRenderer::blend(TVG_UNUSED BlendMethod method)
{
    // TODO:
    return false;
}

bool GlRenderer::renderImage(TVG_UNUSED void* data)
{
    auto sdata = static_cast<GlShape*>(data);

    if (!sdata) {
        return false;
    }

    if (sdata->texId == 0) {
        return false;
    }

    GL_CHECK(glScissor(sdata->viewPort.x, sdata->viewPort.y, sdata->viewPort.w, sdata->viewPort.h));

    sdata->geometry->beginClipMask();

    sdata->geometry->bind();

    sdata->geometry->draw(RenderUpdateFlag::Image);

    sdata->geometry->unBind();

    sdata->geometry->endClipMask();

    return true;
}

bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    size_t flags = static_cast<size_t>(sdata->updateFlag);

    GL_CHECK(glScissor(sdata->viewPort.x, surface.h - sdata->viewPort.y - sdata->viewPort.h, sdata->viewPort.w,
                       sdata->viewPort.h));

    sdata->geometry->beginClipMask();

    sdata->geometry->bind();

    if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
        sdata->geometry->draw(RenderUpdateFlag::Gradient);
    }

    if (flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Transform)) {
        sdata->geometry->draw(RenderUpdateFlag::Color);
    }

    if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
        sdata->geometry->draw(RenderUpdateFlag::Stroke);
    }

    sdata->geometry->unBind();

    sdata->geometry->endClipMask();

    return true;
}

bool GlRenderer::dispose(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    if (sdata->texId) {
        GL_CHECK(glDeleteTextures(1, &sdata->texId));
    }

    delete sdata;
    return true;
}

RenderData GlRenderer::prepare(Surface* image, TVG_UNUSED const RenderMesh* mesh, TVG_UNUSED RenderData data,
                               const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, uint8_t opacity,
                               TVG_UNUSED RenderUpdateFlag flags)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = new GlShape;
    }

    sdata->viewPort = mViewPort;
    sdata->viewWd = static_cast<float>(this->surface.w);
    sdata->viewHt = static_cast<float>(this->surface.h);

    if (flags == RenderUpdateFlag::None) {
        return sdata;
    }

    sdata->texId = genTexture(image);
    sdata->geometry = make_unique<GlGeometry>();

    sdata->geometry->updateTransform(transform, sdata->viewWd, sdata->viewHt);

    TessContext context{&mVertexBuffer, &mIndexBuffer, &mUniformBuffer, mShaders};

    sdata->geometry->tessellate(sdata->texId, image, opacity, &context);


    if (clips.count > 0) {

        for (uint32_t i = 0; i < clips.count; i++) {
            auto clipData = static_cast<GlShape*>(clips.data[i]);

            if (clipData == nullptr) {
                continue;
            }

            sdata->geometry->addClipDraw(clipData->geometry->tessellate(*clipData->rshape, &context));
        }
    }

    return sdata;
}

RenderData GlRenderer::prepare(TVG_UNUSED const Array<RenderData>& scene, TVG_UNUSED RenderData data,
                               TVG_UNUSED const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips,
                               TVG_UNUSED uint8_t opacity, TVG_UNUSED RenderUpdateFlag flags)
{
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = new GlShape;
        sdata->texId = 0;
    }

    sdata->viewPort = mViewPort;
    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = flags;

    if (sdata->updateFlag == RenderUpdateFlag::None) return sdata;

    sdata->geometry = std::make_unique<GlGeometry>();

    // handle scene data
    for (uint32_t i = 0; i < scene.count; i++) {
        auto clipData = reinterpret_cast<GlShape*>(scene.data[i]);

        if (clipData == nullptr) {
            continue;
        }

        sdata->sceneData.push(clipData);
    }

    return sdata;
}

RenderData GlRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform,
                               Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, RenderUpdateFlag flags,
                               TVG_UNUSED bool clipper)
{
    // prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = new GlShape;
        sdata->rshape = &rshape;
        sdata->texId = 0;
    }

    sdata->viewPort = mViewPort;
    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = flags;

    if (sdata->updateFlag == RenderUpdateFlag::None) return sdata;

    sdata->geometry = make_unique<GlGeometry>();

    // invisible?
    uint8_t alphaF = 0, alphaS = 0;
    rshape.fillColor(nullptr, nullptr, nullptr, &alphaF);
    rshape.strokeColor(nullptr, nullptr, nullptr, &alphaS);

    if (((sdata->updateFlag & RenderUpdateFlag::Gradient) == 0) &&
        ((sdata->updateFlag & RenderUpdateFlag::Color) && alphaF == 0) &&
        ((sdata->updateFlag & RenderUpdateFlag::Stroke) && alphaS == 0)) {
        return sdata;
    }

    sdata->geometry->updateTransform(transform, sdata->viewWd, sdata->viewHt);

    TessContext context{&mVertexBuffer, &mIndexBuffer, &mUniformBuffer, mShaders};


    if (rshape.fill) {
        sdata->geometry->tessellate(rshape, RenderUpdateFlag::Gradient, &context);
    } else {
        sdata->geometry->tessellate(rshape, RenderUpdateFlag::Color, &context);
    }

    sdata->geometry->tessellate(rshape, RenderUpdateFlag::Stroke, &context);

    if (clips.count > 0) {

        for (uint32_t i = 0; i < clips.count; i++) {
            auto clipData = static_cast<GlShape*>(clips.data[i]);

            if (clipData == nullptr) {
                continue;
            }

            if (clipData->rshape) {
                sdata->geometry->addClipDraw(clipData->geometry->tessellate(*clipData->rshape, &context));
            } else if (clipData->sceneData.count > 0) {
                sdata->geometry->addClipDraw(sdata->geometry->tessellate(clipData->sceneData, &context));
            }
        }
    }

    return sdata;
}

RenderRegion GlRenderer::viewport()
{
    return mViewPort;
}

bool GlRenderer::viewport(TVG_UNUSED const RenderRegion& vp)
{
    mViewPort = vp;
    return true;
}

int GlRenderer::init(uint32_t threads)
{
    if ((initEngineCnt++) > 0) return true;

    // TODO:

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

GlRenderer::GlRenderer()
    : mVertexBuffer(GlGpuBuffer::Target::ARRAY_BUFFER),
      mIndexBuffer(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER),
      mUniformBuffer(GlGpuBuffer::Target::UNIFORM_BUFFER)
{
}

GlRenderer::~GlRenderer()
{
    mShaders.clear();

    --rendererCnt;

    if (rendererCnt == 0 && initEngineCnt == 0) _termEngine();
}

void GlRenderer::initShaders()
{
    // Solid Color Renderer
    {
        std::string vs_shader(color_vert, color_vert_size);
        std::string fs_shader(color_frag, color_frag_size);
        mShaders.emplace_back(GlProgram::gen(GlShader::gen(vs_shader.c_str(), fs_shader.c_str())));
    }

    // Linear Gradient Renderer
    {
        std::string vs_shader(gradient_vert, gradient_vert_size);
        std::string fs_shader(linear_gradient_frag, linear_gradient_frag_size);
        mShaders.emplace_back(GlProgram::gen(GlShader::gen(vs_shader.c_str(), fs_shader.c_str())));

        // Radial Gradient Renderer
        std::string rs_shader(radial_gradient_frag, radial_gradient_frag_size);
        mShaders.emplace_back(GlProgram::gen(GlShader::gen(vs_shader.c_str(), rs_shader.c_str())));
    }
    // Image Renderer
    {
        std::string vs_shader(image_vert, image_vert_size);
        std::string fs_shader(image_frag, image_frag_size);
        mShaders.emplace_back(GlProgram::gen(GlShader::gen(vs_shader.c_str(), fs_shader.c_str())));
    }
    // stencil Renderer
    {
        std::string vs_shader(stencil_vert, stencil_vert_size);
        std::string fs_shader(stencil_frag, stencil_frag_size);
        mShaders.emplace_back(GlProgram::gen(GlShader::gen(vs_shader.c_str(), fs_shader.c_str())));
    }
}

uint32_t GlRenderer::genTexture(Surface* image)
{
    GLuint tex = 0;

    GL_CHECK(glGenTextures(1, &tex));

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->w, image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->data));

    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    return tex;
}
