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

#ifdef _WIN32
// TODO: cross-platform realization
#include <windows.h>
#endif

#define WG_SSAA_SAMPLES (2)

WgRenderer::WgRenderer()
{
    initialize();
}


WgRenderer::~WgRenderer()
{
    release();
}


void WgRenderer::initialize()
{
    mContext.initialize();
    mPipelines.initialize(mContext);
    mOpacityPool.initialize(mContext);
    mBlendMethodPool.initialize(mContext);
    mCompositeMethodPool.initialize(mContext);
    WgMeshDataGroup::MeshDataPool = new WgMeshDataPool();
}


void WgRenderer::release()
{
    mRenderDataShapePool.release(mContext);
    WgMeshDataGroup::MeshDataPool->release(mContext);
    delete WgMeshDataGroup::MeshDataPool;
    mCompositorStack.clear();
    mRenderStorageStack.clear();
    mRenderStoragePool.release(mContext);
    mCompositeMethodPool.release(mContext);
    mBlendMethodPool.release(mContext);
    mOpacityPool.release(mContext);
    mRenderStorageRoot.release(mContext);
    mRenderStorageScreen.release(mContext);
    mRenderTarget.release(mContext);
    wgpuSurfaceUnconfigure(mSurface);
    wgpuSurfaceRelease(mSurface);
    mPipelines.release();
    mContext.release();
}


RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape)
        renderDataShape = mRenderDataShapePool.allocate(mContext);
    
    // update geometry
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Stroke))
        renderDataShape->updateMeshes(mContext, rshape);

     // update paint settings
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettings blendSettings(mTargetSurface.cs, opacity);
        renderDataShape->bindGroupPaint.initialize(mContext.device, mContext.queue, modelMat, blendSettings);
    }

    // setup fill settings
    renderDataShape->renderSettingsShape.update(mContext, rshape.fill, rshape.color, flags);
    if (rshape.stroke)
        renderDataShape->renderSettingsStroke.update(mContext, rshape.stroke->fill, rshape.stroke->color, flags);

    return renderDataShape;
}


RenderData WgRenderer::prepare(TVG_UNUSED const Array<RenderData>& scene, TVG_UNUSED RenderData data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, TVG_UNUSED RenderUpdateFlag flags)
{
    return nullptr;
}


RenderData WgRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    // get or create render data shape
    auto renderDataPicture = (WgRenderDataPicture*)data;
    if (!renderDataPicture)
        renderDataPicture = new WgRenderDataPicture();

    // update paint settings
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettings blendSettings(surface->cs, opacity);
        renderDataPicture->bindGroupPaint.initialize(mContext.device, mContext.queue, modelMat, blendSettings);
    }

    // update image data
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Image)) {
        WgGeometryData geometryData;
        if (mesh->triangleCnt == 0) geometryData.appendImageBox(surface->w, surface->h);
        else geometryData.appendMesh(mesh);
        renderDataPicture->meshData.release(mContext);
        renderDataPicture->meshData.update(mContext, &geometryData);
        renderDataPicture->imageData.update(mContext, surface);
        renderDataPicture->bindGroupPicture.initialize(
            mContext.device, mContext.queue,
            mContext.samplerLinear,
            renderDataPicture->imageData.textureView);
    }

    return renderDataPicture;
}

bool WgRenderer::preRender()
{
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    mCommandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);
    mRenderStorageStack.push(&mRenderStorageRoot);
    //mRenderStorageRoot.clear(mCommandEncoder);
    mRenderStorageRoot.beginRenderPass(mCommandEncoder, true);
    return true;
}


