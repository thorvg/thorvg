/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include "tvgWgpuRenderer.h"

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include "tvgWgpuRenderData.h"
#include "tvgWgpuShaderSrc.h"

// constructor
WgpuRenderer::WgpuRenderer() {
    initialize();
}

// destructor
WgpuRenderer::~WgpuRenderer() {
    release();
}

// initialize renderer
void WgpuRenderer::initialize() {
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
        #ifdef _DEBUG
        assert(!status);
        if (status != WGPURequestAdapterStatus_Success)
            std::cout << "Adapter request: " << message << std::endl;
        #endif
        *((WGPUAdapter *)pUserData) = adapter;
    };
    // request adapter
    wgpuInstanceRequestAdapter(mInstance, &requestAdapterOptions, onAdapterRequestEnded, &mAdapter);
    assert(mAdapter);

    // adapter enumarate fueatures
    WGPUFeatureName featureNames[32]{};
    size_t featuresCount = wgpuAdapterEnumerateFeatures(mAdapter, featureNames);
    // get adapter properties
    WGPUAdapterProperties adapterProperties{};
    wgpuAdapterGetProperties(mAdapter, &adapterProperties);
    // get supported limits
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
        #ifdef _DEBUG
        assert(!status);
        if (status != WGPURequestDeviceStatus_Success)
            std::cout << "Device request: " << message << std::endl;
        #endif
        *((WGPUDevice *)pUserData) = device;
    };
    // request device
    wgpuAdapterRequestDevice(mAdapter, &deviceDesc, onDeviceRequestEnded, &mDevice);
    assert(mDevice);

    #ifdef _DEBUG
    // on device error function
    auto onDeviceError = [](WGPUErrorType type, char const* message, void* pUserData) {
        std::cout << "Uncaptured device error: " << message << std::endl;
    };
    // set device error handling
    wgpuDeviceSetUncapturedErrorCallback(mDevice, onDeviceError, nullptr);
    #endif

    // get queue
    mQueue = wgpuDeviceGetQueue(mDevice);
    assert(mQueue);

    #ifdef _DEBUG
    // queue work done function
    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* pUserData) {
        std::cout << "Queued work finished with status: " << status << std::endl;
    };
    // submittet queue work done function
    wgpuQueueOnSubmittedWorkDone(mQueue, onQueueWorkDone, nullptr);
    #endif

    // create brushes
    mBrushColor.initialize(mDevice);
}

// release renderer
void WgpuRenderer::release() {
    // swapchaine release
    if (mSwapChain) wgpuSwapChainRelease(mSwapChain);
    // serface release
    if (mSurface) wgpuSurfaceRelease(mSurface);
    // create brushes
    mBrushColor.release();
    // device release
    if (mDevice) wgpuDeviceDestroy(mDevice);
    if (mDevice) wgpuDeviceRelease(mDevice);
    // adapter release
    if (mAdapter) wgpuAdapterRelease(mAdapter);
    // instance release
    if (mInstance) wgpuInstanceRelease(mInstance);
}

// prepare render shape
RenderData WgpuRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) {
    // get or create render data shape
    auto renderDataShape = (WgpuRenderDataShape *)data;
    if (!renderDataShape) {
        renderDataShape = new WgpuRenderDataShape(&rshape);
        renderDataShape->initialize(mDevice);
        renderDataShape->mBrushColorDataBindGroup.initialize(mDevice, mBrushColor);
    }

    static float vertexBuffer[] = {
        0.0f,     0.0f, 0.0f,
        100.0f,   0.0f, 0.0f,
        0.0f,   100.0f, 0.0f,
        200.0f, 800.0f, 0.0f
    };
    static uint32_t indexBuffer[] = {
        0, 1, 2,
        1, 2, 3
    };
    size_t vertexCount = sizeof(vertexBuffer) / sizeof(float) / 3;
    size_t indexCount = sizeof(indexBuffer) / sizeof(uint32_t);

    // update geometry data
    renderDataShape->mGeometryDataFill.update(mDevice, mQueue, vertexBuffer, vertexCount, indexBuffer, indexCount);

    // brush color data
    WgpuBrushColorData mBrushColorData{};
    mBrushColorData.updateMatrix(mViewMatrix, transform);
    mBrushColorData.uColorInfo = { 1.0f, 0.0f, 1.0f, 1.0f };
    // update brush color data
    renderDataShape->mBrushColorDataBindGroup.update(mQueue, mBrushColorData);

    // return render data shape
    return renderDataShape;
}

