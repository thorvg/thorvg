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
#include <math.h>
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
    Matrix* transform = nullptr;
    SwSurface* surface = nullptr;
    RenderUpdateFlag flags = RenderUpdateFlag::None;
    Array<RenderData> clips;
    uint32_t opacity;
    SwBBox bbox = {{0, 0}, {0, 0}};       //Whole Rendering Region

    void bounds(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h)
    {
        //Range over?
        auto xx = bbox.min.x > 0 ? bbox.min.x : 0;
        auto yy = bbox.min.y > 0 ? bbox.min.y : 0;

        if (x) *x = xx;
        if (y) *y = yy;
        if (w) *w = bbox.max.x - xx;
        if (h) *h = bbox.max.y - yy;
    }

    virtual bool dispose() = 0;
};


struct SwShapeTask : SwTask
{
    SwShape shape;
    const Shape* sdata = nullptr;
    bool cmpStroking;

    void run(unsigned tid) override
    {
        if (opacity == 0) return;  //Invisible

        /* Valid filling & stroking each increases the value by 1.
           This value is referenced for compositing shape & stroking. */
        uint32_t addStroking = 0;

        //Valid Stroking?
        uint8_t strokeAlpha = 0;
        auto strokeWidth = sdata->strokeWidth();
        if (HALF_STROKE(strokeWidth) > 0) {
            sdata->strokeColor(nullptr, nullptr, nullptr, &strokeAlpha);
        }

        SwSize clip = {static_cast<SwCoord>(surface->w), static_cast<SwCoord>(surface->h)};

        //invisible shape turned to visible by alpha.
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
                if (!shapePrepare(&shape, sdata, tid, clip, transform, bbox)) goto end;
                if (renderShape) {
                    /* We assume that if stroke width is bigger than 2,
                       shape outline below stroke could be full covered by stroke drawing.
                       Thus it turns off antialising in that condition. */
                    auto antiAlias = (strokeAlpha == 255 && strokeWidth > 2) ? false : true;
                    if (!shapeGenRle(&shape, sdata, clip, antiAlias, clips.count > 0 ? true : false)) goto end;
                    ++addStroking;
                }
            }
        }

        //Fill
        if (flags & (RenderUpdateFlag::Gradient | RenderUpdateFlag::Transform | RenderUpdateFlag::Color)) {
            auto fill = sdata->fill();
            if (fill) {
                auto ctable = (flags & RenderUpdateFlag::Gradient) ? true : false;
                if (ctable) shapeResetFill(&shape);
                if (!shapeGenFillColors(&shape, fill, transform, surface, opacity, ctable)) goto end;
                ++addStroking;
            } else {
                shapeDelFill(&shape);
            }
        }

        //Stroke
        if (flags & (RenderUpdateFlag::Stroke | RenderUpdateFlag::Transform)) {
            if (strokeAlpha > 0) {
                shapeResetStroke(&shape, sdata, transform);
                if (!shapeGenStrokeRle(&shape, sdata, tid, transform, clip, bbox)) goto end;
                ++addStroking;
            } else {
                shapeDelStroke(&shape);
            }
        }

        //Clip Path
        for (auto clip = clips.data; clip < (clips.data + clips.count); ++clip) {
            auto clipper = &static_cast<SwShapeTask*>(*clip)->shape;
            //Clip shape rle
            if (shape.rle) {
                if (clipper->rect) rleClipRect(shape.rle, &clipper->bbox);
                else if (clipper->rle) rleClipPath(shape.rle, clipper->rle);
            }
            //Clip stroke rle
            if (shape.strokeRle) {
                if (clipper->rect) rleClipRect(shape.strokeRle, &clipper->bbox);
                else if (clipper->rle) rleClipPath(shape.strokeRle, clipper->rle);
            }
        }
    end:
        shapeDelOutline(&shape, tid);
        if (addStroking > 1 && opacity < 255) cmpStroking = true;
        else cmpStroking = false;
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

    void run(unsigned tid) override
    {
        SwSize clip = {static_cast<SwCoord>(surface->w), static_cast<SwCoord>(surface->h)};

        //Invisible shape turned to visible by alpha.
        auto prepareImage = false;
        if (!imagePrepared(&image) && ((flags & RenderUpdateFlag::Image) || (opacity > 0))) prepareImage = true;

        if (prepareImage) {
            imageReset(&image);
            if (!imagePrepare(&image, pdata, tid, clip, transform, bbox)) goto end;

            //Clip Path?
            if (clips.count > 0) {
                if (!imageGenRle(&image, pdata, clip, bbox, false, true)) goto end;
                if (image.rle) {
                    for (auto clip = clips.data; clip < (clips.data + clips.count); ++clip) {
                        auto clipper = &static_cast<SwShapeTask*>(*clip)->shape;
                        if (clipper->rect) rleClipRect(image.rle, &clipper->bbox);
                        else if (clipper->rle) rleClipPath(image.rle, clipper->rle);
                    }
                }
            }
        }
        image.data = const_cast<uint32_t*>(pdata->data());
    end:
        imageDelOutline(&image, tid);
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
    for (auto task = tasks.data; task < (tasks.data + tasks.count); ++task) (*task)->done();
    tasks.clear();

    return true;
}


