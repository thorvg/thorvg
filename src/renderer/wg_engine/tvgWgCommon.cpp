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

void WgContext::initialize(WGPUInstance instance, WGPUSurface surface)
{
    assert(instance);
    assert(surface);
    
    // store global instance and surface
    this->instance = instance;
    this->surface = surface;

    // request adapter options
    WGPURequestAdapterOptions requestAdapterOptions{};
    requestAdapterOptions.nextInChain = nullptr;
    requestAdapterOptions.compatibleSurface = surface;
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

    // adapter enumerate features
    size_t featuresCount = wgpuAdapterEnumerateFeatures(adapter, featureNames);
    wgpuAdapterGetProperties(adapter, &adapterProperties);
    wgpuAdapterGetLimits(adapter, &supportedLimits);

    // request device
    WGPUDeviceDescriptor deviceDesc{};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "The device";
    deviceDesc.requiredFeatureCount = featuresCount;
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
    
    // create default nearest and linear samplers
    samplerNearest = createSampler(WGPUFilterMode_Nearest, WGPUMipmapFilterMode_Nearest);
    assert(samplerNearest);
    samplerLinear = createSampler(WGPUFilterMode_Linear, WGPUMipmapFilterMode_Linear);
    assert(samplerLinear);
    allocateIndexBufferFan(1024);
    assert(indexBufferFan);
}


