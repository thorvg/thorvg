/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#ifdef THORVG_SW_OPENMP_SUPPORT
    #include <omp.h>
#endif
#include <algorithm>
#include <atomic>
#include "tvgSwCommon.h"
#include "tvgTaskScheduler.h"
#include "tvgSwRenderer.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
static atomic<int32_t> rendererCnt{-1};
static SwMpool* globalMpool = nullptr;
static uint32_t threadsCnt = 0;

struct SwTask : Task
{
    SwSurface* surface = nullptr;
    SwMpool* mpool = nullptr;
    SwBBox bbox;                          //Rendering Region
    Matrix transform;
    Array<RenderData> clips;
    RenderUpdateFlag flags = RenderUpdateFlag::None;
    uint8_t opacity;
    bool pushed = false;                  //Pushed into task list?
    bool disposed = false;                //Disposed task?

    RenderRegion bounds()
    {
        //Can we skip the synchronization?
        done();

        RenderRegion region;

        //Range over?
        region.x = bbox.min.x > 0 ? bbox.min.x : 0;
        region.y = bbox.min.y > 0 ? bbox.min.y : 0;
        region.w = bbox.max.x - region.x;
        region.h = bbox.max.y - region.y;
        if (region.w < 0) region.w = 0;
        if (region.h < 0) region.h = 0;

        return region;
    }

    virtual void dispose() = 0;
    virtual bool clip(SwRle* target) = 0;
    virtual ~SwTask() {}
};


struct SwShapeTask : SwTask
{
    SwShape shape;
    const RenderShape* rshape = nullptr;
    bool clipper = false;

    /* We assume that if the stroke width is greater than 2,
       the shape's outline beneath the stroke could be adequately covered by the stroke drawing.
       Therefore, antialiasing is disabled under this condition.
       Additionally, the stroke style should not be dashed. */
    bool antialiasing(float strokeWidth)
    {
        return strokeWidth < 2.0f || rshape->stroke->dash.count > 0 || rshape->stroke->first || rshape->trimpath() || rshape->stroke->color.a < 255;
    }

    float validStrokeWidth(bool clipper)
    {
        if (!rshape->stroke) return 0.0f;

        auto width = rshape->stroke->width;
        if (tvg::zero(width)) return 0.0f;

        if (!clipper && (!rshape->stroke->fill && (MULTIPLY(rshape->stroke->color.a, opacity) == 0))) return 0.0f;
        if (tvg::zero(rshape->stroke->trim.begin - rshape->stroke->trim.end)) return 0.0f;

        return (width * sqrt(transform.e11 * transform.e11 + transform.e12 * transform.e12));
    }

    bool clip(SwRle* target) override
    {
        if (shape.strokeRle) return rleClip(target, shape.strokeRle);
        if (shape.fastTrack) return rleClip(target, &bbox);
        if (shape.rle) return rleClip(target, shape.rle);
        return false;
    }

    void run(unsigned tid) override
    {
        //Invisible
        if (opacity == 0 && !clipper) {
            bbox.reset();
            return;
        }

        auto strokeWidth = validStrokeWidth(clipper);
        SwBBox renderRegion{};
        auto updateShape = flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Transform | RenderUpdateFlag::Clip);
        auto updateFill = false;

        //Shape
        if (updateShape || flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Gradient)) {
            updateFill = (MULTIPLY(rshape->color.a, opacity) || rshape->fill);
            if (updateShape) shapeReset(&shape);
            if (updateFill || clipper) {
                if (shapePrepare(&shape, rshape, transform, bbox, renderRegion, mpool, tid, clips.count > 0 ? true : false)) {
                    if (!shapeGenRle(&shape, rshape, antialiasing(strokeWidth))) goto err;
                } else {
                    updateFill = false;
                    renderRegion.reset();
                }
            }
        }
        //Fill
        if (updateFill) {
            if (auto fill = rshape->fill) {
                auto ctable = (flags & RenderUpdateFlag::Gradient) ? true : false;
                if (ctable) shapeResetFill(&shape);
                if (!shapeGenFillColors(&shape, fill, transform, surface, opacity, ctable)) goto err;
            }
        }
        //Stroke
        if (updateShape || flags & RenderUpdateFlag::Stroke) {
            if (strokeWidth > 0.0f) {
                shapeResetStroke(&shape, rshape, transform);
                if (!shapeGenStrokeRle(&shape, rshape, transform, bbox, renderRegion, mpool, tid)) goto err;
                if (auto fill = rshape->strokeFill()) {
                    auto ctable = (flags & RenderUpdateFlag::GradientStroke) ? true : false;
                    if (ctable) shapeResetStrokeFill(&shape);
                    if (!shapeGenStrokeFillColors(&shape, fill, transform, surface, opacity, ctable)) goto err;
                }
            } else {
                shapeDelStroke(&shape);
            }
        }

        //Clear current task memorypool here if the clippers would use the same memory pool
        shapeDelOutline(&shape, mpool, tid);

        //Clip Path
        ARRAY_FOREACH(p, clips) {
            auto clipper = static_cast<SwTask*>(*p);
            auto clipShapeRle = shape.rle ? clipper->clip(shape.rle) : true;
            auto clipStrokeRle = shape.strokeRle ? clipper->clip(shape.strokeRle) : true;
            if (!clipShapeRle && !clipStrokeRle) goto err;
        }

        bbox = renderRegion; //sync

        return;

    err:
        bbox.reset();
        shapeReset(&shape);
        rleReset(shape.strokeRle);
        shapeDelOutline(&shape, mpool, tid);
    }

    void dispose() override
    {
       shapeFree(&shape);
    }
};


