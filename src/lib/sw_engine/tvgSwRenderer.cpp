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
#include "tvgTaskScheduler.h"
#include "tvgSwRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
static bool initEngine = false;
static uint32_t rendererCnt = 0;

struct SwTask : Task
{
    SwShape shape;
    const Shape* sdata = nullptr;
    const Shape* compData = nullptr;
    CompMethod compMethod = CompMethod::None;
    Matrix* transform = nullptr;
    Matrix* compTransform = nullptr;
    SwSurface* surface = nullptr;
    RenderUpdateFlag flags = RenderUpdateFlag::None;

    void run() override
    {
        //Valid Stroking?
        uint8_t strokeAlpha = 0;
        auto strokeWidth = sdata->strokeWidth();
        if (HALF_STROKE(strokeWidth) > 0) {
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

        //Composite clip-path
        if(compData && compMethod == CompMethod::ClipPath) {
            SwShape compShape;
            if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform)) {
                shapeReset(&compShape);

                //Make composite rle
                if (!shapePrepare(&compShape, compData, clip, compTransform)) return;
                if (!shapeGenRle(&compShape, compData, clip, true)) return;

                //Clip to fill(path) rle
                if (shape.rle && compShape.rect) rleClipRect(shape.rle, &compShape.bbox);
                else if (shape.rle && compShape.rle) rleClipPath(shape.rle, compShape.rle);

                //Clip to stroke rle
                if (shape.strokeRle && compShape.rect) rleClipRect(shape.strokeRle, &compShape.bbox);
                else if (shape.strokeRle && compShape.rle) rleClipPath(shape.strokeRle, compShape.rle);
            }
        }
        shapeDelOutline(&shape);
    }
};

static void _termEngine()
{
    if (rendererCnt > 0) return;

    //TODO: Clean up global resources
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwRenderer::~SwRenderer()
{
    clear();

    if (surface) delete(surface);

    --rendererCnt;
    if (!initEngine) _termEngine();
}


bool SwRenderer::clear()
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
    return rasterClear(surface);
}


bool SwRenderer::postRender()
{
    tasks.clear();

    return true;
}


bool SwRenderer::render(const Shape& shape, void *data)
{
    auto task = static_cast<SwTask*>(data);
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

    return true;
}


bool SwRenderer::dispose(TVG_UNUSED const Shape& sdata, void *data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return true;

    task->get();
    shapeFree(&task->shape);
    if (task->transform) free(task->transform);
    if (task->compTransform) free(task->compTransform);
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
    task->compData = static_cast<Shape*>(task->sdata->composite());
    task->compMethod = task->sdata->compositeMethod();

    if (transform) {
        if (!task->transform) task->transform = static_cast<Matrix*>(malloc(sizeof(Matrix)));
        *task->transform = transform->m;
    } else {
        if (task->transform) free(task->transform);
        task->transform = nullptr;
    }

    if (task->compData) {
        if (!task->compTransform) task->compTransform = static_cast<Matrix*>(malloc(sizeof(Matrix)));
        *task->compTransform = ((Shape*)task->compData)->transform();
    } else {
        if (task->compTransform) free(task->compTransform);
        task->compTransform = nullptr;
    }

    task->surface = surface;
    task->flags = flags;

    tasks.push_back(task);
    TaskScheduler::request(task);

    return task;
}


bool SwRenderer::init()
{
    if (rendererCnt > 0) return false;
    if (initEngine) return true;

    //TODO:

    initEngine = true;

    return true;
}


bool SwRenderer::term()
{
    if (!initEngine) return true;

    initEngine = false;

   _termEngine();

    return true;
}

SwRenderer* SwRenderer::gen()
{
    ++rendererCnt;
    return new SwRenderer();
}
