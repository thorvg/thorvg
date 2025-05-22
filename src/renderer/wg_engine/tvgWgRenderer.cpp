/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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

#include <atomic>
#include "tvgTaskScheduler.h"
#include "tvgWgRenderer.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

static atomic<int32_t> rendererCnt{-1};


void WgRenderer::release()
{
    // check for general context availability
    if (!mContext.queue) return;

    // dispose stored objects
    disposeObjects();

    // clear render data paint pools
    mRenderDataShapePool.release(mContext);
    mRenderDataPicturePool.release(mContext);
    mRenderDataViewportPool.release(mContext);
    mRenderDataEffectParamsPool.release(mContext);
    WgMeshDataPool::gMeshDataPool->release(mContext);

    // clear render  pool
    mRenderTargetPool.release(mContext);

    // clear rendering tree stacks
    mCompositorList.clear();
    mRenderTargetStack.clear();
    mRenderTargetRoot.release(mContext);

    // release context handles
    mCompositor.release(mContext);
    mContext.release();

    // release gpu handles
    clearTargets();
}


void WgRenderer::disposeObjects()
{
    ARRAY_FOREACH(p, mDisposeRenderDatas) {
        auto renderData = (WgRenderDataPaint*)(*p);
        if (renderData->type() == Type::Shape) {
            mRenderDataShapePool.free(mContext, (WgRenderDataShape*)renderData);
        } else {
            mRenderDataPicturePool.free(mContext, (WgRenderDataPicture*)renderData);
        }
    }
    mDisposeRenderDatas.clear();
}


void WgRenderer::releaseSurfaceTexture()
{
    if (surfaceTexture.texture) {
        wgpuTextureRelease(surfaceTexture.texture);
        surfaceTexture.texture = nullptr;
    }
}


void WgRenderer::clearTargets()
{
    releaseSurfaceTexture();
    if (surface) wgpuSurfaceUnconfigure(surface);
    targetTexture = nullptr;
    surface = nullptr;
    mTargetSurface.stride = 0;
    mTargetSurface.w = 0;
    mTargetSurface.h = 0;

}


bool WgRenderer::surfaceConfigure(WGPUSurface surface, WgContext& context, uint32_t width, uint32_t height)
{
    // store target surface properties
    this->surface = surface;
    if (width == 0 || height == 0 || !surface) return false;

    // setup surface configuration
    WGPUSurfaceConfiguration surfaceConfiguration {
        .device = context.device,
        .format = context.preferredFormat,
        .usage = WGPUTextureUsage_RenderAttachment,
    #ifdef __EMSCRIPTEN__
        .alphaMode = WGPUCompositeAlphaMode_Premultiplied,
    #endif
        .width = width,
        .height = height,
    #ifdef __EMSCRIPTEN__
        .presentMode = WGPUPresentMode_Fifo
    #elif __linux__
    #else
        .presentMode = WGPUPresentMode_Immediate
    #endif
    };
    wgpuSurfaceConfigure(surface, &surfaceConfiguration);
    return true;
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/


RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape)
        renderDataShape = mRenderDataShapePool.allocate(mContext);

    // update geometry
    if ((!data) || (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Stroke))) {
        renderDataShape->updateMeshes(mContext, rshape, transform, mBufferPool.pool);
    }

    // update paint settings
    if ((!data) || (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend))) {
        renderDataShape->update(mContext, transform, mTargetSurface.cs, opacity);
        renderDataShape->fillRule = rshape.rule;
    }

    // setup fill settings
    renderDataShape->viewport = mViewport;
    renderDataShape->opacity = opacity;
    if (flags & RenderUpdateFlag::Gradient && rshape.fill) renderDataShape->renderSettingsShape.updateFill(mContext, rshape.fill);
    else if (flags & RenderUpdateFlag::Color) renderDataShape->renderSettingsShape.updateColor(mContext, rshape.color);
    if (rshape.stroke) {
        if (flags & RenderUpdateFlag::GradientStroke && rshape.stroke->fill) renderDataShape->renderSettingsStroke.updateFill(mContext, rshape.stroke->fill);
        else if (flags & RenderUpdateFlag::Stroke) renderDataShape->renderSettingsStroke.updateColor(mContext, rshape.stroke->color);
    }

    // store clips data
    renderDataShape->updateClips(clips);

    return renderDataShape;
}


