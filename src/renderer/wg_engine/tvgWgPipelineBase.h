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

#ifndef _TVG_WG_PIPELINE_BASE_H_
#define _TVG_WG_PIPELINE_BASE_H_

#include "tvgWgCommon.h"

struct WgPipelineMatrix {
    float transform[4*4]{};
};

struct WgPipelineData {
    WgPipelineMatrix uMatrix{}; // @binding(0)
    void updateMatrix(const float* viewMatrix, const RenderTransform* transform);
};

struct WgPipelineBindGroup {
    WGPUBuffer uBufferMatrix{};
    WGPUBindGroup mBindGroup{};
    void bind(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex);
};

class WgPipelineBase {
public:
    WGPUBindGroupLayout mBindGroupLayout{}; // @group(0)
    WGPUPipelineLayout mPipelineLayout{};
    WGPUShaderModule mShaderModule{};
    WGPURenderPipeline mRenderPipeline{};
public:
    virtual void initialize(WGPUDevice device) = 0;
    virtual void release() = 0;

    void set(WGPURenderPassEncoder renderPassEncoder);
};

#endif // _TVG_WG_PIPELINE_BASE_H_
