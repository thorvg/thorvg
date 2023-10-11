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

// pipeline matrix
struct WgPipelineMatrix {
    float transform[4*4]{};
};

// pipeline data
struct WgPipelineData {
    // uMatrix
    WgPipelineMatrix uMatrix{};
    // update matrix
    void updateMatrix(const float* viewMatrix, const RenderTransform* transform);
};

// pipeline bind group
class WgPipelineBindGroup {
public:
    // data handles
    WGPUBuffer uBufferMatrix{};
    // handles
    WGPUBindGroup mBindGroup{};
    // bind
    void bind(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex);
};

// pipeline pipeline
class WgPipelineBase {
public:
    // handles
    WGPUBindGroupLayout mBindGroupLayout{}; // @group(0)
    WGPUPipelineLayout  mPipelineLayout{};
    WGPUShaderModule    mShaderModule{};
    WGPURenderPipeline  mRenderPipeline{};
public:
    // initialize and release
    virtual void initialize(WGPUDevice device) = 0;
    virtual void release() = 0;

    // utils
    void set(WGPURenderPassEncoder renderPassEncoder);
};

#endif // _TVG_WG_PIPELINE_BASE_H_
