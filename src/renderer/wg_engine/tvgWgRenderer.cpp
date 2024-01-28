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
}


void WgRenderer::release()
{
    mCompositorStack.clear();
    mRenderStorageStack.clear();
    mRenderStoragePool.release(mContext);
    mCompositeMethodPool.release(mContext);
    mBlendMethodPool.release(mContext);
    mOpacityPool.release(mContext);
    mRenderStorageRoot.release(mContext);
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
        renderDataShape = new WgRenderDataShape();
    
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
    mRenderStorageRoot.clear(mCommandEncoder);
    mRenderStorageStack.push(&mRenderStorageRoot);
    return true;
}


bool WgRenderer::renderShape(RenderData data)
{
    // render shape to render target
    mRenderTarget.renderShape(mCommandEncoder, (WgRenderDataShape *)data);
    // blend shape with current render storage
    WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
    mRenderStorageStack.last()->blend(mCommandEncoder, &mRenderTarget, blendMethod);
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
    // render image to render target
    mRenderTarget.renderPicture(mCommandEncoder, (WgRenderDataPicture *)data);
    // blend image with current render storage
    WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
    mRenderStorageStack.last()->blend(mCommandEncoder, &mRenderTarget, blendMethod);
    return true;
}


bool WgRenderer::postRender()
{
    mRenderStorageStack.pop();
    mContext.executeCommandEncoder(mCommandEncoder);
    wgpuCommandEncoderRelease(mCommandEncoder);
    return true;
}


bool WgRenderer::dispose(RenderData data)
{
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData) renderData->release(mContext);
    return true;
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

    WGPUImageCopyTexture source{};
    source.texture = mRenderStorageRoot.texStorage;
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

    mRenderTarget.initialize(mContext, w, h);
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

    mRenderTarget.initialize(mContext, w, h);
    mRenderStorageRoot.initialize(mContext, w, h);

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

    // allocate new render storage and push it to top of render tree
    WgRenderStorage* renderStorage = mRenderStoragePool.allocate(mContext, mTargetSurface.w, mTargetSurface.h);
    renderStorage->clear(mCommandEncoder);
    mRenderStorageStack.push(renderStorage);

    return true;
}


bool WgRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
    if (cmp->method == CompositeMethod::None) {
        // get two last render targets
        WgRenderStorage* renderStorageSrc = mRenderStorageStack.last();
        mRenderStorageStack.pop();
    
        // blent scene to current render storage
        WgBindGroupBlendMethod* blendMethod = mBlendMethodPool.allocate(mContext, mBlendMethod);
        mRenderStorageStack.last()->blend(mCommandEncoder, renderStorageSrc, blendMethod);

        // back render targets to the pool
        mRenderStoragePool.free(mContext, renderStorageSrc);
    } else {
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
