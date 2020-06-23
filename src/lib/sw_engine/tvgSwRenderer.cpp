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

#include "tvgSwCommon.h"
#include "tvgSwRenderer.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static RenderInitializer renderInit;

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

bool SwRenderer::clear()
{
    return rasterClear(surface);
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


bool SwRenderer::render(const Shape& sdata, void *data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return false;

    if (task->prepared.valid()) task->prepared.get();

    uint8_t r, g, b, a;

    if (auto fill = sdata.fill()) {
        rasterGradientShape(surface, task->shape, fill->id());
    } else {
        sdata.fill(&r, &g, &b, &a);
        if (a > 0) rasterSolidShape(surface, task->shape, r, g, b, a);
    }

    sdata.strokeColor(&r, &g, &b, &a);
    if (a > 0) rasterStroke(surface, task->shape, r, g, b, a);

    return true;
}


bool SwRenderer::dispose(const Shape& sdata, void *data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return true;
    if (task->prepared.valid()) task->prepared.wait();
    shapeFree(task->shape);
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

    if (flags == RenderUpdateFlag::None || task->prepared.valid()) return task;

    task->sdata = &sdata;
    task->clip = {static_cast<SwCoord>(surface.w), static_cast<SwCoord>(surface.h)};

    if (transform) task->transform = &transform->m;
    else task->transform = nullptr;

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

    task->prepared = async((launch::async | launch::deferred), asyncTask, task);

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
