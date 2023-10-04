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

#include "tvgWgRenderer.h"

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include "tvgWgRenderData.h"
#include "tvgWgShaderSrc.h"

// constructor
WgRenderer::WgRenderer() {
    initialize();
}

// destructor
WgRenderer::~WgRenderer() {
    release();
}

// initialize renderer
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
    mBrushFill.initialize(mDevice);
    mBrushColor.initialize(mDevice);
    mGeometryDataFill.initialize(mDevice);
    mDataBindGroupFill.initialize(mDevice, mBrushFill);
}

// release renderer
void WgRenderer::release() {
    // remove stencil hendles
    if (mStencilTex) wgpuTextureDestroy(mStencilTex);
    if (mStencilTex) wgpuTextureRelease(mStencilTex);
    if (mStencilTexView) wgpuTextureViewRelease(mStencilTexView);
    // swapchaine release
    if (mSwapChain) wgpuSwapChainRelease(mSwapChain);
    // serface release
    if (mSurface) wgpuSurfaceRelease(mSurface);
    // create brushes
    mDataBindGroupFill.release();
    mGeometryDataFill.release();
    mBrushFill.release();
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
RenderData WgRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) {
    // get or create render data shape
    auto renderDataShape = (WgRenderDataShape *)data;
    if (!renderDataShape) {
        renderDataShape = new WgRenderDataShape();
        renderDataShape->mRenderShape = &rshape;
        renderDataShape->initialize(mDevice);
        renderDataShape->mBrushColorDataBindGroup.initialize(mDevice, mBrushColor);
    }
    // release render data
    renderDataShape->releaseRenderData();

    // outlines
    Array<Array<float>*> outlines;
    // iterate commands
    size_t pntIndex = 0;
    for (uint32_t cmdIndex = 0; cmdIndex < rshape.path.cmds.count; cmdIndex++) {
        // get current command
        PathCommand cmd = rshape.path.cmds[cmdIndex];
        // MoveTo command
        if (cmd == PathCommand::MoveTo) {
            // create new outline
            outlines.push(new Array<float>);
            // get last outline
            auto outline = outlines.last();
            // get point coordinates
            float x = rshape.path.pts[pntIndex].x;
            float y = rshape.path.pts[pntIndex].y;
            // push new vertex
            outline->push(x);
            outline->push(y);
            outline->push(0.0f);
            // move to next point
            pntIndex++;
        } else // LineTo command
        if (cmd == PathCommand::LineTo) {
            // get last outline
            auto outline = outlines.last();
            // get point coordinates
            float x = rshape.path.pts[pntIndex].x;
            float y = rshape.path.pts[pntIndex].y;
            // push new vertex
            outline->push(x);
            outline->push(y);
            outline->push(0.0f);
            // move to next point
            pntIndex++;
        } else // Close command
        if (cmd == PathCommand::Close) {
            // get last outline
            auto outline = outlines.last();
            // close path
            if ((outline) && (outline->count > 1)) {
                // get point coordinates
                float x = outline->data[0]; // x
                float y = outline->data[1]; // y
                // push new vertex
                outline->push(x);
                outline->push(y);
                outline->push(0.0f);
            }
        } else
        if (cmd == PathCommand::CubicTo) {
            // get last outline
            auto outline = outlines.last();
            // get cubic base points
            Point p0{};
            p0.x = outline->data[outline->count - 3];
            p0.y = outline->data[outline->count - 2];
            Point p1{};
            p1.x = rshape.path.pts[pntIndex + 0].x;
            p1.y = rshape.path.pts[pntIndex + 0].y;
            Point p2{};
            p2.x = rshape.path.pts[pntIndex + 1].x;
            p2.y = rshape.path.pts[pntIndex + 1].y;
            Point p3{};
            p3.x = rshape.path.pts[pntIndex + 2].x;
            p3.y = rshape.path.pts[pntIndex + 2].y;
            // calculate cubic spline
            const size_t segs = 16;
            for (size_t i = 1; i <= segs; i++) {
                float t = i / (float)segs;
                // get coefficients
                float t0 = 1 * (1.0f - t) * (1.0f - t) * (1.0f - t);
                float t1 = 3 * (1.0f - t) * (1.0f - t) * t;
                float t2 = 3 * (1.0f - t) * t * t;
                float t3 = 1 * t * t * t;
                // compute coordinates
                float x = p0.x * t0 + p1.x * t1 + p2.x * t2 + p3.x * t3;
                float y = p0.y * t0 + p1.y * t1 + p2.y * t2 + p3.y * t3;
                // push new vertex
                outline->push(x);
                outline->push(y);
                outline->push(0.0f);
            }
            // move to next point
            pntIndex += 3;
        } 
    }
    // create render datas
    Array<uint32_t> indexBuffer;
    for (size_t i = 0; i < outlines.count; i++) {
        // get current outline
        auto outline = outlines[i];
        // get vertex count
        size_t vertexCount = outline->count / 3;
        assert(vertexCount > 2);
        // debug index buffer (for triangle fan)
        indexBuffer.clear();
        indexBuffer.reserve((vertexCount - 2) * 3);
        for (size_t j = 0; j < vertexCount - 2; j++) {
            indexBuffer.push(0);
            indexBuffer.push(j + 1);
            indexBuffer.push(j + 2);
        }
        // create new geometry data
        WgGeometryData* geometryData = new WgGeometryData();
        geometryData->initialize(mDevice);
        geometryData->update(mDevice, mQueue, outline->data, vertexCount, indexBuffer.data, indexBuffer.count);
        // push geometry data
        renderDataShape->mGeometryDataFill.push(geometryData);
    }
    // delete outlines
    for (size_t i = 0; i < outlines.count; i++)
        delete outlines[i];

    // brush color data
    WgBrushColorData mBrushColorData{};
    mBrushColorData.updateMatrix(mViewMatrix, transform);
    mBrushColorData.uColorInfo = { { 1.0f, 1.0f, 0.0f, 1.0f } };
    // update brush fill data
    renderDataShape->mBrushColorDataBindGroup.update(mQueue, mBrushColorData);

    // return render data shape
    return renderDataShape;
}