RenderData WgRenderer::prepare(RenderSurface* surface, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    // get or create render data shape
    auto renderDataPicture = (WgRenderDataPicture*)data;
    if (!renderDataPicture)
        renderDataPicture = mRenderDataPicturePool.allocate(mContext);

    // update paint settings
    renderDataPicture->viewport = mViewport;
    renderDataPicture->opacity = opacity;
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        renderDataPicture->update(mContext, transform, surface->cs, opacity);
    }

    // update image data
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Image)) {
        renderDataPicture->updateSurface(mContext, surface);
    }

    // store clips data
    renderDataPicture->updateClips(clips);

    return renderDataPicture;
}

bool WgRenderer::preRender()
{
    // invalidate context
    if (mContext.invalid()) return false;
    // push rot render target to the render tree stack
    assert(mRenderTargetStack.count == 0);
    mRenderTargetStack.push(&mRenderTargetRoot);
    // create root compose settings
    WgCompose* compose = new WgCompose();
    compose->aabb = {0, 0, (int32_t)mTargetSurface.w, (int32_t)mTargetSurface.h };
    compose->blend = BlendMethod::Normal;
    compose->method = MaskMethod::None;
    compose->opacity = 255;
    mCompositorList.push(compose);
    // create root scene scene
    WgSceneTask* sceneTask = new WgSceneTask(&mRenderTargetRoot, compose, nullptr);
    mRenderTaskList.push(sceneTask);
    mSceneTaskStack.push(sceneTask);
    return true;
}


bool WgRenderer::renderShape(RenderData data)
{
    // create new paint task
    WgPaintTask* paintTask = new WgPaintTask((WgRenderDataPaint*)data, mBlendMethod);
    // append shape task to current scene task
    WgSceneTask* sceneTask = mSceneTaskStack.last();
    sceneTask->children.push(paintTask);
    // append shape task to tasks list
    mRenderTaskList.push(paintTask);
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
        // create new paint task
    WgPaintTask* paintTask = new WgPaintTask((WgRenderDataPaint*)data, mBlendMethod);
    // append shape task to current scene task
    WgSceneTask* sceneTask = mSceneTaskStack.last();
    sceneTask->children.push(paintTask);
    // append shape task to tasks list
    mRenderTaskList.push(paintTask);
    return true;
}


bool WgRenderer::postRender()
{
    // create command encoder for drawing
    WGPUCommandEncoder commandEncoder = mContext.createCommandEncoder();
    // execure rendering (all the fun is here)
    WgSceneTask* sceneTaskRoot = mSceneTaskStack.last();
    sceneTaskRoot->run(mContext, mCompositor, commandEncoder);
    // execute and release command encoder
    mContext.submitCommandEncoder(commandEncoder);
    mContext.releaseCommandEncoder(commandEncoder);
    // pop root scene task
    mSceneTaskStack.pop();
    assert(mSceneTaskStack.count == 0);
    // pop root render target from the render tree stack
    mRenderTargetStack.pop();
    assert(mRenderTargetStack.count == 0);
    // delete all tasks
    ARRAY_FOREACH(p, mRenderTaskList) { delete (*p); };
    mRenderTaskList.clear();
    // delete all compositions
    ARRAY_FOREACH(p, mCompositorList) { delete (*p); };
    mCompositorList.clear();
    // clear viewport list and store allocated handles to pool
    ARRAY_FOREACH(p, mRenderDataViewportList)
        mRenderDataViewportPool.free(mContext, *p);
    mRenderDataViewportList.clear();
    return true;
}


void WgRenderer::dispose(RenderData data) {
    if (!mContext.queue) return;
    ScopedLock lock(mDisposeKey);
    mDisposeRenderDatas.push(data);
}