bool SwRenderer::sync()
{
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

    //Free Composite Caches
    for (auto comp = compositors.data; comp < (compositors.data + compositors.count); ++comp) {
        free((*comp)->compositor->image.data);
        delete((*comp)->compositor);
        delete(*comp);
    }
    compositors.reset();

    return true;
}


bool SwRenderer::renderImage(RenderData data)
{
    auto task = static_cast<SwImageTask*>(data);
    task->done();

    if (task->opacity == 0) return true;

    return rasterImage(surface, &task->image, task->transform, task->bbox, task->opacity);
}


bool SwRenderer::renderShape(RenderData data)
{
    auto task = static_cast<SwShapeTask*>(data);
    task->done();

    if (task->opacity == 0) return true;

    uint32_t opacity;
    Compositor* cmp = nullptr;

    //Do Stroking Composition
    if (task->cmpStroking) {
        uint32_t x, y, w, h;
        task->bounds(&x, &y, &w, &h);
        opacity = 255;
        cmp = target(x, y, w, h);
        beginComposite(cmp, CompositeMethod::None, task->opacity);
    //No Stroking Composition
    } else {
        opacity = task->opacity;
    }

    //Main raster stage
    uint8_t r, g, b, a;

    if (auto fill = task->sdata->fill()) {
        rasterGradientShape(surface, &task->shape, fill->id());
    } else{
        task->sdata->fillColor(&r, &g, &b, &a);
        a = static_cast<uint8_t>((opacity * (uint32_t) a) / 255);
        if (a > 0) rasterSolidShape(surface, &task->shape, r, g, b, a);
    }

    task->sdata->strokeColor(&r, &g, &b, &a);
    a = static_cast<uint8_t>((opacity * (uint32_t) a) / 255);
    if (a > 0) rasterStroke(surface, &task->shape, r, g, b, a);

    if (task->cmpStroking) endComposite(cmp);

    return true;
}

bool SwRenderer::region(RenderData data, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h)
{
    static_cast<SwTask*>(data)->bounds(x, y, w, h);

    return true;
}


bool SwRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint32_t opacity)
{
    if (!cmp) return false;
    auto p = static_cast<SwCompositor*>(cmp);

    p->method = method;
    p->opacity = opacity;

    //Current Context?
    if (p->method != CompositeMethod::None) {
        surface = p->recoverSfc;
        surface->compositor = p;
    }

    return true;
}