// prepare scene
RenderData WgpuRenderer::prepare(const Array<RenderData>& scene, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    // nothing
    return nullptr;
}

// prepare surface
RenderData WgpuRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    // nothing
    return nullptr;
}

// pre render
bool WgpuRenderer::preRender() {
    // nothing
    return true;
}

// render shape
bool WgpuRenderer::renderShape(RenderData data) {
    mRenderDatas.push(data);
    return true;
}

// render image
bool WgpuRenderer::renderImage(RenderData data) {
    // nothing
    return true;
}

// post render
bool WgpuRenderer::postRender() {
    // nothing
    return true;
}

// dispose render data
bool WgpuRenderer::dispose(RenderData data) {
    // release render data
    auto renderData = (WgpuRenderData *)data;
    if (renderData)
        renderData->release();
    // nothing
    return true;
}

// get region of render data
RenderRegion WgpuRenderer::region(RenderData data) {
    // nothing
    return { 0, 0, INT32_MAX, INT32_MAX };
}

// get current viewport
RenderRegion WgpuRenderer::viewport() {
    // nothing
    return { 0, 0, INT32_MAX, INT32_MAX };
}

// set current viewport
bool WgpuRenderer::viewport(const RenderRegion& vp) {
    // nothing
    return true;
}

// blend
bool WgpuRenderer::blend(BlendMethod method) {
    return false;
}

// color space
ColorSpace WgpuRenderer::colorSpace() {
    return ColorSpace::Unsupported;
}

// clear
bool WgpuRenderer::clear() {
    return true;
}

// sync
bool WgpuRenderer::sync() {
    // get buffer
    WGPUTextureView backBufferView = wgpuSwapChainGetCurrentTextureView(mSwapChain);
    
    // command buffer descriptor
    WGPUCommandBufferDescriptor commandBufferDesc{};
    commandBufferDesc.nextInChain = nullptr;
    commandBufferDesc.label = "The command buffer";
    WGPUCommandBuffer commandsBuffer = nullptr; {
        // command encoder descriptor
        WGPUCommandEncoderDescriptor commandEncoderDesc{};
        commandEncoderDesc.nextInChain = nullptr;
        commandEncoderDesc.label = "The command encoder";
        // begin render pass
        WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(mDevice, &commandEncoderDesc); {
            // render pass color attachment
            WGPURenderPassColorAttachment colorAttachment{};
            colorAttachment.view = backBufferView;
            colorAttachment.resolveTarget = nullptr;
            colorAttachment.loadOp = WGPULoadOp_Clear;
            colorAttachment.storeOp = WGPUStoreOp_Store;
            colorAttachment.clearValue = { 0.1f, 0.1f, 0.1f, 1.0 };
            // render pass descriptor
            WGPURenderPassDescriptor renderPassDesc{};
            renderPassDesc.nextInChain = nullptr;
            renderPassDesc.label = "The render pass";
            renderPassDesc.colorAttachmentCount = 1;
            renderPassDesc.colorAttachments = &colorAttachment;
            renderPassDesc.depthStencilAttachment = nullptr;
            renderPassDesc.occlusionQuerySet = nullptr;
            renderPassDesc.timestampWriteCount = 0;
            renderPassDesc.timestampWrites = nullptr;
            // begin render pass
            WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc); {
                wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mBrushColor.mRenderPipeline);
                for (size_t i = 0; i < mRenderDatas.count; i++) {
                    WgpuRenderData* renderData = (WgpuRenderData*)(mRenderDatas[i]);
                    renderData->sync(renderPassEncoder);
                }
                mRenderDatas.clear();
            }
            // end render pass
            wgpuRenderPassEncoderEnd(renderPassEncoder);
            wgpuRenderPassEncoderRelease(renderPassEncoder);

            // release backbuffer and present
            wgpuTextureViewRelease(backBufferView);
        }
        commandsBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDesc);
        wgpuCommandEncoderRelease(commandEncoder);
    }
    // queue submit and command buffer release
    wgpuQueueSubmit(mQueue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
    
    wgpuSwapChainPresent(mSwapChain);
    return true;
}

