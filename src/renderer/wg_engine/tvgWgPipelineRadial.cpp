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

#include "tvgWgPipelineRadial.h"
#include "tvgWgShaderSrc.h"

//************************************************************************
// WgPipelineBindGroupRadial
//************************************************************************

void WgPipelineDataRadial::updateGradient(RadialGradient* radialGradient) {
    const Fill::ColorStop* stops = nullptr;
    auto stopCnt = radialGradient->colorStops(&stops);

    uGradientInfo.nStops[0] = stopCnt;
    uGradientInfo.nStops[1] = 0.5f;

    for (uint32_t i = 0; i < stopCnt; ++i) {
        uGradientInfo.stopPoints[i] = stops[i].offset;
        uGradientInfo.stopColors[i * 4 + 0] = stops[i].r / 255.f;
        uGradientInfo.stopColors[i * 4 + 1] = stops[i].g / 255.f;
        uGradientInfo.stopColors[i * 4 + 2] = stops[i].b / 255.f;
        uGradientInfo.stopColors[i * 4 + 3] = stops[i].a / 255.f;
    }

    radialGradient->radial(
        &uGradientInfo.centerPos[0],
        &uGradientInfo.centerPos[1],
        &uGradientInfo.radius[0]);
}

//************************************************************************
// WgPipelineBindGroupRadial
//************************************************************************

void WgPipelineBindGroupRadial::initialize(WGPUDevice device, WgPipelineRadial& pipelinePipelineRadial) {
        // buffer uniform uMatrix
    WGPUBufferDescriptor bufferUniformDesc_uMatrix{};
    bufferUniformDesc_uMatrix.nextInChain = nullptr;
    bufferUniformDesc_uMatrix.label = "Buffer uniform pipeline radial uMatrix";
    bufferUniformDesc_uMatrix.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferUniformDesc_uMatrix.size = sizeof(WgPipelineMatrix);
    bufferUniformDesc_uMatrix.mappedAtCreation = false;
    uBufferMatrix = wgpuDeviceCreateBuffer(device, &bufferUniformDesc_uMatrix);
    assert(uBufferMatrix);
    // buffer uniform uColorInfo
    WGPUBufferDescriptor bufferUniformDesc_uGradientInfo{};
    bufferUniformDesc_uGradientInfo.nextInChain = nullptr;
    bufferUniformDesc_uGradientInfo.label = "Buffer uniform pipeline radial uGradientInfo";
    bufferUniformDesc_uGradientInfo.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferUniformDesc_uGradientInfo.size = sizeof(WgPipelineRadialGradientInfo);
    bufferUniformDesc_uGradientInfo.mappedAtCreation = false;
    uBufferGradientInfo = wgpuDeviceCreateBuffer(device, &bufferUniformDesc_uGradientInfo);
    assert(uBufferGradientInfo);

    // bind group entry @binding(0) uMatrix
    WGPUBindGroupEntry bindGroupEntry_uMatrix{};
    bindGroupEntry_uMatrix.nextInChain = nullptr;
    bindGroupEntry_uMatrix.binding = 0;
    bindGroupEntry_uMatrix.buffer = uBufferMatrix;
    bindGroupEntry_uMatrix.offset = 0;
    bindGroupEntry_uMatrix.size = sizeof(WgPipelineMatrix);
    bindGroupEntry_uMatrix.sampler = nullptr;
    bindGroupEntry_uMatrix.textureView = nullptr;
    // bind group entry @binding(1) uGradientInfo
    WGPUBindGroupEntry bindGroupEntry_uGradientInfo{};
    bindGroupEntry_uGradientInfo.nextInChain = nullptr;
    bindGroupEntry_uGradientInfo.binding = 1;
    bindGroupEntry_uGradientInfo.buffer = uBufferGradientInfo;
    bindGroupEntry_uGradientInfo.offset = 0;
    bindGroupEntry_uGradientInfo.size = sizeof(WgPipelineRadialGradientInfo);
    bindGroupEntry_uGradientInfo.sampler = nullptr;
    bindGroupEntry_uGradientInfo.textureView = nullptr;
    // bind group entries
    WGPUBindGroupEntry bindGroupEntries[] {
        bindGroupEntry_uMatrix,      // @binding(0) uMatrix
        bindGroupEntry_uGradientInfo // @binding(1) uGradientInfo
    };
    // bind group descriptor
    WGPUBindGroupDescriptor bindGroupDescPipeline{};
    bindGroupDescPipeline.nextInChain = nullptr;
    bindGroupDescPipeline.label = "The binding group pipeline radial";
    bindGroupDescPipeline.layout = pipelinePipelineRadial.mBindGroupLayout;
    bindGroupDescPipeline.entryCount = 2;
    bindGroupDescPipeline.entries = bindGroupEntries;
    mBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescPipeline);
    assert(mBindGroup);
}

