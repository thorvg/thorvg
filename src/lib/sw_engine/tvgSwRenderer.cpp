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

enum class SwRenderType { Shape, Image };

struct SwTask : Task
{
    SwRenderType type;

    Matrix* transform = nullptr;
    SwSurface* surface = nullptr;
    RenderUpdateFlag flags = RenderUpdateFlag::None;
    vector<Composite> compList;
    uint32_t opacity;

    virtual bool dispose() = 0;
};


struct SwShapeTask : SwTask
{
    SwShape shape;
    const Shape* sdata = nullptr;

    void prepareTask(unsigned tid)
    {
        if (opacity == 0) return;  //Invisible

        //Valid Stroking?
        uint8_t strokeAlpha = 0;
        auto strokeWidth = sdata->strokeWidth();
        if (HALF_STROKE(strokeWidth) > 0) {
            sdata->strokeColor(nullptr, nullptr, nullptr, &strokeAlpha);
        }

        SwSize clip = {static_cast<SwCoord>(surface->w), static_cast<SwCoord>(surface->h)};

        //Invisiable shape turned to visible by alpha.
        auto prepareShape = false;
        if (!shapePrepared(&shape) && ((flags & RenderUpdateFlag::Color) || (opacity > 0))) prepareShape = true;

        //Shape
        if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform) || prepareShape) {
            uint8_t alpha = 0;
            sdata->fillColor(nullptr, nullptr, nullptr, &alpha);
            alpha = static_cast<uint8_t>(static_cast<uint32_t>(alpha) * opacity / 255);
            bool renderShape = (alpha > 0 || sdata->fill());
            if (renderShape || strokeAlpha) {
                shapeReset(&shape);
                if (!shapePrepare(&shape, sdata, tid, clip, transform)) goto end;
                if (renderShape) {
                    /* We assume that if stroke width is bigger than 2,
                       shape outline below stroke could be full covered by stroke drawing.
                       Thus it turns off antialising in that condition. */
                    auto antiAlias = (strokeAlpha > 0 && strokeWidth > 2) ? false : true;
                    if (!shapeGenRle(&shape, sdata, clip, antiAlias, compList.size() > 0 ? true : false)) goto end;
                }
            }
        }

        //Fill
        if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform)) {
            auto fill = sdata->fill();
            if (fill) {
                auto ctable = (flags & RenderUpdateFlag::Gradient) ? true : false;
                if (ctable) shapeResetFill(&shape);
                if (!shapeGenFillColors(&shape, fill, transform, surface, ctable)) goto end;
            } else {
                shapeDelFill(&shape);
            }
        }
        //Stroke
        if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
            if (strokeAlpha > 0) {
                shapeResetStroke(&shape, sdata, transform);
                if (!shapeGenStrokeRle(&shape, sdata, tid, transform, clip)) goto end;
            } else {
                shapeDelStroke(&shape);
            }
        }

        //Composition
        for (auto comp : compList) {
             SwShape *compShape = &static_cast<SwShapeTask*>(comp.edata)->shape;
             if (comp.method == CompositeMethod::ClipPath) {
                  //Clip to fill(path) rle
                  if (shape.rle && compShape->rect) rleClipRect(shape.rle, &compShape->bbox);
                  else if (shape.rle && compShape->rle) rleClipPath(shape.rle, compShape->rle);

                  //Clip to stroke rle
                  if (shape.strokeRle && compShape->rect) rleClipRect(shape.strokeRle, &compShape->bbox);
                  else if (shape.strokeRle && compShape->rle) rleClipPath(shape.strokeRle, compShape->rle);
             }
        }
    end:
        shapeDelOutline(&shape, tid);
    }

    void run(unsigned tid) override
    {
       prepareTask(tid);
    }

    bool dispose() override
    {
       shapeFree(&shape);
       return true;
    }
};


struct SwImageTask : SwTask
{
    SwImage image;
    const Picture* pdata = nullptr;
    uint32_t *pixels = nullptr;

