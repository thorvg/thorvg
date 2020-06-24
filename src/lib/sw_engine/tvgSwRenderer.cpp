/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
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
    if (progress.valid()) progress.get();
}


bool SwRenderer::clear()
{
    if (progress.valid()) return false;
    return true;
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
    if (progress.valid()) {
        progress.get();
        return true;
    }
    return false;
}


bool SwRenderer::render(const Shape& sdata, void *data)
{
    //Do Nothing

    return true;
}


bool SwRenderer::dispose(const Shape& sdata, void *data)
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
        //Shape
        if (task->flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) {
            shapeReset(task->shape);
            uint8_t alpha = 0;
            task->sdata->fill(nullptr, nullptr, nullptr, &alpha);
            if (alpha > 0 || task->sdata->fill()) {
                if (!shapeGenRle(task->shape, task->sdata, task->clip, task->transform)) return;
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
            if (task->sdata->strokeWidth() > FLT_EPSILON) {
                shapeResetStroke(task->shape, task->sdata);
                uint8_t alpha = 0;
                task->sdata->strokeColor(nullptr, nullptr, nullptr, &alpha);
                if (alpha > 0) {
                    if (!shapeGenStrokeRle(task->shape, task->sdata, task->clip)) return;
                }
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
