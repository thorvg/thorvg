/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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

#ifndef _TVG_GL_RENDERER_CPP_
#define _TVG_GL_RENDERER_CPP_

#include "tvgGlShaderSrc.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlGeometry.h"
#include "tvgGlCommon.h"
#include "tvgGlRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static RenderInitializer renderInit;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

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


bool GlRenderer::flush()
{
    GL_CHECK(glFinish());
    mColorProgram->unload();

    return true;
}


bool GlRenderer::preRender()
{
    // Blend function for pre multiplied alpha
    GL_CHECK(glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));
    return true;
}


bool GlRenderer::postRender()
{
    //TODO: called just after render()

    return true;
}


bool GlRenderer::render(const Shape& shape, void *data)
{
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    uint8_t r, g, b, a;
    size_t flags = static_cast<size_t>(sdata->updateFlag);

    GL_CHECK(glViewport(0, 0, sdata->viewWd, sdata->viewHt));

    uint32_t geometryCnt = sdata->geometry->getPrimitiveCount();
    for (uint32_t i = 0; i < geometryCnt; ++i)
    {
        mColorProgram->load();
        if (flags & RenderUpdateFlag::Color)
        {
            shape.fill(&r, &g, &b, &a);
            drawPrimitive(*(sdata->geometry), (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f, i, RenderUpdateFlag::Color);
        }
        if (flags & RenderUpdateFlag::Stroke)
        {
            shape.strokeColor(&r, &g, &b, &a);
            drawPrimitive(*(sdata->geometry), (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f, i, RenderUpdateFlag::Stroke);
        }
    }

    return true;
}


bool GlRenderer::dispose(TVG_UNUSED const Shape& shape, void *data)
{
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    delete sdata;
    return true;
}


void* GlRenderer::prepare(const Shape& shape, void* data, TVG_UNUSED const RenderTransform* transform, RenderUpdateFlag flags)
{
    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata)
    {
        sdata = new GlShape;
        assert(sdata);
    }
    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = flags;

    if (sdata->updateFlag == RenderUpdateFlag::None) return nullptr;

    initShaders();

    sdata->geometry = make_unique<GlGeometry>();

    //invisible?
    uint8_t alphaF, alphaS;
    shape.fill(nullptr, nullptr, nullptr, &alphaF);
    shape.strokeColor(nullptr, nullptr, nullptr, &alphaS);
    auto strokeWd = shape.strokeWidth();

    if (alphaF == 0 && alphaS == 0) return sdata;

    if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke) )
    {
        if (!sdata->geometry->decomposeOutline(shape)) return sdata;
        if (!sdata->geometry->generateAAPoints(shape, static_cast<float>(strokeWd), sdata->updateFlag)) return sdata;
        if (!sdata->geometry->tesselate(shape, sdata->viewWd, sdata->viewHt, sdata->updateFlag)) return sdata;
    }
    return sdata;
}


int GlRenderer::init()
{
    return RenderInitializer::init(renderInit, new GlRenderer);
}


int GlRenderer::term()
{
    if (inst()->mColorProgram.get())
    {
        inst()->mColorProgram.reset(nullptr);
    }
    return RenderInitializer::term(renderInit);
}


uint32_t GlRenderer::unref()
{
    return RenderInitializer::unref(renderInit);
}


uint32_t GlRenderer::ref()
{
    return RenderInitializer::ref(renderInit);
}


GlRenderer* GlRenderer::inst()
{
    //We know renderer type, avoid dynamic_cast for performance.
    return static_cast<GlRenderer*>(RenderInitializer::inst(renderInit));
}


void GlRenderer::initShaders()
{
    if (!mColorProgram.get())
    {
        shared_ptr<GlShader> shader = GlShader::gen(COLOR_VERT_SHADER, COLOR_FRAG_SHADER);
        mColorProgram = GlProgram::gen(shader);
    }
    mColorProgram->load();
    mColorUniformLoc = mColorProgram->getUniformLocation("uColor");
    mVertexAttrLoc = mColorProgram->getAttributeLocation("aLocation");
}


void GlRenderer::drawPrimitive(GlGeometry& geometry, float r, float g, float b, float a, uint32_t primitiveIndex, RenderUpdateFlag flag)
{
    mColorProgram->setUniformValue(mColorUniformLoc, r, g, b, a);
    geometry.draw(mVertexAttrLoc, primitiveIndex, flag);
    geometry.disableVertex(mVertexAttrLoc);

}

#endif /* _TVG_GL_RENDERER_CPP_ */