    void prepareTask(unsigned tid)
    {
        SwSize clip = {static_cast<SwCoord>(surface->w), static_cast<SwCoord>(surface->h)};

        //Invisiable shape turned to visible by alpha.
        auto prepareImage = false;
        if (!imagePrepared(&image) && ((flags & RenderUpdateFlag::Image) || (opacity > 0))) prepareImage = true;

        if (prepareImage) {
            imageReset(&image);
            if (!imagePrepare(&image, pdata, tid, clip, transform)) goto end;

            if (compList.size() > 0) {
                if (!imageGenRle(&image, pdata, clip, false, true)) goto end;

                //Composition
                for (auto comp : compList) {
                     SwShape *compShape = &static_cast<SwShapeTask*>(comp.edata)->shape;
                     if (comp.method == CompositeMethod::ClipPath) {
                          //Clip to fill(path) rle
                          if (image.rle && compShape->rect) rleClipRect(image.rle, &compShape->bbox);
                          else if (image.rle && compShape->rle) rleClipPath(image.rle, compShape->rle);
                     }
                }
            }
        }

        if (this->pixels) image.data = this->pixels;
    end:
        imageDelOutline(&image, tid);
    }

    void run(unsigned tid) override
    {
        prepareTask(tid);
    }

    bool dispose() override
    {
       imageFree(&image);
       return true;
    }
};


static void _termEngine()
{
    if (rendererCnt > 0) return;

    mpoolTerm();
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
    for (auto task : tasks) task->done();
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


bool SwRenderer::render(TVG_UNUSED const Picture& picture, void *data)
{
    auto task = static_cast<SwImageTask*>(data);
    task->done();

    return rasterImage(surface, &task->image, task->opacity, task->transform);
}

bool SwRenderer::render(TVG_UNUSED const Shape& shape, void *data)
{
    auto task = static_cast<SwShapeTask*>(data);
    task->done();

    uint8_t r, g, b, a;
    if (auto fill = task->sdata->fill()) {
        //FIXME: pass opacity to apply gradient fill?
        rasterGradientShape(surface, &task->shape, fill->id());
    } else{
        task->sdata->fillColor(&r, &g, &b, &a);
        a = static_cast<uint8_t>((task->opacity * (uint32_t) a) / 255);
        if (a > 0) rasterSolidShape(surface, &task->shape, r, g, b, a);
    }
    task->sdata->strokeColor(&r, &g, &b, &a);
    a = static_cast<uint8_t>((task->opacity * (uint32_t) a) / 255);
    if (a > 0) rasterStroke(surface, &task->shape, r, g, b, a);

    return true;
}


bool SwRenderer::dispose(void *data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return true;

    task->done();
    task->dispose();
    if (task->transform) free(task->transform);
    delete(task);

    return true;
}


void SwRenderer::prepareCommon(SwTask* task, const RenderTransform* transform, uint32_t opacity, vector<Composite>& compList, RenderUpdateFlag flags)
{
    if (compList.size() > 0) {
        //Guarantee composition targets get ready.
        for (auto comp : compList)  static_cast<SwShapeTask*>(comp.edata)->done();
        task->compList.assign(compList.begin(), compList.end());
    }

    if (transform) {
        if (!task->transform) task->transform = static_cast<Matrix*>(malloc(sizeof(Matrix)));
        *task->transform = transform->m;
    } else {
        if (task->transform) free(task->transform);
        task->transform = nullptr;
    }

    task->opacity = opacity;
    task->surface = surface;
    task->flags = flags;

    tasks.push_back(task);
    TaskScheduler::request(task);
}


void* SwRenderer::prepare(const Picture& pdata, void* data, uint32_t *pixels, const RenderTransform* transform, uint32_t opacity, vector<Composite>& compList, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwImageTask*>(data);
    if (!task) {
        task = new SwImageTask;
        if (!task) return nullptr;
    }

    if (flags == RenderUpdateFlag::None) return task;

    //Finish previous task if it has duplicated request.
    task->done();

    task->pdata = &pdata;
    task->pixels = pixels;

    prepareCommon(task, transform, opacity, compList, flags);

    return task;
}


void* SwRenderer::prepare(const Shape& sdata, void* data, const RenderTransform* transform, uint32_t opacity, vector<Composite>& compList, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwShapeTask*>(data);
    if (!task) {
        task = new SwShapeTask;
        if (!task) return nullptr;
    }

    if (flags == RenderUpdateFlag::None) return task;

    //Finish previous task if it has duplicated request.
    task->done();
    task->sdata = &sdata;

    prepareCommon(task, transform, opacity, compList, flags);

    return task;
}


bool SwRenderer::init(uint32_t threads)
{
    if (rendererCnt > 0) return false;
    if (initEngine) return true;

    if (!mpoolInit(threads)) return false;

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
