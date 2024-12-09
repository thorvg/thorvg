/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

#include "tvgWgRenderer.h"

WgRenderer::WgRenderer()
{
}


WgRenderer::~WgRenderer()
{
    release();
}


void WgRenderer::initialize()
{
}


void WgRenderer::release()
{
    // check for general context availability
    if (!mContext.queue) return;

    // dispose stored objects
    disposeObjects();

    // clear render data paint pools
    mRenderDataShapePool.release(mContext);
    mRenderDataPicturePool.release(mContext);
    WgMeshDataPool::gMeshDataPool->release(mContext);

    // clear render storage pool
    mRenderStoragePool.release(mContext);

    // clear rendering tree stacks
    mCompositorStack.clear();
    mRenderStorageStack.clear();
    mRenderStorageRoot.release(mContext);

    // release context handles
    mCompositor.release(mContext);
    mContext.release();

    // release gpu handles
    clearTargets();
}


void WgRenderer::disposeObjects()
{
    for (uint32_t i = 0; i < mDisposeRenderDatas.count; i++) {
        WgRenderDataPaint* renderData = (WgRenderDataPaint*)mDisposeRenderDatas[i];
        if (renderData->type() == Type::Shape) {
            mRenderDataShapePool.free(mContext, (WgRenderDataShape*)renderData);
        } else {
            mRenderDataPicturePool.free(mContext, (WgRenderDataPicture*)renderData);
        }
    }
    mDisposeRenderDatas.clear();
}


RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const Matrix& transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape)
        renderDataShape = mRenderDataShapePool.allocate(mContext);

    // update geometry
    if ((!data) || (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Stroke))) {
        renderDataShape->updateMeshes(mContext, rshape, transform);
    }

    // update paint settings
    if ((!data) || (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend))) {
        renderDataShape->update(mContext, transform, mTargetSurface.cs, opacity);
        renderDataShape->fillRule = rshape.rule;
    }

    // setup fill settings
    renderDataShape->viewport = mViewport;
    renderDataShape->opacity = opacity;
    renderDataShape->renderSettingsShape.update(mContext, rshape.fill, rshape.color, flags);
    if (rshape.stroke)
        renderDataShape->renderSettingsStroke.update(mContext, rshape.stroke->fill, rshape.stroke->color, flags);

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
    // push rot render storage to the render tree stack
    assert(mRenderStorageStack.count == 0);
    mRenderStorageStack.push(&mRenderStorageRoot);
    // create command encoder for drawing
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    mCommandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);
    // start root render pass
    mCompositor.beginRenderPass(mCommandEncoder, mRenderStorageStack.last(), true);
    return true;
}


bool WgRenderer::renderShape(RenderData data)
{
    // temporary simple render data to the current render target
    mCompositor.renderShape(mContext, (WgRenderDataShape*)data, mBlendMethod);
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
    // temporary simple render data to the current render target
    mCompositor.renderImage(mContext, (WgRenderDataPicture*)data, mBlendMethod);
    return true;
}


bool WgRenderer::postRender()
{
    // end root render pass
    mCompositor.endRenderPass();
    // release command encoder
    const WGPUCommandBufferDescriptor commandBufferDesc{};
    WGPUCommandBuffer commandsBuffer = wgpuCommandEncoderFinish(mCommandEncoder, &commandBufferDesc);
    wgpuQueueSubmit(mContext.queue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
    wgpuCommandEncoderRelease(mCommandEncoder);
    // pop root render storage to the render tree stack
    mRenderStorageStack.pop();
    assert(mRenderStorageStack.count == 0);
    return true;
}


void WgRenderer::dispose(RenderData data) {
    if (!mContext.queue) return;
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData) {
        ScopedLock lock(mDisposeKey);
        mDisposeRenderDatas.push(data);
    }
}


RenderRegion WgRenderer::region(RenderData data)
{
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData->type() == Type::Shape) {
        return renderData->aabb;
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
    return true;
}


void WgRenderer::releaseSurfaceTexture()
{
    if (surfaceTexture.texture) {
        wgpuTextureRelease(surfaceTexture.texture);
        surfaceTexture.texture = nullptr;
    }
}


