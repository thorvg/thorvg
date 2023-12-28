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

#include "tvgWgCommon.h"
#include <iostream>

//*****************************************************************************
// context
//*****************************************************************************

void WgContext::initialize()
{
    // create instance
    WGPUInstanceDescriptor instanceDesc{};
    instanceDesc.nextInChain = nullptr;
    instance = wgpuCreateInstance(&instanceDesc);
    assert(instance);

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
    wgpuInstanceRequestAdapter(instance, &requestAdapterOptions, onAdapterRequestEnded, &adapter);
    assert(adapter);

    // adapter enumarate fueatures
    size_t featuresCount = wgpuAdapterEnumerateFeatures(adapter, featureNames);
    wgpuAdapterGetProperties(adapter, &adapterProperties);
    wgpuAdapterGetLimits(adapter, &supportedLimits);

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
    wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceRequestEnded, &device);
    assert(device);

    // on device error function
    auto onDeviceError = [](WGPUErrorType type, char const* message, void* pUserData) {
        TVGERR("WG_RENDERER", "Uncaptured device error: %s", message);
        // TODO: remove direct error message
        std::cout << message << std::endl;
    };
    // set device error handling
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);

    queue = wgpuDeviceGetQueue(device);
    assert(queue);
}


void WgContext::release()
{
    if (device) {
        wgpuDeviceDestroy(device);
        wgpuDeviceRelease(device);
    }
    if (adapter) wgpuAdapterRelease(adapter);
    if (instance) wgpuInstanceRelease(instance);
}


void WgContext::executeCommandEncoder(WGPUCommandEncoder commandEncoder)
{
    // command buffer descriptor
    WGPUCommandBufferDescriptor commandBufferDesc{};
    commandBufferDesc.nextInChain = nullptr;
    commandBufferDesc.label = "The command buffer";
    WGPUCommandBuffer commandsBuffer = nullptr;
    commandsBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDesc);
    wgpuQueueSubmit(queue, 1, &commandsBuffer);
    wgpuCommandBufferRelease(commandsBuffer);
}


//*****************************************************************************
// bind group
//*****************************************************************************

void WgBindGroup::set(WGPURenderPassEncoder encoder, uint32_t groupIndex)
{
    wgpuRenderPassEncoderSetBindGroup(encoder, groupIndex, mBindGroup, 0, nullptr);
}


WGPUBindGroupEntry WgBindGroup::makeBindGroupEntryBuffer(uint32_t binding, WGPUBuffer buffer)
{
    WGPUBindGroupEntry bindGroupEntry{};
    bindGroupEntry.nextInChain = nullptr;
    bindGroupEntry.binding = binding;
    bindGroupEntry.buffer = buffer;
    bindGroupEntry.offset = 0;
    bindGroupEntry.size = wgpuBufferGetSize(buffer);
    bindGroupEntry.sampler = nullptr;
    bindGroupEntry.textureView = nullptr;
    return bindGroupEntry;
}


WGPUBindGroupEntry WgBindGroup::makeBindGroupEntrySampler(uint32_t binding, WGPUSampler sampler)
{
    WGPUBindGroupEntry bindGroupEntry{};
    bindGroupEntry.nextInChain = nullptr;
    bindGroupEntry.binding = binding;
    bindGroupEntry.buffer = nullptr;
    bindGroupEntry.offset = 0;
    bindGroupEntry.size = 0;
    bindGroupEntry.sampler = sampler;
    bindGroupEntry.textureView = nullptr;
    return bindGroupEntry;
}


WGPUBindGroupEntry WgBindGroup::makeBindGroupEntryTextureView(uint32_t binding, WGPUTextureView textureView)
{
    WGPUBindGroupEntry bindGroupEntry{};
    bindGroupEntry.nextInChain = nullptr;
    bindGroupEntry.binding = binding;
    bindGroupEntry.buffer = nullptr;
    bindGroupEntry.offset = 0;
    bindGroupEntry.size = 0;
    bindGroupEntry.sampler = nullptr;
    bindGroupEntry.textureView = textureView;
    return bindGroupEntry;
}