// target
bool WgpuRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) {
    // check properties
    assert(w > 0);
    assert(h > 0);

    // store target surface properties
    mTargetSurface.stride = stride;
    mTargetSurface.w = w;
    mTargetSurface.h = h;

    // update view matrix
    mViewMatrix[ 0] = +2.0f / w; mViewMatrix[ 1] = +0.0f;     mViewMatrix[ 2] = +0.0f; mViewMatrix[ 3] = +0.0f;
    mViewMatrix[ 4] = +0.0f;     mViewMatrix[ 5] = -2.0f / h; mViewMatrix[ 6] = +0.0f; mViewMatrix[ 7] = +0.0f;
    mViewMatrix[ 8] = +0.0f;     mViewMatrix[ 9] = +0.0f;     mViewMatrix[10] = -1.0f; mViewMatrix[11] = +0.0f;
    mViewMatrix[12] = -1.0f;     mViewMatrix[13] = +1.0f;     mViewMatrix[14] = +0.0f; mViewMatrix[15] = +1.0f;

    // return result
    return true;
}

// target for native window handle
bool WgpuRenderer::target(void* window, uint32_t w, uint32_t h) {
    // check properties
    assert(window);
    assert(w > 0);
    assert(h > 0);

    // store target surface properties
    mTargetSurface.stride = w;
    mTargetSurface.w = w > 0 ? w : 1;
    mTargetSurface.h = h > 0 ? h : 1;

    // update view matrix
    mViewMatrix[ 0] = +2.0f / w; mViewMatrix[ 1] = +0.0f;     mViewMatrix[ 2] = +0.0f; mViewMatrix[ 3] = +0.0f;
    mViewMatrix[ 4] = +0.0f;     mViewMatrix[ 5] = -2.0f / h; mViewMatrix[ 6] = +0.0f; mViewMatrix[ 7] = +0.0f;
    mViewMatrix[ 8] = +0.0f;     mViewMatrix[ 9] = +0.0f;     mViewMatrix[10] = -1.0f; mViewMatrix[11] = +0.0f;
    mViewMatrix[12] = -1.0f;     mViewMatrix[13] = +1.0f;     mViewMatrix[14] = +0.0f; mViewMatrix[15] = +1.0f;

    #ifdef _WIN32
    // surface descriptor from windows hwnd
    WGPUSurfaceDescriptorFromWindowsHWND surfaceDescHwnd{};
    surfaceDescHwnd.chain.next = nullptr;
    surfaceDescHwnd.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
    surfaceDescHwnd.hinstance = GetModuleHandle(NULL);
    surfaceDescHwnd.hwnd = (HWND)window;
    // surface descriptor
    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = (const WGPUChainedStruct*)&surfaceDescHwnd;
    surfaceDesc.label = "The surface";
    // create surface
    mSurface = wgpuInstanceCreateSurface(mInstance, &surfaceDesc);
    assert(mSurface);
    #endif

    // get preferred format
    //WGPUTextureFormat swapChainFormat = wgpuSurfaceGetPreferredFormat(mSurface, mAdapter);
    //WGPUTextureFormat swapChainFormat = WGPUTextureFormat_RGBA8Unorm;
    WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8UnormSrgb;
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

    return true;
}

// target
Compositor* WgpuRenderer::target(const RenderRegion& region, ColorSpace cs) {
    // not supported
    return nullptr;
}

// begin composite
bool WgpuRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity) {
    //TODO: delete the given compositor and restore the context
    return false;
}

// end composite
bool WgpuRenderer::endComposite(Compositor* cmp) {
    //TODO: delete the given compositor and restore the context
    return false;
}

// generate renderer webgpu
WgpuRenderer* WgpuRenderer::gen() {
    // create new renderer instance
    return new WgpuRenderer();
}

// initialize renderer
bool WgpuRenderer::init(uint32_t threads) {
    return true;
}

// terminate renderer
bool WgpuRenderer::term() {
    return true;
}
