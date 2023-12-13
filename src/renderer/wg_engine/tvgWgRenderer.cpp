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

WgRenderer::WgRenderer() {
    initialize();
}

WgRenderer::~WgRenderer() {
    release();
}

void WgRenderer::initialize() {
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

void WgRenderer::release() {
    if (mStencilTex) {
        wgpuTextureDestroy(mStencilTex);
        wgpuTextureRelease(mStencilTex);
    }
    if (mStencilTexView) wgpuTextureViewRelease(mStencilTexView);
    if (mSwapChain) wgpuSwapChainRelease(mSwapChain);
    if (mSurface) wgpuSurfaceRelease(mSurface);
    mBindGroupCanvasWnd.release();
    mBindGroupPaintWnd.release();
    mGeometryDataWnd.release();
    mPipelines.release();
    if (mDevice) {
        wgpuDeviceDestroy(mDevice);
        wgpuDeviceRelease(mDevice);
    }
    if (mAdapter) wgpuAdapterRelease(mAdapter);
    if (mInstance) wgpuInstanceRelease(mInstance);
}

RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) {
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

RenderData WgRenderer::prepare(const Array<RenderData>& scene, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    return nullptr;
}

RenderData WgRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
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

bool WgRenderer::preRender() {
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

bool WgRenderer::postRender() {
    return true;
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
            // render pass depth stencil attachment
            WGPURenderPassDepthStencilAttachment depthStencilAttachment{};
            depthStencilAttachment.view = mStencilTexView;
            depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
            depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
            depthStencilAttachment.depthClearValue = 1.0f;
            depthStencilAttachment.depthReadOnly = false;
            depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
            depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
            depthStencilAttachment.stencilClearValue = 0;
            depthStencilAttachment.stencilReadOnly = false;
            // render pass color attachment
            WGPURenderPassColorAttachment colorAttachment{};
            colorAttachment.view = backBufferView;
            colorAttachment.resolveTarget = nullptr;
            if (mClearBuffer) {
                colorAttachment.loadOp = WGPULoadOp_Clear;
                colorAttachment.clearValue = {0, 0, 0, 0};
            } else {
                colorAttachment.loadOp = WGPULoadOp_Load;
            }
            colorAttachment.storeOp = WGPUStoreOp_Store;
            colorAttachment.clearValue = { 0.0f, 0.0f, 0.0f, 1.0 };
            // render pass descriptor
            WGPURenderPassDescriptor renderPassDesc{};
            renderPassDesc.nextInChain = nullptr;
            renderPassDesc.label = "The render pass";
            renderPassDesc.colorAttachmentCount = 1;
            renderPassDesc.colorAttachments = &colorAttachment;
            renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
            //renderPassDesc.depthStencilAttachment = nullptr;
            renderPassDesc.occlusionQuerySet = nullptr;
            renderPassDesc.timestampWriteCount = 0;
            renderPassDesc.timestampWrites = nullptr;
            // begin render pass
            WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc); {
                // iterate render data
                for (size_t i = 0; i < mRenderDatas.count; i++) {
                    WgRenderDataShape* renderData = (WgRenderDataShape*)(mRenderDatas[i]);
                    
                    // draw shape geometry
                    wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
                    for (uint32_t j = 0; j < renderData->mGeometryDataShape.count; j++) {
                        // draw to stencil (first pass)
                        mPipelines.mPipelineFillShape.use(renderPassEncoder, mBindGroupCanvasWnd, renderData->mBindGroupPaint);
                        renderData->mGeometryDataShape[j]->draw(renderPassEncoder);
                        // fill shape (second pass)
                        WgRenderDataShapeSettings& settings = renderData->mRenderSettingsShape;
                        if (settings.mFillType == WgRenderDataShapeFillType::Solid)
                            mPipelines.mPipelineSolid.use(renderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupSolid);
                        else if (settings.mFillType == WgRenderDataShapeFillType::Linear)
                            mPipelines.mPipelineLinear.use(renderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupLinear);
                        else if (settings.mFillType == WgRenderDataShapeFillType::Radial)
                            mPipelines.mPipelineRadial.use(renderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupRadial);
                        mGeometryDataWnd.draw(renderPassEncoder);
                    }

                    // draw stroke geometry
                    if (renderData->mGeometryDataStroke.count > 0) {
                        // draw strokes to stencil (first pass)
                        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 255);
                        for (uint32_t j = 0; j < renderData->mGeometryDataStroke.count; j++) {
                            mPipelines.mPipelineFillStroke.use(renderPassEncoder, mBindGroupCanvasWnd, renderData->mBindGroupPaint);
                            renderData->mGeometryDataStroke[j]->draw(renderPassEncoder);
                        }
                        // fill shape (second pass)
                        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
                        WgRenderDataShapeSettings& settings = renderData->mRenderSettingsStroke;
                        if (settings.mFillType == WgRenderDataShapeFillType::Solid)
                            mPipelines.mPipelineSolid.use(renderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupSolid);
                        else if (settings.mFillType == WgRenderDataShapeFillType::Linear)
                            mPipelines.mPipelineLinear.use(renderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupLinear);
                        else if (settings.mFillType == WgRenderDataShapeFillType::Radial)
                            mPipelines.mPipelineRadial.use(renderPassEncoder, mBindGroupCanvasWnd, mBindGroupPaintWnd, settings.mBindGroupRadial);
                        mGeometryDataWnd.draw(renderPassEncoder);
                    }

                    // render image
                    if (renderData->mGeometryDataImage.count > 0) {
                        wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, 0);
                        for (uint32_t j = 0; j < renderData->mGeometryDataImage.count; j++) {
                            mPipelines.mPipelineImage.use(
                                renderPassEncoder,
                                mBindGroupCanvasWnd,
                                renderData->mBindGroupPaint,
                                renderData->mBindGroupPicture);
                            renderData->mGeometryDataImage[j]->drawImage(renderPassEncoder);
                        }
                    }
                }
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
    wgpuQueueSubmit(mQueue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
    
    // go to the next frame
    wgpuSwapChainPresent(mSwapChain);

    mRenderDatas.clear();
    return true;
}

bool WgRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) {
    // store target surface properties
    mTargetSurface.stride = stride;
    mTargetSurface.w = w;
    mTargetSurface.h = h;

    // update view matrix
    mViewMatrix[0]  = +2.0f / w; mViewMatrix[1]  = +0.0f;     mViewMatrix[2]  = +0.0f; mViewMatrix[3]  = +0.0f;
    mViewMatrix[4]  = +0.0f;     mViewMatrix[5]  = -2.0f / h; mViewMatrix[6]  = +0.0f; mViewMatrix[7]  = +0.0f;
    mViewMatrix[8]  = +0.0f;     mViewMatrix[9]  = +0.0f;     mViewMatrix[10] = -1.0f; mViewMatrix[11] = +0.0f;
    mViewMatrix[12] = -1.0f;     mViewMatrix[13] = +1.0f;     mViewMatrix[14] = +0.0f; mViewMatrix[15] = +1.0f;

    // TODO: Add ability to render into offscreen buffer
    return true;
}