bool WgRenderer::sync()
{
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
    const WGPUCommandEncoderDescriptor commandEncoderDesc{};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);

    // show root offscreen buffer
    mCompositor.blit(mContext, commandEncoder, &mRenderStorageRoot, dstTextureView);

    // release command encoder
    const WGPUCommandBufferDescriptor commandBufferDesc{};
    WGPUCommandBuffer commandsBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDesc);
    wgpuQueueSubmit(mContext.queue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
    wgpuCommandEncoderRelease(commandEncoder);

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
        mRenderStoragePool.initialize(mContext, width, height);
        mRenderStorageRoot.initialize(mContext, width, height);
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

    // update retnder targets dimentions
    if ((mTargetSurface.w != width) || (mTargetSurface.h != height)) {
        // release render tagets
        mRenderStoragePool.release(mContext);
        mRenderStorageRoot.release(mContext);
        clearTargets();

        mRenderStoragePool.initialize(mContext, width, height);
        mRenderStorageRoot.initialize(mContext, width, height);
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
    #ifndef __EMSCRIPTEN__
        .presentMode = WGPUPresentMode_Fifo,
    #endif
    };
    wgpuSurfaceConfigure(surface, &surfaceConfiguration);
    return true;
}


RenderCompositor* WgRenderer::target(const RenderRegion& region, TVG_UNUSED ColorSpace cs, TVG_UNUSED CompositionFlag flags)
{
    mCompositorStack.push(new WgCompose);
    mCompositorStack.last()->aabb = region;
    return mCompositorStack.last();
}


bool WgRenderer::beginComposite(RenderCompositor* cmp, MaskMethod method, uint8_t opacity)
{
    // save current composition settings
    WgCompose* compose = (WgCompose *)cmp;
    compose->method = method;
    compose->opacity = opacity;
    compose->blend = mBlendMethod;
    // end current render pass
    mCompositor.endRenderPass();
    // allocate new render storage and push to the render tree stack
    WgRenderStorage* storage = mRenderStoragePool.allocate(mContext);
    mRenderStorageStack.push(storage);
    // begin newly added render pass
    WGPUColor color{};
    if ((compose->method == MaskMethod::None) && (compose->blend != BlendMethod::Normal)) color = { 1.0, 1.0, 1.0, 0.0 };
    mCompositor.beginRenderPass(mCommandEncoder, mRenderStorageStack.last(), true, color);
    return true;
}


bool WgRenderer::endComposite(RenderCompositor* cmp)
{
    // get current composition settings
    WgCompose* comp = (WgCompose *)cmp;
    // end current render pass
    mCompositor.endRenderPass();
    // finish scene blending
    if (comp->method == MaskMethod::None) {
        // get source and destination render storages
        WgRenderStorage* src = mRenderStorageStack.last();
        mRenderStorageStack.pop();
        WgRenderStorage* dst = mRenderStorageStack.last();
        // begin previous render pass
        mCompositor.beginRenderPass(mCommandEncoder, dst, false);
        // apply composition
        mCompositor.renderScene(mContext, src, comp);
        // back render targets to the pool
        mRenderStoragePool.free(mContext, src);
    } else { // finish composition
        // get source, mask and destination render storages
        WgRenderStorage* src = mRenderStorageStack.last();
        mRenderStorageStack.pop();
        WgRenderStorage* msk = mRenderStorageStack.last();
        mRenderStorageStack.pop();
        WgRenderStorage* dst = mRenderStorageStack.last();
        // begin previous render pass
        mCompositor.beginRenderPass(mCommandEncoder, dst, false);
        // apply composition
        mCompositor.composeScene(mContext, src, msk, comp);
        // back render targets to the pool
        mRenderStoragePool.free(mContext, src);
        mRenderStoragePool.free(mContext, msk);
    }

    // delete current compositor settings
    delete mCompositorStack.last();
    mCompositorStack.pop();

    return true;
}


bool WgRenderer::prepare(TVG_UNUSED RenderEffect* effect)
{
    //TODO: Return if the current post effect requires the region expansion
    return false;
}


bool WgRenderer::effect(TVG_UNUSED RenderCompositor* cmp, TVG_UNUSED const RenderEffect* effect, TVG_UNUSED uint8_t opacity, TVG_UNUSED bool direct)
{
    TVGLOG("WG_ENGINE", "SceneEffect(%d) is not supported", (int)effect->type);
    return false;
}


WgRenderer* WgRenderer::gen()
{
    return new WgRenderer();
}


bool WgRenderer::init(TVG_UNUSED uint32_t threads)
{
    return true;
}


bool WgRenderer::term()
{
    return true;
}