WGPUBindGroupLayoutEntry WgBindGroup::makeBindGroupLayoutEntryBuffer(uint32_t binding)
{
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry{};
    bindGroupLayoutEntry.nextInChain = nullptr;
    bindGroupLayoutEntry.binding = binding;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntry.buffer.nextInChain = nullptr;
    bindGroupLayoutEntry.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntry.buffer.hasDynamicOffset = false;
    bindGroupLayoutEntry.buffer.minBindingSize = 0;
    return bindGroupLayoutEntry;
}


WGPUBindGroupLayoutEntry WgBindGroup::makeBindGroupLayoutEntrySampler(uint32_t binding)
{
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry{};
    bindGroupLayoutEntry.nextInChain = nullptr;
    bindGroupLayoutEntry.binding = binding;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntry.sampler.nextInChain = nullptr;
    bindGroupLayoutEntry.sampler.type = WGPUSamplerBindingType_Filtering;
    return bindGroupLayoutEntry;
}


WGPUBindGroupLayoutEntry WgBindGroup::makeBindGroupLayoutEntryTextureView(uint32_t binding)
{
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry{};
    bindGroupLayoutEntry.nextInChain = nullptr;
    bindGroupLayoutEntry.binding = binding;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntry.texture.nextInChain = nullptr;
    bindGroupLayoutEntry.texture.sampleType = WGPUTextureSampleType_Float;
    bindGroupLayoutEntry.texture.viewDimension = WGPUTextureViewDimension_2D;
    bindGroupLayoutEntry.texture.multisampled = false;
    return bindGroupLayoutEntry;
}


WGPUBuffer WgBindGroup::createBuffer(WGPUDevice device, WGPUQueue queue, const void *data, size_t size)
{
    WGPUBufferDescriptor bufferDescriptor{};
    bufferDescriptor.nextInChain = nullptr;
    bufferDescriptor.label = "The uniform buffer";
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferDescriptor.size = size;
    bufferDescriptor.mappedAtCreation = false;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
    assert(buffer);
    wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    return buffer;
}


WGPUBindGroup WgBindGroup::createBindGroup(WGPUDevice device, WGPUBindGroupLayout layout, const WGPUBindGroupEntry* bindGroupEntries, uint32_t count)
{
    WGPUBindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.nextInChain = nullptr;
    bindGroupDesc.label = "The binding group sampler";
    bindGroupDesc.layout = layout;
    bindGroupDesc.entryCount = count;
    bindGroupDesc.entries = bindGroupEntries;
    return wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}


WGPUBindGroupLayout WgBindGroup::createBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutEntry* bindGroupLayoutEntries, uint32_t count)
{
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = "The bind group layout";
    bindGroupLayoutDesc.entryCount = count;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries; // @binding
    return wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
}


void WgBindGroup::releaseBuffer(WGPUBuffer& buffer)
{
    if (buffer) {
        wgpuBufferDestroy(buffer);
        wgpuBufferRelease(buffer);
        buffer = nullptr;
    }
}


void WgBindGroup::releaseBindGroup(WGPUBindGroup& bindGroup)
{
    if (bindGroup) wgpuBindGroupRelease(bindGroup);
    bindGroup = nullptr;
}


void WgBindGroup::releaseBindGroupLayout(WGPUBindGroupLayout& bindGroupLayout)
{
    if (bindGroupLayout) wgpuBindGroupLayoutRelease(bindGroupLayout);
    bindGroupLayout = nullptr;
}

//*****************************************************************************
// pipeline
//*****************************************************************************