struct SwImageTask : SwTask
{
    SwImage image;
    RenderSurface* source;                //Image source

    bool clip(SwRle* target) override
    {
        TVGERR("SW_ENGINE", "Image is used as ClipPath?");
        return true;
    }

    void run(unsigned tid) override
    {
        auto clipRegion = bbox;

        //Convert colorspace if it's not aligned.
        rasterConvertCS(source, surface->cs);
        rasterPremultiply(source);

        image.data = source->data;
        image.w = source->w;
        image.h = source->h;
        image.stride = source->stride;
        image.channelSize = source->channelSize;

        //Invisible shape turned to visible by alpha.
        if ((flags & (RenderUpdateFlag::Image | RenderUpdateFlag::Transform | RenderUpdateFlag::Color)) && (opacity > 0)) {
            imageReset(&image);
            if (!image.data || image.w == 0 || image.h == 0) goto end;

            if (!imagePrepare(&image, transform, clipRegion, bbox, mpool, tid)) goto end;

            if (clips.count > 0) {
                if (!imageGenRle(&image, bbox, false)) goto end;
                if (image.rle) {
                    //Clear current task memorypool here if the clippers would use the same memory pool
                    imageDelOutline(&image, mpool, tid);
                    ARRAY_FOREACH(p, clips) {
                        auto clipper = static_cast<SwTask*>(*p);
                        if (!clipper->clip(image.rle)) goto err;
                    }
                    return;
                }
            }
        }
        goto end;
    err:
        rleReset(image.rle);
    end:
        imageDelOutline(&image, mpool, tid);
    }

    void dispose() override
    {
       imageFree(&image);
    }
};


static void _renderFill(SwShapeTask* task, SwSurface* surface, uint8_t opacity)
{
    if (auto fill = task->rshape->fill) {
        rasterGradientShape(surface, &task->shape, fill, opacity);
    } else {
        RenderColor c;
        task->rshape->fillColor(&c.r, &c.g, &c.b, &c.a);
        c.a = MULTIPLY(opacity, c.a);
        if (c.a > 0) rasterShape(surface, &task->shape, c);
    }
}

