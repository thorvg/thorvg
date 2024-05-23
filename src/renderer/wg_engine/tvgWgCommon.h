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

#ifndef _TVG_WG_COMMON_H_
#define _TVG_WG_COMMON_H_

#include <cassert>
#include <webgpu/webgpu.h>
#include "tvgCommon.h"
#include "tvgRender.h"

#define WG_VERTEX_BUFFER_MIN_SIZE 2048
#define WG_INDEX_BUFFER_MIN_SIZE 2048

enum class WgPipelineBlendType {
    Src = 0, // S
    Normal,  // (Sa * S) + (255 - Sa) * D
    Add,     // (S + D)
    Mult,    // (S * D)
    Min,     // min(S, D)
    Max      // max(S, D)
};

struct WgPipelines;

struct WgContext {
    WGPUInstance instance{};
    WGPUAdapter adapter{};
    WGPUDevice device{};
    WGPUQueue queue{};

    WGPUFeatureName featureNames[32]{};
    WGPUAdapterProperties adapterProperties{};
    WGPUSupportedLimits supportedLimits{};

    WGPUSampler samplerNearest{};
    WGPUSampler samplerLinear{};
    WGPUBuffer indexBufferFan{};

    WgPipelines* pipelines{}; // external handle (do not release)
    
    void initialize();
    void release();

    void executeCommandEncoder(WGPUCommandEncoder commandEncoder);

    WGPUSampler createSampler(WGPUFilterMode minFilter, WGPUMipmapFilterMode mipmapFilter);
    WGPUTexture createTexture2d(WGPUTextureUsageFlags usage, WGPUTextureFormat format, uint32_t width, uint32_t height, char const * label);
    WGPUTexture createTexture2dMS(WGPUTextureUsageFlags usage, WGPUTextureFormat format, uint32_t width, uint32_t height, uint32_t sc, char const * label);
    WGPUTextureView createTextureView2d(WGPUTexture texture, char const * label);
    WGPUBuffer createBuffer(WGPUBufferUsageFlags usage, uint64_t size, char const * label);

    void releaseSampler(WGPUSampler& sampler);
    void releaseTexture(WGPUTexture& texture);
    void releaseTextureView(WGPUTextureView& textureView);
    void releaseBuffer(WGPUBuffer& buffer);

    void allocateVertexBuffer(WGPUBuffer& buffer, const void *data, uint64_t size);
    void allocateIndexBuffer(WGPUBuffer& buffer, const void *data, uint64_t size);
    void allocateIndexBufferFan(uint64_t size);
    void releaseVertexBuffer(WGPUBuffer& buffer);
    void releaseIndexBuffer(WGPUBuffer& buffer);
};

struct WgBindGroup
{
    WGPUBindGroup mBindGroup{};

    void set(WGPURenderPassEncoder encoder, uint32_t groupIndex);
    void set(WGPUComputePassEncoder encoder, uint32_t groupIndex);

    static WGPUBindGroupEntry makeBindGroupEntryBuffer(uint32_t binding, WGPUBuffer buffer);
    static WGPUBindGroupEntry makeBindGroupEntrySampler(uint32_t binding, WGPUSampler sampler);
    static WGPUBindGroupEntry makeBindGroupEntryTextureView(uint32_t binding, WGPUTextureView textureView);

    static WGPUBindGroupLayoutEntry makeBindGroupLayoutEntryBuffer(uint32_t binding);
    static WGPUBindGroupLayoutEntry makeBindGroupLayoutEntrySampler(uint32_t binding);
    static WGPUBindGroupLayoutEntry makeBindGroupLayoutEntryTexture(uint32_t binding);
    static WGPUBindGroupLayoutEntry makeBindGroupLayoutEntryStorageTexture(uint32_t binding, WGPUStorageTextureAccess access);

