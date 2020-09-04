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
#include "tvgSwCommon.h"
#include "tvgSwRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
static RenderInitializer renderInit;

struct SwTask : Task
{
    SwShape shape;
    const Shape* sdata = nullptr;
    Matrix* transform = nullptr;
    SwSurface* surface = nullptr;
    RenderUpdateFlag flags = RenderUpdateFlag::None;

    void run() override
    {
        //Valid Stroking?
        uint8_t strokeAlpha = 0;
        auto strokeWidth = sdata->strokeWidth();
        if (strokeWidth > FLT_EPSILON) {
            sdata->strokeColor(nullptr, nullptr, nullptr, &strokeAlpha);
        }

        SwSize clip = {static_cast<SwCoord>(surface->w), static_cast<SwCoord>(surface->h)};

        //Shape
        if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) {
            shapeReset(&shape);
            uint8_t alpha = 0;
            sdata->fill(nullptr, nullptr, nullptr, &alpha);
            bool renderShape = (alpha > 0 || sdata->fill());
            if (renderShape || strokeAlpha) {
                if (!shapePrepare(&shape, sdata, clip, transform)) return;
                if (renderShape) {
                    auto antiAlias = (strokeAlpha > 0 && strokeWidth >= 2) ? false : true;
                    if (!shapeGenRle(&shape, sdata, clip, antiAlias)) return;
                }
            }
        }
        //Fill
        if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
            auto fill = sdata->fill();
            if (fill) {
                auto ctable = (flags & RenderUpdateFlag::Gradient) ? true : false;
                if (ctable) shapeResetFill(&shape);
                if (!shapeGenFillColors(&shape, fill, transform, surface, ctable)) return;
            } else {
                shapeDelFill(&shape);
            }
        }
        //Stroke
        if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
            if (strokeAlpha > 0) {
                shapeResetStroke(&shape, sdata, transform);
                if (!shapeGenStrokeRle(&shape, sdata, transform, clip)) return;
            } else {
                shapeDelStroke(&shape);
            }
        }
        shapeDelOutline(&shape);
    }
};


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwRenderer::~SwRenderer()
{
    flush();
    if (surface) delete(surface);
}


bool SwRenderer::clear()
{
    for (auto task : tasks) task->get();
    tasks.clear();

    return flush();
}


bool SwRenderer::flush()
{
    for (auto task : tasks) task->get();
    tasks.clear();

    return true;
}


bool SwRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, uint32_t cs)
{
    if (!buffer || stride == 0 || w == 0 || h == 0) return false;

    if (!surface) {
        surface = new SwSurface;
        if (!surface) return false;
    }

    surface->buffer = buffer;
    surface->stride = stride;
    surface->w = w;
    surface->h = h;
    surface->cs = cs;

    return rasterCompositor(surface);
}


bool SwRenderer::preRender()
{
    rasterClear(surface);

    //before we start rendering, we should finish all preparing tasks
    for (auto task : tasks) {
        task->get();

        uint8_t r, g, b, a;
        if (auto fill = task->sdata->fill()) {
            rasterGradientShape(surface, &task->shape, fill->id());
        } else{
            task->sdata->fill(&r, &g, &b, &a);
            if (a > 0) rasterSolidShape(surface, &task->shape, r, g, b, a);
        }
        task->sdata->strokeColor(&r, &g, &b, &a);
        if (a > 0) rasterStroke(surface, &task->shape, r, g, b, a);
    }
    tasks.clear();

    return true;
}


bool SwRenderer::dispose(TVG_UNUSED const Shape& sdata, void *data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return true;

    task->get();
    shapeFree(&task->shape);
    if (task->transform) free(task->transform);
    delete(task);

    return true;
}


void* SwRenderer::prepare(const Shape& sdata, void* data, const RenderTransform* transform, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwTask*>(data);
    if (!task) {
        task = new SwTask;
        if (!task) return nullptr;
    }

    if (flags == RenderUpdateFlag::None || task->valid()) return task;

    task->sdata = &sdata;

    if (transform) {
        if (!task->transform) task->transform = static_cast<Matrix*>(malloc(sizeof(Matrix)));
        *task->transform = transform->m;
    } else {
        if (task->transform) free(task->transform);
        task->transform = nullptr;
    }

    task->surface = surface;
    task->flags = flags;

    tasks.push_back(task);
    TaskScheduler::request(task);

    return task;
}


int SwRenderer::init()
{
    return RenderInitializer::init(renderInit, new SwRenderer);
}


int SwRenderer::term()
{
    return RenderInitializer::term(renderInit);
}


uint32_t SwRenderer::unref()
{
    return RenderInitializer::unref(renderInit);
}


uint32_t SwRenderer::ref()
{
    return RenderInitializer::ref(renderInit);
}


SwRenderer* SwRenderer::inst()
{
    //We know renderer type, avoid dynamic_cast for performance.
    return static_cast<SwRenderer*>(RenderInitializer::inst(renderInit));
}
