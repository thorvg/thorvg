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
    // Will be adding glClearColor for input buffer
    mDrawCommands.clear();

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
    // Blend function for straight alpha
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glEnable(GL_SCISSOR_TEST));
    GL_CHECK(glScissor(mViewPort.x, mViewPort.y, mViewPort.w, mViewPort.h));
    GL_CHECK(glClear(GL_STENCIL_BUFFER_BIT));

    mVertexBuffer.copyToGPU();
    mIndexBuffer.copyToGPU();
    mUniformBuffer.copyToGPU();

    mBlitVertexBuffer.copyToGPU();
    mBlitIndexBuffer.copyToGPU();
    mBlitUniformBuffer.copyToGPU();

    for (auto& cmd : mDrawCommands) {
        cmd.execute();
    }

    GL_CHECK(glFlush());
    return true;
}

RenderRegion GlRenderer::region(TVG_UNUSED RenderData data)
{
    // TODO generate region based on shape bounds
    return RenderRegion{0, 0, static_cast<int32_t>(surface.w), static_cast<int32_t>(surface.h)};
}

bool GlRenderer::preRender()
{
    clearCompositors();

    return true;
}

bool GlRenderer::postRender()
{
    mFboStack.clear();

    return true;
}

Compositor* GlRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    // TODO: Prepare frameBuffer based on region

    auto cmp = new GlCompositor(surface.w, surface.h);

    mCompositors.push(cmp);

    return cmp;
}

bool GlRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity)
{
    auto glCmp = static_cast<GlCompositor*>(cmp);

    if (!glCmp) {
        return false;
    }

    glCmp->opacity = opacity;
    glCmp->method = method;

    // check before bind to fbo
    if (glCmp->fboId[0] == 0 || glCmp->fboId[1] == 0) {
        return false;
    }


    if (method == CompositeMethod::None) {
        mFboStack.push(glCmp->targetFbo());
    } else {
        mFboStack.push(glCmp->sourceFbo());
    }

    mCurrentFbo = mFboStack.last();

    return true;
}

bool GlRenderer::endComposite(Compositor* cmp)
{
    auto glCmp = static_cast<GlCompositor*>(cmp);

    if (!glCmp) {
        return false;
    }


    if (glCmp->method != CompositeMethod::None) {
        assert(mFboStack.data[mFboStack.count - 1] == glCmp->sourceFbo());
        assert(mFboStack.data[mFboStack.count - 2] == glCmp->targetFbo());
        mFboStack.pop();
        mFboStack.pop();
    } else {
        assert(mFboStack.last() == glCmp->targetFbo());
        mFboStack.pop();
    }

    if (mFboStack.count == 0) {
        mCurrentFbo = 0;
    } else {
        mCurrentFbo = mFboStack.last();
    }

    // if method is not NONE, generate a blit draw command
    prepareBlitCMD(glCmp);

    return true;
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

    GlRenderCommand cmd{};
    cmd.fboId = mCurrentFbo;
    cmd.viewPort =
        RenderRegion{sdata->viewPort.x, static_cast<int32_t>(surface.h - sdata->viewPort.y - sdata->viewPort.h),
                     sdata->viewPort.w, sdata->viewPort.h};

    cmd.geometry = sdata->geometry.get();

    sdata->geometry->draw(RenderUpdateFlag::Image, cmd.commands);

    mDrawCommands.emplace_back(cmd);

    return true;
}

bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    size_t flags = static_cast<size_t>(sdata->updateFlag);

    GlRenderCommand cmd{};

    cmd.fboId = mCurrentFbo;
    cmd.viewPort =
        RenderRegion{sdata->viewPort.x, static_cast<int32_t>(surface.h - sdata->viewPort.y - sdata->viewPort.h),
                     sdata->viewPort.w, sdata->viewPort.h};

    cmd.geometry = sdata->geometry.get();

    if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
        sdata->geometry->draw(RenderUpdateFlag::Gradient, cmd.commands);
    }

    if (flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Transform)) {
        sdata->geometry->draw(RenderUpdateFlag::Color, cmd.commands);
    }

    if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
        sdata->geometry->draw(RenderUpdateFlag::Stroke, cmd.commands);
    }

    mDrawCommands.emplace_back(cmd);

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
        sdata->geometry->tessellate(rshape, opacity, RenderUpdateFlag::Gradient, &context);
    } else {
        sdata->geometry->tessellate(rshape, opacity, RenderUpdateFlag::Color, &context);
    }

    sdata->geometry->tessellate(rshape, opacity, RenderUpdateFlag::Stroke, &context);

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
      mUniformBuffer(GlGpuBuffer::Target::UNIFORM_BUFFER),
      mBlitVertexBuffer(GlGpuBuffer::Target::ARRAY_BUFFER),
      mBlitIndexBuffer(GlGpuBuffer::Target::ELEMENT_ARRAY_BUFFER),
      mBlitUniformBuffer(GlGpuBuffer::Target::UNIFORM_BUFFER),
      mShaders(),
      mDrawCommands(),
      mFboStack()
{
}

