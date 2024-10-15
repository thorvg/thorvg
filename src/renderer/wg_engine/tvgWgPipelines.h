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

#ifndef _TVG_WG_PIPELINES_H_
#define _TVG_WG_PIPELINES_H_

#include "tvgWgBindGroups.h"

class WgPipelines {
private:
    // shaders helpers
    WGPUShaderModule shader_stencil{};
    // shaders normal blend
    WGPUShaderModule shader_solid{};
    WGPUShaderModule shader_radial{};
    WGPUShaderModule shader_linear{};
    WGPUShaderModule shader_image{};
    WGPUShaderModule shader_scene{};
    // shaders custom blend
    WGPUShaderModule shader_solid_blend{};
    WGPUShaderModule shader_radial_blend{};
    WGPUShaderModule shader_linear_blend{};
    WGPUShaderModule shader_image_blend{};
    WGPUShaderModule shader_scene_blend{};
    // shader scene compose
    WGPUShaderModule shader_scene_compose{};
    WGPUShaderModule shader_merge_masks;
    // shader blit
    WGPUShaderModule shader_blit{};
private:
    // layouts helpers
    WGPUPipelineLayout layout_stencil{};
    // layouts normal blend
    WGPUPipelineLayout layout_solid{};
    WGPUPipelineLayout layout_gradient{};
    WGPUPipelineLayout layout_image{};
    WGPUPipelineLayout layout_scene{};
    // layouts custom blend
    WGPUPipelineLayout layout_solid_blend{};
    WGPUPipelineLayout layout_gradient_blend{};
    WGPUPipelineLayout layout_image_blend{};
    WGPUPipelineLayout layout_scene_blend{};
    // layouts scene compose
    WGPUPipelineLayout layout_scene_compose{};
    WGPUPipelineLayout layout_merge_masks{};
    // layouts blit
    WGPUPipelineLayout layout_blit{};
public:
    // pipelines helpers
    WGPURenderPipeline winding{};
    WGPURenderPipeline evenodd{};
    WGPURenderPipeline direct{};
    WGPURenderPipeline clip_path{};
    // pipelines normal blend
    WGPURenderPipeline solid{};
    WGPURenderPipeline radial{};
    WGPURenderPipeline linear{};
    WGPURenderPipeline image{};
    WGPURenderPipeline scene{};
    // pipelines custom blend
    WGPURenderPipeline solid_blend[18]{};
    WGPURenderPipeline radial_blend[18]{};
    WGPURenderPipeline linear_blend[18]{};
    WGPURenderPipeline image_blend[18]{};
    WGPURenderPipeline scene_blend[18]{};
    // pipelines compose
    WGPURenderPipeline scene_compose[11]{};
    WGPURenderPipeline scene_clip{};
    WGPUComputePipeline merge_masks{};
    // pipeline blit
    WGPURenderPipeline blit{};
public:
    // layouts
    WgBindGroupLayouts layouts;
private:
    void releaseGraphicHandles(WgContext& context);
    void releaseComputeHandles(WgContext& context);
private:
    WGPUShaderModule createShaderModule(WGPUDevice device, const char* label, const char* code);
    WGPUPipelineLayout createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount);
    WGPURenderPipeline createRenderPipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const char* vsEntryPoint, const char* fsEntryPoint,
        const WGPUPipelineLayout pipelineLayout,
        const WGPUVertexBufferLayout *vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
        const WGPUColorWriteMaskFlags writeMask, const WGPUTextureFormat colorTargetFormat,
        const WGPUCompareFunction stencilFunctionFrnt, const WGPUStencilOperation stencilOperationFrnt,
        const WGPUCompareFunction stencilFunctionBack, const WGPUStencilOperation stencilOperationBack,
        const WGPUPrimitiveState primitiveState, const WGPUMultisampleState multisampleState, const WGPUBlendState blendState);
    WGPUComputePipeline createComputePipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const char* entryPoint,
        const WGPUPipelineLayout pipelineLayout);
    void releaseComputePipeline(WGPUComputePipeline& computePipeline);
    void releaseRenderPipeline(WGPURenderPipeline& renderPipeline);
    void releasePipelineLayout(WGPUPipelineLayout& pipelineLayout);
    void releaseShaderModule(WGPUShaderModule& shaderModule);
public:
    void initialize(WgContext& context);
    void release(WgContext& context);
};

#endif // _TVG_WG_PIPELINES_H_
