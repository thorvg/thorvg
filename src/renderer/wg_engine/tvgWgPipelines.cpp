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

#include "tvgWgPipelines.h"
#include "tvgWgShaderSrc.h"
#include <cstring>

WGPUShaderModule WgPipelines::createShaderModule(WGPUDevice device, const char* label, const char* code)
{
    WGPUShaderModuleWGSLDescriptor shaderModuleWGSLDesc{};
    shaderModuleWGSLDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderModuleWGSLDesc.code = code;
    const WGPUShaderModuleDescriptor shaderModuleDesc { .nextInChain = &shaderModuleWGSLDesc.chain, .label = label };
    return wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
}


WGPUPipelineLayout WgPipelines::createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount)
{
    const WGPUPipelineLayoutDescriptor pipelineLayoutDesc { .bindGroupLayoutCount = bindGroupLayoutsCount, .bindGroupLayouts = bindGroupLayouts };
    return wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
}


WGPURenderPipeline WgPipelines::createRenderPipeline(
    WGPUDevice device, const char* pipelineLabel,
    const WGPUShaderModule shaderModule, const WGPUPipelineLayout pipelineLayout,
    const WGPUVertexBufferLayout *vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
    const WGPUColorWriteMaskFlags writeMask,
    const WGPUCompareFunction stencilFunctionFrnt, const WGPUStencilOperation stencilOperationFrnt,
    const WGPUCompareFunction stencilFunctionBack, const WGPUStencilOperation stencilOperationBack,
    const WGPUPrimitiveState primitiveState, const WGPUMultisampleState multisampleState, const WGPUBlendState blendState)
{
    const WGPUColorTargetState colorTargetState { .format = WGPUTextureFormat_RGBA8Unorm, .blend = &blendState, .writeMask = writeMask };
    const WGPUColorTargetState colorTargetStates[] { colorTargetState };
    const WGPUDepthStencilState depthStencilState {
        .format = WGPUTextureFormat_Stencil8, .depthCompare = WGPUCompareFunction_Always,
        .stencilFront = { .compare = stencilFunctionFrnt, .failOp = stencilOperationFrnt, .depthFailOp = stencilOperationFrnt, .passOp = stencilOperationFrnt },
        .stencilBack =  { .compare = stencilFunctionBack, .failOp = stencilOperationBack, .depthFailOp = stencilOperationBack, .passOp = stencilOperationBack },
        .stencilReadMask = 0xFFFFFFFF, .stencilWriteMask = 0xFFFFFFFF
    };
    const WGPUVertexState   vertexState   { .module = shaderModule, .entryPoint = "vs_main", .bufferCount = vertexBufferLayoutsCount, .buffers = vertexBufferLayouts };
    const WGPUFragmentState fragmentState { .module = shaderModule, .entryPoint = "fs_main", .targetCount = 1, .targets = colorTargetStates };
    const WGPURenderPipelineDescriptor renderPipelineDesc {
        .label = pipelineLabel,
        .layout = pipelineLayout,
        .vertex = vertexState,
        .primitive = primitiveState,
        .depthStencil = &depthStencilState,
        .multisample = multisampleState,
        .fragment = &fragmentState
    };
    return wgpuDeviceCreateRenderPipeline(device, &renderPipelineDesc);
}


WGPUComputePipeline WgPipelines::createComputePipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const char* entryPoint,
        const WGPUPipelineLayout pipelineLayout)
{
    const WGPUComputePipelineDescriptor computePipelineDesc{
        .label = pipelineLabel,
        .layout = pipelineLayout,
        .compute = { 
            .module = shaderModule,
            .entryPoint = entryPoint
        }
    };
    return wgpuDeviceCreateComputePipeline(device, &computePipelineDesc);
}


void WgPipelines::releaseComputePipeline(WGPUComputePipeline& computePipeline)
{
    if (computePipeline) {
        wgpuComputePipelineRelease(computePipeline);
        computePipeline = nullptr;
    }
}


void WgPipelines::releaseRenderPipeline(WGPURenderPipeline& renderPipeline)
{
    if (renderPipeline) {
        wgpuRenderPipelineRelease(renderPipeline);
        renderPipeline = nullptr;
    }
}


void WgPipelines::releasePipelineLayout(WGPUPipelineLayout& pipelineLayout)
{
    if (pipelineLayout) {
        wgpuPipelineLayoutRelease(pipelineLayout);
        pipelineLayout = nullptr;
    }
}