void WgPipeline::allocate(WGPUDevice device,
                          WGPUVertexBufferLayout vertexBufferLayouts[], uint32_t attribsCount,
                          WGPUBindGroupLayout bindGroupLayouts[], uint32_t bindGroupsCount,
                          WGPUCompareFunction stencilCompareFunction, WGPUStencilOperation stencilOperation,
                          const char* shaderSource, const char* shaderLabel, const char* pipelineLabel)
{
    mShaderModule = createShaderModule(device, shaderSource, shaderLabel);
    assert(mShaderModule);

    mPipelineLayout = createPipelineLayout(device, bindGroupLayouts, bindGroupsCount);
    assert(mPipelineLayout);

    mRenderPipeline = createRenderPipeline(device,
                                           vertexBufferLayouts, attribsCount,
                                           stencilCompareFunction, stencilOperation,
                                           mPipelineLayout, mShaderModule, pipelineLabel);
    assert(mRenderPipeline);
}


void WgPipeline::release()
{
    destroyRenderPipeline(mRenderPipeline);
    destroyShaderModule(mShaderModule);
    destroyPipelineLayout(mPipelineLayout);
}


void WgPipeline::set(WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mRenderPipeline);
};


WGPUBlendState WgPipeline::makeBlendState()
{
    WGPUBlendState blendState{};
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.alpha.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    return blendState;
}


WGPUColorTargetState WgPipeline::makeColorTargetState(const WGPUBlendState* blendState)
{
    WGPUColorTargetState colorTargetState{};
    colorTargetState.nextInChain = nullptr;
    colorTargetState.format = WGPUTextureFormat_BGRA8Unorm; // (WGPUTextureFormat_BGRA8UnormSrgb)
    colorTargetState.blend = blendState;
    colorTargetState.writeMask = WGPUColorWriteMask_All;
    return colorTargetState;
}


WGPUVertexBufferLayout WgPipeline::makeVertexBufferLayout(const WGPUVertexAttribute* vertexAttributes, uint32_t count, uint64_t stride)
{
    WGPUVertexBufferLayout vertexBufferLayoutPos{};
    vertexBufferLayoutPos.arrayStride = stride;
    vertexBufferLayoutPos.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayoutPos.attributeCount = count;
    vertexBufferLayoutPos.attributes = vertexAttributes;
    return vertexBufferLayoutPos;
}


WGPUVertexState WgPipeline::makeVertexState(WGPUShaderModule shaderModule, const WGPUVertexBufferLayout* buffers, uint32_t count)
{
    WGPUVertexState vertexState{};
    vertexState.nextInChain = nullptr;
    vertexState.module = shaderModule;
    vertexState.entryPoint = "vs_main";
    vertexState.constantCount = 0;
    vertexState.constants = nullptr;
    vertexState.bufferCount = count;
    vertexState.buffers = buffers;
    return vertexState;
}


WGPUPrimitiveState WgPipeline::makePrimitiveState()
{
    WGPUPrimitiveState primitiveState{};
    primitiveState.nextInChain = nullptr;
    primitiveState.topology = WGPUPrimitiveTopology_TriangleList;
    primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitiveState.frontFace = WGPUFrontFace_CCW;
    primitiveState.cullMode = WGPUCullMode_None;
    return primitiveState;
}

WGPUDepthStencilState WgPipeline::makeDepthStencilState(WGPUCompareFunction compare, WGPUStencilOperation operation)
{
    WGPUDepthStencilState depthStencilState{};
    depthStencilState.nextInChain = nullptr;
    depthStencilState.format = WGPUTextureFormat_Stencil8;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.depthCompare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.compare = compare;
    depthStencilState.stencilFront.failOp = operation;
    depthStencilState.stencilFront.depthFailOp = operation;
    depthStencilState.stencilFront.passOp = operation;
    depthStencilState.stencilBack.compare = compare;
    depthStencilState.stencilBack.failOp = operation;
    depthStencilState.stencilBack.depthFailOp = operation;
    depthStencilState.stencilBack.passOp = operation;
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;
    depthStencilState.depthBias = 0;
    depthStencilState.depthBiasSlopeScale = 0.0f;
    depthStencilState.depthBiasClamp = 0.0f;
    return depthStencilState;
}


