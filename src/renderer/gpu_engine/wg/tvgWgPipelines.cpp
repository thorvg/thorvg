/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#include "tvgWgShaderSrc.h"
#include "tvgWgPipelines.h"
#include "tvgWgRenderData.h"
#include <cstring>

static const char* shaderBlendNames[]{
    "fs_main_Normal",
    "fs_main_Multiply",
    "fs_main_Screen",
    "fs_main_Overlay",
    "fs_main_Darken",
    "fs_main_Lighten",
    "fs_main_ColorDodge",
    "fs_main_ColorBurn",
    "fs_main_HardLight",
    "fs_main_SoftLight",
    "fs_main_Difference",
    "fs_main_Exclusion",
    "fs_main_Hue",
    "fs_main_Saturation",
    "fs_main_Color",
    "fs_main_Luminosity",
    "fs_main_Add",
    "fs_main_Normal"  // TODO: a padding for reserved Hardmix.
};

static const WGPUVertexAttribute vertexAttributePos{.format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0};
static const WGPUVertexAttribute vertexAttributeColor{.format = WGPUVertexFormat_Unorm8x4, .offset = 0, .shaderLocation = 1};
static const WGPUVertexAttribute vertexAttributeTex{.format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 1};
static const WGPUVertexAttribute vertexAttributesPos[]{vertexAttributePos};
static const WGPUVertexAttribute vertexAttributesColor[]{vertexAttributeColor};
static const WGPUVertexAttribute vertexAttributesTex[]{vertexAttributeTex};
static const WGPUVertexBufferLayout vertexBufferLayoutPos{.stepMode = WGPUVertexStepMode_Vertex, .arrayStride = 8, .attributeCount = 1, .attributes = vertexAttributesPos};
// Solid colors advance per instance for single draws and per vertex for batches.
static const WGPUVertexBufferLayout vertexBufferLayoutColor{.stepMode = WGPUVertexStepMode_Instance, .arrayStride = sizeof(RenderColor), .attributeCount = 1, .attributes = vertexAttributesColor};
static const WGPUVertexBufferLayout vertexBufferLayoutColorBatch{.stepMode = WGPUVertexStepMode_Vertex, .arrayStride = sizeof(RenderColor), .attributeCount = 1, .attributes = vertexAttributesColor};
static const WGPUVertexBufferLayout vertexBufferLayoutTex{.stepMode = WGPUVertexStepMode_Vertex, .arrayStride = 8, .attributeCount = 1, .attributes = vertexAttributesTex};
static WGPUVertexBufferLayout vertexBufferLayoutsSolid[]{vertexBufferLayoutPos, vertexBufferLayoutColor};
static WGPUVertexBufferLayout vertexBufferLayoutsSolidBatch[]{vertexBufferLayoutPos, vertexBufferLayoutColorBatch};
static WGPUVertexBufferLayout vertexBufferLayoutsShape[]{vertexBufferLayoutPos};
static WGPUVertexBufferLayout vertexBufferLayoutsImage[]{vertexBufferLayoutPos, vertexBufferLayoutTex};

static WGPUDepthStencilState _depthOnlyState(WGPUCompareFunction compare)
{
    const WGPUStencilFaceState stencil{.compare = WGPUCompareFunction_Always, .failOp = WGPUStencilOperation_Keep, .depthFailOp = WGPUStencilOperation_Keep, .passOp = WGPUStencilOperation_Keep};
    return {.format = WGPUTextureFormat_Depth24PlusStencil8, .depthWriteEnabled = WGPUOptionalBool_False, .depthCompare = compare, .stencilFront = stencil, .stencilBack = stencil, .stencilReadMask = 0, .stencilWriteMask = 0};
}

WGPUShaderModule WgPipelines::createShaderModule(WGPUDevice device, const char* label, const char* code)
{
    WGPUShaderSourceWGSL shaderSourceWGSL {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = { .data = code, .length = WGPU_STRLEN }
    };
    const WGPUShaderModuleDescriptor shaderModuleDesc {
        .nextInChain = &shaderSourceWGSL.chain,
        .label = { .data = label, .length = WGPU_STRLEN }
    };
    return wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
}