static void _renderStroke(SwShapeTask* task, SwSurface* surface, uint8_t opacity)
{
    if (auto strokeFill = task->rshape->strokeFill()) {
        rasterGradientStroke(surface, &task->shape, strokeFill, opacity);
    } else {
        RenderColor c;
        if (task->rshape->strokeFill(&c.r, &c.g, &c.b, &c.a)) {
            c.a = MULTIPLY(opacity, c.a);
            if (c.a > 0) rasterStroke(surface, &task->shape, c);
        }
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

SwRenderer::SwRenderer()
{
    if (TaskScheduler::onthread()) {
        TVGLOG("SW_RENDERER", "Running on a non-dominant thread!, Renderer(%p)", this);
        mpool = mpoolInit(threadsCnt);
        sharedMpool = false;
    } else {
        mpool = globalMpool;
        sharedMpool = true;
    }

    ++rendererCnt;
}


SwRenderer::~SwRenderer()
{
    clearCompositors();

    delete(surface);

    if (!sharedMpool) mpoolTerm(mpool);

    --rendererCnt;
}


bool SwRenderer::clear()
{
    if (surface) return rasterClear(surface, 0, 0, surface->w, surface->h);
    return false;
}


bool SwRenderer::sync()
{
    ARRAY_FOREACH(p, tasks) {
        if ((*p)->disposed) {
            delete(*p);
        } else {
            (*p)->done();
            (*p)->pushed = false;
        }
    }
    tasks.clear();

    return true;
}


RenderRegion SwRenderer::viewport()
{
    return vport;
}


bool SwRenderer::viewport(const RenderRegion& vp)
{
    vport = vp;
    return true;
}


bool SwRenderer::target(pixel_t* data, uint32_t stride, uint32_t w, uint32_t h, ColorSpace cs)
{
    if (!data || stride == 0 || w == 0 || h == 0 || w > stride) return false;

    clearCompositors();

    if (!surface) surface = new SwSurface;

    surface->data = data;
    surface->stride = stride;
    surface->w = w;
    surface->h = h;
    surface->cs = cs;
    surface->channelSize = CHANNEL_SIZE(cs);
    surface->premultiplied = true;

    return rasterCompositor(surface);
}


bool SwRenderer::preUpdate()
{
    return true;
}


bool SwRenderer::postUpdate()
{
    return true;
}


bool SwRenderer::preRender()
{
    return true;
}


void SwRenderer::clearCompositors()
{
    //Free Composite Caches
    ARRAY_FOREACH(p, compositors) {
        tvg::free((*p)->compositor->image.data);
        delete((*p)->compositor);
        delete(*p);
    }
    compositors.reset();
}


bool SwRenderer::postRender()
{
    //Unmultiply alpha if needed
    if (surface->cs == ColorSpace::ABGR8888S || surface->cs == ColorSpace::ARGB8888S) {
        rasterUnpremultiply(surface);
    }

    ARRAY_FOREACH(p, tasks) {
        if ((*p)->disposed) delete(*p);
        else (*p)->pushed = false;
    }
    tasks.clear();

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
    if (!task) return false;

    task->done();

    if (task->opacity == 0) return true;

    //Main raster stage
    if (task->rshape->strokeFirst()) {
        _renderStroke(task, surface, task->opacity);
        _renderFill(task, surface, task->opacity);
    } else {
        _renderFill(task, surface, task->opacity);
        _renderStroke(task, surface, task->opacity);
    }

    return true;
}


bool SwRenderer::blend(BlendMethod method)
{
    if (surface->blendMethod == method) return true;
    surface->blendMethod = method;

    switch (method) {
        case BlendMethod::Normal:
            surface->blender = nullptr;
            break;
        case BlendMethod::Multiply:
            surface->blender = opBlendMultiply;
            break;
        case BlendMethod::Screen:
            surface->blender = opBlendScreen;
            break;
        case BlendMethod::Overlay:
            surface->blender = opBlendOverlay;
            break;
        case BlendMethod::Darken:
            surface->blender = opBlendDarken;
            break;
        case BlendMethod::Lighten:
            surface->blender = opBlendLighten;
            break;
        case BlendMethod::ColorDodge:
            surface->blender = opBlendColorDodge;
            break;
        case BlendMethod::ColorBurn:
            surface->blender = opBlendColorBurn;
            break;
        case BlendMethod::HardLight:
            surface->blender = opBlendHardLight;
            break;
        case BlendMethod::SoftLight:
            surface->blender = opBlendSoftLight;
            break;
        case BlendMethod::Difference:
            surface->blender = opBlendDifference;
            break;
        case BlendMethod::Exclusion:
            surface->blender = opBlendExclusion;
            break;
        case BlendMethod::Add:
            surface->blender = opBlendAdd;
            break;
        default:
            TVGLOG("SW_ENGINE", "Non supported blending option = %d", (int) method);
            surface->blender = nullptr;
            break;
    }
    return false;
}


RenderRegion SwRenderer::region(RenderData data)
{
    return static_cast<SwTask*>(data)->bounds();
}


bool SwRenderer::beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity)
{
    if (!cmp) return false;
    auto p = static_cast<SwCompositor*>(cmp);

    p->method = method;
    p->opacity = opacity;

    //Current Context?
    if (p->method != MaskMethod::None) {
        surface = p->recoverSfc;
        surface->compositor = p;
    }

    return true;
}


const RenderSurface* SwRenderer::mainSurface()
{
    return surface;
}


SwSurface* SwRenderer::request(int channelSize, bool square)
{
    SwSurface* cmp = nullptr;
    uint32_t w, h;

    if (square) {
        //Same Dimensional Size is demanded for the Post Processing Fast Flipping
        w = h = std::max(surface->w, surface->h);
    } else {
        w = surface->w;
        h = surface->h;
    }

    //Use cached data
    ARRAY_FOREACH(p, compositors) {
        auto cur = *p;
        if (cur->compositor->valid && cur->compositor->image.channelSize == channelSize) {
            if (w == cur->w && h == cur->h) {
                cmp = *p;
                break;
            }
        }
    }

    //New Composition
    if (!cmp) {
        //Inherits attributes from main surface
        cmp = new SwSurface(surface);
        cmp->compositor = new SwCompositor;
        cmp->compositor->image.data = tvg::malloc<pixel_t*>(channelSize * w * h);
        cmp->w = cmp->compositor->image.w = w;
        cmp->h = cmp->compositor->image.h = h;
        cmp->stride = cmp->compositor->image.stride = w;
        cmp->compositor->image.direct = true;
        cmp->compositor->valid = true;
        cmp->channelSize = cmp->compositor->image.channelSize = channelSize;

        compositors.push(cmp);
    }

    //Sync. This may have been modified by post-processing.
    cmp->data = cmp->compositor->image.data;

    return cmp;
}


RenderCompositor* SwRenderer::target(const RenderRegion& region, ColorSpace cs, CompositionFlag flags)
{
    auto x = region.x;
    auto y = region.y;
    auto w = region.w;
    auto h = region.h;
    auto sw = static_cast<int32_t>(surface->w);
    auto sh = static_cast<int32_t>(surface->h);

    //Out of boundary
    if (x >= sw || y >= sh || x + w < 0 || y + h < 0) return nullptr;

    auto cmp = request(CHANNEL_SIZE(cs), (flags & CompositionFlag::PostProcessing));

    //Boundary Check
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > sw) w = (sw - x);
    if (y + h > sh) h = (sh - y);

    if (w == 0 || h == 0) return nullptr;

    cmp->compositor->recoverSfc = surface;
    cmp->compositor->recoverCmp = surface->compositor;
    cmp->compositor->valid = false;
    cmp->compositor->bbox.min.x = x;
    cmp->compositor->bbox.min.y = y;
    cmp->compositor->bbox.max.x = x + w;
    cmp->compositor->bbox.max.y = y + h;

    /* TODO: Currently, only blending might work.
       Blending and composition must be handled together. */
    auto color = (surface->blender && !surface->compositor) ? 0x00ffffff : 0x00000000;
    rasterClear(cmp, x, y, w, h, color);

    //Switch render target
    surface = cmp;

    return cmp->compositor;
}


