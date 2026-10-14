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

#ifndef _TVG_WG_PIPELINES_H_
#define _TVG_WG_PIPELINES_H_

#include "tvgWgCommon.h"

enum class WgRenderSettingsType;

struct WgPipelines
{
    // stencil markup
    WGPURenderPipeline nonzero{};
    WGPURenderPipeline evenodd{};
    WGPURenderPipeline direct{};
    // clip path markup
    WGPURenderPipeline copyStencilToDepth{};        // depth 0.50, clear stencil
    WGPURenderPipeline copyStencilToDepthInterm{};  // depth 0.75, clear stencil
    WGPURenderPipeline copyDepthToStencil{};        // depth 0.50 and 0.75, update stencil
    WGPURenderPipeline mergeDepthStencil{};         // depth 0.75, update stencil
    WGPURenderPipeline clearDepth{};                // depth 1.00, clear ctencil
    // normal blend
    WGPURenderPipeline solid{};
    WGPURenderPipeline radial{};
    WGPURenderPipeline linear{};
    WGPURenderPipeline conic{};
    WGPURenderPipeline solidConv{};          // convex geometry (no stencil)
    WGPURenderPipeline solidBatch{};         // batched convex geometry (no stencil)
    WGPURenderPipeline solidStencilBatch{};  // batched complex geometry cover
    WGPURenderPipeline radialConv{};         // convex geometry (no stencil)
    WGPURenderPipeline linearConv{};         // convex geometry (no stencil)
    WGPURenderPipeline conicConv{};          // convex geometry (no stencil)
    WGPURenderPipeline strokeClip[4]{};
    WGPURenderPipeline image{};
    WGPURenderPipeline imageDirect{};  // image geometry (no stencil)
    WGPURenderPipeline scene{};
    // custom blend
    WGPURenderPipeline solidBlends[18]{};
    WGPURenderPipeline radialBlends[18]{};
    WGPURenderPipeline linearBlends[18]{};
    WGPURenderPipeline conicBlends[18]{};
    WGPURenderPipeline imageBlends[18]{};
    WGPURenderPipeline sceneBlends[18]{};
    // compose
    WGPURenderPipeline sceneCompose[11]{};
    // blit
    WGPURenderPipeline blit{};
    WGPURenderPipeline blitUnpremultiplied{};
    // effects
    WGPURenderPipeline effectGaussianVert{};
    WGPURenderPipeline effectGaussianHorz{};
    WGPURenderPipeline effectDropShadow{};
    WGPURenderPipeline effectFill{};
    WGPURenderPipeline effectTint{};
    WGPURenderPipeline effectTritone{};
    WGPURenderPipeline effectMotionBlur{};

    WGPURenderPipeline solidBlend(WgContext& context, BlendMethod method);
    WGPURenderPipeline radialBlend(WgContext& context, BlendMethod method);
    WGPURenderPipeline linearBlend(WgContext& context, BlendMethod method);
    WGPURenderPipeline conicBlend(WgContext& context, BlendMethod method);
    WGPURenderPipeline clippedStroke(WgContext& context, WgRenderSettingsType type);
    WGPURenderPipeline imageBlend(WgContext& context, BlendMethod method);
    WGPURenderPipeline sceneBlend(WgContext& context, BlendMethod method);
    void initialize(WgContext& context);
    void release(WgContext& context);

private:
    // shaders helpers
    WGPUShaderModule shaderStencil{};
    WGPUShaderModule shaderDepth{};
    // shaders normal blend
    WGPUShaderModule shaderSolid{};
    WGPUShaderModule shaderRadial{};
    WGPUShaderModule shaderLinear{};
    WGPUShaderModule shaderConic{};
    WGPUShaderModule shaderImage{};
    WGPUShaderModule shaderScene{};
    // shaders custom blend
    WGPUShaderModule shaderSolidBlend{};
    WGPUShaderModule shaderRadialBlend{};
    WGPUShaderModule shaderLinearBlend{};
    WGPUShaderModule shaderConicBlend{};
    WGPUShaderModule shaderImageBlend{};
    WGPUShaderModule shaderSceneBlend{};
    // shader scene compose
    WGPUShaderModule shaderSceneCompose{};
    // shader blit
    WGPUShaderModule shaderBlit{};
    // shader effects
    WGPUShaderModule shaderShadow;
    WGPUShaderModule shaderEffects;

    // layouts helpers
    WGPUPipelineLayout layoutStencil{};
    WGPUPipelineLayout layoutDepth{};
    // layouts normal blend
    WGPUPipelineLayout layoutSolid{};
    WGPUPipelineLayout layoutGradient{};
    WGPUPipelineLayout layoutImage{};
    WGPUPipelineLayout layoutScene{};
    // layouts custom blend
    WGPUPipelineLayout layoutSolidBlend{};
    WGPUPipelineLayout layoutGradientBlend{};
    WGPUPipelineLayout layoutImageBlend{};
    WGPUPipelineLayout layoutSceneBlend{};
    // layouts scene compose
    WGPUPipelineLayout layoutSceneCompose{};
    // layouts blit
    WGPUPipelineLayout layoutBlit{};
    // layouts effects
    WGPUPipelineLayout layoutShadow{};
    WGPUPipelineLayout layoutEffects{};

    void releaseGraphicHandles(WgContext& context);
    WGPUShaderModule createShaderModule(WGPUDevice device, const char* label, const char* code);
    WGPUPipelineLayout createPipelineLayout(WGPUDevice device, WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount);
    WGPURenderPipeline createRenderPipeline(WGPUDevice device, const char* pipelineLabel, WGPUShaderModule shaderModule, const char* vsEntryPoint, const char* fsEntryPoint,
                                            WGPUPipelineLayout pipelineLayout, WGPUVertexBufferLayout* vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount, WGPUColorWriteMask writeMask,
                                            WGPUTextureFormat colorTargetFormat, WGPUBlendState blendState, WGPUDepthStencilState depthStencilState, WGPUMultisampleState multisampleState);
    WGPURenderPipeline createBlendPipeline(WgContext& context, const char* pipelineLabel, WGPUShaderModule shaderModule, const char* fsEntryPoint, WGPUPipelineLayout pipelineLayout,
                                           WGPUVertexBufferLayout* vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount, WGPUCompareFunction stencilCompare);
    void releaseRenderPipeline(WGPURenderPipeline& renderPipeline);
    void releasePipelineLayout(WGPUPipelineLayout& pipelineLayout);
    void releaseShaderModule(WGPUShaderModule& shaderModule);
    WGPUDepthStencilState makeDepthStencilState(WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled, WGPUCompareFunction stencilFunctionFrnt, WGPUStencilOperation stencilOperationFrnt);
    WGPUDepthStencilState makeDepthStencilState(WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled, WGPUCompareFunction stencilFunctionFrnt, WGPUStencilOperation stencilOperationFrnt,
                                                WGPUCompareFunction stencilFunctionBack, WGPUStencilOperation stencilOperationBack);
};

#endif // _TVG_WG_PIPELINES_H_
