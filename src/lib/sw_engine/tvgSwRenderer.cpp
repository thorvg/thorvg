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
#ifndef _TVG_SW_RENDERER_CPP_
#define _TVG_SW_RENDERER_CPP_

using namespace std;

#include "tvgSwCommon.h"
#include "tvgSwRenderer.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
namespace tvg {
    struct SwTask
    {
        SwShape shape;
        const Shape* sdata;
        SwSize clip;
        Matrix* transform;
        RenderUpdateFlag flags;
        future<void> progress;
    };
}

static RenderInitializer renderInit;


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwRenderer::~SwRenderer()
{
    flush();
}


bool SwRenderer::clear()
{
    return flush();
}


bool SwRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h)
{
    if (!buffer || stride == 0 || w == 0 || h == 0) return false;

    surface.buffer = buffer;
    surface.stride = stride;
    surface.w = w;
    surface.h = h;

    return true;
}


bool SwRenderer::preRender()
{
    //before we start rendering, we should finish all preparing tasks
    while (prepareTasks.size() > 0) {
        auto task = prepareTasks.front();
        if (task->progress.valid()) task->progress.get();
        prepareTasks.pop();
        renderTasks.push(task);
    }
    return true;
}


bool SwRenderer::postRender()
{
    auto asyncTask = [](SwRenderer* renderer) {
        renderer->doRender();
    };

    progress = async(launch::async, asyncTask, this);

    return true;
}


void SwRenderer::doRender()
{
    rasterClear(surface);

    while (renderTasks.size() > 0) {
        auto task = renderTasks.front();
        uint8_t r, g, b, a;
        if (auto fill = task->sdata->fill()) {
            rasterGradientShape(surface, task->shape, fill->id());
        } else{
            task->sdata->fill(&r, &g, &b, &a);
            if (a > 0) rasterSolidShape(surface, task->shape, r, g, b, a);
        }
        task->sdata->strokeColor(&r, &g, &b, &a);
       if (a > 0) rasterStroke(surface, task->shape, r, g, b, a);
        renderTasks.pop();
    }
}


bool SwRenderer::flush()
{
    while (prepareTasks.size() > 0) {
        auto task = prepareTasks.front();
        if (task->progress.valid()) task->progress.get();
        prepareTasks.pop();
    }

    if (progress.valid()) progress.get();

    return true;
}


bool SwRenderer::render(TVG_UNUSED const Shape& sdata, TVG_UNUSED void *data)
{
    //Do Nothing

    return true;
}


bool SwRenderer::dispose(TVG_UNUSED const Shape& sdata, void *data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return true;
    if (task->progress.valid()) task->progress.get();
    shapeFree(task->shape);
    if (task->transform) free(task->transform);
    free(task);
    return true;
}


void* SwRenderer::prepare(const Shape& sdata, void* data, const RenderTransform* transform, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwTask*>(data);
    if (!task) {
        task = static_cast<SwTask*>(calloc(1, sizeof(SwTask)));
        if (!task) return nullptr;
    }

    if (flags == RenderUpdateFlag::None || task->progress.valid()) return task;

    task->sdata = &sdata;
    task->clip = {static_cast<SwCoord>(surface.w), static_cast<SwCoord>(surface.h)};

    if (transform) {
        if (!task->transform) task->transform = static_cast<Matrix*>(malloc(sizeof(Matrix)));
        assert(task->transform);
        *task->transform = transform->m;
    } else {
        if (task->transform) free(task->transform);
        task->transform = nullptr;
    }

    task->flags = flags;

    auto asyncTask = [](SwTask* task) {

        //Valid Stroking?
        uint8_t strokeAlpha = 0;
        auto strokeWidth = task->sdata->strokeWidth();
        if (strokeWidth > FLT_EPSILON) {
            task->sdata->strokeColor(nullptr, nullptr, nullptr, &strokeAlpha);
        }

        //Shape
        if (task->flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) {
            shapeReset(task->shape);
            uint8_t alpha = 0;
            task->sdata->fill(nullptr, nullptr, nullptr, &alpha);
            bool renderShape = (alpha > 0 || task->sdata->fill());
            if (renderShape || strokeAlpha) {
                if (!shapePrepare(task->shape, task->sdata, task->clip, task->transform)) return;
                if (renderShape) {
                    auto antiAlias = (strokeAlpha > 0 && strokeWidth >= 2) ? false : true;
                    if (!shapeGenRle(task->shape, task->sdata, task->clip, antiAlias)) return;
                }
            }
        }
        //Fill
        if (task->flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
            auto fill = task->sdata->fill();
            if (fill) {
                auto ctable = (task->flags & RenderUpdateFlag::Gradient) ? true : false;
                if (ctable) shapeResetFill(task->shape);
                if (!shapeGenFillColors(task->shape, fill, task->transform, ctable)) return;
            } else {
                shapeDelFill(task->shape);
            }
        }
        //Stroke
        if (task->flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
            if (strokeAlpha > 0) {
                shapeResetStroke(task->shape, task->sdata, task->transform);
                if (!shapeGenStrokeRle(task->shape, task->sdata, task->transform, task->clip)) return;
            } else {
                shapeDelStroke(task->shape);
            }
        }
        shapeDelOutline(task->shape);
    };

    prepareTasks.push(task);
    task->progress = async((launch::async | launch::deferred), asyncTask, task);

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

#endif /* _TVG_SW_RENDERER_CPP_ */