Compositor* SwRenderer::target(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    SwSurface* cmp = nullptr;

    //Use cached data
    for (auto p = compositors.data; p < (compositors.data + compositors.count); ++p) {
        if ((*p)->compositor->valid) {
            cmp = *p;
            break;
        }
    }

    //New Composition
    if (!cmp) {
        cmp = new SwSurface;
        if (!cmp) goto err;

        //Inherits attributes from main surface
        *cmp = *surface;

        cmp->compositor = new SwCompositor;
        if (!cmp->compositor) goto err;

        //SwImage, Optimize Me: Surface size from MainSurface(WxH) to Parameter W x H
        cmp->compositor->image.data = (uint32_t*) malloc(sizeof(uint32_t) * surface->w * surface->h);
        if (!cmp->compositor->image.data) goto err;
        compositors.push(cmp);
    }

    //Boundary Check
    if (x + w > surface->w) w = (surface->w - x);
    if (y + h > surface->h) h = (surface->h - y);

#ifdef THORVG_LOG_ENABLED
    printf("SW_ENGINE: Using intermediate composition [Region: %d %d %d %d]\n", x, y, w, h);
#endif

    cmp->compositor->recoverSfc = surface;
    cmp->compositor->recoverCmp = surface->compositor;
    cmp->compositor->valid = false;
    cmp->compositor->bbox.min.x = x;
    cmp->compositor->bbox.min.y = y;
    cmp->compositor->bbox.max.x = x + w;
    cmp->compositor->bbox.max.y = y + h;
    cmp->compositor->image.w = surface->w;
    cmp->compositor->image.h = surface->h;

    //We know partial clear region
    cmp->buffer = cmp->compositor->image.data + (cmp->stride * y + x);
    cmp->w = w;
    cmp->h = h;

    rasterClear(cmp);

    //Recover context
    cmp->buffer = cmp->compositor->image.data;
    cmp->w = cmp->compositor->image.w;
    cmp->h = cmp->compositor->image.h;

    //Switch render target
    surface = cmp;

    return cmp->compositor;

err:
    if (cmp) {
        if (cmp->compositor) delete(cmp->compositor);
        delete(cmp);
    }

    return nullptr;
}


bool SwRenderer::endComposite(Compositor* cmp)
{
    if (!cmp) return false;

    auto p = static_cast<SwCompositor*>(cmp);
    p->valid = true;

    //Recover Context
    surface = p->recoverSfc;
    surface->compositor = p->recoverCmp;

    //Default is alpha blending
    if (p->method == CompositeMethod::None) {
        return rasterImage(surface, &p->image, nullptr, p->bbox, p->opacity);
    }

    return true;
}


bool SwRenderer::dispose(RenderData data)
{
    auto task = static_cast<SwTask*>(data);
    if (!task) return true;

    task->done();
    task->dispose();
    if (task->transform) free(task->transform);
    delete(task);

    return true;
}


void* SwRenderer::prepareCommon(SwTask* task, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flags)
{
    if (flags == RenderUpdateFlag::None) return task;

    //Finish previous task if it has duplicated request.
    task->done();

    if (clips.count > 0) {
        //Guarantee composition targets get ready.
        for (auto clip = clips.data; clip < (clips.data + clips.count); ++clip) {
            static_cast<SwShapeTask*>(*clip)->done();
        }
        task->clips = clips;
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

    tasks.push(task);
    TaskScheduler::request(task);

    return task;
}


RenderData SwRenderer::prepare(const Picture& pdata, RenderData data, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwImageTask*>(data);
    if (!task) {
        task = new SwImageTask;
        if (!task) return nullptr;
        task->pdata = &pdata;
    }
    return prepareCommon(task, transform, opacity, clips, flags);
}


RenderData SwRenderer::prepare(const Shape& sdata, RenderData data, const RenderTransform* transform, uint32_t opacity, Array<RenderData>& clips, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwShapeTask*>(data);
    if (!task) {
        task = new SwShapeTask;
        if (!task) return nullptr;
        task->sdata = &sdata;
    }
    return prepareCommon(task, transform, opacity, clips, flags);
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
