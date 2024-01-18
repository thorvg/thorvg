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
    mBindGroupOpacityPool.initialize(mContext);
}


void WgRenderer::release()
{
    mCompositorStack.clear();
    mRenderTargetStack.clear();
    mBindGroupOpacityPool.release(mContext);
    mRenderTargetPool.release(mContext);
    mRenderTargetRoot.release(mContext);
    mRenderTargetWnd.release(mContext);
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
            renderDataPicture->imageData.sampler,
            renderDataPicture->imageData.textureView);
    }

    return renderDataPicture;
}


bool WgRenderer::preRender()
{
    // command encoder descriptor
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    mCommandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);
    // render datas
    mRenderTargetStack.push(&mRenderTargetRoot);
    mRenderTargetRoot.beginRenderPass(mCommandEncoder, true);
    return true;
}


bool WgRenderer::renderShape(RenderData data)
{
    mRenderTargetStack.last()->renderShape((WgRenderDataShape *)data);
    mRenderTargetStack.last()->renderStroke((WgRenderDataShape *)data);
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
    mRenderTargetStack.last()->renderPicture((WgRenderDataPicture *)data);
    return true;
}


bool WgRenderer::postRender()
{
    mRenderTargetRoot.endRenderPass();
    mRenderTargetStack.pop();
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
    WGPUTextureView backBufferView = mContext.createTextureView2d(backBuffer.texture, "Surface texture view");
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);
    mRenderTargetWnd.beginRenderPass(commandEncoder, backBufferView, true);
    mRenderTargetWnd.blitColor(mContext, &mRenderTargetRoot);
    mRenderTargetWnd.endRenderPass();
    mContext.executeCommandEncoder(commandEncoder);
    wgpuCommandEncoderRelease(commandEncoder);
    wgpuTextureViewRelease(backBufferView);
    wgpuSurfacePresent(mSurface);
    return true;
}


bool WgRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h)
{
    // store target surface properties
    mTargetSurface.stride = stride;
    mTargetSurface.w = w;
    mTargetSurface.h = h;

    mRenderTargetRoot.initialize(mContext, w, h);
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

    // // get preferred format
    // //WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
    // WGPUTextureFormat swapChainFormat = WGPUTextureFormat_RGBA8Unorm;
    // // swapchain descriptor
    // WGPUSwapChainDescriptor swapChainDesc{};
    // swapChainDesc.nextInChain = nullptr;
    // swapChainDesc.label = "The swapchain";
    // swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    // swapChainDesc.format = swapChainFormat;
    // swapChainDesc.width = mTargetSurface.w;
    // swapChainDesc.height = mTargetSurface.h;
    // swapChainDesc.presentMode = WGPUPresentMode_Mailbox;

    WGPUSurfaceConfiguration surfaceConfiguration{};
    surfaceConfiguration.nextInChain = nullptr;
    surfaceConfiguration.device = mContext.device;
    surfaceConfiguration.format = WGPUTextureFormat_RGBA8Unorm;
    surfaceConfiguration.usage = WGPUTextureUsage_RenderAttachment;
    surfaceConfiguration.viewFormatCount = 0;
    surfaceConfiguration.viewFormats = nullptr;
    surfaceConfiguration.alphaMode = WGPUCompositeAlphaMode_Auto;
    surfaceConfiguration.width = mTargetSurface.w;
    surfaceConfiguration.height = mTargetSurface.h;
    surfaceConfiguration.presentMode = WGPUPresentMode_Mailbox;
    wgpuSurfaceConfigure(mSurface, &surfaceConfiguration);

    mRenderTargetWnd.initialize(mContext, w, h);
    mRenderTargetRoot.initialize(mContext, w, h);

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

    // end current render target
    mRenderTargetStack.last()->endRenderPass();

    // create new render target and begin new render pass
    WgRenderTarget* renderTarget = mRenderTargetPool.allocate(mContext, mTargetSurface.w, mTargetSurface.h);
    renderTarget->beginRenderPass(mCommandEncoder, true);
    mRenderTargetStack.push(renderTarget);

    return true;
}


bool WgRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
    if (cmp->method == CompositeMethod::None) {
        // end current render pass
        mRenderTargetStack.last()->endRenderPass();

        // get two last render targets
        WgRenderTarget* renderTargetSrc = mRenderTargetStack.last();
        mRenderTargetStack.pop();
    
        // apply current render target
        WgRenderTarget* renderTarget = mRenderTargetStack.last();
        renderTarget->beginRenderPass(mCommandEncoder, false);

        // blit scene to current render tartget
        WgBindGroupOpacity* mBindGroupOpacity = mBindGroupOpacityPool.allocate(mContext, cmp->opacity);
        renderTarget->blit(mContext, renderTargetSrc, mBindGroupOpacity);

        // back render targets to the pool
        mRenderTargetPool.free(mContext, renderTargetSrc);
    } else {
        // end current render pass
        mRenderTargetStack.last()->endRenderPass();

        // get two last render targets
        WgRenderTarget* renderTargetSrc = mRenderTargetStack.last();
        mRenderTargetStack.pop();
        WgRenderTarget* renderTargetMsk = mRenderTargetStack.last();
        mRenderTargetStack.pop();
    
        // apply current render target
        WgRenderTarget* renderTarget = mRenderTargetStack.last();
        renderTarget->beginRenderPass(mCommandEncoder, false);
        renderTarget->compose(mContext, renderTargetSrc, renderTargetMsk, cmp->method);

        // back render targets to the pool
        mRenderTargetPool.free(mContext, renderTargetSrc);
        mRenderTargetPool.free(mContext, renderTargetMsk);
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
