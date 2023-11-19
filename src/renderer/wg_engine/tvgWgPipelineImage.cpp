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

#include "tvgWgPipelineImage.h"
#include "tvgWgShaderSrc.h"

//************************************************************************
// WgPipelineDataImage
//************************************************************************

void WgPipelineDataImage::updateFormat(const ColorSpace format) {
    uColorInfo.format = (uint32_t)format;
}

void WgPipelineDataImage::updateOpacity(const uint8_t opacity) {
    uColorInfo.opacity = opacity / 255.0f; // alpha
}

//************************************************************************
// WgPipelineBindGroupImage
//************************************************************************

void WgPipelineBindGroupImage::initialize(WGPUDevice device, WgPipelineImage& pipelineImage, Surface* surface) {
    // buffer uniform uMatrix
    WGPUBufferDescriptor bufferUniformDesc_uMatrix{};
    bufferUniformDesc_uMatrix.nextInChain = nullptr;
    bufferUniformDesc_uMatrix.label = "Buffer uniform pipeline image uMatrix";
    bufferUniformDesc_uMatrix.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferUniformDesc_uMatrix.size = sizeof(WgPipelineMatrix);
    bufferUniformDesc_uMatrix.mappedAtCreation = false;
    uBufferMatrix = wgpuDeviceCreateBuffer(device, &bufferUniformDesc_uMatrix);
    assert(uBufferMatrix);
    // buffer uniform uColorInfo
    WGPUBufferDescriptor bufferUniformDesc_uColorInfo{};
    bufferUniformDesc_uColorInfo.nextInChain = nullptr;
    bufferUniformDesc_uColorInfo.label = "Buffer uniform pipeline image uColorInfo";
    bufferUniformDesc_uColorInfo.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    bufferUniformDesc_uColorInfo.size = sizeof(WgPipelineImageColorInfo);
    bufferUniformDesc_uColorInfo.mappedAtCreation = false;
    uBufferColorInfo = wgpuDeviceCreateBuffer(device, &bufferUniformDesc_uColorInfo);
    assert(uBufferColorInfo);
    // sampler uniform uSamplerBase
    WGPUSamplerDescriptor samplerDesc_uSamplerBase{};
    samplerDesc_uSamplerBase.nextInChain = nullptr;
    samplerDesc_uSamplerBase.label = "Sampler uniform pipeline image uSamplerBase";
    samplerDesc_uSamplerBase.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc_uSamplerBase.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc_uSamplerBase.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc_uSamplerBase.magFilter = WGPUFilterMode_Nearest;
    samplerDesc_uSamplerBase.minFilter = WGPUFilterMode_Nearest;
    samplerDesc_uSamplerBase.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    samplerDesc_uSamplerBase.lodMinClamp = 0.0f;
    samplerDesc_uSamplerBase.lodMaxClamp = 32.0f;
    samplerDesc_uSamplerBase.compare = WGPUCompareFunction_Undefined;
    samplerDesc_uSamplerBase.maxAnisotropy = 1;
    uSamplerBase = wgpuDeviceCreateSampler(device, &samplerDesc_uSamplerBase);
    assert(uSamplerBase);
    // webgpu texture data holder
    WGPUTextureDescriptor textureDesc{};
    textureDesc.nextInChain = nullptr;
    textureDesc.label = "Texture base pipeline image";
    textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.size = { surface->w, surface->h, 1 };
    textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;
    mTextureBase = wgpuDeviceCreateTexture(device, &textureDesc);
    assert(mTextureBase);
    // texture view uniform uTextureViewBase
    WGPUTextureViewDescriptor textureViewDesc{};
    textureViewDesc.nextInChain = nullptr;
    textureViewDesc.label = "The depth-stencil texture view";
    textureViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
    textureViewDesc.dimension = WGPUTextureViewDimension_2D;
    textureViewDesc.baseMipLevel = 0;
    textureViewDesc.mipLevelCount = 1;
    textureViewDesc.baseArrayLayer = 0;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.aspect = WGPUTextureAspect_All;
    uTextureViewBase = wgpuTextureCreateView(mTextureBase, &textureViewDesc);
    assert(uTextureViewBase);

    // bind group entry @binding(0) uMatrix
    WGPUBindGroupEntry bindGroupEntry_uMatrix{};
    bindGroupEntry_uMatrix.nextInChain = nullptr;
    bindGroupEntry_uMatrix.binding = 0;
    bindGroupEntry_uMatrix.buffer = uBufferMatrix;
    bindGroupEntry_uMatrix.offset = 0;
    bindGroupEntry_uMatrix.size = sizeof(WgPipelineMatrix);
    bindGroupEntry_uMatrix.sampler = nullptr;
    bindGroupEntry_uMatrix.textureView = nullptr;
    // bind group entry @binding(1) uColorInfo
    WGPUBindGroupEntry bindGroupEntry_uColorInfo{};
    bindGroupEntry_uColorInfo.nextInChain = nullptr;
    bindGroupEntry_uColorInfo.binding = 1;
    bindGroupEntry_uColorInfo.buffer = uBufferColorInfo;
    bindGroupEntry_uColorInfo.offset = 0;
    bindGroupEntry_uColorInfo.size = sizeof(WgPipelineImageColorInfo);
    bindGroupEntry_uColorInfo.sampler = nullptr;
    bindGroupEntry_uColorInfo.textureView = nullptr;
    // bind group entry @binding(2) uSamplerBase
    WGPUBindGroupEntry bindGroupEntry_uSamplerBase{};
    bindGroupEntry_uSamplerBase.nextInChain = nullptr;
    bindGroupEntry_uSamplerBase.binding = 2;
    bindGroupEntry_uSamplerBase.buffer = nullptr;
    bindGroupEntry_uSamplerBase.offset = 0;
    bindGroupEntry_uSamplerBase.size = 0;
    bindGroupEntry_uSamplerBase.sampler = uSamplerBase;
    bindGroupEntry_uSamplerBase.textureView = nullptr;
    // bind group entry @binding(3) uTextureViewBase
    WGPUBindGroupEntry bindGroupEntry_uTextureViewBase{};
    bindGroupEntry_uTextureViewBase.nextInChain = nullptr;
    bindGroupEntry_uTextureViewBase.binding = 3;
    bindGroupEntry_uTextureViewBase.buffer = nullptr;
    bindGroupEntry_uTextureViewBase.offset = 0;
    bindGroupEntry_uTextureViewBase.size = 0;
    bindGroupEntry_uTextureViewBase.sampler = nullptr;
    bindGroupEntry_uTextureViewBase.textureView = uTextureViewBase;
    // bind group entries
    WGPUBindGroupEntry bindGroupEntries[] {
        bindGroupEntry_uMatrix,         // @binding(0) uMatrix
        bindGroupEntry_uColorInfo,      // @binding(1) uColorInfo
        bindGroupEntry_uSamplerBase,    // @binding(2) uSamplerBase
        bindGroupEntry_uTextureViewBase // @binding(3) uTextureViewBase
    };
    // bind group descriptor
    WGPUBindGroupDescriptor bindGroupDescPipeline{};
    bindGroupDescPipeline.nextInChain = nullptr;
    bindGroupDescPipeline.label = "The binding group pipeline image";
    bindGroupDescPipeline.layout = pipelineImage.mBindGroupLayout;
    bindGroupDescPipeline.entryCount = 4;
    bindGroupDescPipeline.entries = bindGroupEntries;
    mBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescPipeline);
    assert(mBindGroup);
}