bool SwRenderer::endComposite(RenderCompositor* cmp)
{
    if (!cmp) return false;

    auto p = static_cast<SwCompositor*>(cmp);

    //Recover Context
    surface = p->recoverSfc;
    surface->compositor = p->recoverCmp;

    //only invalid (currently used) surface can be composited
    if (p->valid) return true;
    p->valid = true;

    //Default is alpha blending
    if (p->method == MaskMethod::None) {
        auto m = tvg::identity();
        return rasterImage(surface, &p->image, m, p->bbox, p->opacity);
    }

    return true;
}


void SwRenderer::prepare(RenderEffect* effect, const Matrix& transform)
{
    switch (effect->type) {
        case SceneEffect::GaussianBlur: effectGaussianBlurUpdate(static_cast<RenderEffectGaussianBlur*>(effect), transform); break;
        case SceneEffect::DropShadow: effectDropShadowUpdate(static_cast<RenderEffectDropShadow*>(effect), transform); break;
        case SceneEffect::Fill: effectFillUpdate(static_cast<RenderEffectFill*>(effect)); break;
        case SceneEffect::Tint: effectTintUpdate(static_cast<RenderEffectTint*>(effect)); break;
        case SceneEffect::Tritone: effectTritoneUpdate(static_cast<RenderEffectTritone*>(effect)); break;
        default: break;
    }
}


bool SwRenderer::region(RenderEffect* effect)
{
    switch (effect->type) {
        case SceneEffect::GaussianBlur: return effectGaussianBlurRegion(static_cast<RenderEffectGaussianBlur*>(effect));
        case SceneEffect::DropShadow: return effectDropShadowRegion(static_cast<RenderEffectDropShadow*>(effect));
        default: return false;
    }
}


