#include "tvgWgpuBrushColor.h"
#include "tvgWgpuShaderSrc.h"

// create
void WgpuBrushColor::create(WGPUDevice device) {
    // bind group layout group 0
    // bind group layout descriptor @group(0) @binding(0) uMatrix
    WGPUBindGroupLayoutEntry bindGroupLayoutEntryMatrix{};
    bindGroupLayoutEntryMatrix.nextInChain = nullptr;
    bindGroupLayoutEntryMatrix.binding = 0;
    bindGroupLayoutEntryMatrix.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntryMatrix.buffer.nextInChain = nullptr;
    bindGroupLayoutEntryMatrix.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntryMatrix.buffer.hasDynamicOffset = false;
    bindGroupLayoutEntryMatrix.buffer.minBindingSize = 0;
    // bind group layout descriptor @group(0) @binding(1) uColorInfo
    WGPUBindGroupLayoutEntry bindGroupLayoutEntryColorInfo{};
    bindGroupLayoutEntryColorInfo.nextInChain = nullptr;
    bindGroupLayoutEntryColorInfo.binding = 1;
    bindGroupLayoutEntryColorInfo.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntryColorInfo.buffer.nextInChain = nullptr;
    bindGroupLayoutEntryColorInfo.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntryColorInfo.buffer.hasDynamicOffset = false;
    bindGroupLayoutEntryColorInfo.buffer.minBindingSize = 0;
    // bind group layout entries scene @group(0)
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] {
        bindGroupLayoutEntryMatrix,
        bindGroupLayoutEntryColorInfo
    };
    // bind group layout descriptor scene @group(0)
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescScene{};
    bindGroupLayoutDescScene.nextInChain = nullptr;
    bindGroupLayoutDescScene.label = "Brush color bind group layout scene";
    bindGroupLayoutDescScene.entryCount = 2;
    bindGroupLayoutDescScene.entries = bindGroupLayoutEntries; // @binding
    mBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescScene);
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
    depthStencilState.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.depthCompare = WGPUCompareFunction_Always;
    // depthStencilState.stencilFront
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
    // depthStencilState.stencilBack
    depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilBack.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack.passOp = WGPUStencilOperation_Keep;
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
    shaderModuleWGSLDesc.code = cShaderSource_BrushColor;
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
    WGPUColorTargetState colorTargetState{};
    colorTargetState.nextInChain = nullptr;
    colorTargetState.format = WGPUTextureFormat_BGRA8Unorm;
    colorTargetState.blend = &blendState;
    colorTargetState.writeMask = WGPUColorWriteMask_All;
    // color target states
    WGPUColorTargetState colorTargetStates[] = {
        colorTargetState
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
    renderPipelineDesc.label = "Solid pipeline state";
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
void WgpuBrushColor::release() {
    // render pipeline release
    wgpuRenderPipelineRelease(mRenderPipeline);
    // shader module release
    wgpuShaderModuleRelease(mShaderModule);
    // pipeline layout release
    wgpuPipelineLayoutRelease(mPipelineLayout);
    // bind group layouts release
    wgpuBindGroupLayoutRelease(mBindGroupLayout);
}