void WgPipelineBindGroupImage::release() {
    if (uTextureViewBase) { 
        wgpuTextureViewRelease(uTextureViewBase);
        uTextureViewBase = nullptr;
    }
    if (mTextureBase) { 
        wgpuTextureDestroy(mTextureBase);
        wgpuTextureRelease(mTextureBase);
        mTextureBase = nullptr;
    }
    if (uSamplerBase) { 
        wgpuSamplerRelease(uSamplerBase);
        uSamplerBase = nullptr;
    }
    if (uBufferColorInfo) { 
        wgpuBufferDestroy(uBufferColorInfo);
        wgpuBufferRelease(uBufferColorInfo);
        uBufferColorInfo = nullptr;
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

void WgPipelineBindGroupImage::update(WGPUQueue queue, WgPipelineDataImage& pipelineDataImage, Surface* surface) {
    wgpuQueueWriteBuffer(queue, uBufferMatrix, 0, &pipelineDataImage.uMatrix, sizeof(pipelineDataImage.uMatrix));
    wgpuQueueWriteBuffer(queue, uBufferColorInfo, 0, &pipelineDataImage.uColorInfo, sizeof(pipelineDataImage.uColorInfo));
    WGPUImageCopyTexture imageCopyTexture{};
    imageCopyTexture.nextInChain = nullptr;
    imageCopyTexture.texture = mTextureBase;
    imageCopyTexture.mipLevel = 0;
    imageCopyTexture.origin = { 0, 0, 0 };
    imageCopyTexture.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout textureDataLayout{};
    textureDataLayout.nextInChain = nullptr;
    textureDataLayout.offset = 0;
    textureDataLayout.bytesPerRow = 4 * surface->w;
    textureDataLayout.rowsPerImage = surface->h;
    WGPUExtent3D writeSize{};
    writeSize.width = surface->w;
    writeSize.height = surface->h;
    writeSize.depthOrArrayLayers = 1;
    wgpuQueueWriteTexture(queue, &imageCopyTexture, surface->data, 4 * surface->w * surface->h, &textureDataLayout, &writeSize);
}

//************************************************************************
// WgPipelineImage
//************************************************************************

void WgPipelineImage::initialize(WGPUDevice device) {
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
    // bind group layout descriptor @group(0) @binding(2) uSamplerBase
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry_uSamplerBase{};
    bindGroupLayoutEntry_uSamplerBase.nextInChain = nullptr;
    bindGroupLayoutEntry_uSamplerBase.binding = 2;
    bindGroupLayoutEntry_uSamplerBase.visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntry_uSamplerBase.sampler.nextInChain = nullptr;
    bindGroupLayoutEntry_uSamplerBase.sampler.type = WGPUSamplerBindingType_Filtering;
    // bind group layout descriptor @group(0) @binding(3) uTextureViewBase
    WGPUBindGroupLayoutEntry bindGroupLayoutEntry_uTextureViewBase{};
    bindGroupLayoutEntry_uTextureViewBase.nextInChain = nullptr;
    bindGroupLayoutEntry_uTextureViewBase.binding = 3;
    bindGroupLayoutEntry_uTextureViewBase.visibility = WGPUShaderStage_Fragment;
    bindGroupLayoutEntry_uTextureViewBase.texture.nextInChain = nullptr;
    bindGroupLayoutEntry_uTextureViewBase.texture.sampleType = WGPUTextureSampleType_Float;
    bindGroupLayoutEntry_uTextureViewBase.texture.viewDimension = WGPUTextureViewDimension_2D;
    bindGroupLayoutEntry_uTextureViewBase.texture.multisampled = false;
    // bind group layout entries @group(0)
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        bindGroupLayoutEntry_uMatrix,
        bindGroupLayoutEntry_uColorInfo,
        bindGroupLayoutEntry_uSamplerBase,
        bindGroupLayoutEntry_uTextureViewBase
    };
    // bind group layout descriptor scene @group(0)
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.nextInChain = nullptr;
    bindGroupLayoutDesc.label = "Bind group layout pipeline image";
    bindGroupLayoutDesc.entryCount = 4;
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
    pipelineLayoutDesc.label = "Pipeline pipeline layout image";
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
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Zero;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Zero;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Zero;
    // depthStencilState.stencilBack
    depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
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
    shaderModuleWGSLDesc.code = cShaderSource_PipelineImage;
    // shader module descriptor
    WGPUShaderModuleDescriptor shaderModuleDesc{};
    shaderModuleDesc.nextInChain = &shaderModuleWGSLDesc.chain;
    shaderModuleDesc.label = "The shader module pipeline image";
    shaderModuleDesc.hintCount = 0;
    shaderModuleDesc.hints = nullptr;
    mShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    assert(mShaderModule);

    // vertex attributes
    WGPUVertexAttribute vertexAttributesPos[] = {
        { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 } // position
    };
    // vertex buffer layout position
    WGPUVertexBufferLayout vertexBufferLayoutPos{};
    vertexBufferLayoutPos.arrayStride = sizeof(float) * 2; // position
    vertexBufferLayoutPos.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayoutPos.attributeCount = 1; // position
    vertexBufferLayoutPos.attributes = vertexAttributesPos;
    // vertex attributes
    WGPUVertexAttribute vertexAttributesTex[] = {
        { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 1 } // tex coords
    };
    // vertex buffer layout tex coords
    WGPUVertexBufferLayout vertexBufferLayoutTex{};
    vertexBufferLayoutTex.arrayStride = sizeof(float) * 2; // tex coords
    vertexBufferLayoutTex.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayoutTex.attributeCount = 1; // tex coords
    vertexBufferLayoutTex.attributes = vertexAttributesTex;
    // vertex attributes
    WGPUVertexBufferLayout vertexBufferLayouts[] = {
        vertexBufferLayoutPos,
        vertexBufferLayoutTex
    };
    
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
    renderPipelineDesc.label = "Render pipeline pipeline image";
    // renderPipelineDesc.layout
    renderPipelineDesc.layout = mPipelineLayout;
    // renderPipelineDesc.vertex
    renderPipelineDesc.vertex.nextInChain = nullptr;
    renderPipelineDesc.vertex.module = mShaderModule;
    renderPipelineDesc.vertex.entryPoint = "vs_main";
    renderPipelineDesc.vertex.constantCount = 0;
    renderPipelineDesc.vertex.constants = nullptr;
    renderPipelineDesc.vertex.bufferCount = 2;
    renderPipelineDesc.vertex.buffers = vertexBufferLayouts; // buffer index
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

void WgPipelineImage::release() {
    wgpuRenderPipelineRelease(mRenderPipeline);
    wgpuShaderModuleRelease(mShaderModule);
    wgpuPipelineLayoutRelease(mPipelineLayout);
    wgpuBindGroupLayoutRelease(mBindGroupLayout);
}
