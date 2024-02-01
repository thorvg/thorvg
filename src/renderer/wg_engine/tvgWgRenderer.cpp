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
    mPipelines.initialize(mContext.device);
}


void WgRenderer::release()
{
    mPipelines.release();
    if (mSwapChain) wgpuSwapChainRelease(mSwapChain);
    if (mSurface) wgpuSurfaceRelease(mSurface);
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
    return true;
}


bool WgRenderer::renderShape(RenderData data)
{
    mRenderDatas.push(data);
    return true;
}


bool WgRenderer::renderImage(RenderData data)
{
    mRenderDatas.push(data);
    return true;
}


bool WgRenderer::postRender()
{
    return true;
}


void WgRenderer::dispose(RenderData data)
{
    auto renderData = (WgRenderDataPaint*)data;
    if (renderData) renderData->release(mContext);
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
    WGPUTextureView backBufferView = wgpuSwapChainGetCurrentTextureView(mSwapChain);
    
    // command encoder descriptor
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mContext.device, &commandEncoderDesc);

    // render datas
    mRenderTarget.beginRenderPass(commandEncoder, backBufferView);
    for (size_t i = 0; i < mRenderDatas.count; i++) {
        WgRenderDataPaint* renderData = (WgRenderDataShape*)(mRenderDatas[i]);
        if (renderData->identifier() == TVG_CLASS_ID_SHAPE) {
            mRenderTarget.renderShape((WgRenderDataShape *)renderData);
            mRenderTarget.renderStroke((WgRenderDataShape *)renderData);
        } else if (renderData->identifier() == TVG_CLASS_ID_PICTURE) {
            mRenderTarget.renderPicture((WgRenderDataPicture *)renderData);
        }
    }
    mRenderTarget.endRenderPass();

    mContext.executeCommandEncoder(commandEncoder);
    wgpuCommandEncoderRelease(commandEncoder);

    // go to the next frame
    wgpuTextureViewRelease(backBufferView);
    wgpuSwapChainPresent(mSwapChain);

    mRenderDatas.clear();

    return true;
}


bool WgRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h)
{
    // store target surface properties
    mTargetSurface.stride = stride;
    mTargetSurface.w = w;
    mTargetSurface.h = h;

    // TODO: Add ability to render into offscreen buffer
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

    // get preferred format
    WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
    // swapchain descriptor
    WGPUSwapChainDescriptor swapChainDesc{};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.label = "The swapchain";
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.format = swapChainFormat;
    swapChainDesc.width = mTargetSurface.w;
    swapChainDesc.height = mTargetSurface.h;
    swapChainDesc.presentMode = WGPUPresentMode_Mailbox;
    mSwapChain = wgpuDeviceCreateSwapChain(mContext.device, mSurface, &swapChainDesc);
    assert(mSwapChain);

    mRenderTarget.initialize(mContext, mPipelines, w, h);
    return true;
}


Compositor* WgRenderer::target(TVG_UNUSED const RenderRegion& region, TVG_UNUSED ColorSpace cs)
{
    return nullptr;
}


bool WgRenderer::beginComposite(TVG_UNUSED Compositor* cmp, TVG_UNUSED CompositeMethod method, TVG_UNUSED uint8_t opacity)
{
    return false;
}


bool WgRenderer::endComposite(TVG_UNUSED Compositor* cmp)
{
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