GlRenderer::~GlRenderer()
{
    mShaders.clear();

    clearCompositors();

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
    // masking Renderer
    {
        std::string vs_shader(masking_vert, masking_vert_size);
        std::string fs_shader(masking_frag, masking_frag_size);
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

void GlRenderer::clearCompositors()
{
    for (uint32_t i = 0; i < mCompositors.count; i++) {
        delete mCompositors.data[i];
    }

    mCompositors.clear();
}

void GlRenderer::prepareBlitCMD(GlCompositor* cmp)
{
    if (mBlitGeometry == nullptr) {
        mBlitGeometry.reset(new GlGeometry);
    }

    // TODO we can reuse blit geometry mesh
    Array<float>    vertices{};
    Array<uint32_t> indices{};

    vertices.reserve(4 * 4);
    indices.reserve(6);

    // p1
    vertices.push(-1.f);
    vertices.push(1.f);
    vertices.push(0.f);
    vertices.push(1.f);
    // p2
    vertices.push(-1.f);
    vertices.push(-1.f);
    vertices.push(0.f);
    vertices.push(0.f);
    // p3
    vertices.push(1.f);
    vertices.push(-1.f);
    vertices.push(1.f);
    vertices.push(0.f);
    // p4
    vertices.push(1.f);
    vertices.push(1.f);
    vertices.push(1.f);
    vertices.push(1.f);

    indices.push(0);
    indices.push(1);
    indices.push(3);

    indices.push(3);
    indices.push(1);
    indices.push(2);

    GlCommand cmd;

    cmd.vertexBuffer = mBlitVertexBuffer.push(vertices.data, vertices.count * sizeof(float));
    cmd.indexBuffer = mBlitIndexBuffer.push(indices.data, indices.count * sizeof(uint32_t));

    cmd.shader = mShaders[PipelineType::kMasking].get();

    // layout
    cmd.vertexLayouts.emplace_back(VertexLayout{0, 2, 4 * sizeof(float), 0});
    cmd.vertexLayouts.emplace_back(VertexLayout{1, 2, 4 * sizeof(float), 2 * sizeof(float)});
    // bindings
    // textures
    cmd.bindings.emplace_back(BindingResource(0, cmp->targetTex(), cmd.shader->getUniformLocation("uDstTexture")));
    cmd.bindings.emplace_back(BindingResource(1, cmp->sourceTex(), cmd.shader->getUniformLocation("uSrcTexture")));
    // infos
    {
        int32_t infos[4];
        infos[0] = static_cast<int32_t>(cmp->method);
        infos[1] = cmp->opacity;
        infos[2] = 0;
        infos[3] = 0;

        auto bufferView = mBlitUniformBuffer.push(infos, 4 * sizeof(int32_t));

        cmd.bindings.emplace_back(
            BindingResource(0, cmd.shader->getUniformBlockIndex("MaskInfo"), bufferView, 4 * sizeof(int32_t)));
    }

    cmd.drawCount = 6;
    cmd.drawStart = 0;

    GlRenderCommand blitCmd;

    blitCmd.fboId = mCurrentFbo;
    blitCmd.geometry = mBlitGeometry.get();
    blitCmd.viewPort.x = 0;
    blitCmd.viewPort.y = 0;
    blitCmd.viewPort.w = static_cast<int32_t>(surface.w);
    blitCmd.viewPort.h = static_cast<int32_t>(surface.h);

    blitCmd.commands.emplace_back(cmd);

    mDrawCommands.emplace_back(blitCmd);
}