bool WgRenderer::renderShape(RenderData data)
{
    // get current render storage
    WgPipelineBlendType blendType = WgPipelines::blendMethodToBlendType(mBlendMethod);
    WgRenderStorage* renderStorage = mRenderStorageStack.last();
    assert(renderStorage);
    // use hardware blend
    if (WgPipelines::isBlendMethodSupportsHW(mBlendMethod))
        renderStorage->renderShape((WgRenderDataShape *)data, blendType);
    else { // use custom blend
        // terminate current render pass
        renderStorage->endRenderPass();
        // render image to render target
        mRenderTarget.beginRenderPass(mCommandEncoder, true);
        mRenderTarget.renderShape((WgRenderDataShape *)data, blendType);
        mRenderTarget.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        renderStorage->blend(mCommandEncoder, &mRenderTarget, blendMethod);
        // restore current render pass
        renderStorage->beginRenderPass(mCommandEncoder, false);
    }
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
    // get current render storage
    WgPipelineBlendType blendType = WgPipelines::blendMethodToBlendType(mBlendMethod);
    WgRenderStorage* renderStorage = mRenderStorageStack.last();
    assert(renderStorage);
    // use hardware blend
    if (WgPipelines::isBlendMethodSupportsHW(mBlendMethod))
        renderStorage->renderPicture((WgRenderDataPicture *)data, blendType);
    else { // use custom blend
        // terminate current render pass
        renderStorage->endRenderPass();
        // render image to render target
        mRenderTarget.beginRenderPass(mCommandEncoder, true);
        mRenderTarget.renderPicture((WgRenderDataPicture *)data, blendType);
        mRenderTarget.endRenderPass();
        // blend shape with current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        renderStorage->blend(mCommandEncoder, &mRenderTarget, blendMethod);
        // restore current render pass
        renderStorage->beginRenderPass(mCommandEncoder, false);
    }
    return true;
}


bool WgRenderer::postRender()
{
    mRenderStorageRoot.endRenderPass();
    mRenderStorageStack.pop();
    mContext.executeCommandEncoder(mCommandEncoder);
    wgpuCommandEncoderRelease(mCommandEncoder);
    return true;
}


void WgRenderer::dispose(RenderData data)
{
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData) {
        if (renderData->identifier() == TVG_CLASS_ID_SHAPE)
            mRenderDataShapePool.free(mContext, (WgRenderDataShape*)renderData);
        else
            renderData->release(mContext);
    }
}


RenderRegion WgRenderer::region(TVG_UNUSED RenderData data)
{
    return { 0, 0, INT32_MAX, INT32_MAX };
}


RenderRegion WgRenderer::viewport() {
    return { 0, 0, INT32_MAX, INT32_MAX };
}


bool WgRenderer::viewport(TVG_UNUSED const RenderRegion& vp)
{
    return true;
}


bool WgRenderer::blend(TVG_UNUSED BlendMethod method)
{
    mBlendMethod = method;
    return false;
}


ColorSpace WgRenderer::colorSpace()
{
    return ColorSpace::Unsupported;
}


bool WgRenderer::clear()
{
    return true;
}


bool WgRenderer::sync()
{
    WGPUSurfaceTexture backBuffer{};
    wgpuSurfaceGetCurrentTexture(mSurface, &backBuffer);
    
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);

    mRenderStorageScreen.antialias(commandEncoder, &mRenderStorageRoot);

    WGPUImageCopyTexture source{};
    source.texture = mRenderStorageScreen.texColor;
    WGPUImageCopyTexture dest{};
    dest.texture = backBuffer.texture;
    WGPUExtent3D copySize{};
    copySize.width = mTargetSurface.w;
    copySize.height = mTargetSurface.h;
    copySize.depthOrArrayLayers = 1;
    wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &source, &dest, &copySize);

    mContext.executeCommandEncoder(commandEncoder);
    wgpuCommandEncoderRelease(commandEncoder);
    
    wgpuSurfacePresent(mSurface);
    return true;
}


bool WgRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h)
{
    // store target surface properties
    mTargetSurface.stride = stride;
    mTargetSurface.w = w;
    mTargetSurface.h = h;

    mRenderTarget.initialize(mContext, w * WG_SSAA_SAMPLES, h * WG_SSAA_SAMPLES);
    return true;
}