    static WGPUBuffer createBuffer(WGPUDevice device, WGPUQueue queue, const void *data, size_t size);
    static WGPUBindGroup createBindGroup(WGPUDevice device, WGPUBindGroupLayout layout, const WGPUBindGroupEntry* bindGroupEntries, uint32_t count);
    static WGPUBindGroupLayout createBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutEntry* bindGroupLayoutEntries, uint32_t count);

    static void releaseBuffer(WGPUBuffer& buffer);
    static void releaseBindGroup(WGPUBindGroup& bindGroup);
    static void releaseBindGroupLayout(WGPUBindGroupLayout& bindGroupLayout);
};

struct WgPipeline
{
protected:
    WGPUPipelineLayout mPipelineLayout{};
    WGPUShaderModule mShaderModule{};
public:
    virtual void initialize(WGPUDevice device) = 0;
    virtual void release();

    static WGPUPipelineLayout createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, uint32_t count);
    static WGPUShaderModule createShaderModule(WGPUDevice device, const char* code, const char* label);
    static void destroyPipelineLayout(WGPUPipelineLayout& pipelineLayout);
    static void destroyShaderModule(WGPUShaderModule& shaderModule);
};

struct WgRenderPipeline: public WgPipeline
{
protected:
    WGPURenderPipeline mRenderPipeline{};
    void allocate(WGPUDevice device, WgPipelineBlendType blendType,
                  WGPUVertexBufferLayout vertexBufferLayouts[], uint32_t attribsCount,
                  WGPUBindGroupLayout bindGroupLayouts[], uint32_t bindGroupsCount,
                  WGPUCompareFunction compareFront, WGPUStencilOperation operationFront,
                  WGPUCompareFunction compareBack, WGPUStencilOperation operationBack,
                  const char* shaderSource, const char* shaderLabel, const char* pipelineLabel);
public:
    void release() override;
    void set(WGPURenderPassEncoder renderPassEncoder);

    static WGPUBlendState makeBlendState(WgPipelineBlendType blendType);
    static WGPUColorTargetState makeColorTargetState(const WGPUBlendState* blendState);
    static WGPUVertexBufferLayout makeVertexBufferLayout(const WGPUVertexAttribute* vertexAttributes, uint32_t count, uint64_t stride);
    static WGPUVertexState makeVertexState(WGPUShaderModule shaderModule, const WGPUVertexBufferLayout* buffers, uint32_t count);
    static WGPUPrimitiveState makePrimitiveState();
    static WGPUDepthStencilState makeDepthStencilState(WGPUCompareFunction compareFront, WGPUStencilOperation operationFront, WGPUCompareFunction compareBack, WGPUStencilOperation operationBack);
    static WGPUMultisampleState makeMultisampleState();
    static WGPUFragmentState makeFragmentState(WGPUShaderModule shaderModule, WGPUColorTargetState* targets, uint32_t size);

    static WGPURenderPipeline createRenderPipeline(WGPUDevice device, WgPipelineBlendType blendType,
                                                   WGPUVertexBufferLayout vertexBufferLayouts[], uint32_t attribsCount,
                                                   WGPUCompareFunction compareFront, WGPUStencilOperation operationFront,
                                                   WGPUCompareFunction compareBack, WGPUStencilOperation operationBack,
                                                   WGPUPipelineLayout pipelineLayout, WGPUShaderModule shaderModule,
                                                   const char* pipelineLabel);
    static void destroyRenderPipeline(WGPURenderPipeline& renderPipeline);
};

#define WG_COMPUTE_WORKGROUP_SIZE_X 8
#define WG_COMPUTE_WORKGROUP_SIZE_Y 8

struct WgComputePipeline: public WgPipeline
{
protected:
    WGPUComputePipeline mComputePipeline{};
    void allocate(WGPUDevice device,
                  WGPUBindGroupLayout bindGroupLayouts[], uint32_t bindGroupsCount,
                  const char* shaderSource, const char* shaderLabel, const char* pipelineLabel);

public:
    void release() override;
    void set(WGPUComputePassEncoder computePassEncoder);
};

#endif // _TVG_WG_COMMON_H_
