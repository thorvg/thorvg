/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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
#include <cassert>

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
    const WGPUColorWriteMaskFlags writeMask, const WGPUTextureFormat colorTargetFormat, const WGPUBlendState blendState,
    const WGPUDepthStencilState depthStencilState, const WGPUMultisampleState multisampleState)
{
    const WGPUColorTargetState colorTargetState { .format = colorTargetFormat, .blend = &blendState, .writeMask = writeMask };
    const WGPUColorTargetState colorTargetStates[] { colorTargetState };
    const WGPUPrimitiveState primitiveState { .topology = WGPUPrimitiveTopology_TriangleList };
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


WGPUDepthStencilState WgPipelines::makeDepthStencilState(
    const WGPUCompareFunction depthCompare, WGPUBool depthWriteEnabled,
    const WGPUCompareFunction stencilFunction, const WGPUStencilOperation stencilOperation)
{
    return makeDepthStencilState(depthCompare, depthWriteEnabled, stencilFunction, stencilOperation, stencilFunction, stencilOperation);
}


WGPUDepthStencilState WgPipelines::makeDepthStencilState(
    const WGPUCompareFunction depthCompare, WGPUBool depthWriteEnabled,
    const WGPUCompareFunction stencilFunctionFrnt, const WGPUStencilOperation stencilOperationFrnt,
    const WGPUCompareFunction stencilFunctionBack, const WGPUStencilOperation stencilOperationBack)
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
    const WGPUVertexAttribute vertexAttributePos { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0 };
    const WGPUVertexAttribute vertexAttributeTex { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 1 };
    const WGPUVertexAttribute vertexAttributesPos[] { vertexAttributePos };
    const WGPUVertexAttribute vertexAttributesTex[] { vertexAttributeTex };
    const WGPUVertexBufferLayout vertexBufferLayoutPos { .arrayStride = 8, .stepMode = WGPUVertexStepMode_Vertex, .attributeCount = 1, .attributes = vertexAttributesPos };
    const WGPUVertexBufferLayout vertexBufferLayoutTex { .arrayStride = 8, .stepMode = WGPUVertexStepMode_Vertex, .attributeCount = 1, .attributes = vertexAttributesTex };
    const WGPUVertexBufferLayout vertexBufferLayoutsShape[] { vertexBufferLayoutPos };
    const WGPUVertexBufferLayout vertexBufferLayoutsImage[] { vertexBufferLayoutPos, vertexBufferLayoutTex };
    const WGPUMultisampleState multisampleState   { .count = 4, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false };
    const WGPUMultisampleState multisampleStateX1 { .count = 1, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false };
    const WGPUTextureFormat offscreenTargetFormat = WGPUTextureFormat_RGBA8Unorm;

    // blend states
    WGPUBlendComponent blendComponentSrc { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_Zero };
    WGPUBlendComponent blendComponentNrm { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha };
    const WGPUBlendState blendStateSrc { .color = blendComponentSrc, .alpha = blendComponentSrc };
    const WGPUBlendState blendStateNrm { .color = blendComponentNrm, .alpha = blendComponentNrm };

    const WgBindGroupLayouts& layouts = context.layouts;
    // bind group layouts helpers
    const WGPUBindGroupLayout bindGroupLayoutsStencil[] { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un };
    const WGPUBindGroupLayout bindGroupLayoutsDepth[]   { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un, layouts.layoutBuffer1Un };
    // bind group layouts normal blend
    const WGPUBindGroupLayout bindGroupLayoutsSolid[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un, layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsGradient[] { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un, layouts.layoutTexSampledBuff2Un };
    const WGPUBindGroupLayout bindGroupLayoutsImage[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsScene[]    { layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    // bind group layouts custom blend
    const WGPUBindGroupLayout bindGroupLayoutsSolidBlend[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsGradientBlend[] { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un, layouts.layoutTexSampledBuff2Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsImageBlend[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer2Un, layouts.layoutTexSampled, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsSceneBlend[]    { layouts.layoutTexSampled, layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    // bind group layouts scene compose
    const WGPUBindGroupLayout bindGroupLayoutsSceneCompose[] { layouts.layoutTexSampled, layouts.layoutTexSampled };
    // bind group layouts blit
    const WGPUBindGroupLayout bindGroupLayoutsBlit[] { layouts.layoutTexSampled };
    // bind group layouts effects
    const WGPUBindGroupLayout bindGroupLayoutsGaussian[] { layouts.layoutTexStrorage1RO, layouts.layoutTexStrorage1WO, layouts.layoutBuffer1Un, layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsDropShadow[] { layouts.layoutTexStrorage2RO, layouts.layoutTexStrorage1WO, layouts.layoutBuffer1Un, layouts.layoutBuffer1Un };

    // depth stencil state markup
    const WGPUDepthStencilState depthStencilStateNonZero = makeDepthStencilState(WGPUCompareFunction_Always, false, WGPUCompareFunction_Always, WGPUStencilOperation_IncrementWrap, WGPUCompareFunction_Always, WGPUStencilOperation_DecrementWrap);
    const WGPUDepthStencilState depthStencilStateEvenOdd = makeDepthStencilState(WGPUCompareFunction_Always, false, WGPUCompareFunction_Always, WGPUStencilOperation_Invert);
    const WGPUDepthStencilState depthStencilStateDirect  = makeDepthStencilState(WGPUCompareFunction_Always, false, WGPUCompareFunction_Always, WGPUStencilOperation_Replace);
    // depth stencil state clip path
    const WGPUDepthStencilState depthStencilStateCopyStencilToDepth    = makeDepthStencilState(WGPUCompareFunction_Always,  true,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateCopyStencilToDepthInt = makeDepthStencilState(WGPUCompareFunction_Greater, true,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateCopyDepthToStencil    = makeDepthStencilState(WGPUCompareFunction_Equal,   false, WGPUCompareFunction_Always,   WGPUStencilOperation_Replace);
    const WGPUDepthStencilState depthStencilStateMergeDepthStencil     = makeDepthStencilState(WGPUCompareFunction_Equal,   true,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);
    const WGPUDepthStencilState depthStencilStateClearDepth            = makeDepthStencilState(WGPUCompareFunction_Always,  true,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);
    // depth stencil state blend, compose and blit
    const WGPUDepthStencilState depthStencilStateShape = makeDepthStencilState(WGPUCompareFunction_Always, false,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateScene = makeDepthStencilState(WGPUCompareFunction_Always, false,  WGPUCompareFunction_Always, WGPUStencilOperation_Zero);

    // shaders
    char shaderSourceBuff[16384]{};
    shader_stencil = createShaderModule(context.device, "The shader stencil", cShaderSrc_Stencil);
    shader_depth   = createShaderModule(context.device, "The shader depth", cShaderSrc_Depth);
    // shader normal blend
    shader_solid  = createShaderModule(context.device, "The shader solid",  cShaderSrc_Solid);
    shader_radial = createShaderModule(context.device, "The shader radial", cShaderSrc_Radial);
    shader_linear = createShaderModule(context.device, "The shader linear", cShaderSrc_Linear);
    shader_image  = createShaderModule(context.device, "The shader image",  cShaderSrc_Image);
    shader_scene  = createShaderModule(context.device, "The shader scene",  cShaderSrc_Scene);
    // shader custom blend
    shader_solid_blend  = createShaderModule(context.device, "The shader blend solid",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Solid_Blend), cShaderSrc_BlendFuncs));
    shader_linear_blend = createShaderModule(context.device, "The shader blend linear", strcat(strcpy(shaderSourceBuff, cShaderSrc_Linear_Blend), cShaderSrc_BlendFuncs));
    shader_radial_blend = createShaderModule(context.device, "The shader blend radial", strcat(strcpy(shaderSourceBuff, cShaderSrc_Radial_Blend), cShaderSrc_BlendFuncs));
    shader_image_blend  = createShaderModule(context.device, "The shader blend image",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Image_Blend), cShaderSrc_BlendFuncs));
    shader_scene_blend  = createShaderModule(context.device, "The shader blend scene",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Scene_Blend), cShaderSrc_BlendFuncs));
    // shader compose
    shader_scene_compose = createShaderModule(context.device, "The shader scene composition", cShaderSrc_Scene_Compose);
    // shader blit
    shader_blit = createShaderModule(context.device, "The shader blit", cShaderSrc_Blit);
    // shader effects
    shader_gaussian = createShaderModule(context.device, "The shader gaussian", cShaderSrc_GaussianBlur);
    shader_dropshadow = createShaderModule(context.device, "The shader drop shadow", cShaderSrc_DropShadow);

    // layouts
    layout_stencil = createPipelineLayout(context.device, bindGroupLayoutsStencil, 2);
    layout_depth = createPipelineLayout(context.device, bindGroupLayoutsDepth, 3);
    // layouts normal blend
    layout_solid    = createPipelineLayout(context.device, bindGroupLayoutsSolid, 3);
    layout_gradient = createPipelineLayout(context.device, bindGroupLayoutsGradient, 3);
    layout_image    = createPipelineLayout(context.device, bindGroupLayoutsImage, 3);
    layout_scene    = createPipelineLayout(context.device, bindGroupLayoutsScene, 2);
    // layouts custom blend
    layout_solid_blend    = createPipelineLayout(context.device, bindGroupLayoutsSolidBlend, 4);
    layout_gradient_blend = createPipelineLayout(context.device, bindGroupLayoutsGradientBlend, 4);
    layout_image_blend    = createPipelineLayout(context.device, bindGroupLayoutsImageBlend, 4);
    layout_scene_blend    = createPipelineLayout(context.device, bindGroupLayoutsSceneBlend, 3);
    // layout compose
    layout_scene_compose = createPipelineLayout(context.device, bindGroupLayoutsSceneCompose, 2);
    // layout blit
    layout_blit = createPipelineLayout(context.device, bindGroupLayoutsBlit, 1);
    // layout effects
    layout_gaussian = createPipelineLayout(context.device, bindGroupLayoutsGaussian, 4);
    layout_dropshadow = createPipelineLayout(context.device, bindGroupLayoutsDropShadow, 4);

    // render pipeline nonzero
    nonzero = createRenderPipeline(
        context.device, "The render pipeline nonzero",
        shader_stencil, "vs_main", "fs_main",
        layout_stencil, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateNonZero, multisampleState);
    // render pipeline even-odd
    evenodd = createRenderPipeline(
        context.device, "The render pipeline even-odd",
        shader_stencil, "vs_main", "fs_main",
        layout_stencil, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateEvenOdd, multisampleState);
    // render pipeline direct
    direct = createRenderPipeline(
        context.device, "The render pipeline direct",
        shader_stencil, "vs_main", "fs_main",
        layout_stencil, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateDirect, multisampleState);

    // render pipeline copy stencil to depth (front)
    copy_stencil_to_depth = createRenderPipeline(
        context.device, "The render pipeline copy stencil to depth front",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateCopyStencilToDepth , multisampleState);
    // render pipeline copy stencil to depth (intermidiate)
    copy_stencil_to_depth_interm = createRenderPipeline(
        context.device, "The render pipeline copy stencil to depth intermidiate",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateCopyStencilToDepthInt, multisampleState);
    // render pipeline depth to stencil
    copy_depth_to_stencil = createRenderPipeline(
        context.device, "The render pipeline depth to stencil",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateCopyDepthToStencil, multisampleState);
    // render pipeline merge depth with stencil
    merge_depth_stencil = createRenderPipeline(
        context.device, "The render pipeline merge depth with stencil",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateMergeDepthStencil, multisampleState);
    // render pipeline clear depth
    clear_depth = createRenderPipeline(
        context.device, "The render pipeline clear depth",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateClearDepth, multisampleState);

    // render pipeline solid
    solid = createRenderPipeline(
        context.device, "The render pipeline solid",
        shader_solid, "vs_main", "fs_main",
        layout_solid, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // render pipeline radial
    radial = createRenderPipeline(
        context.device, "The render pipeline radial",
        shader_radial, "vs_main", "fs_main",
        layout_gradient, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // render pipeline linear
    linear = createRenderPipeline(
        context.device, "The render pipeline linear",
        shader_linear, "vs_main", "fs_main",
        layout_gradient, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // render pipeline image
    image = createRenderPipeline(
        context.device, "The render pipeline image",
        shader_image, "vs_main", "fs_main",
        layout_image, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // render pipeline scene
    scene = createRenderPipeline(
        context.device, "The render pipeline scene",
        shader_scene, "vs_main", "fs_main",
        layout_scene, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateScene, multisampleState);

    // blend shader names
    const char* shaderBlendNames[] {
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
        "fs_main_Normal", //TODO: a padding for reserved Hue.
        "fs_main_Normal", //TODO: a padding for reserved Saturation.
        "fs_main_Normal", //TODO: a padding for reserved Color.
        "fs_main_Normal", //TODO: a padding for reserved Luminosity.
        "fs_main_Add",
        "fs_main_Normal"  //TODO: a padding for reserved Hardmix.
    };

    // render pipeline shape blend
    for (uint32_t i = 0; i < 18; i++) {
        // blend solid
        solid_blend[i] = createRenderPipeline(
            context.device, "The render pipeline solid blend",
            shader_solid_blend, "vs_main", shaderBlendNames[i],
            layout_solid_blend, vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilStateShape, multisampleState);
        // blend radial
        radial_blend[i] = createRenderPipeline(
            context.device, "The render pipeline radial blend",
            shader_radial_blend, "vs_main", shaderBlendNames[i],
            layout_gradient_blend, vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilStateShape, multisampleState);
        // blend linear
        linear_blend[i] = createRenderPipeline(
            context.device, "The render pipeline linear blend",
            shader_linear_blend, "vs_main", shaderBlendNames[i],
            layout_gradient_blend, vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilStateShape, multisampleState);
        // blend image
        image_blend[i] = createRenderPipeline(
            context.device, "The render pipeline image blend",
            shader_image_blend, "vs_main", shaderBlendNames[i],
            layout_image_blend, vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilStateShape, multisampleState);
        // blend scene
        scene_blend[i] = createRenderPipeline(
            context.device, "The render pipeline scene blend",
            shader_scene_blend, "vs_main", shaderBlendNames[i],
            layout_scene_blend, vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilStateScene, multisampleState);
    }

    // compose shader names
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

    // compose shader blend states
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
        scene_compose[i] = createRenderPipeline(
            context.device, "The render pipeline scene composition",
            shader_scene_compose, "vs_main", shaderComposeNames[i],
            layout_scene_compose, vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, composeBlends[i],
            depthStencilStateScene, multisampleState);
    }

    // render pipeline blit
    blit = createRenderPipeline(
        context.device, "The render pipeline blit",
        shader_blit, "vs_main", "fs_main", 
        layout_blit, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, context.preferredFormat, blendStateSrc, // must be preferred screen pixel format
        depthStencilStateScene, multisampleStateX1);

    // compute pipeline gaussian blur
    gaussian_horz = createComputePipeline(context.device, "The compute pipeline gaussian blur horizontal", shader_gaussian, "cs_main_horz", layout_gaussian);
    gaussian_vert = createComputePipeline(context.device, "The compute pipeline gaussian blur vertical",   shader_gaussian, "cs_main_vert", layout_gaussian);
    // compute pipeline drop shadow
    dropshadow = createComputePipeline(context.device, "The compute pipeline drop shadow blend", shader_dropshadow, "cs_main", layout_dropshadow);
}

void WgPipelines::releaseGraphicHandles(WgContext& context)
{
    // pipeline effects
    releaseComputePipeline(dropshadow);
    releaseComputePipeline(gaussian_vert);
    releaseComputePipeline(gaussian_horz);
    // pipeline blit
    releaseRenderPipeline(blit);
    // pipelines compose
    for (uint32_t i = 0; i < 11; i++)
        releaseRenderPipeline(scene_compose[i]);
    // pipelines custom blend
    for (uint32_t i = 0; i < 18; i++) {
        releaseRenderPipeline(scene_blend[i]);
        releaseRenderPipeline(image_blend[i]);
        releaseRenderPipeline(linear_blend[i]);
        releaseRenderPipeline(radial_blend[i]);
        releaseRenderPipeline(solid_blend[i]);
    }
    // pipelines normal blend
    releaseRenderPipeline(scene);
    releaseRenderPipeline(image);
    releaseRenderPipeline(linear);
    releaseRenderPipeline(radial);
    releaseRenderPipeline(solid);
    // pipelines clip path markup
    releaseRenderPipeline(clear_depth);
    releaseRenderPipeline(merge_depth_stencil);
    releaseRenderPipeline(copy_depth_to_stencil);
    releaseRenderPipeline(copy_stencil_to_depth_interm);
    releaseRenderPipeline(copy_stencil_to_depth);
    // pipelines stencil markup
    releaseRenderPipeline(direct);
    releaseRenderPipeline(evenodd);
    releaseRenderPipeline(nonzero);
    // layouts
    releasePipelineLayout(layout_dropshadow);
    releasePipelineLayout(layout_gaussian);
    releasePipelineLayout(layout_blit);
    releasePipelineLayout(layout_scene_compose);
    releasePipelineLayout(layout_scene_blend);
    releasePipelineLayout(layout_image_blend);
    releasePipelineLayout(layout_gradient_blend);
    releasePipelineLayout(layout_solid_blend);
    releasePipelineLayout(layout_scene);
    releasePipelineLayout(layout_image);
    releasePipelineLayout(layout_gradient);
    releasePipelineLayout(layout_solid);
    releasePipelineLayout(layout_depth);
    releasePipelineLayout(layout_stencil);
    // shaders
    releaseShaderModule(shader_dropshadow);
    releaseShaderModule(shader_gaussian);
    releaseShaderModule(shader_blit);
    releaseShaderModule(shader_scene_compose);
    releaseShaderModule(shader_scene_blend);
    releaseShaderModule(shader_image_blend);
    releaseShaderModule(shader_linear_blend);
    releaseShaderModule(shader_radial_blend);
    releaseShaderModule(shader_solid_blend);
    releaseShaderModule(shader_scene);
    releaseShaderModule(shader_image);
    releaseShaderModule(shader_linear);
    releaseShaderModule(shader_radial);
    releaseShaderModule(shader_solid);
    releaseShaderModule(shader_depth);
    releaseShaderModule(shader_stencil);
}


void WgPipelines::release(WgContext& context)
{
    releaseGraphicHandles(context);
}