// prepare scene
RenderData WgRenderer::prepare(const Array<RenderData>& scene, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    // nothing
    return nullptr;
}

// prepare surface
RenderData WgRenderer::prepare(Surface* surface, const RenderMesh* mesh, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags) {
    // nothing
    return nullptr;
}

// pre render
bool WgRenderer::preRender() {
    // nothing
    return true;
}

// render shape
bool WgRenderer::renderShape(RenderData data) {
    mRenderDatas.push(data);
    return true;
}

// render image
bool WgRenderer::renderImage(RenderData data) {
    // nothing
    return true;
}

// post render
bool WgRenderer::postRender() {
    // nothing
    return true;
}

// dispose render data
bool WgRenderer::dispose(RenderData data) {
    // release render data
    auto renderData = (WgRenderData *)data;
    if (renderData)
        renderData->release();
    // nothing
    return true;
}

// get region of render data
RenderRegion WgRenderer::region(RenderData data) {
    // nothing
    return { 0, 0, INT32_MAX, INT32_MAX };
}

// get current viewport
RenderRegion WgRenderer::viewport() {
    // nothing
    return { 0, 0, INT32_MAX, INT32_MAX };
}

// set current viewport
bool WgRenderer::viewport(const RenderRegion& vp) {
    // nothing
    return true;
}

// blend
bool WgRenderer::blend(BlendMethod method) {
    return false;
}

// color space
ColorSpace WgRenderer::colorSpace() {
    return ColorSpace::Unsupported;
}

// clear
bool WgRenderer::clear() {
    return true;
}

// sync
bool WgRenderer::sync() {
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
            colorAttachment.loadOp = WGPULoadOp_Clear;
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
                    // get render data
                    WgRenderDataShape* renderData = (WgRenderDataShape*)(mRenderDatas[0]);
                    
                    // iterate geometry data
                    for (uint32_t j = 0; j < renderData->mGeometryDataFill.count; j++) {
                        // draw to stencil
                        mBrushFill.set(renderPassEncoder);
                        mDataBindGroupFill.bind(renderPassEncoder, 0);
                        renderData->mGeometryDataFill[j]->draw(renderPassEncoder);

                        // draw brush
                        mBrushColor.set(renderPassEncoder);
                        renderData->mBrushColorDataBindGroup.bind(renderPassEncoder, 0);
                        mGeometryDataFill.draw(renderPassEncoder);
                    }
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
bool WgRenderer::target(uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h) {
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
bool WgRenderer::target(void* window, uint32_t w, uint32_t h) {
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
    WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
    //WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8UnormSrgb;
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

    // update brush geometry data
    static float vertexData[] = {
            0.0f,     0.0f, 0.0f,
        (float)w,     0.0f, 0.0f,
        (float)w, (float)h, 0.0f,
            1.0f, (float)h, 0.0f
    };
    static uint32_t indexData[] = { 0, 1, 2, 0, 2, 3 };
    // update render data brush
    mGeometryDataFill.update(mDevice, mQueue, vertexData, 4, indexData, 6);
    WgBrushFillData brushFillData{};
    brushFillData.updateMatrix(mViewMatrix, nullptr);
    mDataBindGroupFill.update(mQueue, brushFillData);

    return true;
}

// target
Compositor* WgRenderer::target(const RenderRegion& region, ColorSpace cs) {
    // not supported
    return nullptr;
}

// begin composite
bool WgRenderer::beginComposite(Compositor* cmp, CompositeMethod method, uint8_t opacity) {
    //TODO: delete the given compositor and restore the context
    return false;
}

// end composite
bool WgRenderer::endComposite(Compositor* cmp) {
    //TODO: delete the given compositor and restore the context
    return false;
}

// generate renderer webgpu
WgRenderer* WgRenderer::gen() {
    // create new renderer instance
    return new WgRenderer();
}

// initialize renderer
bool WgRenderer::init(uint32_t threads) {
    return true;
}

// terminate renderer
bool WgRenderer::term() {
    return true;
}