WGPUPipelineLayout WgPipelines::createPipelineLayout(WGPUDevice device, WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount)
{
    const WGPUPipelineLayoutDescriptor pipelineLayoutDesc { .bindGroupLayoutCount = bindGroupLayoutsCount, .bindGroupLayouts = bindGroupLayouts };
    return wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
}

WGPURenderPipeline WgPipelines::createRenderPipeline(
    WGPUDevice device, const char* pipelineLabel,
    WGPUShaderModule shaderModule, const char* vsEntryPoint, const char* fsEntryPoint,
    WGPUPipelineLayout pipelineLayout,
    WGPUVertexBufferLayout* vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
    WGPUColorWriteMask writeMask, WGPUTextureFormat colorTargetFormat, WGPUBlendState blendState,
    WGPUDepthStencilState depthStencilState, WGPUMultisampleState multisampleState)
{
    const WGPUColorTargetState colorTargetState { .format = colorTargetFormat, .blend = &blendState, .writeMask = writeMask };
    const WGPUColorTargetState colorTargetStates[] { colorTargetState };
    const WGPUPrimitiveState primitiveState { .topology = WGPUPrimitiveTopology_TriangleList };
    const WGPUVertexState   vertexState   { .module = shaderModule, .entryPoint = { .data = vsEntryPoint, .length = WGPU_STRLEN }, .bufferCount = vertexBufferLayoutsCount, .buffers = vertexBufferLayouts };
    const WGPUFragmentState fragmentState { .module = shaderModule, .entryPoint = { .data = fsEntryPoint, .length = WGPU_STRLEN }, .targetCount = 1, .targets = colorTargetStates };
    const WGPURenderPipelineDescriptor renderPipelineDesc {
        .label = { .data = pipelineLabel, .length = WGPU_STRLEN },
        .layout = pipelineLayout,
        .vertex = vertexState,
        .primitive = primitiveState,
        .depthStencil = &depthStencilState,
        .multisample = multisampleState,
        .fragment = &fragmentState
    };
    return wgpuDeviceCreateRenderPipeline(device, &renderPipelineDesc);
}

WGPURenderPipeline WgPipelines::createBlendPipeline(
    WgContext& context, const char* pipelineLabel, WGPUShaderModule shaderModule,
    const char* fsEntryPoint, WGPUPipelineLayout pipelineLayout,
    WGPUVertexBufferLayout* vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
    WGPUCompareFunction stencilCompare)
{
    WGPUBlendComponent blendComponentSrc{.operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_Zero};
    const WGPUBlendState blendStateSrc{.color = blendComponentSrc, .alpha = blendComponentSrc};
    const WGPUDepthStencilState depthStencilState = makeDepthStencilState(
        WGPUCompareFunction_Always, WGPUOptionalBool_False,
        stencilCompare,
        WGPUStencilOperation_Zero);
    const WGPUMultisampleState multisampleState{.count = 4, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false};

    return createRenderPipeline(
        context.device, pipelineLabel,
        shaderModule, "vs_main", fsEntryPoint,
        pipelineLayout, vertexBufferLayouts, vertexBufferLayoutsCount,
        WGPUColorWriteMask_All, WGPUTextureFormat_RGBA8Unorm, blendStateSrc,
        depthStencilState, multisampleState);
}

WGPURenderPipeline WgPipelines::solidBlend(WgContext& context, BlendMethod method)
{
    auto index = (uint32_t)method;
    if (!solidBlends[index]) solidBlends[index] = createBlendPipeline(context, "The render pipeline solid blend", shaderSolidBlend, shaderBlendNames[index], layoutSolidBlend, vertexBufferLayoutsSolid, 2, WGPUCompareFunction_NotEqual);
    return solidBlends[index];
}

WGPURenderPipeline WgPipelines::radialBlend(WgContext& context, BlendMethod method)
{
    auto index = (uint32_t)method;
    if (!radialBlends[index]) radialBlends[index] = createBlendPipeline(context, "The render pipeline radial blend", shaderRadialBlend, shaderBlendNames[index], layoutGradientBlend, vertexBufferLayoutsShape, 1, WGPUCompareFunction_NotEqual);
    return radialBlends[index];
}