WGPUMultisampleState WgPipeline::makeMultisampleState()
{
    WGPUMultisampleState multisampleState{};
    multisampleState.nextInChain = nullptr;
    multisampleState.count = 1;
    multisampleState.mask = 0xFFFFFFFF;
    multisampleState.alphaToCoverageEnabled = false;
    return multisampleState;
}


WGPUFragmentState WgPipeline::makeFragmentState(WGPUShaderModule shaderModule, WGPUColorTargetState* targets, uint32_t size)
{
    WGPUFragmentState fragmentState{};
    fragmentState.nextInChain = nullptr;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    fragmentState.targetCount = size;
    fragmentState.targets = targets;
    return fragmentState;
}


WGPUPipelineLayout WgPipeline::createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, uint32_t count)
{
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.nextInChain = nullptr;
    pipelineLayoutDesc.label = "The Pipeline layout";
    pipelineLayoutDesc.bindGroupLayoutCount = count;
    pipelineLayoutDesc.bindGroupLayouts = bindGroupLayouts;
    return wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
}


WGPUShaderModule WgPipeline::createShaderModule(WGPUDevice device, const char* code, const char* label)
{
    WGPUShaderModuleWGSLDescriptor shaderModuleWGSLDesc{};
	shaderModuleWGSLDesc.chain.next = nullptr;
	shaderModuleWGSLDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderModuleWGSLDesc.code = code;
    WGPUShaderModuleDescriptor shaderModuleDesc{};
    shaderModuleDesc.nextInChain = &shaderModuleWGSLDesc.chain;
    shaderModuleDesc.label = label;
    shaderModuleDesc.hintCount = 0;
    shaderModuleDesc.hints = nullptr;
    return wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
}


WGPURenderPipeline WgPipeline::createRenderPipeline(WGPUDevice device,
                                                    WGPUVertexBufferLayout vertexBufferLayouts[], uint32_t attribsCount,
                                                    WGPUCompareFunction stencilCompareFunction, WGPUStencilOperation stencilOperation,
                                                    WGPUPipelineLayout pipelineLayout, WGPUShaderModule shaderModule,
                                                    const char* pipelineName)
{
    WGPUBlendState blendState = makeBlendState();
    WGPUColorTargetState colorTargetStates[] = { 
        makeColorTargetState(&blendState)
    };

    WGPUVertexState vertexState = makeVertexState(shaderModule, vertexBufferLayouts, attribsCount);
    WGPUPrimitiveState primitiveState = makePrimitiveState();
    WGPUDepthStencilState depthStencilState = makeDepthStencilState(stencilCompareFunction, stencilOperation);
    WGPUMultisampleState multisampleState = makeMultisampleState();
    WGPUFragmentState fragmentState = makeFragmentState(shaderModule, colorTargetStates, 1);

    WGPURenderPipelineDescriptor renderPipelineDesc{};
    renderPipelineDesc.nextInChain = nullptr;
    renderPipelineDesc.label = pipelineName;
    renderPipelineDesc.layout = pipelineLayout;
    renderPipelineDesc.vertex = vertexState;
    renderPipelineDesc.primitive = primitiveState;
    renderPipelineDesc.depthStencil = &depthStencilState;
    renderPipelineDesc.multisample = multisampleState;
    renderPipelineDesc.fragment = &fragmentState;
    return wgpuDeviceCreateRenderPipeline(device, &renderPipelineDesc);
}


void WgPipeline::destroyPipelineLayout(WGPUPipelineLayout& pipelineLayout)
{
    if (pipelineLayout) wgpuPipelineLayoutRelease(pipelineLayout);
    pipelineLayout = nullptr;
}


void WgPipeline::destroyShaderModule(WGPUShaderModule& shaderModule)
{
    if (shaderModule) wgpuShaderModuleRelease(shaderModule);
    shaderModule = nullptr;
}


void WgPipeline::destroyRenderPipeline(WGPURenderPipeline& renderPipeline)
{
    if (renderPipeline) wgpuRenderPipelineRelease(renderPipeline);
    renderPipeline = nullptr;
}