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
        assert(!status);
        if (status != WGPURequestAdapterStatus_Success)
            std::cout << "Adapter request: " << message << std::endl;
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
        assert(!status);
        if (status != WGPURequestDeviceStatus_Success)
            std::cout << "Device request: " << message << std::endl;
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
}

// release renderer
void WgpuRenderer::release() {
    // device release
    wgpuDeviceDestroy(mDevice);
    wgpuDeviceRelease(mDevice);
    // adapter release
    wgpuAdapterRelease(mAdapter);
    // instance release
    wgpuInstanceRelease(mInstance);
}

// prepare render shape
RenderData WgpuRenderer::prepare(const RenderShape& rshape, RenderData data, const RenderTransform* transform, Array<RenderData>& clips, uint8_t opacity, RenderUpdateFlag flags, bool clipper) {
    // nothing
    return nullptr;
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
    // nothing
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
    // one of the main method!
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

    // return result
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