RenderRegion WgRenderer::region(RenderData data)
{
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData->type() == Type::Shape) {
        auto& v1 = renderData->aabb.min;
        auto& v2 = renderData->aabb.max;
        RenderRegion renderRegion;
        renderRegion.x = static_cast<int32_t>(nearbyint(v1.x));
        renderRegion.y = static_cast<int32_t>(nearbyint(v1.y));
        renderRegion.w = static_cast<int32_t>(nearbyint(v2.x)) - renderRegion.x;
        renderRegion.h = static_cast<int32_t>(nearbyint(v2.y)) - renderRegion.y;
        return renderRegion;
    }
    return { 0, 0, (int32_t)mTargetSurface.w, (int32_t)mTargetSurface.h };
}


RenderRegion WgRenderer::viewport() {
    return mViewport;
}


bool WgRenderer::viewport(const RenderRegion& vp)
{
    mViewport = vp;
    return true;
}


bool WgRenderer::blend(BlendMethod method)
{
    mBlendMethod = method;
    return true;
}


ColorSpace WgRenderer::colorSpace()
{
    return ColorSpace::Unknown;
}


const RenderSurface* WgRenderer::mainSurface()
{
    return &mTargetSurface;
}


bool WgRenderer::clear()
{
    if (mContext.invalid()) return false;

    //TODO: clear the current target buffer only if clear() is called
    return true;
}


bool WgRenderer::sync()
{
    if (mContext.invalid()) return false;

    disposeObjects();

    // if texture buffer used
    WGPUTexture dstTexture = targetTexture;
    if (surface) {
        releaseSurfaceTexture();
        wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
        dstTexture = surfaceTexture.texture;
    }
    // there is no external dest buffer
    if (!dstTexture) return false;

    // get external dest buffer
    WGPUTextureView dstTextureView = mContext.createTextureView(dstTexture);

    // create command encoder
    WGPUCommandEncoder commandEncoder = mContext.createCommandEncoder();
    // show root offscreen buffer
    mCompositor.blit(mContext, commandEncoder, &mRenderTargetRoot, dstTextureView);
    // execute and release command encoder
    mContext.submitCommandEncoder(commandEncoder);
    mContext.releaseCommandEncoder(commandEncoder);

    // release dest buffer view
    mContext.releaseTextureView(dstTextureView);
    return true;
}


// render target handle
bool WgRenderer::target(WGPUDevice device, WGPUInstance instance, void* target, uint32_t width, uint32_t height, int type)
{
    // release all existing handles
    if (!instance || !device || !target) {
        // release all handles
        release();
        return true;
    }

    // can not initialize renderer, give up
    if (!instance || !device || !target || !width || !height)
        return false;

    // device or instance was changed, need to recreate all instances
    if ((mContext.device != device) || (mContext.instance != instance)) {
        // release all handles
        release();

        // initialize base rendering handles
        mContext.initialize(instance, device);

        // initialize render tree instances
        mRenderTargetPool.initialize(mContext, width, height);
        mRenderTargetRoot.initialize(mContext, width, height);
        mCompositor.initialize(mContext, width, height);

        // store target properties
        mTargetSurface.stride = width;
        mTargetSurface.w = width;
        mTargetSurface.h = height;

        // configure surface (must be called after context creation)
        if (type == 0) {
            surface = (WGPUSurface)target;
            surfaceConfigure(surface, mContext, width, height);
        } else targetTexture = (WGPUTexture)target;
        return true;
    }

    // update render targets dimentions
    if ((mTargetSurface.w != width) || (mTargetSurface.h != height) || (type == 0 ? (surface != (WGPUSurface)target) : (targetTexture != (WGPUTexture)target))) {
        // release render tagets
        mRenderTargetPool.release(mContext);
        mRenderTargetRoot.release(mContext);
        clearTargets();

        mRenderTargetPool.initialize(mContext, width, height);
        mRenderTargetRoot.initialize(mContext, width, height);
        mCompositor.resize(mContext, width, height);

        // store target properties
        mTargetSurface.stride = width;
        mTargetSurface.w = width;
        mTargetSurface.h = height;

        // configure surface (must be called after context creation)
        if (type == 0) {
            surface = (WGPUSurface)target;
            surfaceConfigure(surface, mContext, width, height);
        } else targetTexture = (WGPUTexture)target;
        return true;
    }

    return true;
}