void WgPipelines::releaseShaderModule(WGPUShaderModule& shaderModule)
{
    if (shaderModule) {
        wgpuShaderModuleRelease(shaderModule);
        shaderModule = nullptr;
    }
}


void WgPipelines::initialize(WgContext& context)
{
    // share pipelines to context
    assert(!context.pipelines);
    context.pipelines = this;

    // initialize bind group layouts
    layouts.initialize(context);

    // common pipeline settings
    const WGPUVertexAttribute vertexAttributePos { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0 };
    const WGPUVertexAttribute vertexAttributeTex { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 1 };
    const WGPUVertexAttribute vertexAttributesPos[] { vertexAttributePos };
    const WGPUVertexAttribute vertexAttributesTex[] { vertexAttributeTex };
    const WGPUVertexBufferLayout vertexBufferLayoutPos { .arrayStride = 8, .stepMode = WGPUVertexStepMode_Vertex, .attributeCount = 1, .attributes = vertexAttributesPos };
    const WGPUVertexBufferLayout vertexBufferLayoutTex { .arrayStride = 8, .stepMode = WGPUVertexStepMode_Vertex, .attributeCount = 1, .attributes = vertexAttributesTex };
    const WGPUVertexBufferLayout vertexBufferLayoutsShape[] { vertexBufferLayoutPos };
    const WGPUVertexBufferLayout vertexBufferLayoutsImage[] { vertexBufferLayoutPos, vertexBufferLayoutTex };
    const WGPUPrimitiveState primitiveState { .topology = WGPUPrimitiveTopology_TriangleList };
    const WGPUMultisampleState multisampleState { .count = 1, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false };

    // blend states
    const WGPUBlendState blendStateSrc {
        .color = { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_Zero },
        .alpha = { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_Zero }
    };
    const WGPUBlendState blendStateNrm {
        .color = { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha },
        .alpha = { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha }
    };
    const WGPUBlendState blendStates[] {
        blendStateSrc, // WgPipelineBlendType::SrcOver
        blendStateNrm, // WgPipelineBlendType::Normal
        blendStateSrc  // WgPipelineBlendType::Custom (same as SrcOver)
    };

    // bind group layouts
    const WGPUBindGroupLayout bindGroupLayoutsStencil[] = {
        layouts.layoutBuffer1Un,
        layouts.layoutBuffer2Un
    };
    const WGPUBindGroupLayout bindGroupLayoutsSolid[] = {
        layouts.layoutBuffer1Un,
        layouts.layoutBuffer2Un,
        layouts.layoutBuffer1Un
    };
    const WGPUBindGroupLayout bindGroupLayoutsGradient[] = {
        layouts.layoutBuffer1Un,
        layouts.layoutBuffer2Un,
        layouts.layoutTexSampledBuff1Un
    };
    const WGPUBindGroupLayout bindGroupLayoutsImage[] = {
        layouts.layoutBuffer1Un,
        layouts.layoutBuffer2Un,
        layouts.layoutTexSampled
    };
    const WGPUBindGroupLayout bindGroupLayoutsMergeMasks[] = {
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexStrorage1WO
    };
    const WGPUBindGroupLayout bindGroupLayoutsBlend[] = {
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexStrorage1WO,
        layouts.layoutBuffer1Un
    };
    const WGPUBindGroupLayout bindGroupLayoutsCompose[] = {
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexStrorage1WO
    };
    const WGPUBindGroupLayout bindGroupLayoutsCopy[] = {
        layouts.layoutTexStrorage1RO,
        layouts.layoutTexScreen1WO
    };

    // pipeline layouts
    layoutStencil = createPipelineLayout(context.device, bindGroupLayoutsStencil, 2);
    layoutSolid = createPipelineLayout(context.device, bindGroupLayoutsSolid, 3);
    layoutGradient = createPipelineLayout(context.device, bindGroupLayoutsGradient, 3);
    layoutImage = createPipelineLayout(context.device, bindGroupLayoutsImage, 3);
    layoutBlend = createPipelineLayout(context.device, bindGroupLayoutsBlend, 4);
    layoutCompose = createPipelineLayout(context.device, bindGroupLayoutsCompose, 4);
    layoutMergeMasks = createPipelineLayout(context.device, bindGroupLayoutsMergeMasks, 3);
    layoutCopy = createPipelineLayout(context.device, bindGroupLayoutsCopy, 2);

    // graphics shader modules
    shaderStencil = createShaderModule(context.device, "The shader stencil", cShaderSrc_Stencil);
    shaderSolid = createShaderModule(context.device, "The shader solid", cShaderSrc_Solid);
    shaderRadial = createShaderModule(context.device, "The shader radial", cShaderSrc_Radial);
    shaderLinear = createShaderModule(context.device, "The shader linear", cShaderSrc_Linear);
    shaderImage = createShaderModule(context.device, "The shader image", cShaderSrc_Image);
    // computes shader modules
    shaderMergeMasks = createShaderModule(context.device, "The shader merge mask", cShaderSrc_MergeMasks);
    shaderCopy = createShaderModule(context.device, "The shader copy", cShaderSrc_Copy);

    // render pipeline winding
    winding = createRenderPipeline(
        context.device, "The render pipeline winding",
        shaderStencil, layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None,
        WGPUCompareFunction_Always, WGPUStencilOperation_IncrementWrap,
        WGPUCompareFunction_Always, WGPUStencilOperation_DecrementWrap,
        primitiveState, multisampleState, blendStateSrc);
    // render pipeline even-odd
    evenodd = createRenderPipeline(
        context.device, "The render pipeline even-odd",
        shaderStencil, layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None,
        WGPUCompareFunction_Always, WGPUStencilOperation_Invert,
        WGPUCompareFunction_Always, WGPUStencilOperation_Invert,
        primitiveState, multisampleState, blendStateSrc);
    // render pipeline direct
    direct = createRenderPipeline(
        context.device, "The render pipeline direct",
        shaderStencil, layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None,
        WGPUCompareFunction_Always, WGPUStencilOperation_Replace,
        WGPUCompareFunction_Always, WGPUStencilOperation_Replace,
        primitiveState, multisampleState, blendStateSrc);
    // render pipeline clip path
    clipPath = createRenderPipeline(
        context.device, "The render pipeline clip path",
        shaderStencil, layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All,
        WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
        WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
        primitiveState, multisampleState, blendStateSrc);

    // render pipeline solid
    for (uint32_t i = 0; i < 3; i++) {
        solid[i] = createRenderPipeline(
            context.device, "The render pipeline solid",
            shaderSolid, layoutSolid,
            vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // render pipeline radial
    for (uint32_t i = 0; i < 3; i++) {
        radial[i] = createRenderPipeline(
            context.device, "The render pipeline radial",
            shaderRadial, layoutGradient,
            vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // render pipeline linear
    for (uint32_t i = 0; i < 3; i++) {
        linear[i] = createRenderPipeline(
            context.device, "The render pipeline linear",
            shaderLinear, layoutGradient,
            vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // render pipeline image
    for (uint32_t i = 0; i < 3; i++) {
        image[i] = createRenderPipeline(
            context.device, "The render pipeline image",
            shaderImage, layoutImage,
            vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // compute pipelines
    mergeMasks = createComputePipeline(context.device, "The pipeline merge masks", shaderMergeMasks, "cs_main", layoutMergeMasks);
    copy = createComputePipeline(context.device, "The pipeline copy", shaderCopy, "cs_main", layoutCopy);

    // compute shader blend names
    const char* shaderBlendNames[] {
        "cs_main_Normal",
        "cs_main_Add",
        "cs_main_Screen",
        "cs_main_Multiply",
        "cs_main_Overlay",
        "cs_main_Difference",
        "cs_main_Exclusion",
        "cs_main_SrcOver",
        "cs_main_Darken",
        "cs_main_Lighten",
        "cs_main_ColorDodge",
        "cs_main_ColorBurn",
        "cs_main_HardLight",
        "cs_main_SoftLight"
    };

    // create blend shaders
    char shaderSourceBuff[16384];
    shaderBlendSolid = createShaderModule(context.device, "The shader blend solid", strcat(strcpy(shaderSourceBuff, cShaderSrc_BlendHeader_Solid), cShaderSrc_Blend_Funcs));
    shaderBlendGradient = createShaderModule(context.device, "The shader blend gradient", strcat(strcpy(shaderSourceBuff, cShaderSrc_BlendHeader_Gradient), cShaderSrc_Blend_Funcs));
    shaderBlendImage = createShaderModule(context.device, "The shader blend image", strcat(strcpy(shaderSourceBuff, cShaderSrc_BlendHeader_Image), cShaderSrc_Blend_Funcs));

    // create blend pipelines
    const size_t shaderBlendNamesCnt = sizeof(shaderBlendNames) / sizeof(shaderBlendNames[0]);
    const size_t pipesBlendCnt = sizeof(blendSolid) / sizeof(blendSolid[0]);
    assert(shaderBlendNamesCnt == pipesBlendCnt);
    for (uint32_t i = 0; i < pipesBlendCnt; i++) {
        blendSolid[i] = createComputePipeline(context.device, "The pipeline blend solid", shaderBlendSolid, shaderBlendNames[i], layoutBlend);
        blendGradient[i] = createComputePipeline(context.device, "The pipeline blend gradient", shaderBlendGradient, shaderBlendNames[i], layoutBlend);
        blendImage[i] = createComputePipeline(context.device, "The pipeline blend image", shaderBlendImage, shaderBlendNames[i], layoutBlend);
    }

    // compute shader compose names
    const char* shaderComposeNames[] {
        "cs_main_None",
        "cs_main_ClipPath",
        "cs_main_AlphaMask",
        "cs_main_InvAlphaMask",
        "cs_main_LumaMask",
        "cs_main_InvLumaMask",
        "cs_main_AddMask",
        "cs_main_SubtractMask",
        "cs_main_IntersectMask",
        "cs_main_DifferenceMask",
        "cs_main_LightenMask",
        "cs_main_DarkenMask"
    };

    // create compose shaders
    shaderCompose = createShaderModule(context.device, "The shader compose", cShaderSrc_Compose);

    // create compose pipelines
    const size_t shaderComposeNamesCnt = sizeof(shaderComposeNames) / sizeof(shaderComposeNames[0]);
    const size_t pipesComposeCnt = sizeof(compose) / sizeof(compose[0]);
    assert(shaderComposeNamesCnt == pipesComposeCnt);
    for (uint32_t i = 0; i < pipesComposeCnt; i++)
        compose[i] = createComputePipeline(context.device, "The pipeline compose", shaderCompose, shaderComposeNames[i], layoutCompose);
}

void WgPipelines::releaseGraphicHandles(WgContext& context)
{
    releaseRenderPipeline(clipPath);
    for (uint32_t i = 0; i < 3; i++) {
        releaseRenderPipeline(image[i]);
        releaseRenderPipeline(linear[i]);
        releaseRenderPipeline(radial[i]);
        releaseRenderPipeline(solid[i]);
    }
    releaseRenderPipeline(direct);
    releaseRenderPipeline(evenodd);
    releaseRenderPipeline(winding);
    releasePipelineLayout(layoutImage);
    releasePipelineLayout(layoutGradient);
    releasePipelineLayout(layoutSolid);
    releasePipelineLayout(layoutStencil);
    releaseShaderModule(shaderImage);
    releaseShaderModule(shaderLinear);
    releaseShaderModule(shaderRadial);
    releaseShaderModule(shaderSolid);
    releaseShaderModule(shaderStencil);
}


void WgPipelines::releaseComputeHandles(WgContext& context)
{
    size_t pipesComposeCnt = sizeof(compose) / sizeof(compose[0]);
    for (uint32_t i = 0; i < pipesComposeCnt; i++)
        releaseComputePipeline(compose[i]);
    const size_t pipesBlendCnt = sizeof(blendSolid)/sizeof(blendSolid[0]);
    for (uint32_t i = 0; i < pipesBlendCnt; i++) {
        releaseComputePipeline(blendImage[i]);
        releaseComputePipeline(blendSolid[i]);
        releaseComputePipeline(blendGradient[i]);
    }
    releaseComputePipeline(copy);
    releaseComputePipeline(mergeMasks);
    releasePipelineLayout(layoutCompose);
    releasePipelineLayout(layoutBlend);
    releasePipelineLayout(layoutMergeMasks);
    releasePipelineLayout(layoutCopy);
    releaseShaderModule(shaderCompose);
    releaseShaderModule(shaderBlendImage);
    releaseShaderModule(shaderBlendGradient);
    releaseShaderModule(shaderBlendSolid);
    releaseShaderModule(shaderMergeMasks);
    releaseShaderModule(shaderCopy);
}

void WgPipelines::release(WgContext& context)
{
    releaseComputeHandles(context);
    releaseGraphicHandles(context);
    layouts.release(context);
}
