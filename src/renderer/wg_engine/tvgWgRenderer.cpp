/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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
#include "tvgWgRenderData.h"

WgRenderer::WgRenderer() {
    initialize();
}

WgRenderer::~WgRenderer() {
    release();
}

void WgRenderer::initialize() {
    mContext.initialize();
    mPipelines.initialize(mContext.mDevice);
}

void WgRenderer::release() {
    mPipelines.release();
    if (mSwapChain) wgpuSwapChainRelease(mSwapChain);
    if (mSurface) wgpuSurfaceRelease(mSurface);
    mPipelines.release();
    mContext.release();
}

RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) {
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape) {
        renderDataShape = new WgRenderDataShape();
        renderDataShape->initialize(mContext.mDevice);
    }
    
    // update geometry
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Stroke)) {
        renderDataShape->releaseRenderData();
        renderDataShape->tesselate(mContext.mDevice, mContext.mQueue, rshape);
        renderDataShape->stroke(mContext.mDevice, mContext.mQueue, rshape);
    }

    // setup shape properties
    renderDataShape->mRenderSettingsShape.update(mContext.mDevice, mContext.mQueue, rshape.fill, rshape.color, flags);
    if (rshape.stroke)
        renderDataShape->mRenderSettingsStroke.update(mContext.mDevice, mContext.mQueue, rshape.stroke->fill, rshape.stroke->color, flags);

    // update paint settings
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettigs blendSettigs(ColorSpace::ABGR8888, opacity);
        renderDataShape->mBindGroupPaint.initialize(mContext.mDevice, mContext.mQueue, modelMat, blendSettigs);
    }

    return renderDataShape;
}

RenderData WgRenderer::prepare(const Array<RenderData>& scene, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    return nullptr;
}

RenderData WgRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape) {
        renderDataShape = new WgRenderDataShape();
        renderDataShape->initialize(mContext.mDevice);
    }
    
    // update image data
    if (flags & (RenderUpdateFlag::Color | RenderUpdateFlag::Image)) {
        renderDataShape->releaseRenderData();
        renderDataShape->tesselate(mContext.mDevice, mContext.mQueue, surface, mesh);
        renderDataShape->mBindGroupPicture.initialize(mContext.mDevice, mContext.mQueue,
            renderDataShape->mImageData.mSampler,
            renderDataShape->mImageData.mTextureView);
    }

    // update paint settings
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettigs blendSettigs(surface->cs, opacity);
        renderDataShape->mBindGroupPaint.initialize(mContext.mDevice, mContext.mQueue, modelMat, blendSettigs);
    }

    return renderDataShape;
}

bool WgRenderer::dispose(RenderData data) {
    auto renderData = (WgRenderData*)data;
    if (renderData) renderData->release();
    return true;
}

RenderRegion WgRenderer::region(RenderData data) {
    return { 0, 0, INT32_MAX, INT32_MAX };
}

RenderRegion WgRenderer::viewport() {
    return { 0, 0, INT32_MAX, INT32_MAX };
}

bool WgRenderer::viewport(const RenderRegion& vp) {
    return true;
}

bool WgRenderer::blend(BlendMethod method) {
    return false;
}

ColorSpace WgRenderer::colorSpace() {
    return ColorSpace::Unsupported;
}

bool WgRenderer::clear() {
    return true;
}

bool WgRenderer::sync() {
    WGPUTextureView backBufferView = wgpuSwapChainGetCurrentTextureView(mSwapChain);
    
    // command encoder descriptor
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mContext.mDevice, &commandEncoderDesc);

    // render datas
    mmRenderTarget.beginRenderPass(commandEncoder, backBufferView);
    for (size_t i = 0; i < mRenderDatas.count; i++) {
        WgRenderDataShape* renderData = (WgRenderDataShape*)(mRenderDatas[i]);
        mmRenderTarget.renderShape(renderData);
        mmRenderTarget.renderStroke(renderData);
        mmRenderTarget.renderImage(renderData);
    }
    mmRenderTarget.endRenderPass();

    // execute command encoder
    mContext.executeCommandEncoder(commandEncoder);
    wgpuCommandEncoderRelease(commandEncoder);

    // go to the next frame
    wgpuTextureViewRelease(backBufferView);
    wgpuSwapChainPresent(mSwapChain);

    mRenderDatas.clear();

    return true;
}

bool WgRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) {
    return false;
}

// target for native window handle
bool WgRenderer::target(void* window, uint32_t w, uint32_t h) {
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
    mSurface = wgpuInstanceCreateSurface(mContext.mInstance, &surfaceDesc);
    assert(mSurface);
    // get preferred format
    WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
    // swapchain descriptor
    WGPUSwapChainDescriptor swapChainDesc{};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.label = "The swapchain";
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.format = swapChainFormat;
    swapChainDesc.width = w;
    swapChainDesc.height = h;
    swapChainDesc.presentMode = WGPUPresentMode_Mailbox;
    mSwapChain = wgpuDeviceCreateSwapChain(mContext.mDevice, mSurface, &swapChainDesc);
    assert(mSwapChain);
    mmRenderTarget.initialize(mContext, mPipelines, w, h);
    return true;
}

bool WgRenderer::preRender() {
    return true;
}

Compositor* WgRenderer::target(const RenderRegion& region, ColorSpace cs) {
    return make_unique<tvg::Compositor>().get();
}

bool WgRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity) {
    return true;
}

bool WgRenderer::renderShape(RenderData data) {
    mRenderDatas.push(data);
    return true;
}

bool WgRenderer::renderImage(RenderData data) {
    mRenderDatas.push(data);
    return true;
}

bool WgRenderer::endComposite(Compositor* cmp) {
    return true;
}

bool WgRenderer::postRender() {
    return true;
}

WgRenderer* WgRenderer::gen() {
    return new WgRenderer();
}

bool WgRenderer::init(uint32_t threads) {
    return true;
}

bool WgRenderer::term() {
    return true;
}
