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

enum class WgPipelineBlendType { SrcOver = 0, Normal, Custom };

class WgPipelines {
private:
    // graphics pipeline shaders
    WGPUShaderModule shaderStencil{};
    WGPUShaderModule shaderSolid{};
    WGPUShaderModule shaderRadial{};
    WGPUShaderModule shaderLinear{};
    WGPUShaderModule shaderImage{};
    // compute pipeline shaders
    WGPUShaderModule shaderMergeMasks;
    WGPUShaderModule shaderBlendSolid;
    WGPUShaderModule shaderBlendGradient;
    WGPUShaderModule shaderBlendImage;
    WGPUShaderModule shaderCompose;
    WGPUShaderModule shaderCopy;
private:
    // graphics pipeline layouts
    WGPUPipelineLayout layoutStencil{};
    WGPUPipelineLayout layoutSolid{};
    WGPUPipelineLayout layoutGradient{};
    WGPUPipelineLayout layoutImage{};
    // compute pipeline layouts
    WGPUPipelineLayout layoutMergeMasks{};
    WGPUPipelineLayout layoutBlend{};
    WGPUPipelineLayout layoutCompose{};
    WGPUPipelineLayout layoutCopy{};
public:
    // graphics pipeline
    WgBindGroupLayouts layouts;
    WGPURenderPipeline winding{};
    WGPURenderPipeline evenodd{};
    WGPURenderPipeline direct{};
    WGPURenderPipeline solid[3]{};
    WGPURenderPipeline radial[3]{};
    WGPURenderPipeline linear[3]{};
    WGPURenderPipeline image[3]{};
    WGPURenderPipeline clipPath{};
    // compute pipeline
    WGPUComputePipeline mergeMasks;
    WGPUComputePipeline blendSolid[14];
    WGPUComputePipeline blendGradient[14];
    WGPUComputePipeline blendImage[14];
    WGPUComputePipeline compose[12];
    WGPUComputePipeline copy;
private:
    void releaseGraphicHandles(WgContext& context);
    void releaseComputeHandles(WgContext& context);
private:
    WGPUShaderModule createShaderModule(WGPUDevice device, const char* label, const char* code);
    WGPUPipelineLayout createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount);
    WGPURenderPipeline createRenderPipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const WGPUPipelineLayout pipelineLayout,
        const WGPUVertexBufferLayout *vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
        const WGPUColorWriteMaskFlags writeMask,
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