WgRenderer::WgRenderer()
{
    if (TaskScheduler::onthread()) {
        TVGLOG("WG_RENDERER", "Running on a non-dominant thread!, Renderer(%p)", this);
        mBufferPool.pool = new WgGeometryBufferPool;
        mBufferPool.individual = true;
    } else {
        mBufferPool.pool = WgGeometryBufferPool::instance();
    }

    ++rendererCnt;
}


WgRenderer::~WgRenderer()
{
    release();

    if (mBufferPool.individual) delete(mBufferPool.pool);

    --rendererCnt;
}


RenderCompositor* WgRenderer::target(const RenderRegion& region, TVG_UNUSED ColorSpace cs, TVG_UNUSED CompositionFlag flags)
{
    // create and setup compose data
    WgCompose* compose = new WgCompose();
    compose->aabb = region;
    if (flags & PostProcessing) {
        compose->aabb = region;
        compose->rdViewport = mRenderDataViewportPool.allocate(mContext);
        compose->rdViewport->update(mContext, region);
        mRenderDataViewportList.push(compose->rdViewport);
    }
    mCompositorList.push(compose);
    return compose;
}


bool WgRenderer::beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity)
{
    // save current composition settings
    WgCompose* compose = (WgCompose*)cmp;
    compose->method = method;
    compose->opacity = opacity;
    compose->blend = mBlendMethod;
    WgSceneTask* sceneTaskCurrent = mSceneTaskStack.last();
    // allocate new render target and push to the render tree stack
    WgRenderTarget* renderTarget = mRenderTargetPool.allocate(mContext);
    mRenderTargetStack.push(renderTarget);
    // create and setup new scene task
    WgSceneTask* sceneTask = new WgSceneTask(renderTarget, compose, sceneTaskCurrent);
    sceneTaskCurrent->children.push(sceneTask);
    mRenderTaskList.push(sceneTask);
    mSceneTaskStack.push(sceneTask);
    return true;
}


bool WgRenderer::endComposite(RenderCompositor* cmp)
{
    // finish scene blending
    if (cmp->method == MaskMethod::None) {
        // get source and destination render targets
        WgRenderTarget* src = mRenderTargetStack.last();
        mRenderTargetStack.pop();
        // pop source scene
        WgSceneTask* srcScene = mSceneTaskStack.last();
        mSceneTaskStack.pop();
        // setup render target compose destitations
        srcScene->renderTargetDst = mSceneTaskStack.last()->renderTarget;
        srcScene->renderTargetMsk =  nullptr;
        // back render targets to the pool
        mRenderTargetPool.free(mContext, src);
    } else { // finish scene composition
        // get source, mask and destination render targets
        WgRenderTarget* src = mRenderTargetStack.last();
        mRenderTargetStack.pop();
        WgRenderTarget* msk = mRenderTargetStack.last();
        mRenderTargetStack.pop();
        // get source and mask scenes
        WgSceneTask* srcScene = mSceneTaskStack.last();
        mSceneTaskStack.pop();
        WgSceneTask* mskScene = mSceneTaskStack.last();
        mSceneTaskStack.pop();
        // setup render target compose destitations
        srcScene->renderTargetDst = mSceneTaskStack.last()->renderTarget;
        srcScene->renderTargetMsk = mskScene->renderTarget;
        // back render targets to the pool
        mRenderTargetPool.free(mContext, src);
        mRenderTargetPool.free(mContext, msk);
    }
    return true;
}


