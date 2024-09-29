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
    const WGPUShaderModule shaderModule, const char* vsEntryPoint, const char* fsEntryPoint,
    const WGPUPipelineLayout pipelineLayout,
    const WGPUVertexBufferLayout *vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
    const WGPUColorWriteMaskFlags writeMask, const WGPUTextureFormat colorTargetFormat,
    const WGPUCompareFunction stencilFunctionFrnt, const WGPUStencilOperation stencilOperationFrnt,
    const WGPUCompareFunction stencilFunctionBack, const WGPUStencilOperation stencilOperationBack,
    const WGPUPrimitiveState primitiveState, const WGPUMultisampleState multisampleState, const WGPUBlendState blendState)
{
    const WGPUColorTargetState colorTargetState { .format = colorTargetFormat, .blend = &blendState, .writeMask = writeMask };
    const WGPUColorTargetState colorTargetStates[] { colorTargetState };
    const WGPUDepthStencilState depthStencilState {
        .format = WGPUTextureFormat_Stencil8, .depthCompare = WGPUCompareFunction_Always,
        .stencilFront = { .compare = stencilFunctionFrnt, .failOp = stencilOperationFrnt, .depthFailOp = stencilOperationFrnt, .passOp = stencilOperationFrnt },
        .stencilBack =  { .compare = stencilFunctionBack, .failOp = stencilOperationBack, .depthFailOp = stencilOperationBack, .passOp = stencilOperationBack },
        .stencilReadMask = 0xFFFFFFFF, .stencilWriteMask = 0xFFFFFFFF
    };
    const WGPUVertexState   vertexState   { .module = shaderModule, .entryPoint = vsEntryPoint, .bufferCount = vertexBufferLayoutsCount, .buffers = vertexBufferLayouts };
    const WGPUFragmentState fragmentState { .module = shaderModule, .entryPoint = fsEntryPoint, .targetCount = 1, .targets = colorTargetStates };
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
    const WGPUTextureFormat offscreenTargetFormat = WGPUTextureFormat_RGBA8Unorm;

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
        layouts.layoutTexSampledBuff2Un
    };
    const WGPUBindGroupLayout bindGroupLayoutsImage[] = {
        layouts.layoutBuffer1Un,
        layouts.layoutBuffer2Un,
        layouts.layoutTexSampled
    };
    const WGPUBindGroupLayout bindGroupLayoutsSceneComp[] = {
        layouts.layoutTexSampled,
        layouts.layoutTexSampled
    };
    const WGPUBindGroupLayout bindGroupLayoutsSceneBlend[] = {
        layouts.layoutTexSampled,
        layouts.layoutBuffer1Un
    };
    const WGPUBindGroupLayout bindGroupLayoutsBlit[] = {
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

    // pipeline layouts
    layoutStencil = createPipelineLayout(context.device, bindGroupLayoutsStencil, 2);
    layoutSolid = createPipelineLayout(context.device, bindGroupLayoutsSolid, 3);
    layoutGradient = createPipelineLayout(context.device, bindGroupLayoutsGradient, 3);
    layoutImage = createPipelineLayout(context.device, bindGroupLayoutsImage, 3);
    layoutSceneComp = createPipelineLayout(context.device, bindGroupLayoutsSceneComp, 2);
    layoutSceneBlend = createPipelineLayout(context.device, bindGroupLayoutsSceneBlend, 2);
    layoutBlit = createPipelineLayout(context.device, bindGroupLayoutsBlit, 1);
    layoutBlend = createPipelineLayout(context.device, bindGroupLayoutsBlend, 4);
    layoutMergeMasks = createPipelineLayout(context.device, bindGroupLayoutsMergeMasks, 3);

    // graphics shader modules
    shaderStencil = createShaderModule(context.device, "The shader stencil", cShaderSrc_Stencil);
    shaderSolid = createShaderModule(context.device, "The shader solid", cShaderSrc_Solid);
    shaderRadial = createShaderModule(context.device, "The shader radial", cShaderSrc_Radial);
    shaderLinear = createShaderModule(context.device, "The shader linear", cShaderSrc_Linear);
    shaderImage = createShaderModule(context.device, "The shader image", cShaderSrc_Image);
    shaderSceneComp = createShaderModule(context.device, "The shader scene composition", cShaderSrc_Scene_Comp);
    shaderSceneBlend = createShaderModule(context.device, "The shader scene blend", cShaderSrc_Scene_Blend);
    shaderBlit = createShaderModule(context.device, "The shader blit", cShaderSrc_Blit);
    // computes shader modules
    shaderMergeMasks = createShaderModule(context.device, "The shader merge mask", cShaderSrc_MergeMasks);

    // render pipeline winding
    winding = createRenderPipeline(
        context.device, "The render pipeline winding",
        shaderStencil, "vs_main", "fs_main", layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat,
        WGPUCompareFunction_Always, WGPUStencilOperation_IncrementWrap,
        WGPUCompareFunction_Always, WGPUStencilOperation_DecrementWrap,
        primitiveState, multisampleState, blendStateSrc);
    // render pipeline even-odd
    evenodd = createRenderPipeline(
        context.device, "The render pipeline even-odd",
        shaderStencil, "vs_main", "fs_main", layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat,
        WGPUCompareFunction_Always, WGPUStencilOperation_Invert,
        WGPUCompareFunction_Always, WGPUStencilOperation_Invert,
        primitiveState, multisampleState, blendStateSrc);
    // render pipeline direct
    direct = createRenderPipeline(
        context.device, "The render pipeline direct",
        shaderStencil,"vs_main", "fs_main",  layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat,
        WGPUCompareFunction_Always, WGPUStencilOperation_Replace,
        WGPUCompareFunction_Always, WGPUStencilOperation_Replace,
        primitiveState, multisampleState, blendStateSrc);
    // render pipeline clip path
    clipPath = createRenderPipeline(
        context.device, "The render pipeline clip path",
        shaderStencil, "vs_main", "fs_main", layoutStencil,
        vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat,
        WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
        WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
        primitiveState, multisampleState, blendStateSrc);

    // render pipeline solid
    for (uint32_t i = 0; i < 2; i++) {
        solid[i] = createRenderPipeline(
            context.device, "The render pipeline solid",
            shaderSolid, "vs_main", "fs_main", layoutSolid,
            vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // render pipeline radial
    for (uint32_t i = 0; i < 2; i++) {
        radial[i] = createRenderPipeline(
            context.device, "The render pipeline radial",
            shaderRadial, "vs_main", "fs_main", layoutGradient,
            vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // render pipeline linear
    for (uint32_t i = 0; i < 2; i++) {
        linear[i] = createRenderPipeline(
            context.device, "The render pipeline linear",
            shaderLinear, "vs_main", "fs_main", layoutGradient,
            vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // render pipeline image
    for (uint32_t i = 0; i < 2; i++) {
        image[i] = createRenderPipeline(
            context.device, "The render pipeline image",
            shaderImage, "vs_main", "fs_main", layoutImage,
            vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStates[i]);
    }

    // render pipeline blit
    blit = createRenderPipeline(context.device, "The render pipeline blit",
            shaderBlit, "vs_main", "fs_main", layoutBlit,
            vertexBufferLayoutsImage, 2,
            // must be preferred screen pixel format
            WGPUColorWriteMask_All, context.preferredFormat,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStateSrc);

    // render pipeline blend
    sceneBlend = createRenderPipeline(context.device, "The render pipeline scene blend",
            shaderSceneBlend, "vs_main", "fs_main", layoutSceneBlend,
            vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, blendStateNrm);

    // render pipeline scene clip path
    sceneClip = createRenderPipeline(
        context.device, "The render pipeline scene clip path",
        shaderSceneComp, "vs_main", "fs_main_ClipPath", layoutSceneComp,
        vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat,
        WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
        WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
        primitiveState, multisampleState, blendStateNrm);

    // TODO: remove fs_main_ClipPath shader from list after removing CompositeMethod::ClipPath value
    // compose shader names
    const char* shaderComposeNames[] {
        "fs_main_None",
        "fs_main_ClipPath", // TODO: remove after CompositeMethod updated
        "fs_main_AlphaMask",
        "fs_main_InvAlphaMask",
        "fs_main_LumaMask",
        "fs_main_InvLumaMask",
        "fs_main_AddMask",
        "fs_main_SubtractMask",
        "fs_main_IntersectMask",
        "fs_main_DifferenceMask",
        "fs_main_LightenMask",
        "fs_main_DarkenMask"
    };

    // TODO: remove fs_main_ClipPath shader from list after removing CompositeMethod::ClipPath value
    // compose shader blend states
    const WGPUBlendState composeBlends[] {
        blendStateNrm, // None
        blendStateNrm, // ClipPath // TODO: remove after CompositeMethod updated
        blendStateNrm, // AlphaMask
        blendStateNrm, // InvAlphaMask
        blendStateNrm, // LumaMask
        blendStateNrm, // InvLumaMask
        blendStateSrc, // AddMask
        blendStateSrc, // SubtractMask
        blendStateSrc, // IntersectMask
        blendStateSrc, // DifferenceMask
        blendStateSrc, // LightenMask
        blendStateSrc  // DarkenMask
    };

    // render pipeline scene composition
    size_t shaderComposeNamesCnt = sizeof(shaderComposeNames) / sizeof(shaderComposeNames[0]);
    size_t composeBlendspCnt = sizeof(composeBlends) / sizeof(composeBlends[0]);
    size_t sceneCompCnt = sizeof(sceneComp) / sizeof(sceneComp[0]);
    assert(shaderComposeNamesCnt == composeBlendspCnt);
    assert(composeBlendspCnt == sceneCompCnt);
    for (uint32_t i = 0; i < sceneCompCnt; i++) {
        sceneComp[i] = createRenderPipeline(
            context.device, "The render pipeline scene composition",
            shaderSceneComp, "vs_main", shaderComposeNames[i], layoutSceneComp,
            vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            WGPUCompareFunction_Always, WGPUStencilOperation_Zero,
            primitiveState, multisampleState, composeBlends[i]);
    }

    // compute pipelines
    mergeMasks = createComputePipeline(context.device, "The pipeline merge masks", shaderMergeMasks, "cs_main", layoutMergeMasks);

    // compute shader blend names
    const char* shaderBlendNames[] {
        "cs_main_Normal",
        "cs_main_Multiply",
        "cs_main_Screen",
        "cs_main_Overlay",
        "cs_main_Darken",
        "cs_main_Lighten",
        "cs_main_ColorDodge",
        "cs_main_ColorBurn",
        "cs_main_HardLight",
        "cs_main_SoftLight",
        "cs_main_Difference",
        "cs_main_Exclusion",
        "cs_main_Normal",  //TODO: a padding for reserved Hue.
        "cs_main_Normal",  //TODO: a padding for reserved Saturation.
        "cs_main_Normal",  //TODO: a padding for reserved Color.
        "cs_main_Normal",  //TODO: a padding for reserved Luminosity.
        "cs_main_Add",
        "cs_main_Normal"   //TODO: a padding for reserved Hardmix.
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
}

void WgPipelines::releaseGraphicHandles(WgContext& context)
{
    releaseRenderPipeline(blit);
    releaseRenderPipeline(clipPath);
    releaseRenderPipeline(sceneBlend);
    releaseRenderPipeline(sceneClip);
    size_t pipesSceneCompCnt = sizeof(sceneComp) / sizeof(sceneComp[0]);
    for (uint32_t i = 0; i < pipesSceneCompCnt; i++)
        releaseRenderPipeline(sceneComp[i]);
    for (uint32_t i = 0; i < 2; i++) {
        releaseRenderPipeline(image[i]);
        releaseRenderPipeline(linear[i]);
        releaseRenderPipeline(radial[i]);
        releaseRenderPipeline(solid[i]);
    }
    releaseRenderPipeline(direct);
    releaseRenderPipeline(evenodd);
    releaseRenderPipeline(winding);
    releasePipelineLayout(layoutBlit);
    releasePipelineLayout(layoutSceneBlend);
    releasePipelineLayout(layoutSceneComp);
    releasePipelineLayout(layoutImage);
    releasePipelineLayout(layoutGradient);
    releasePipelineLayout(layoutSolid);
    releasePipelineLayout(layoutStencil);
    releaseShaderModule(shaderBlit);
    releaseShaderModule(shaderSceneBlend);
    releaseShaderModule(shaderSceneComp);
    releaseShaderModule(shaderImage);
    releaseShaderModule(shaderLinear);
    releaseShaderModule(shaderRadial);
    releaseShaderModule(shaderSolid);
    releaseShaderModule(shaderStencil);
}


void WgPipelines::releaseComputeHandles(WgContext& context)
{
    const size_t pipesBlendCnt = sizeof(blendSolid)/sizeof(blendSolid[0]);
    for (uint32_t i = 0; i < pipesBlendCnt; i++) {
        releaseComputePipeline(blendImage[i]);
        releaseComputePipeline(blendSolid[i]);
        releaseComputePipeline(blendGradient[i]);
    }
    releaseComputePipeline(mergeMasks);
    releasePipelineLayout(layoutBlend);
    releasePipelineLayout(layoutMergeMasks);
    releaseShaderModule(shaderBlendImage);
    releaseShaderModule(shaderBlendGradient);
    releaseShaderModule(shaderBlendSolid);
    releaseShaderModule(shaderMergeMasks);
}

void WgPipelines::release(WgContext& context)
{
    releaseComputeHandles(context);
    releaseGraphicHandles(context);
    layouts.release(context);
}
