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

#include <iostream>
#ifdef _WIN32
// TODO: cross-platform realization
#include <windows.h>
#endif
#include "tvgWgRenderData.h"
#include "tvgWgShaderSrc.h"

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
    // create instance
    WGPUInstanceDescriptor instanceDesc{};
    instanceDesc.nextInChain = nullptr;
    mInstance = wgpuCreateInstance(&instanceDesc);
    assert(mInstance);

    // request adapter options
    WGPURequestAdapterOptions requestAdapterOptions{};
    requestAdapterOptions.nextInChain = nullptr;
    requestAdapterOptions.compatibleSurface = nullptr;
    requestAdapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;
    requestAdapterOptions.forceFallbackAdapter = false;
    // on adapter request ended function
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) {
        if (status != WGPURequestAdapterStatus_Success)
            TVGERR("WG_RENDERER", "Adapter request: %s", message);
        *((WGPUAdapter*)pUserData) = adapter;
    };
    // request adapter
    wgpuInstanceRequestAdapter(mInstance, &requestAdapterOptions, onAdapterRequestEnded, &mAdapter);
    assert(mAdapter);

    // adapter enumarate fueatures
    WGPUFeatureName featureNames[32]{};
    size_t featuresCount = wgpuAdapterEnumerateFeatures(mAdapter, featureNames);
    WGPUAdapterProperties adapterProperties{};
    wgpuAdapterGetProperties(mAdapter, &adapterProperties);
    WGPUSupportedLimits supportedLimits{};
    wgpuAdapterGetLimits(mAdapter, &supportedLimits);

    // reguest device
    WGPUDeviceDescriptor deviceDesc{};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "The device";
    deviceDesc.requiredFeaturesCount = featuresCount;
    deviceDesc.requiredFeatures = featureNames;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    deviceDesc.deviceLostCallback = nullptr;
    deviceDesc.deviceLostUserdata = nullptr;
    // on device request ended function
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) {
        if (status != WGPURequestDeviceStatus_Success)
            TVGERR("WG_RENDERER", "Device request: %s", message);
        *((WGPUDevice*)pUserData) = device;
    };
    // request device
    wgpuAdapterRequestDevice(mAdapter, &deviceDesc, onDeviceRequestEnded, &mDevice);
    assert(mDevice);

    // on device error function
    auto onDeviceError = [](WGPUErrorType type, char const* message, void* pUserData) {
        TVGERR("WG_RENDERER", "Uncaptured device error: %s", message);
        // TODO: remove direct error message
        std::cout << message << std::endl;
    };
    // set device error handling
    wgpuDeviceSetUncapturedErrorCallback(mDevice, onDeviceError, nullptr);

    mQueue = wgpuDeviceGetQueue(mDevice);
    assert(mQueue);
    
    // create pipelines
    mPipelines.initialize(mDevice);
}


void WgRenderer::release()
{
    mRenderTarget.release();
    mPipelines.release();
    if (mDevice) {
        wgpuDeviceDestroy(mDevice);
        wgpuDeviceRelease(mDevice);
    }
    if (mAdapter) wgpuAdapterRelease(mAdapter);
    if (mInstance) wgpuInstanceRelease(mInstance);
}


RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper)
{
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape) {
        renderDataShape = new WgRenderDataShape();
        renderDataShape->initialize(mDevice);
    }
    
    // update geometry
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Stroke)) {
        renderDataShape->releaseRenderData();
        renderDataShape->tesselate(mDevice, mQueue, rshape);
        renderDataShape->stroke(mDevice, mQueue, rshape);
    }

     // update paint settings
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettings blendSettings(mTargetSurface.cs);
        renderDataShape->mBindGroupPaint.initialize(mDevice, mQueue, modelMat, blendSettings);
    }

    // setup fill settings
    renderDataShape->mRenderSettingsShape.update(mDevice, mQueue, rshape.fill, rshape.color, flags);
    if (rshape.stroke)
        renderDataShape->mRenderSettingsStroke.update(mDevice, mQueue, rshape.stroke->fill, rshape.stroke->color, flags);

    return renderDataShape;
}


RenderData WgRenderer::prepare(TVG_UNUSED const Array<RenderData>& scene, TVG_UNUSED RenderData data, TVG_UNUSED const RenderTransform* transform, TVG_UNUSED Array<RenderData>& clips, TVG_UNUSED uint8_t opacity, TVG_UNUSED RenderUpdateFlag flags)
{
    return nullptr;
}


RenderData WgRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags)
{
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape*)data;
    if (!renderDataShape) {
        renderDataShape = new WgRenderDataShape();
        renderDataShape->initialize(mDevice);
    }

    // update paint settings
    if (flags & (RenderUpdateFlag::Transform | RenderUpdateFlag::Blend)) {
        WgShaderTypeMat4x4f modelMat(transform);
        WgShaderTypeBlendSettings blendSettings(surface->cs);
        renderDataShape->mBindGroupPaint.initialize(mDevice, mQueue, modelMat, blendSettings);
    }
    
    // update image data
    if (flags & (RenderUpdateFlag::Path | RenderUpdateFlag::Image)) {
        renderDataShape->releaseRenderData();
        renderDataShape->tesselate(mDevice, mQueue, surface, mesh);
        renderDataShape->mBindGroupPicture.initialize(
            mDevice, mQueue,
            renderDataShape->mImageData.mSampler,
            renderDataShape->mImageData.mTextureView);
    }

    return renderDataShape;
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


bool WgRenderer::dispose(RenderData data)
{
    auto renderData = (WgRenderData*)data;
    if (renderData) renderData->release();
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
    mClearBuffer = true;
    return true;
}


bool WgRenderer::sync()
{
    WGPUTextureView backBufferView = wgpuSwapChainGetCurrentTextureView(mSwapChain);
    
    // command encoder descriptor
    WGPUCommandEncoderDescriptor commandEncoderDesc{};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "The command encoder";
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mDevice, &commandEncoderDesc);

    // render datas
    mRenderTarget.beginRenderPass(commandEncoder, backBufferView);
    for (size_t i = 0; i < mRenderDatas.count; i++) {
        WgRenderDataShape* renderData = (WgRenderDataShape*)(mRenderDatas[i]);
        mRenderTarget.renderShape(renderData);
        mRenderTarget.renderStroke(renderData);
        mRenderTarget.renderImage(renderData);
    }
    mRenderTarget.endRenderPass();

    // execute command encoder
    WGPUCommandBufferDescriptor commandBufferDesc{};
    commandBufferDesc.nextInChain = nullptr;
    commandBufferDesc.label = "The command buffer";
    WGPUCommandBuffer commandsBuffer{};
    commandsBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDesc);
    wgpuQueueSubmit(mQueue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
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
    mSurface = wgpuInstanceCreateSurface(mInstance, &surfaceDesc);
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
    mSwapChain = wgpuDeviceCreateSwapChain(mDevice, mSurface, &swapChainDesc);
    assert(mSwapChain);

    mRenderTarget.initialize(mDevice, mQueue, mPipelines, w, h);
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