void WgContext::release()
{
    releaseBuffer(indexBufferFan);
    releaseSampler(samplerNearest);
    releaseSampler(samplerLinear);
    if (device) {
        wgpuDeviceDestroy(device);
        wgpuDeviceRelease(device);
        device =  nullptr;
    }
    if (adapter) {
        wgpuAdapterRelease(adapter);
        adapter = nullptr;
    }
    if (surface) {
        wgpuSurfaceUnconfigure(surface);
        wgpuSurfaceRelease(surface);
        surface = nullptr;
    }
    if (instance) {
        wgpuInstanceRelease(instance);
        instance = nullptr;
    }
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


WGPUSampler WgContext::createSampler(WGPUFilterMode minFilter, WGPUMipmapFilterMode mipmapFilter) 
{
    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.nextInChain = nullptr;
    samplerDesc.label = "The sampler";
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = minFilter;
    samplerDesc.minFilter = minFilter;
    samplerDesc.mipmapFilter = mipmapFilter;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.compare = WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;
    return wgpuDeviceCreateSampler(device, &samplerDesc);
}


WGPUTexture WgContext::createTexture2d(WGPUTextureUsageFlags usage, WGPUTextureFormat format, uint32_t width, uint32_t height, char const * label) {
    WGPUTextureDescriptor textureDesc{};
    textureDesc.nextInChain = nullptr;
    textureDesc.label = label;
    textureDesc.usage = usage;
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size = { width, height, 1 };
    textureDesc.format = format;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    return wgpuDeviceCreateTexture(device, &textureDesc);
}


WGPUTexture WgContext::createTexture2dMS(WGPUTextureUsageFlags usage, WGPUTextureFormat format, uint32_t width, uint32_t height, uint32_t sc, char const * label)
{
    WGPUTextureDescriptor textureDesc{};
    textureDesc.nextInChain = nullptr;
    textureDesc.label = label;
    textureDesc.usage = usage;
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size = { width, height, 1 };
    textureDesc.format = format;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = sc;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    return wgpuDeviceCreateTexture(device, &textureDesc);
}


WGPUTextureView WgContext::createTextureView2d(WGPUTexture texture, char const * label)
{
    WGPUTextureViewDescriptor textureViewDescColor{};
    textureViewDescColor.nextInChain = nullptr;
    textureViewDescColor.label = label;
    textureViewDescColor.format = wgpuTextureGetFormat(texture);
    textureViewDescColor.dimension = WGPUTextureViewDimension_2D;
    textureViewDescColor.baseMipLevel = 0;
    textureViewDescColor.mipLevelCount = 1;
    textureViewDescColor.baseArrayLayer = 0;
    textureViewDescColor.arrayLayerCount = 1;
    textureViewDescColor.aspect = WGPUTextureAspect_All;
    return wgpuTextureCreateView(texture, &textureViewDescColor);
};


WGPUBuffer WgContext::createBuffer(WGPUBufferUsageFlags usage, uint64_t size,char const * label)
{
    WGPUBufferDescriptor bufferDesc{};
    bufferDesc.nextInChain = nullptr;
    bufferDesc.label = label;
    bufferDesc.usage = usage;
    bufferDesc.size = size;
    bufferDesc.mappedAtCreation = false;
    return wgpuDeviceCreateBuffer(device, &bufferDesc);
}


void WgContext::releaseSampler(WGPUSampler& sampler)
{
    if (sampler) {
        wgpuSamplerRelease(sampler);
        sampler = nullptr;
    }
}


void WgContext::releaseTexture(WGPUTexture& texture)
{
    if (texture) {
        wgpuTextureDestroy(texture);
        wgpuTextureRelease(texture);
        texture = nullptr;
    }
    
}


void WgContext::releaseTextureView(WGPUTextureView& textureView)
{
    if (textureView) {
        wgpuTextureViewRelease(textureView);
        textureView = nullptr;
    }
}


void WgContext::releaseBuffer(WGPUBuffer& buffer)
{
    if (buffer) { 
        wgpuBufferDestroy(buffer);
        wgpuBufferRelease(buffer);
        buffer = nullptr;
    }
}


void WgContext::allocateVertexBuffer(WGPUBuffer& buffer, const void *data, uint64_t size)
{
    if ((buffer) && (wgpuBufferGetSize(buffer) >= size))
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    else {
        // create new buffer and upload data
        releaseBuffer(buffer);
        WGPUBufferDescriptor bufferDesc{};
        bufferDesc.nextInChain = nullptr;
        bufferDesc.label = "The vertex buffer";
        bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
        bufferDesc.size = size > WG_VERTEX_BUFFER_MIN_SIZE ? size : WG_VERTEX_BUFFER_MIN_SIZE;
        bufferDesc.mappedAtCreation = false;
        buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    }
}


void WgContext::allocateIndexBuffer(WGPUBuffer& buffer, const void *data, uint64_t size)
{
    if ((buffer) && (wgpuBufferGetSize(buffer) >= size))
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    else {
        // create new buffer and upload data
        releaseBuffer(buffer);
        WGPUBufferDescriptor bufferDesc{};
        bufferDesc.nextInChain = nullptr;
        bufferDesc.label = "The index buffer";
        bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
        bufferDesc.size = size > WG_INDEX_BUFFER_MIN_SIZE ? size : WG_INDEX_BUFFER_MIN_SIZE;
        bufferDesc.mappedAtCreation = false;
        buffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
        wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
    }
}


void WgContext::allocateIndexBufferFan(uint64_t vertsCount)
{
    assert(vertsCount >= 3);
    uint64_t indexCount = (vertsCount - 2) * 3;
    if ((!indexBufferFan) || (wgpuBufferGetSize(indexBufferFan) < indexCount * sizeof(uint32_t))) {
        Array<uint32_t> indexes(indexCount);
        for (size_t i = 0; i < vertsCount - 2; i++) {
            indexes.push(0);
            indexes.push(i + 1);
            indexes.push(i + 2);
        }
        releaseBuffer(indexBufferFan);
        WGPUBufferDescriptor bufferDesc{};
        bufferDesc.nextInChain = nullptr;
        bufferDesc.label = "The index buffer";
        bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
        bufferDesc.size = indexCount * sizeof(uint32_t);
        bufferDesc.mappedAtCreation = false;
        indexBufferFan = wgpuDeviceCreateBuffer(device, &bufferDesc);
        wgpuQueueWriteBuffer(queue, indexBufferFan, 0, &indexes[0], indexCount * sizeof(uint32_t));
    }
}


void WgContext::releaseVertexBuffer(WGPUBuffer& buffer)
{
    releaseBuffer(buffer);
}


void WgContext::releaseIndexBuffer(WGPUBuffer& buffer)
{
    releaseBuffer(buffer);
}

//*****************************************************************************
// bind group
//*****************************************************************************

void WgBindGroup::set(WGPURenderPassEncoder encoder, uint32_t groupIndex)
{
    wgpuRenderPassEncoderSetBindGroup(encoder, groupIndex, mBindGroup, 0, nullptr);
}


void WgBindGroup::set(WGPUComputePassEncoder encoder, uint32_t groupIndex)
{
    wgpuComputePassEncoderSetBindGroup(encoder, groupIndex, mBindGroup, 0, nullptr);
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
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
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


WGPUBindGroupLayoutEntry WgBindGroup::makeBindGroupLayoutEntryTexture(uint32_t binding)
{
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry{};
    bindGroupLayoutEntry.nextInChain = nullptr;
    bindGroupLayoutEntry.binding = binding;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
    bindGroupLayoutEntry.texture.nextInChain = nullptr;
    bindGroupLayoutEntry.texture.sampleType = WGPUTextureSampleType_Float;
    bindGroupLayoutEntry.texture.viewDimension = WGPUTextureViewDimension_2D;
    bindGroupLayoutEntry.texture.multisampled = false;
    return bindGroupLayoutEntry;
}


WGPUBindGroupLayoutEntry WgBindGroup::makeBindGroupLayoutEntryStorage(uint32_t binding, WGPUStorageTextureAccess access, WGPUTextureFormat format)
{
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry{};
    bindGroupLayoutEntry.nextInChain = nullptr;
    bindGroupLayoutEntry.binding = binding;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
    bindGroupLayoutEntry.storageTexture.nextInChain = nullptr;
    bindGroupLayoutEntry.storageTexture.access = access;
    bindGroupLayoutEntry.storageTexture.format = format;
    bindGroupLayoutEntry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
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
    bindGroupDesc.label = "The binding group";
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

void WgPipeline::release()
{
    destroyShaderModule(mShaderModule);
    destroyPipelineLayout(mPipelineLayout);
}


WGPUPipelineLayout WgPipeline::createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, uint32_t count)
{
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.nextInChain = nullptr;
    pipelineLayoutDesc.label = "The pipeline layout";
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

//*****************************************************************************
// render pipeline
//*****************************************************************************

void WgRenderPipeline::allocate(WGPUDevice device, WgPipelineBlendType blendType, WGPUColorWriteMask writeMask,
                                WGPUVertexBufferLayout vertexBufferLayouts[], uint32_t attribsCount,
                                WGPUBindGroupLayout bindGroupLayouts[], uint32_t bindGroupsCount,
                                WGPUCompareFunction compareFront, WGPUStencilOperation operationFront,
                                WGPUCompareFunction compareBack, WGPUStencilOperation operationBack,
                                const char* shaderSource, const char* shaderLabel, const char* pipelineLabel)
{
    mShaderModule = createShaderModule(device, shaderSource, shaderLabel);
    assert(mShaderModule);

    mPipelineLayout = createPipelineLayout(device, bindGroupLayouts, bindGroupsCount);
    assert(mPipelineLayout);

    mRenderPipeline = createRenderPipeline(device, blendType, writeMask,
                                           vertexBufferLayouts, attribsCount,
                                           compareFront, operationFront,
                                           compareBack, operationBack,
                                           mPipelineLayout, mShaderModule, pipelineLabel);
    assert(mRenderPipeline);
}


void WgRenderPipeline::release()
{
    destroyRenderPipeline(mRenderPipeline);
    WgPipeline::release();
}


void WgRenderPipeline::set(WGPURenderPassEncoder renderPassEncoder)
{
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mRenderPipeline);
};


WGPUBlendState WgRenderPipeline::makeBlendState(WgPipelineBlendType blendType)
{
    WGPUBlendState blendState{};
    // srcover
    if (blendType == WgPipelineBlendType::SrcOver) {
        blendState.color.operation = WGPUBlendOperation_Add;
        blendState.color.srcFactor = WGPUBlendFactor_One;
        blendState.color.dstFactor = WGPUBlendFactor_Zero;
    // normal
    } else if (blendType == WgPipelineBlendType::Normal) {
        blendState.color.operation = WGPUBlendOperation_Add;
        blendState.color.srcFactor = WGPUBlendFactor_One;
        blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    // custom
    } else if (blendType == WgPipelineBlendType::Custom) {
        blendState.color.operation = WGPUBlendOperation_Add;
        blendState.color.srcFactor = WGPUBlendFactor_One;
        blendState.color.dstFactor = WGPUBlendFactor_Zero;
    }
    blendState.alpha = blendState.color;
    return blendState;
}


WGPUColorTargetState WgRenderPipeline::makeColorTargetState(const WGPUBlendState* blendState, const WGPUColorWriteMask writeMask)
{
    WGPUColorTargetState colorTargetState{};
    colorTargetState.nextInChain = nullptr;
    colorTargetState.format = WGPUTextureFormat_RGBA8Unorm;
    colorTargetState.blend = blendState;
    colorTargetState.writeMask = writeMask;
    return colorTargetState;
}


WGPUVertexBufferLayout WgRenderPipeline::makeVertexBufferLayout(const WGPUVertexAttribute* vertexAttributes, uint32_t count, uint64_t stride)
{
    WGPUVertexBufferLayout vertexBufferLayoutPos{};
    vertexBufferLayoutPos.arrayStride = stride;
    vertexBufferLayoutPos.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayoutPos.attributeCount = count;
    vertexBufferLayoutPos.attributes = vertexAttributes;
    return vertexBufferLayoutPos;
}


WGPUVertexState WgRenderPipeline::makeVertexState(WGPUShaderModule shaderModule, const WGPUVertexBufferLayout* buffers, uint32_t count)
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


WGPUPrimitiveState WgRenderPipeline::makePrimitiveState()
{
    WGPUPrimitiveState primitiveState{};
    primitiveState.nextInChain = nullptr;
    primitiveState.topology = WGPUPrimitiveTopology_TriangleList;
    primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitiveState.frontFace = WGPUFrontFace_CCW;
    primitiveState.cullMode = WGPUCullMode_None;
    return primitiveState;
}


WGPUDepthStencilState WgRenderPipeline::makeDepthStencilState(WGPUCompareFunction compareFront, WGPUStencilOperation operationFront, WGPUCompareFunction compareBack, WGPUStencilOperation operationBack)
{
    WGPUDepthStencilState depthStencilState{};
    depthStencilState.nextInChain = nullptr;
    depthStencilState.format = WGPUTextureFormat_Stencil8;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.depthCompare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.compare = compareFront;
    depthStencilState.stencilFront.failOp = operationFront;
    depthStencilState.stencilFront.depthFailOp = operationFront;
    depthStencilState.stencilFront.passOp = operationFront;
    depthStencilState.stencilBack.compare = compareBack;
    depthStencilState.stencilBack.failOp = operationBack;
    depthStencilState.stencilBack.depthFailOp = operationBack;
    depthStencilState.stencilBack.passOp = operationBack;
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;
    depthStencilState.depthBias = 0;
    depthStencilState.depthBiasSlopeScale = 0.0f;
    depthStencilState.depthBiasClamp = 0.0f;
    return depthStencilState;
}


WGPUMultisampleState WgRenderPipeline::makeMultisampleState()
{
    WGPUMultisampleState multisampleState{};
    multisampleState.nextInChain = nullptr;
    multisampleState.count = 1;
    multisampleState.mask = 0xFFFFFFFF;
    multisampleState.alphaToCoverageEnabled = false;
    return multisampleState;
}


WGPUFragmentState WgRenderPipeline::makeFragmentState(WGPUShaderModule shaderModule, WGPUColorTargetState* targets, uint32_t size)
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


WGPURenderPipeline WgRenderPipeline::createRenderPipeline(WGPUDevice device, WgPipelineBlendType blendType, WGPUColorWriteMask writeMask,
                                                          WGPUVertexBufferLayout vertexBufferLayouts[], uint32_t attribsCount,
                                                          WGPUCompareFunction compareFront, WGPUStencilOperation operationFront,
                                                          WGPUCompareFunction compareBack, WGPUStencilOperation operationBack,
                                                          WGPUPipelineLayout pipelineLayout, WGPUShaderModule shaderModule,
                                                          const char* pipelineName)
{
    WGPUBlendState blendState = makeBlendState(blendType);
    WGPUColorTargetState colorTargetStates[] = { 
        makeColorTargetState(&blendState, writeMask)
    };

    WGPUVertexState vertexState = makeVertexState(shaderModule, vertexBufferLayouts, attribsCount);
    WGPUPrimitiveState primitiveState = makePrimitiveState();
    WGPUDepthStencilState depthStencilState = makeDepthStencilState(compareFront, operationFront, compareBack, operationBack);
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


void WgRenderPipeline::destroyRenderPipeline(WGPURenderPipeline& renderPipeline)
{
    if (renderPipeline) wgpuRenderPipelineRelease(renderPipeline);
    renderPipeline = nullptr;
}

//*****************************************************************************
// compute pipeline
//*****************************************************************************

void WgComputePipeline::allocate(WGPUDevice device,
                                 WGPUBindGroupLayout bindGroupLayouts[], uint32_t bindGroupsCount,
                                 const char* shaderSource, const char* shaderLabel, const char* pipelineLabel)
{
    mShaderModule = createShaderModule(device, shaderSource, shaderLabel);
    assert(mShaderModule);

    mPipelineLayout = createPipelineLayout(device, bindGroupLayouts, bindGroupsCount);
    assert(mPipelineLayout);

    WGPUComputePipelineDescriptor computePipelineDesc{};
    computePipelineDesc.nextInChain = nullptr;
    computePipelineDesc.label = pipelineLabel;
    computePipelineDesc.layout = mPipelineLayout;
    computePipelineDesc.compute.nextInChain = nullptr;
    computePipelineDesc.compute.module = mShaderModule;
    computePipelineDesc.compute.entryPoint = "cs_main";
    computePipelineDesc.compute.constantCount = 0;
    computePipelineDesc.compute.constants = nullptr;

    mComputePipeline = wgpuDeviceCreateComputePipeline(device, &computePipelineDesc);
    assert(mComputePipeline);
}

void WgComputePipeline::release()
{
    if (mComputePipeline) wgpuComputePipelineRelease(mComputePipeline);
    mComputePipeline = nullptr;
    WgPipeline::release();
}


void WgComputePipeline::set(WGPUComputePassEncoder computePassEncoder)
{
    wgpuComputePassEncoderSetPipeline(computePassEncoder, mComputePipeline);
}
