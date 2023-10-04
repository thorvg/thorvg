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

#include "tvgWgBrushFill.h"
#include "tvgWgShaderSrc.h"

//************************************************************************
// WgBrushFillData
//************************************************************************

// update matrix
void WgBrushFillData::updateMatrix(const float* viewMatrix, const RenderTransform* transform) {
    // create model matrix 4*4
    float modelMatrix[16]{};
    if (transform) {
        modelMatrix[0]  = transform->m.e11;
        modelMatrix[1]  = transform->m.e21;
        modelMatrix[3]  = transform->m.e31;
        modelMatrix[4]  = transform->m.e12;
        modelMatrix[5]  = transform->m.e22;
        modelMatrix[7]  = transform->m.e32;
        modelMatrix[10] = 1.0f;
        modelMatrix[12] = transform->m.e13;
        modelMatrix[13] = transform->m.e23;
        modelMatrix[15] = transform->m.e33;
    } else {
        modelMatrix[0] = 1.0f;
        modelMatrix[5] = 1.0f;
        modelMatrix[10] = 1.0f;
        modelMatrix[15] = 1.0f;
    }
    // matrix multiply
    for(auto i = 0; i < 4; ++i) {
        for(auto j = 0; j < 4; ++j) {
            float sum = 0.0;
            for (auto k = 0; k < 4; ++k)
                sum += viewMatrix[k*4+i] * modelMatrix[j*4+k];
            uMatrix.matrix[j*4+i] = sum;
        }
    }
}

//************************************************************************
// WgBrushFillDataBindGroup
//************************************************************************

// initialise
void WgBrushFillDataBindGroup::initialize(WGPUDevice device, WgBrushFill& brushFill) {
    // buffer uniform uMatrix
    WGPUBufferDescriptor bufferUniformDesc_uMatrix{};
    bufferUniformDesc_uMatrix.nextInChain = nullptr;
    bufferUniformDesc_uMatrix.label = "Buffer uniform brush fill uMatrix";
    bufferUniformDesc_uMatrix.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferUniformDesc_uMatrix.size = sizeof(WgBrushFillData_Matrix);
    bufferUniformDesc_uMatrix.mappedAtCreation = false;
    mBufferUniform_uMatrix = wgpuDeviceCreateBuffer(device, &bufferUniformDesc_uMatrix);
    assert(mBufferUniform_uMatrix);

    // bind group entry @binding(0) uMatrix
    WGPUBindGroupEntry bindGroupEntry_uMatrix{};
    bindGroupEntry_uMatrix.nextInChain = nullptr;
    bindGroupEntry_uMatrix.binding = 0;
    bindGroupEntry_uMatrix.buffer = mBufferUniform_uMatrix;
    bindGroupEntry_uMatrix.offset = 0;
    bindGroupEntry_uMatrix.size = sizeof(WgBrushFillData_Matrix);
    bindGroupEntry_uMatrix.sampler = nullptr;
    bindGroupEntry_uMatrix.textureView = nullptr;
    // bind group entries
    WGPUBindGroupEntry bindGroupEntries[] {
        bindGroupEntry_uMatrix // @binding(0) uMatrix
    };
    // bind group descriptor
    WGPUBindGroupDescriptor bindGroupDescBrush{};
    bindGroupDescBrush.nextInChain = nullptr;
    bindGroupDescBrush.label = "The binding group brush color";
    bindGroupDescBrush.layout = brushFill.mBindGroupLayout;
    bindGroupDescBrush.entryCount = 1;
    bindGroupDescBrush.entries = bindGroupEntries;
    mBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescBrush);
    assert(mBindGroup);
};

// release
void WgBrushFillDataBindGroup::release() {
    // destroy uniform buffers
    wgpuBufferDestroy(mBufferUniform_uMatrix);
    wgpuBufferRelease(mBufferUniform_uMatrix);
    // release bind group
    wgpuBindGroupRelease(mBindGroup);
};

// bind
void WgBrushFillDataBindGroup::bind(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex) {
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, groupIndex, mBindGroup, 0, nullptr);
}

// update buffers
void WgBrushFillDataBindGroup::update(WGPUQueue queue, WgBrushFillData& data) {
    // write uMatrux buffer
    wgpuQueueWriteBuffer(queue, mBufferUniform_uMatrix, 0, &data.uMatrix, sizeof(data.uMatrix));
};

//***********************************************************************
// WgBrushFill
//***********************************************************************

// create
void WgBrushFill::initialize(WGPUDevice device) {
    // bind group layout group 0
    // bind group layout descriptor @binding(0) uMatrix
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry_uMatrix{};
    bindGroupLayoutEntry_uMatrix.nextInChain = nullptr;
    bindGroupLayoutEntry_uMatrix.binding = 0;
    bindGroupLayoutEntry_uMatrix.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntry_uMatrix.buffer.nextInChain = nullptr;
    bindGroupLayoutEntry_uMatrix.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntry_uMatrix.buffer.hasDynamicOffset = false;
    bindGroupLayoutEntry_uMatrix.buffer.minBindingSize = 0;
    // bind group layout entries @group(0)
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        bindGroupLayoutEntry_uMatrix
    };
    // bind group layout descriptor @group(0)
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = "Brush color bind group layout";
    bindGroupLayoutDesc.entryCount = 1;
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
    pipelineLayoutDesc.label = "Brush color pipeline layout";
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
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Invert;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Invert;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Invert;
    // depthStencilState.stencilBack
    depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilBack.failOp = WGPUStencilOperation_Invert;
    depthStencilState.stencilBack.depthFailOp = WGPUStencilOperation_Invert;
    depthStencilState.stencilBack.passOp = WGPUStencilOperation_Invert;
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
    shaderModuleWGSLDesc.code = cShaderSource_BrushFill;
    // shader module descriptor
    WGPUShaderModuleDescriptor shaderModuleDesc{};
    shaderModuleDesc.nextInChain = &shaderModuleWGSLDesc.chain;
    shaderModuleDesc.label = "The shader module brush color";
    shaderModuleDesc.hintCount = 0;
    shaderModuleDesc.hints = nullptr;
    mShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    assert(mShaderModule);

    // vertex attributes
    WGPUVertexAttribute vertexAttributes[] = {
        { WGPUVertexFormat_Float32x3, sizeof(float) * 0, 0 }, // position
    };
    // vertex buffer layout
    WGPUVertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.arrayStride = sizeof(float) * 3; // position
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
    renderPipelineDesc.label = "Fill pipeline state";
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

// release
void WgBrushFill::release() {
    // render pipeline release
    wgpuRenderPipelineRelease(mRenderPipeline);
    // shader module release
    wgpuShaderModuleRelease(mShaderModule);
    // pipeline layout release
    wgpuPipelineLayoutRelease(mPipelineLayout);
    // bind group layouts release
    wgpuBindGroupLayoutRelease(mBindGroupLayout);
}
