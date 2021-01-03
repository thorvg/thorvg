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

#include "tvgGlRenderer.h"
#include "tvgGlGpuBuffer.h"
#include "tvgGlGeometry.h"
#include "tvgGlPropertyInterface.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
static bool initEngine = false;
static uint32_t rendererCnt = 0;


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
    GL_CHECK(glFinish());
    GlRenderTask::unload();
    return true;
}


bool GlRenderer::region(TVG_UNUSED RenderData data, TVG_UNUSED uint32_t* x, TVG_UNUSED uint32_t* y, TVG_UNUSED uint32_t* w, TVG_UNUSED uint32_t* h)
{
    return true;
}


bool GlRenderer::preRender()
{
    if (mRenderTasks.size() == 0)
    {
        initShaders();
    }
    GlRenderTask::unload();

    // Blend function for straight alpha
    GL_CHECK(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GL_CHECK(glEnable(GL_BLEND));
    return true;
}


bool GlRenderer::postRender()
{
    //TODO: called just after render()

    return true;
}


Compositor* GlRenderer::target(TVG_UNUSED uint32_t x, TVG_UNUSED uint32_t y, TVG_UNUSED uint32_t w, TVG_UNUSED uint32_t h)
{
    //TODO: Prepare frameBuffer & Setup render target for composition
    return nullptr;
}


bool GlRenderer::beginComposite(TVG_UNUSED Compositor* cmp, CompositeMethod method, uint32_t opacity)
{
    //TODO: delete the given compositor and restore the context
    return false;
}


bool GlRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
    //TODO: delete the given compositor and restore the context
    return false;
}


bool GlRenderer::renderImage(TVG_UNUSED void* data)
{
    return false;
}


bool GlRenderer::renderShape(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    uint8_t r, g, b, a;
    size_t flags = static_cast<size_t>(sdata->updateFlag);

    GL_CHECK(glViewport(0, 0, sdata->viewWd, sdata->viewHt));

    uint32_t primitiveCount = sdata->geometry->getPrimitiveCount();
    for (uint32_t i = 0; i < primitiveCount; ++i)
    {
        if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform))
        {
            const Fill* gradient = sdata->shape->fill();
            if (gradient != nullptr)
            {
		drawPrimitive(*sdata, gradient, i, RenderUpdateFlag::Gradient);
	    }
        }

        if(flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Transform))
        {
            sdata->shape->fillColor(&r, &g, &b, &a);
            if (a > 0)
            {
                drawPrimitive(*sdata, r, g, b, a, i, RenderUpdateFlag::Color);
            }
        }

        if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform))
        {
            sdata->shape->strokeColor(&r, &g, &b, &a);
            if (a > 0)
            {
                drawPrimitive(*sdata, r, g, b, a, i, RenderUpdateFlag::Stroke);
            }
        }
    }

    return true;
}


bool GlRenderer::dispose(RenderData data)
{
    auto sdata = static_cast<GlShape*>(data);
    if (!sdata) return false;

    delete sdata;
    return true;
}


RenderData GlRenderer::prepare(TVG_UNUSED const Picture& picture, TVG_UNUSED RenderData data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED uint32_t opacity, TVG_UNUSED Array<RenderData>& clips, TVG_UNUSED RenderUpdateFlag flags)
{
    //TODO:
    return nullptr;
}


RenderData GlRenderer::prepare(const Shape& shape, RenderData data, const RenderTransform* transform, TVG_UNUSED uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flags)
{
    //prepare shape data
    GlShape* sdata = static_cast<GlShape*>(data);
    if (!sdata) {
        sdata = new GlShape;
        if (!sdata) return nullptr;
        sdata->shape = &shape;
    }

    sdata->viewWd = static_cast<float>(surface.w);
    sdata->viewHt = static_cast<float>(surface.h);
    sdata->updateFlag = flags;

    if (sdata->updateFlag == RenderUpdateFlag::None) return sdata;

    sdata->geometry = make_unique<GlGeometry>();

    //invisible?
    uint8_t alphaF, alphaS;
    shape.fillColor(nullptr, nullptr, nullptr, &alphaF);
    shape.strokeColor(nullptr, nullptr, nullptr, &alphaS);
    auto strokeWd = shape.strokeWidth();

    if ( ((sdata->updateFlag & RenderUpdateFlag::Gradient) == 0) &&
         ((sdata->updateFlag & RenderUpdateFlag::Color) && alphaF == 0) &&
         ((sdata->updateFlag & RenderUpdateFlag::Stroke) && alphaS == 0) )
    {
        return sdata;
    }

    sdata->geometry->updateTransform(transform, sdata->viewWd, sdata->viewHt);

    if (sdata->updateFlag & (RenderUpdateFlag::Color | RenderUpdateFlag::Stroke | RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform) )
    {
        if (!sdata->geometry->decomposeOutline(shape)) return sdata;
        if (!sdata->geometry->generateAAPoints(shape, static_cast<float>(strokeWd), sdata->updateFlag)) return sdata;
        if (!sdata->geometry->tesselate(shape, sdata->viewWd, sdata->viewHt, sdata->updateFlag)) return sdata;
    }
    return sdata;
}