void WgRenderer::prepare(RenderEffect* effect, const Matrix& transform)
{
    // prepare gaussian blur data
    if (effect->type == SceneEffect::GaussianBlur) {
        auto renderEffect = (RenderEffectGaussianBlur*)effect;
        auto renderData = (WgRenderDataEffectParams*)renderEffect->rd;
        if (!renderData) {
            renderData = mRenderDataEffectParamsPool.allocate(mContext);
            renderEffect->rd = renderData;
        }
        renderData->update(mContext, renderEffect, transform);
        effect->valid = true;
    } else
    // prepare drop shadow data
    if (effect->type == SceneEffect::DropShadow) {
        auto renderEffect = (RenderEffectDropShadow*)effect;
        auto renderData = (WgRenderDataEffectParams*)renderEffect->rd;
        if (!renderData) {
            renderData = mRenderDataEffectParamsPool.allocate(mContext);
            renderEffect->rd = renderData;
        }
        renderData->update(mContext, renderEffect, transform);
        effect->valid = true;
    } else
    // prepare fill
    if (effect->type == SceneEffect::Fill) {
        auto renderEffect = (RenderEffectFill*)effect;
        auto renderData = (WgRenderDataEffectParams*)renderEffect->rd;
        if (!renderData) {
            renderData = mRenderDataEffectParamsPool.allocate(mContext);
            renderEffect->rd = renderData;
        }
        renderData->update(mContext, renderEffect);
        effect->valid = true;
    } else
    // prepare tint
    if (effect->type == SceneEffect::Tint) {
        auto renderEffect = (RenderEffectTint*)effect;
        auto renderData = (WgRenderDataEffectParams*)renderEffect->rd;
        if (!renderData) {
            renderData = mRenderDataEffectParamsPool.allocate(mContext);
            renderEffect->rd = renderData;
        }
        renderData->update(mContext, renderEffect);
        effect->valid = true;
    } else
    // prepare tritone
    if (effect->type == SceneEffect::Tritone) {
        auto renderEffect = (RenderEffectTritone*)effect;
        auto renderData = (WgRenderDataEffectParams*)renderEffect->rd;
        if (!renderData) {
            renderData = mRenderDataEffectParamsPool.allocate(mContext);
            renderEffect->rd = renderData;
        }
        renderData->update(mContext, renderEffect);
        effect->valid = true;
    }
}


bool WgRenderer::region(RenderEffect* effect)
{
    // update gaussian blur region
    if (effect->type == SceneEffect::GaussianBlur) {
        auto gaussian = (RenderEffectGaussianBlur*)effect;
        auto renderData = (WgRenderDataEffectParams*)gaussian->rd;
        if (gaussian->direction != 2) {
            gaussian->extend.x = -renderData->extend;
            gaussian->extend.w = +renderData->extend * 2;
        }
        if (gaussian->direction != 1) {
            gaussian->extend.y = -renderData->extend;
            gaussian->extend.h = +renderData->extend * 2;
        }
        return true;
    } else
    // update drop shadow region
    if (effect->type == SceneEffect::DropShadow) {
        auto dropShadow = (RenderEffectDropShadow*)effect;
        auto renderData = (WgRenderDataEffectParams*)dropShadow->rd;
        dropShadow->extend.x = -(renderData->extend + std::abs(renderData->offset.x));
        dropShadow->extend.w = +(renderData->extend + std::abs(renderData->offset.x)) * 2;
        dropShadow->extend.y = -(renderData->extend + std::abs(renderData->offset.y));
        dropShadow->extend.h = +(renderData->extend + std::abs(renderData->offset.y)) * 2;
        return true;
    }
    return false;
}


bool WgRenderer::render(RenderCompositor* cmp, const RenderEffect* effect, TVG_UNUSED bool direct)
{
    mSceneTaskStack.last()->effect = effect;
    return true;
}


void WgRenderer::dispose(RenderEffect* effect)
{
    auto renderData = (WgRenderDataEffectParams*)effect->rd;
    mRenderDataEffectParamsPool.free(mContext, renderData);
    effect->rd = nullptr;
};


bool WgRenderer::preUpdate()
{
    if (mContext.invalid()) return false;

    return true;
}


bool WgRenderer::postUpdate()
{
    return true;
}


bool WgRenderer::term()
{
    if (rendererCnt > 0) return false;

    //TODO: clean up global resources

    rendererCnt = -1;

    return true;
}


WgRenderer* WgRenderer::gen(TVG_UNUSED uint32_t threads)
{
    //initialize engine
    if (rendererCnt == -1) {
        //TODO:
    }

    return new WgRenderer;
}