bool SwRenderer::render(RenderCompositor* cmp, const RenderEffect* effect, bool direct)
{
    auto p = static_cast<SwCompositor*>(cmp);

    if (p->image.channelSize != sizeof(uint32_t)) {
        TVGERR("SW_ENGINE", "Not supported grayscale Gaussian Blur!");
        return false;
    }
    
    switch (effect->type) {
        case SceneEffect::GaussianBlur: {
            return effectGaussianBlur(p, request(surface->channelSize, true), static_cast<const RenderEffectGaussianBlur*>(effect));
        }
        case SceneEffect::DropShadow: {
            auto cmp1 = request(surface->channelSize, true);
            cmp1->compositor->valid = false;
            auto cmp2 = request(surface->channelSize, true);
            SwSurface* surfaces[] = {cmp1, cmp2};
            auto ret = effectDropShadow(p, surfaces, static_cast<const RenderEffectDropShadow*>(effect), direct);
            cmp1->compositor->valid = true;
            return ret;
        }
        case SceneEffect::Fill: {
            return effectFill(p, static_cast<const RenderEffectFill*>(effect), direct);
        }
        case SceneEffect::Tint: {
            return effectTint(p, static_cast<const RenderEffectTint*>(effect), direct);
        }
        case SceneEffect::Tritone: {
            return effectTritone(p, static_cast<const RenderEffectTritone*>(effect), direct);
        }
        default: return false;
    }
}


void SwRenderer::dispose(RenderEffect* effect) 
{
    tvg::free(effect->rd);
    effect->rd = nullptr;
}


ColorSpace SwRenderer::colorSpace()
{
    if (surface) return surface->cs;
    else return ColorSpace::Unknown;
}


void SwRenderer::dispose(RenderData data)
{
    auto task = static_cast<SwTask*>(data);
    task->done();
    task->dispose();

    if (task->pushed) task->disposed = true;
    else delete(task);
}


void* SwRenderer::prepareCommon(SwTask* task, const Matrix& transform, const Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    if (!surface) return task;
    if (flags == RenderUpdateFlag::None) return task;

    //TODO: Failed threading them. It would be better if it's possible.
    //See: https://github.com/thorvg/thorvg/issues/1409
    //Guarantee composition targets get ready.
    ARRAY_FOREACH(p, clips) {
        static_cast<SwTask*>(*p)->done();
    }

    task->clips = clips;
    task->transform = transform;
    
    //zero size?
    if (task->transform.e11 == 0.0f && task->transform.e12 == 0.0f) return task; //zero width
    if (task->transform.e21 == 0.0f && task->transform.e22 == 0.0f) return task; //zero height

    task->opacity = opacity;
    task->surface = surface;
    task->mpool = mpool;
    task->flags = flags;
    task->bbox.min.x = std::max(static_cast<SwCoord>(0), static_cast<SwCoord>(vport.x));
    task->bbox.min.y = std::max(static_cast<SwCoord>(0), static_cast<SwCoord>(vport.y));
    task->bbox.max.x = std::min(static_cast<SwCoord>(surface->w), static_cast<SwCoord>(vport.x + vport.w));
    task->bbox.max.y = std::min(static_cast<SwCoord>(surface->h), static_cast<SwCoord>(vport.y + vport.h));

    if (!task->pushed) {
        task->pushed = true;
        tasks.push(task);
    }

    TaskScheduler::request(task);

    return task;
}


RenderData SwRenderer::prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    //prepare task
    auto task = static_cast<SwImageTask*>(data);
    if (!task) task = new SwImageTask;
    else task->done();

    task->source = surface;

    return prepareCommon(task, transform, clips, opacity, flags);
}


RenderData SwRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    //prepare task
    auto task = static_cast<SwShapeTask*>(data);
    if (!task) task = new SwShapeTask;
    else task->done();

    task->rshape = &rshape;
    task->clipper = clipper;

    return prepareCommon(task, transform, clips, opacity, flags);
}


bool SwRenderer::term()
{
    if (rendererCnt > 0) return false;

    mpoolTerm(globalMpool);
    globalMpool = nullptr;
    rendererCnt = -1;

    return true;
}


SwRenderer* SwRenderer::gen(uint32_t threads)
{
    //initialize engine
    if (rendererCnt == -1) {
#ifdef THORVG_SW_OPENMP_SUPPORT
        omp_set_num_threads(threads);
#endif
        //Share the memory pool among the renderer
        globalMpool = mpoolInit(threads);
        threadsCnt = threads;
        rendererCnt = 0;
    }

    return new SwRenderer;
}