WGPURenderPipeline WgPipelines::linearBlend(WgContext& context, BlendMethod method)
{
    auto index = (uint32_t)method;
    if (!linearBlends[index]) linearBlends[index] = createBlendPipeline(context, "The render pipeline linear blend", shaderLinearBlend, shaderBlendNames[index], layoutGradientBlend, vertexBufferLayoutsShape, 1, WGPUCompareFunction_NotEqual);
    return linearBlends[index];
}

WGPURenderPipeline WgPipelines::conicBlend(WgContext& context, BlendMethod method)
{
    auto index = (uint32_t)method;
    if (!conicBlends[index]) conicBlends[index] = createBlendPipeline(context, "The render pipeline conic blend", shaderConicBlend, shaderBlendNames[index], layoutGradientBlend, vertexBufferLayoutsShape, 1, WGPUCompareFunction_NotEqual);
    return conicBlends[index];
}

WGPURenderPipeline WgPipelines::clippedStroke(WgContext& context, WgRenderSettingsType type)
{
    auto index = uint32_t(type) - 1;
    auto& pipeline = strokeClip[index];
    if (!pipeline) {
        const WGPUShaderModule shaders[]{shaderSolid, shaderLinear, shaderRadial, shaderConic};
        const bool solid = type == WgRenderSettingsType::Solid;
        const WGPUBlendComponent blendComponent{.operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha};
        const WGPUBlendState blendState{.color = blendComponent, .alpha = blendComponent};
        const auto depthStencilState = _depthOnlyState(WGPUCompareFunction_Equal);
        const WGPUMultisampleState multisampleState{.count = 4, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false};
        pipeline = createRenderPipeline(
            context.device, "The render pipeline clipped stroke",
            shaders[index], "vs_main", "fs_main",
            solid ? layoutSolid : layoutGradient,
            solid ? vertexBufferLayoutsSolid : vertexBufferLayoutsShape, solid ? 2 : 1,
            WGPUColorWriteMask_All, WGPUTextureFormat_RGBA8Unorm, blendState,
            depthStencilState, multisampleState);
    }
    return pipeline;
}

WGPURenderPipeline WgPipelines::imageBlend(WgContext& context, BlendMethod method)
{
    auto index = (uint32_t)method;
    if (!imageBlends[index]) imageBlends[index] = createBlendPipeline(context, "The render pipeline image blend", shaderImageBlend, shaderBlendNames[index], layoutImageBlend, vertexBufferLayoutsImage, 2, WGPUCompareFunction_NotEqual);
    return imageBlends[index];
}