void WgPipelineBindGroupRadial::release() {
    if (uBufferGradientInfo) {
        wgpuBufferDestroy(uBufferGradientInfo);
        wgpuBufferRelease(uBufferGradientInfo);
        uBufferGradientInfo = nullptr;
    }
    if (uBufferMatrix) {
        wgpuBufferDestroy(uBufferMatrix);
        wgpuBufferRelease(uBufferMatrix);
        uBufferMatrix = nullptr;
    }
    if (mBindGroup) {
        wgpuBindGroupRelease(mBindGroup);
        mBindGroup = nullptr;
    }
}

void WgPipelineBindGroupRadial::update(WGPUQueue queue, WgPipelineDataRadial& pipelineDataRadial) {
    wgpuQueueWriteBuffer(queue, uBufferMatrix, 0, &pipelineDataRadial.uMatrix, sizeof(pipelineDataRadial.uMatrix));
    wgpuQueueWriteBuffer(queue, uBufferGradientInfo, 0, &pipelineDataRadial.uGradientInfo, sizeof(pipelineDataRadial.uGradientInfo));
}

//************************************************************************
// WgPipelineRadial
//************************************************************************

void WgPipelineRadial::initialize(WGPUDevice device) {
    // bind group layout group 0
    // bind group layout descriptor @group(0) @binding(0) uMatrix
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry_uMatrix{};
    bindGroupLayoutEntry_uMatrix.nextInChain = nullptr;
    bindGroupLayoutEntry_uMatrix.binding = 0;
    bindGroupLayoutEntry_uMatrix.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntry_uMatrix.buffer.nextInChain = nullptr;
    bindGroupLayoutEntry_uMatrix.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntry_uMatrix.buffer.hasDynamicOffset = false;
    bindGroupLayoutEntry_uMatrix.buffer.minBindingSize = 0;
    // bind group layout descriptor @group(0) @binding(1) uColorInfo
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry_uColorInfo{};
    bindGroupLayoutEntry_uColorInfo.nextInChain = nullptr;
    bindGroupLayoutEntry_uColorInfo.binding = 1;
    bindGroupLayoutEntry_uColorInfo.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntry_uColorInfo.buffer.nextInChain = nullptr;
    bindGroupLayoutEntry_uColorInfo.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntry_uColorInfo.buffer.hasDynamicOffset = false;
    bindGroupLayoutEntry_uColorInfo.buffer.minBindingSize = 0;
    // bind group layout entries @group(0)
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        bindGroupLayoutEntry_uMatrix,
        bindGroupLayoutEntry_uColorInfo
    };
    // bind group layout descriptor scene @group(0)
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = "Bind group layout pipeline radial";
    bindGroupLayoutDesc.entryCount = 2;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries; // @binding
    mBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);
    assert(mBindGroupLayout);

    // pipeline layout

    // bind group layout descriptors
    WGPUBindGroupLayout mBindGroupLayouts[] {
        mBindGroupLayout
    };
    // pipeline layout descriptor
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.nextInChain = nullptr;
    pipelineLayoutDesc.label = "Pipeline pipeline layout radial";
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = mBindGroupLayouts;
    mPipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
    assert(mPipelineLayout);

    // depth stencil state
    WGPUDepthStencilState depthStencilState{};
    depthStencilState.nextInChain = nullptr;
    depthStencilState.format = WGPUTextureFormat_Stencil8;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.depthCompare = WGPUCompareFunction_Always;
    // depthStencilState.stencilFront
    depthStencilState.stencilFront.compare = WGPUCompareFunction_NotEqual;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Zero;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Zero;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Zero;
    // depthStencilState.stencilBack
    depthStencilState.stencilBack.compare = WGPUCompareFunction_NotEqual;
    depthStencilState.stencilBack.failOp = WGPUStencilOperation_Zero;
    depthStencilState.stencilBack.depthFailOp = WGPUStencilOperation_Zero;
    depthStencilState.stencilBack.passOp = WGPUStencilOperation_Zero;
    // stencil mask
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;
    // depth bias
    depthStencilState.depthBias = 0;
    depthStencilState.depthBiasSlopeScale = 0.0f;
    depthStencilState.depthBiasClamp = 0.0f;

    // shader module wgsl descriptor
    WGPUShaderModuleWGSLDescriptor shaderModuleWGSLDesc{};
	shaderModuleWGSLDesc.chain.next = nullptr;
	shaderModuleWGSLDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderModuleWGSLDesc.code = cShaderSource_PipelineRadial;
    // shader module descriptor
    WGPUShaderModuleDescriptor shaderModuleDesc{};
    shaderModuleDesc.nextInChain = &shaderModuleWGSLDesc.chain;
    shaderModuleDesc.label = "The shader module pipeline radial";
    shaderModuleDesc.hintCount = 0;
    shaderModuleDesc.hints = nullptr;
    mShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    assert(mShaderModule);

    // vertex attributes
    WGPUVertexAttribute vertexAttributes[] = {
        { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 }, // position
    };
    // vertex buffer layout
    WGPUVertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.arrayStride = sizeof(float) * 2; // position
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = 1; // position
    vertexBufferLayout.attributes = vertexAttributes;
    
    // blend state
    WGPUBlendState blendState{};
    // blendState.color
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    // blendState.alpha
    blendState.alpha.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;

    // color target state (WGPUTextureFormat_BGRA8UnormSrgb)
    WGPUColorTargetState colorTargetState0{};
    colorTargetState0.nextInChain = nullptr;
    colorTargetState0.format = WGPUTextureFormat_BGRA8Unorm;
    //colorTargetState0.format = WGPUTextureFormat_BGRA8UnormSrgb;
    colorTargetState0.blend = &blendState;
    colorTargetState0.writeMask = WGPUColorWriteMask_All;
    // color target states
    WGPUColorTargetState colorTargetStates[] = {
        colorTargetState0
    };
    // fragmanet state
    WGPUFragmentState fragmentState{};
    fragmentState.nextInChain = nullptr;
    fragmentState.module = mShaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    fragmentState.targetCount = 1;
    fragmentState.targets = colorTargetStates; // render target index

    // render pipeline descriptor
    WGPURenderPipelineDescriptor renderPipelineDesc{};
    renderPipelineDesc.nextInChain = nullptr;
    renderPipelineDesc.label = "Render pipeline pipeline radial";
    // renderPipelineDesc.layout
    renderPipelineDesc.layout = mPipelineLayout;
    // renderPipelineDesc.vertex
    renderPipelineDesc.vertex.nextInChain = nullptr;
    renderPipelineDesc.vertex.module = mShaderModule;
    renderPipelineDesc.vertex.entryPoint = "vs_main";
    renderPipelineDesc.vertex.constantCount = 0;
    renderPipelineDesc.vertex.constants = nullptr;
    renderPipelineDesc.vertex.bufferCount = 1;
    renderPipelineDesc.vertex.buffers = &vertexBufferLayout; // buffer index
    // renderPipelineDesc.primitive
    renderPipelineDesc.primitive.nextInChain = nullptr;
    renderPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    renderPipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    renderPipelineDesc.primitive.cullMode = WGPUCullMode_None;
    // renderPipelineDesc.depthStencil
    renderPipelineDesc.depthStencil = &depthStencilState;
    // renderPipelineDesc.multisample
    renderPipelineDesc.multisample.nextInChain = nullptr;
    renderPipelineDesc.multisample.count = 1;
    renderPipelineDesc.multisample.mask = 0xFFFFFFFF;
    renderPipelineDesc.multisample.alphaToCoverageEnabled = false;
    // renderPipelineDesc.fragment
    renderPipelineDesc.fragment = &fragmentState;
    mRenderPipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDesc);
    assert(mRenderPipeline);
}

void WgPipelineRadial::release() {
    wgpuRenderPipelineRelease(mRenderPipeline);
    wgpuShaderModuleRelease(mShaderModule);
    wgpuPipelineLayoutRelease(mPipelineLayout);
    wgpuBindGroupLayoutRelease(mBindGroupLayout);
}
