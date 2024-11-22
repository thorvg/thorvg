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

    // clear rendering tree stacks
    mCompositorStack.clear();
    mRenderStorageStack.clear();
    mRenderStoragePool.release(mContext);
    mRenderDataShapePool.release(mContext);
    WgMeshDataPool::gMeshDataPool->release(mContext);
    mStorageRoot.release(mContext);

    // release context handles
    mCompositor.release(mContext);
    mPipelines.release(mContext);
    mContext.release();

    // release gpu handles
    clearTargets();
    releaseDevice();
}


void WgRenderer::disposeObjects()
{
    for (uint32_t i = 0; i < mDisposeRenderDatas.count; i++) {
        WgRenderDataPaint* renderData = (WgRenderDataPaint*)mDisposeRenderDatas[i];
        if (renderData->type() == Type::Shape) {
            mRenderDataShapePool.free(mContext, (WgRenderDataShape*)renderData);
        } else {
            renderData->release(mContext);
            delete renderData;
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
        renderDataPicture = new WgRenderDataPicture();

    // update paint settings
    renderDataPicture->viewport = mViewport;
    renderDataPicture->opacity = opacity;
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        renderDataPicture->update(mContext, transform, surface->cs, opacity);
    }

    // update image data
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Image)) {
        mContext.pipelines->layouts.releaseBindGroup(renderDataPicture->bindGroupPicture);
        renderDataPicture->meshData.release(mContext);
        renderDataPicture->meshData.imageBox(mContext, surface->w, surface->h);
        renderDataPicture->imageData.update(mContext, surface);
        renderDataPicture->bindGroupPicture = mContext.pipelines->layouts.createBindGroupTexSampled(
            mContext.samplerLinearRepeat, renderDataPicture->imageData.textureView
        );
    }

    // store clips data
    renderDataPicture->updateClips(clips);

    return renderDataPicture;
}

bool WgRenderer::preRender()
{
    // push rot render storage to the render tree stack
    assert(mRenderStorageStack.count == 0);
    mRenderStorageStack.push(&mStorageRoot);
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
    mCompositor.blit(mContext, commandEncoder, &mStorageRoot, dstTextureView);

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
    // release existing handles
    release();

    // can not initialize renderer, give up
    if (!instance || !target || !width || !height) return false;

    // store or regest gpu device
    this->device = device;
    if (!this->device)
        reguestDevice(instance, (WGPUSurface)target);

    // store target properties
    mTargetSurface.stride = width;
    mTargetSurface.w = width;
    mTargetSurface.h = height;

    // initialize rendering context
    mContext.initialize(instance, this->device);
    mPipelines.initialize(mContext);

    // initialize render tree instances
    mRenderStoragePool.initialize(mContext, width, height);
    mStorageRoot.initialize(mContext, width, height);
    mCompositor.initialize(mContext, width, height);

    // configure surface (must be called after context creation)
    if (type == 0) {
        surface = (WGPUSurface)target;
        surfaceConfigure(surface, mContext, width, height);
    } else targetTexture = (WGPUTexture)target;
    return true;
}


void WgRenderer::reguestDevice(WGPUInstance instance, WGPUSurface surface)
{
    // request adapter
    const WGPURequestAdapterOptions requestAdapterOptions { .compatibleSurface = surface, .powerPreference = WGPUPowerPreference_HighPerformance };
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) { *((WGPUAdapter*)pUserData) = adapter; };
    wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, onAdapterRequestEnded, &this->adapter);

    // get adapter and surface properties
    WGPUFeatureName featureNames[32]{};
    size_t featuresCount = wgpuAdapterEnumerateFeatures(this->adapter, featureNames);

    // request device
    const WGPUDeviceDescriptor deviceDesc { .label = "The owned device", .requiredFeatureCount = featuresCount, .requiredFeatures = featureNames };
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) { *((WGPUDevice*)pUserData) = device; };
    wgpuAdapterRequestDevice(this->adapter, &deviceDesc, onDeviceRequestEnded, &this->device);
    gpuOwner = true;
}


void WgRenderer::releaseDevice()
{
    if (!gpuOwner) return;
    wgpuDeviceRelease(device);
    wgpuDeviceDestroy(device);
    wgpuAdapterRelease(adapter);
    device = nullptr;
    adapter = nullptr;
    gpuOwner = false;
}


void WgRenderer::clearTargets() {
    releaseSurfaceTexture();
    targetTexture = nullptr;
    surface = nullptr;
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


RenderCompositor* WgRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
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
    if ((method == MaskMethod::None) && (opacity != 255)) color = { 1.0, 1.0, 1.0, 0.0 };
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