WGPURenderPipeline WgPipelines::sceneBlend(WgContext& context, BlendMethod method)
{
    auto index = (uint32_t)method;
    if (!sceneBlends[index]) sceneBlends[index] = createBlendPipeline(context, "The render pipeline scene blend", shaderSceneBlend, shaderBlendNames[index], layoutSceneBlend, vertexBufferLayoutsImage, 2, WGPUCompareFunction_Always);
    return sceneBlends[index];
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

WGPUDepthStencilState WgPipelines::makeDepthStencilState(
    WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled,
    WGPUCompareFunction stencilFunction, WGPUStencilOperation stencilOperation)
{
    return makeDepthStencilState(depthCompare, depthWriteEnabled, stencilFunction, stencilOperation, stencilFunction, stencilOperation);
}

WGPUDepthStencilState WgPipelines::makeDepthStencilState(
    WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled,
    WGPUCompareFunction stencilFunctionFrnt, WGPUStencilOperation stencilOperationFrnt,
    WGPUCompareFunction stencilFunctionBack, WGPUStencilOperation stencilOperationBack)
{
    const WGPUDepthStencilState depthStencilState {
        .format = WGPUTextureFormat_Depth24PlusStencil8, .depthWriteEnabled = depthWriteEnabled, .depthCompare = depthCompare,
        .stencilFront = { .compare = stencilFunctionFrnt, .failOp = stencilOperationFrnt, .depthFailOp = WGPUStencilOperation_Zero, .passOp = stencilOperationFrnt },
        .stencilBack =  { .compare = stencilFunctionBack, .failOp = stencilOperationBack, .depthFailOp = WGPUStencilOperation_Zero, .passOp = stencilOperationBack },
        .stencilReadMask = 0xFFFFFFFF, .stencilWriteMask = 0xFFFFFFFF
    };
    return depthStencilState;
}

void WgPipelines::initialize(WgContext& context)
{
    // common pipeline settings
    const WGPUMultisampleState multisampleState{nullptr, 4, 0xFFFFFFFF, false};
    const WGPUMultisampleState multisampleStateX1{nullptr, 1, 0xFFFFFFFF, false};
    const WGPUTextureFormat offscreenTargetFormat = WGPUTextureFormat_RGBA8Unorm;

    // blend states
    WGPUBlendComponent blendComponentSrc{WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_Zero};
    WGPUBlendComponent blendComponentNrm{WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha};
    const WGPUBlendState blendStateSrc{blendComponentSrc, blendComponentSrc};
    const WGPUBlendState blendStateNrm{blendComponentNrm, blendComponentNrm};

    const WgBindGroupLayouts& layouts = context.layouts;

    // bind group layouts helpers
    WGPUBindGroupLayout bindGroupLayoutsStencil[]{layouts.layoutBuffer1Un};
    WGPUBindGroupLayout bindGroupLayoutsDepth[]{layouts.layoutBuffer1Un, layouts.layoutBuffer1Un};

    // bind group layouts normal blend
    WGPUBindGroupLayout bindGroupLayoutsSolid[]{layouts.layoutBuffer1Un};
    WGPUBindGroupLayout bindGroupLayoutsGradient[]{layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled};
    WGPUBindGroupLayout bindGroupLayoutsImage[]{layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled};
    WGPUBindGroupLayout bindGroupLayoutsScene[]{layouts.layoutTexSampled, layouts.layoutBuffer1Un};

    // bind group layouts custom blend
    WGPUBindGroupLayout bindGroupLayoutsSolidBlend[]{layouts.layoutBuffer1Un, layouts.layoutTexSampled};
    WGPUBindGroupLayout bindGroupLayoutsGradientBlend[]{layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled, layouts.layoutTexSampled};
    WGPUBindGroupLayout bindGroupLayoutsImageBlend[]{layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled, layouts.layoutTexSampled};
    WGPUBindGroupLayout bindGroupLayoutsSceneBlend[]{layouts.layoutTexSampled, layouts.layoutTexSampled, layouts.layoutBuffer1Un};

    WGPUBindGroupLayout bindGroupLayoutsSceneCompose[]{layouts.layoutTexSampled, layouts.layoutTexSampled};
    WGPUBindGroupLayout bindGroupLayoutsBlit[]{layouts.layoutTexSampled};
    WGPUBindGroupLayout bindGroupLayoutsShadow[]{layouts.layoutTexSampled, layouts.layoutTexSampled, layouts.layoutBuffer1Un};
    WGPUBindGroupLayout bindGroupLayoutsEffects[]{layouts.layoutTexSampled, layouts.layoutBuffer1Un};

    // depth stencil state markup
    const WGPUDepthStencilState depthStencilStateNonZero = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_IncrementWrap, WGPUCompareFunction_Always, WGPUStencilOperation_DecrementWrap);
    const WGPUDepthStencilState depthStencilStateEvenOdd = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Invert);
    const WGPUDepthStencilState depthStencilStateDirect  = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Replace);

    // depth stencil state clip path
    const WGPUDepthStencilState depthStencilStateCopyStencilToDepth    = makeDepthStencilState(WGPUCompareFunction_Always,  WGPUOptionalBool_True,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateCopyStencilToDepthInt = makeDepthStencilState(WGPUCompareFunction_Greater, WGPUOptionalBool_True,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateCopyDepthToStencil    = makeDepthStencilState(WGPUCompareFunction_Equal,   WGPUOptionalBool_False, WGPUCompareFunction_Always,   WGPUStencilOperation_Replace);
    const WGPUDepthStencilState depthStencilStateMergeDepthStencil     = makeDepthStencilState(WGPUCompareFunction_Equal,   WGPUOptionalBool_True,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);
    const WGPUDepthStencilState depthStencilStateClearDepth            = makeDepthStencilState(WGPUCompareFunction_Always,  WGPUOptionalBool_True,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);

    // depth stencil state blend, compose and blit
    const WGPUDepthStencilState depthStencilStateShape = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateScene = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False,  WGPUCompareFunction_Always, WGPUStencilOperation_Zero);
    const auto depthStencilStateConv = _depthOnlyState(WGPUCompareFunction_Always);

    // shaders
    char shaderSourceBuff[16384]{};
    shaderStencil = createShaderModule(context.device, "The shader stencil", cShaderSrc_Stencil);
    shaderDepth = createShaderModule(context.device, "The shader depth", cShaderSrc_Depth);

    // shader normal blend
    shaderSolid = createShaderModule(context.device, "The shader solid", cShaderSrc_Solid);
    shaderRadial = createShaderModule(context.device, "The shader radial", cShaderSrc_Radial);
    shaderLinear = createShaderModule(context.device, "The shader linear", cShaderSrc_Linear);
    shaderConic = createShaderModule(context.device, "The shader conic", cShaderSrc_Conic);
    shaderImage = createShaderModule(context.device, "The shader image", cShaderSrc_Image);
    shaderScene = createShaderModule(context.device, "The shader scene", cShaderSrc_Scene);

    // shader custom blend
    shaderSolidBlend = createShaderModule(context.device, "The shader blend solid", strcat(strcpy(shaderSourceBuff, cShaderSrc_Solid_Blend), cShaderSrc_BlendFuncs));
    shaderLinearBlend = createShaderModule(context.device, "The shader blend linear", strcat(strcpy(shaderSourceBuff, cShaderSrc_Linear_Blend), cShaderSrc_BlendFuncs));
    shaderRadialBlend = createShaderModule(context.device, "The shader blend radial", strcat(strcpy(shaderSourceBuff, cShaderSrc_Radial_Blend), cShaderSrc_BlendFuncs));
    shaderConicBlend = createShaderModule(context.device, "The shader blend conic", strcat(strcpy(shaderSourceBuff, cShaderSrc_Conic_Blend), cShaderSrc_BlendFuncs));
    shaderImageBlend = createShaderModule(context.device, "The shader blend image", strcat(strcpy(shaderSourceBuff, cShaderSrc_Image_Blend), cShaderSrc_BlendFuncs));
    shaderSceneBlend = createShaderModule(context.device, "The shader blend scene", strcat(strcpy(shaderSourceBuff, cShaderSrc_Scene_Blend), cShaderSrc_BlendFuncs));

    shaderSceneCompose = createShaderModule(context.device, "The shader scene composition", cShaderSrc_Scene_Compose);
    shaderBlit = createShaderModule(context.device, "The shader blit", cShaderSrc_Blit);

    // shader effects
    shaderShadow = createShaderModule(context.device, "The shader effects", cShaderSrc_Shadow);
    shaderEffects = createShaderModule(context.device, "The shader effects", cShaderSrc_Effects);

    // layouts
    layoutStencil = createPipelineLayout(context.device, bindGroupLayoutsStencil, 1);
    layoutDepth = createPipelineLayout(context.device, bindGroupLayoutsDepth, 2);

    // layouts normal blend
    layoutSolid = createPipelineLayout(context.device, bindGroupLayoutsSolid, 1);
    layoutGradient = createPipelineLayout(context.device, bindGroupLayoutsGradient, 3);
    layoutImage = createPipelineLayout(context.device, bindGroupLayoutsImage, 3);
    layoutScene = createPipelineLayout(context.device, bindGroupLayoutsScene, 2);

    // layouts custom blend
    layoutSolidBlend = createPipelineLayout(context.device, bindGroupLayoutsSolidBlend, 2);
    layoutGradientBlend = createPipelineLayout(context.device, bindGroupLayoutsGradientBlend, 4);
    layoutImageBlend = createPipelineLayout(context.device, bindGroupLayoutsImageBlend, 4);
    layoutSceneBlend = createPipelineLayout(context.device, bindGroupLayoutsSceneBlend, 3);

    layoutSceneCompose = createPipelineLayout(context.device, bindGroupLayoutsSceneCompose, 2);
    layoutBlit = createPipelineLayout(context.device, bindGroupLayoutsBlit, 1);

    // layout effects
    layoutShadow = createPipelineLayout(context.device, bindGroupLayoutsShadow, 3);
    layoutEffects = createPipelineLayout(context.device, bindGroupLayoutsEffects, 2);

    // pipelines
    nonzero = createRenderPipeline(context.device, "The render pipeline nonzero", shaderStencil, "vs_main", "fs_main", layoutStencil, vertexBufferLayoutsShape, 1,
                                   WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateNonZero, multisampleState);
    evenodd = createRenderPipeline(context.device, "The render pipeline even-odd", shaderStencil, "vs_main", "fs_main", layoutStencil, vertexBufferLayoutsShape, 1,
                                   WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateEvenOdd, multisampleState);
    direct = createRenderPipeline(context.device, "The render pipeline direct", shaderStencil, "vs_main", "fs_main", layoutStencil, vertexBufferLayoutsShape, 1,
                                  WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateDirect, multisampleState);
    copyStencilToDepth = createRenderPipeline(context.device, "The render pipeline copy stencil to depth front", shaderDepth, "vs_main", "fs_main", layoutDepth, vertexBufferLayoutsShape, 1,
                                              WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateCopyStencilToDepth, multisampleState);
    copyStencilToDepthInterm = createRenderPipeline(context.device, "The render pipeline copy stencil to depth intermediate", shaderDepth, "vs_main", "fs_main", layoutDepth, vertexBufferLayoutsShape, 1,
                                                    WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateCopyStencilToDepthInt, multisampleState);
    copyDepthToStencil = createRenderPipeline(context.device, "The render pipeline depth to stencil", shaderDepth, "vs_main", "fs_main", layoutDepth, vertexBufferLayoutsShape, 1,
                                              WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateCopyDepthToStencil, multisampleState);
    mergeDepthStencil = createRenderPipeline(context.device, "The render pipeline merge depth with stencil", shaderDepth, "vs_main", "fs_main", layoutDepth, vertexBufferLayoutsShape, 1,
                                             WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateMergeDepthStencil, multisampleState);
    clearDepth = createRenderPipeline(context.device, "The render pipeline clear depth", shaderDepth, "vs_main", "fs_main", layoutDepth, vertexBufferLayoutsShape, 1,
                                      WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc, depthStencilStateClearDepth, multisampleState);
    solid = createRenderPipeline(context.device, "The render pipeline solid", shaderSolid, "vs_main", "fs_main", layoutSolid, vertexBufferLayoutsSolid, 2,
                                 WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateShape, multisampleState);
    radial = createRenderPipeline(context.device, "The render pipeline radial", shaderRadial, "vs_main", "fs_main", layoutGradient, vertexBufferLayoutsShape, 1,
                                  WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateShape, multisampleState);
    linear = createRenderPipeline(context.device, "The render pipeline linear", shaderLinear, "vs_main", "fs_main", layoutGradient, vertexBufferLayoutsShape, 1,
                                  WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateShape, multisampleState);
    conic = createRenderPipeline(context.device, "The render pipeline conic", shaderConic, "vs_main", "fs_main", layoutGradient, vertexBufferLayoutsShape, 1,
                                 WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateShape, multisampleState);
    solidConv = createRenderPipeline(context.device, "The render pipeline solid", shaderSolid, "vs_main", "fs_main", layoutSolid, vertexBufferLayoutsSolid, 2,
                                     WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateConv, multisampleState);
    solidBatch = createRenderPipeline(context.device, "The render pipeline solid batch", shaderSolid, "vs_main", "fs_main", layoutSolid, vertexBufferLayoutsSolidBatch, 2,
                                      WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateScene, multisampleState);
    solidStencilBatch = createRenderPipeline(context.device, "The render pipeline solid stencil batch cover", shaderSolid, "vs_main", "fs_main", layoutSolid, vertexBufferLayoutsSolidBatch, 2,
                                             WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateShape, multisampleState);
    radialConv = createRenderPipeline(context.device, "The render pipeline radial", shaderRadial, "vs_main", "fs_main", layoutGradient, vertexBufferLayoutsShape, 1,
                                      WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateConv, multisampleState);
    linearConv = createRenderPipeline(context.device, "The render pipeline linear", shaderLinear, "vs_main", "fs_main", layoutGradient, vertexBufferLayoutsShape, 1,
                                      WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateConv, multisampleState);
    conicConv = createRenderPipeline(context.device, "The render pipeline conic", shaderConic, "vs_main", "fs_main", layoutGradient, vertexBufferLayoutsShape, 1,
                                     WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateConv, multisampleState);
    image = createRenderPipeline(context.device, "The render pipeline image", shaderImage, "vs_main", "fs_main", layoutImage, vertexBufferLayoutsImage, 2,
                                 WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateShape, multisampleState);
    imageDirect = createRenderPipeline(context.device, "The render pipeline image direct", shaderImage, "vs_main", "fs_main", layoutImage, vertexBufferLayoutsImage, 2,
                                       WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateScene, multisampleState);
    scene = createRenderPipeline(context.device, "The render pipeline scene", shaderScene, "vs_main", "fs_main", layoutScene, vertexBufferLayoutsImage, 2,
                                 WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm, depthStencilStateScene, multisampleState);

    const char* shaderComposeNames[] {
        "fs_main_None",
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

    const WGPUBlendState composeBlends[] {
        blendStateNrm, // None
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
    for (uint32_t i = 0; i < 11; i++) {
        sceneCompose[i] = createRenderPipeline(context.device, "The render pipeline scene composition", shaderSceneCompose, "vs_main", shaderComposeNames[i], layoutSceneCompose, vertexBufferLayoutsImage, 2,
                                               WGPUColorWriteMask_All, offscreenTargetFormat, composeBlends[i], depthStencilStateScene, multisampleState);
    }

    blit = createRenderPipeline(context.device, "The render pipeline blit", shaderBlit, "vs_main", "fs_main", layoutBlit, vertexBufferLayoutsImage, 2,
                                WGPUColorWriteMask_All, context.format, blendStateSrc,  // must be preferred screen pixel format
                                depthStencilStateScene, multisampleStateX1);

    // TODO: either premultiplied blit or unpremultplied bit used.
    blitUnpremultiplied = createRenderPipeline(context.device, "The render pipeline blit unpremultiplied", shaderBlit, "vs_main", "fs_main_unpremultiplied", layoutBlit, vertexBufferLayoutsImage, 2,
                                               WGPUColorWriteMask_All, context.format, blendStateSrc,  // must be preferred screen pixel format
                                               depthStencilStateScene, multisampleStateX1);

    // effects
    effectMotionBlur = createRenderPipeline(context.device, "The render pipeline motion blur", shaderEffects, "vs_main", "fs_main_motion", layoutEffects, vertexBufferLayoutsImage, 2,
                                            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc, depthStencilStateScene, multisampleStateX1);

    effectDropShadow = createRenderPipeline(context.device, "The render pipeline drop shadow", shaderShadow, "vs_main", "fs_main_shadow", layoutShadow, vertexBufferLayoutsImage, 2,
                                            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc, depthStencilStateScene, multisampleStateX1);

    effectGaussianVert = createRenderPipeline(context.device, "The render pipeline gaussian vert", shaderEffects, "vs_main", "fs_main_vert", layoutEffects, vertexBufferLayoutsImage, 2,
                                              WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc, depthStencilStateScene, multisampleStateX1);

    effectGaussianHorz = createRenderPipeline(context.device, "The render pipeline gaussian horz", shaderEffects, "vs_main", "fs_main_horz", layoutEffects, vertexBufferLayoutsImage, 2,
                                              WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc, depthStencilStateScene, multisampleStateX1);

    effectFill = createRenderPipeline(context.device, "The render pipeline fill effect", shaderEffects, "vs_main", "fs_main_fill", layoutEffects, vertexBufferLayoutsImage, 2,
                                      WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc, depthStencilStateScene, multisampleStateX1);

    effectTint = createRenderPipeline(context.device, "The render pipeline tint effect", shaderEffects, "vs_main", "fs_main_tint", layoutEffects, vertexBufferLayoutsImage, 2,
                                      WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc, depthStencilStateScene, multisampleStateX1);

    effectTritone = createRenderPipeline(context.device, "The render pipeline tritone effect", shaderEffects, "vs_main", "fs_main_tritone", layoutEffects, vertexBufferLayoutsImage, 2,
                                         WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc, depthStencilStateScene, multisampleStateX1);
}

void WgPipelines::releaseGraphicHandles(WgContext& context)
{
    // pipeline effects
    releaseRenderPipeline(effectTritone);
    releaseRenderPipeline(effectTint);
    releaseRenderPipeline(effectFill);
    releaseRenderPipeline(effectGaussianHorz);
    releaseRenderPipeline(effectGaussianVert);
    releaseRenderPipeline(effectDropShadow);
    releaseRenderPipeline(effectMotionBlur);
    // pipeline blit
    releaseRenderPipeline(blitUnpremultiplied);
    releaseRenderPipeline(blit);
    // pipelines compose
    for (uint32_t i = 0; i < 11; i++)
        releaseRenderPipeline(sceneCompose[i]);
    // pipelines custom blend
    for (uint32_t i = 0; i < 18; i++) {
        releaseRenderPipeline(sceneBlends[i]);
        releaseRenderPipeline(imageBlends[i]);
        releaseRenderPipeline(conicBlends[i]);
        releaseRenderPipeline(linearBlends[i]);
        releaseRenderPipeline(radialBlends[i]);
        releaseRenderPipeline(solidBlends[i]);
    }
    // pipelines normal blend
    for (auto& pipeline : strokeClip)
        releaseRenderPipeline(pipeline);
    releaseRenderPipeline(scene);
    releaseRenderPipeline(image);
    releaseRenderPipeline(imageDirect);
    releaseRenderPipeline(conicConv);
    releaseRenderPipeline(linearConv);
    releaseRenderPipeline(radialConv);
    releaseRenderPipeline(solidStencilBatch);
    releaseRenderPipeline(solidBatch);
    releaseRenderPipeline(solidConv);
    releaseRenderPipeline(conic);
    releaseRenderPipeline(linear);
    releaseRenderPipeline(radial);
    releaseRenderPipeline(solid);
    // pipelines clip path markup
    releaseRenderPipeline(clearDepth);
    releaseRenderPipeline(mergeDepthStencil);
    releaseRenderPipeline(copyDepthToStencil);
    releaseRenderPipeline(copyStencilToDepthInterm);
    releaseRenderPipeline(copyStencilToDepth);
    // pipelines stencil markup
    releaseRenderPipeline(direct);
    releaseRenderPipeline(evenodd);
    releaseRenderPipeline(nonzero);
    // layouts
    releasePipelineLayout(layoutEffects);
    releasePipelineLayout(layoutShadow);
    releasePipelineLayout(layoutBlit);
    releasePipelineLayout(layoutSceneCompose);
    releasePipelineLayout(layoutSceneBlend);
    releasePipelineLayout(layoutImageBlend);
    releasePipelineLayout(layoutGradientBlend);
    releasePipelineLayout(layoutSolidBlend);
    releasePipelineLayout(layoutScene);
    releasePipelineLayout(layoutImage);
    releasePipelineLayout(layoutGradient);
    releasePipelineLayout(layoutSolid);
    releasePipelineLayout(layoutDepth);
    releasePipelineLayout(layoutStencil);
    // shaders
    releaseShaderModule(shaderEffects);
    releaseShaderModule(shaderShadow);
    releaseShaderModule(shaderBlit);
    releaseShaderModule(shaderSceneCompose);
    releaseShaderModule(shaderSceneBlend);
    releaseShaderModule(shaderImageBlend);
    releaseShaderModule(shaderConicBlend);
    releaseShaderModule(shaderLinearBlend);
    releaseShaderModule(shaderRadialBlend);
    releaseShaderModule(shaderSolidBlend);
    releaseShaderModule(shaderScene);
    releaseShaderModule(shaderImage);
    releaseShaderModule(shaderConic);
    releaseShaderModule(shaderLinear);
    releaseShaderModule(shaderRadial);
    releaseShaderModule(shaderSolid);
    releaseShaderModule(shaderDepth);
    releaseShaderModule(shaderStencil);
}

void WgPipelines::release(WgContext& context)
{
    releaseGraphicHandles(context);
}