int GlRenderer::init(uint32_t threads)
{
    if (rendererCnt > 0) return false;
    if (initEngine) return true;

    //TODO:

    initEngine = true;

    return true;
}


int GlRenderer::term()
{
    if (!initEngine) return true;

    initEngine = false;

   _termEngine();

    return true;
}


GlRenderer* GlRenderer::gen()
{
    return new GlRenderer();
}


GlRenderer::~GlRenderer()
{
    mRenderTasks.clear();

    --rendererCnt;
    if (!initEngine) _termEngine();
}


void GlRenderer::initShaders()
{
    // Solid Color Renderer
    mRenderTasks.push_back(GlColorRenderTask::gen());

    // Linear Gradient Renderer
    mRenderTasks.push_back(GlLinearGradientRenderTask::gen());

    // Radial Gradient Renderer
    mRenderTasks.push_back(GlRadialGradientRenderTask::gen());
}


void GlRenderer::drawPrimitive(GlShape& sdata, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t primitiveIndex, RenderUpdateFlag flag)
{
    GlColorRenderTask* renderTask = static_cast<GlColorRenderTask*>(mRenderTasks[GlRenderTask::RenderTypes::RT_Color].get());
    assert(renderTask);
    renderTask->load();
    float* matrix = sdata.geometry->getTransforMatrix();
    PropertyInterface::clearData(renderTask);
    renderTask->setColor(r, g, b, a);
    renderTask->setTransform(FORMAT_SIZE_MAT_4x4, matrix);
    int32_t vertexLoc = renderTask->getLocationPropertyId();
    renderTask->uploadValues();
    sdata.geometry->draw(vertexLoc, primitiveIndex, flag);
    sdata.geometry->disableVertex(vertexLoc);

}


void GlRenderer::drawPrimitive(GlShape& sdata, const Fill* fill, uint32_t primitiveIndex, RenderUpdateFlag flag)
{
    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = fill->colorStops(&stops);
    if (stopCnt < 2)
    {
        return;
    }
    GlGradientRenderTask* rTask = nullptr;
    GlSize size = sdata.geometry->getPrimitiveSize(primitiveIndex);
    float* matrix = sdata.geometry->getTransforMatrix();

    switch (fill->id()) {
        case FILL_ID_LINEAR: {
            float x1, y1, x2, y2;
            GlLinearGradientRenderTask *renderTask = static_cast<GlLinearGradientRenderTask*>(mRenderTasks[GlRenderTask::RenderTypes::RT_LinGradient].get());
            assert(renderTask);
            rTask = renderTask;
            renderTask->load();
            PropertyInterface::clearData(renderTask);
            const LinearGradient* grad = static_cast<const LinearGradient*>(fill);
            grad->linear(&x1, &y1, &x2, &y2);
            renderTask->setStartPosition(x1, y1);
            renderTask->setEndPosition(x2, y2);
            break;
        }
        case FILL_ID_RADIAL: {
            float x1, y1, r1;
            GlRadialGradientRenderTask *renderTask = static_cast<GlRadialGradientRenderTask*>(mRenderTasks[GlRenderTask::RenderTypes::RT_RadGradient].get());
            assert(renderTask);
            rTask = renderTask;
            renderTask->load();
            PropertyInterface::clearData(renderTask);
            const RadialGradient* grad = static_cast<const RadialGradient*>(fill);
            grad->radial(&x1, &y1, &r1);
            renderTask->setStartPosition(x1, y1);
            renderTask->setStartRadius(r1);
            break;
        }
    }
    if (rTask)
    {
        int32_t vertexLoc = rTask->getLocationPropertyId();
        rTask->setPrimitveSize(size.x, size.y);
        rTask->setCanvasSize(sdata.viewWd, sdata.viewHt);
        rTask->setNoise(NOISE_LEVEL);
        rTask->setStopCount((int)stopCnt);
        rTask->setTransform(FORMAT_SIZE_MAT_4x4, matrix);
        for (uint32_t i = 0; i < stopCnt; ++i)
        {
            rTask->setStopColor(i, stops[i].offset, stops[i].r, stops[i].g, stops[i].b, stops[i].a);
        }

        rTask->uploadValues();
        sdata.geometry->draw(vertexLoc, primitiveIndex, flag);
        sdata.geometry->disableVertex(vertexLoc);
    }
}