// target for native window handle
bool WgRenderer::target(void* window, uint32_t w, uint32_t h)
{
    // store target surface properties
    mTargetSurface.stride = w;
    mTargetSurface.w = w > 0 ? w : 1;
    mTargetSurface.h = h > 0 ? h : 1;

    // TODO: replace solution to cross-platform realization
    // surface descriptor from windows hwnd
    WGPUSurfaceDescriptorFromWindowsHWND surfaceDescHwnd{};
    surfaceDescHwnd.chain.next = nullptr;
    surfaceDescHwnd.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
    surfaceDescHwnd.hinstance = GetModuleHandle(NULL);
    surfaceDescHwnd.hwnd = (HWND)window;
    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = (const WGPUChainedStruct*)&surfaceDescHwnd;
    surfaceDesc.label = "The surface";
    mSurface = wgpuInstanceCreateSurface(mContext.instance, &surfaceDesc);
    assert(mSurface);

    WGPUSurfaceConfiguration surfaceConfiguration{};
    surfaceConfiguration.nextInChain = nullptr;
    surfaceConfiguration.device = mContext.device;
    surfaceConfiguration.format = WGPUTextureFormat_RGBA8Unorm;
    surfaceConfiguration.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
    surfaceConfiguration.viewFormatCount = 0;
    surfaceConfiguration.viewFormats = nullptr;
    surfaceConfiguration.alphaMode = WGPUCompositeAlphaMode_Auto;
    surfaceConfiguration.width = mTargetSurface.w;
    surfaceConfiguration.height = mTargetSurface.h;
    surfaceConfiguration.presentMode = WGPUPresentMode_Mailbox;
    wgpuSurfaceConfigure(mSurface, &surfaceConfiguration);

    mRenderTarget.initialize(mContext, w, h, WG_SSAA_SAMPLES);
    mRenderStorageRoot.initialize(mContext, w, h, WG_SSAA_SAMPLES);
    mRenderStorageScreen.initialize(mContext, w, h);

    return true;
}


Compositor* WgRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    mCompositorStack.push(new Compositor);
    return mCompositorStack.last();
}


bool WgRenderer::beginComposite(TVG_UNUSED Compositor* cmp, TVG_UNUSED CompositeMethod method, TVG_UNUSED uint8_t opacity)
{
    // save current composition settings
    cmp->method = method;
    cmp->opacity = opacity;

    // end current render pass
    mRenderStorageStack.last()->endRenderPass();
    // allocate new render storage and push it to top of render tree
    WgRenderStorage* renderStorage = mRenderStoragePool.allocate(mContext, mTargetSurface.w * WG_SSAA_SAMPLES, mTargetSurface.h * WG_SSAA_SAMPLES);
    mRenderStorageStack.push(renderStorage);
    // begin last render pass
    mRenderStorageStack.last()->beginRenderPass(mCommandEncoder, true);
    return true;
}


bool WgRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
    if (cmp->method == CompositeMethod::None) {
        // end current render pass
        mRenderStorageStack.last()->endRenderPass();
        // get two last render targets
        WgRenderStorage* renderStorageSrc = mRenderStorageStack.last();
        mRenderStorageStack.pop();
    
        // blent scene to current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        mRenderStorageStack.last()->blend(mCommandEncoder, renderStorageSrc, blendMethod);

        // back render targets to the pool
        mRenderStoragePool.free(mContext, renderStorageSrc);
        // begin last render pass
        mRenderStorageStack.last()->beginRenderPass(mCommandEncoder, false);
    } else {
        // end current render pass
        mRenderStorageStack.last()->endRenderPass();
        // get two last render targets
        WgRenderStorage* renderStorageSrc = mRenderStorageStack.last();
        mRenderStorageStack.pop();
        WgRenderStorage* renderStorageMsk = mRenderStorageStack.last();
        mRenderStorageStack.pop();
    
        // compose shape and mask
        WgBindGroupOpacity* opacity = mOpacityPool.allocate(mContext, cmp->opacity);
        WgBindGroupCompositeMethod* composeMethod = mCompositeMethodPool.allocate(mContext, cmp->method);
        renderStorageSrc->compose(mCommandEncoder, renderStorageMsk, composeMethod, opacity);

        // blent scene to current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        mRenderStorageStack.last()->blend(mCommandEncoder, renderStorageSrc, blendMethod);

        // back render targets to the pool
        mRenderStoragePool.free(mContext, renderStorageSrc);
        mRenderStoragePool.free(mContext, renderStorageMsk);
        // begin last render pass
        mRenderStorageStack.last()->beginRenderPass(mCommandEncoder, false);
    }

    // delete current compositor
    delete mCompositorStack.last();
    mCompositorStack.pop();

    return true;
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
