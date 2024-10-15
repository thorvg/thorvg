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
    mContext.release();
}


void WgRenderer::initialize()
{
    mPipelines.initialize(mContext);
    WgMeshDataGroup::gMeshDataPool = new WgMeshDataPool();
    WgRenderDataShape::gStrokesGenerator = new WgVertexBufferInd();
}


void WgRenderer::release()
{
    disposeObjects();
    mStorageRoot.release(mContext);
    mRenderStoragePool.release(mContext);
    mRenderDataShapePool.release(mContext);
    delete WgRenderDataShape::gStrokesGenerator;
    WgMeshDataGroup::gMeshDataPool->release(mContext);
    delete WgMeshDataGroup::gMeshDataPool;
    mCompositorStack.clear();
    mRenderStorageStack.clear();
    mPipelines.release(mContext);
    if (gpuOwner) {
        if (device) wgpuDeviceRelease(device);
        device = nullptr;
        if (adapter) wgpuAdapterRelease(adapter);
        adapter = nullptr;
        gpuOwner = false;
    }
    releaseSurfaceTexture();
}


void WgRenderer::disposeObjects()
{
    if (mDisposeRenderDatas.count == 0) return;

    for (auto p = mDisposeRenderDatas.begin(); p < mDisposeRenderDatas.end(); p++) {
        auto renderData = (WgRenderDataPaint*)(*p);
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
    if (!surface) return false;

    releaseSurfaceTexture();

    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

    WGPUTextureView dstView = mContext.createTextureView(surfaceTexture.texture);

    // create command encoder
    const WGPUCommandEncoderDescriptor commandEncoderDesc{};
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);

    // show root offscreen buffer
    mCompositor.blit(mContext, commandEncoder, &mStorageRoot, dstView);

    // release command encoder
    const WGPUCommandBufferDescriptor commandBufferDesc{};
    WGPUCommandBuffer commandsBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDesc);
    wgpuQueueSubmit(mContext.queue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
    wgpuCommandEncoderRelease(commandEncoder);
    mContext.releaseTextureView(dstView);

    return true;
}


// target for native window handle
bool WgRenderer::target(WGPUInstance instance, WGPUSurface surface, uint32_t w, uint32_t h, WGPUDevice device)
{
    gpuOwner = false;
    this->device = device;
    if (!this->device) {
        // request adapter
        const WGPURequestAdapterOptions requestAdapterOptions { .nextInChain = nullptr, .compatibleSurface = surface, .powerPreference = WGPUPowerPreference_HighPerformance, .forceFallbackAdapter = false };
        auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) { *((WGPUAdapter*)pUserData) = adapter; };
        wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, onAdapterRequestEnded, &this->adapter);

        // get adapter and surface properties
        WGPUFeatureName featureNames[32]{};
        size_t featuresCount = wgpuAdapterEnumerateFeatures(this->adapter, featureNames);

        // request device
        const WGPUDeviceDescriptor deviceDesc { .nextInChain = nullptr, .label = "The device", .requiredFeatureCount = featuresCount, .requiredFeatures = featureNames };
        auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) { *((WGPUDevice*)pUserData) = device; };
        wgpuAdapterRequestDevice(this->adapter, &deviceDesc, onDeviceRequestEnded, &this->device);
        gpuOwner = true;
    }

    mContext.initialize(instance, this->device);
    initialize();
    target(surface, w, h);
    mRenderStoragePool.initialize(mContext, w, h);
    mStorageRoot.initialize(mContext, w, h);
    mCompositor.initialize(mContext, w, h);
    return true;
}

bool WgRenderer::target(WGPUSurface surface, uint32_t w, uint32_t h) {
    // store target surface properties
    this->surface = surface;
    mTargetSurface.stride = w;
    mTargetSurface.w = w;
    mTargetSurface.h = h;
    if (w == 0 || h == 0) return false;
    if (!surface) return true;

    WGPUSurfaceConfiguration surfaceConfiguration {
        .device = mContext.device,
        .format = mContext.preferredFormat,
        .usage = WGPUTextureUsage_RenderAttachment,
    #ifdef __EMSCRIPTEN__
        .alphaMode = WGPUCompositeAlphaMode_Premultiplied,
    #endif
        .width = w, .height = h,
    #ifdef __EMSCRIPTEN__
        .presentMode = WGPUPresentMode_Fifo,
    #else
        .presentMode = WGPUPresentMode_Immediate
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
    mCompositor.beginRenderPass(mCommandEncoder, mRenderStorageStack.last(), true);
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
        // apply normal blend
        if (comp->blend == BlendMethod::Normal) {
            // begin previous render pass
            mCompositor.beginRenderPass(mCommandEncoder, dst, false);
            // apply blend
            mCompositor.blendScene(mContext, src, comp);
        // apply custom blend
        } else {
            // apply custom blend
            mCompositor.blend(mCommandEncoder, src, dst, comp->opacity, comp->blend, WgRenderRasterType::Image);
            // begin previous render pass
            mCompositor.beginRenderPass(mCommandEncoder, dst, false);
        }
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


bool WgRenderer::effect(TVG_UNUSED RenderCompositor* cmp, TVG_UNUSED const RenderEffect* effect)
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