// target for native window handle
bool WgRenderer::target(void* window, uint32_t w, uint32_t h) {
    // store target surface properties
    mTargetSurface.stride = w;
    mTargetSurface.w = w > 0 ? w : 1;
    mTargetSurface.h = h > 0 ? h : 1;

    // update view matrix
    mViewMatrix[0]  = +2.0f / w; mViewMatrix[1]  = +0.0f;     mViewMatrix[2]  = +0.0f; mViewMatrix[3]  = +0.0f;
    mViewMatrix[4]  = +0.0f;     mViewMatrix[5]  = -2.0f / h; mViewMatrix[6]  = +0.0f; mViewMatrix[7]  = +0.0f;
    mViewMatrix[8]  = +0.0f;     mViewMatrix[9]  = +0.0f;     mViewMatrix[10] = -1.0f; mViewMatrix[11] = +0.0f;
    mViewMatrix[12] = -1.0f;     mViewMatrix[13] = +1.0f;     mViewMatrix[14] = +0.0f; mViewMatrix[15] = +1.0f;

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

    // depth-stencil texture
    WGPUTextureDescriptor textureDesc{};
    textureDesc.nextInChain = nullptr;
    textureDesc.label = "The depth-stencil texture";
    textureDesc.usage = WGPUTextureUsage_RenderAttachment;
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size = { swapChainDesc.width, swapChainDesc.height, 1 }; // window size
    textureDesc.format = WGPUTextureFormat_Stencil8;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    mStencilTex = wgpuDeviceCreateTexture(mDevice, &textureDesc);
    assert(mStencilTex);

    // depth-stencil texture view
    WGPUTextureViewDescriptor textureViewDesc{};
    textureViewDesc.nextInChain = nullptr;
    textureViewDesc.label = "The depth-stencil texture view";
    textureViewDesc.format = WGPUTextureFormat_Stencil8;
    textureViewDesc.dimension = WGPUTextureViewDimension_2D;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.aspect = WGPUTextureAspect_All;
    mStencilTexView = wgpuTextureCreateView(mStencilTex, &textureViewDesc);
    assert(mStencilTexView);

    // initialize window binding groups
    WgShaderTypeMat4x4f viewMat(w, h);
    mBindGroupCanvasWnd.initialize(mDevice, mQueue, viewMat);
    WgShaderTypeMat4x4f modelMat;
    WgShaderTypeBlendSettings blendSettings(ColorSpace::ABGR8888);
    mBindGroupPaintWnd.initialize(mDevice, mQueue, modelMat, blendSettings);
    // update pipeline geometry data
    WgVertexList wnd;
    wnd.appendRect(WgPoint(0.0f, 0.0f), WgPoint(w, 0.0f), WgPoint(0.0f, h), WgPoint(w, h));
    mGeometryDataWnd.update(mDevice, mQueue, &wnd);

    return true;
}

Compositor* WgRenderer::target(const RenderRegion& region, ColorSpace cs) {
    return nullptr;
}

bool WgRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity) {
    return false;
}

bool WgRenderer::endComposite(Compositor* cmp) {
    return false;
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
