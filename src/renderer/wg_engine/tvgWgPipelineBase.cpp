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

#include "tvgWgPipelineBase.h"

/************************************************************************/
// WgPipelineData
/************************************************************************/

void WgPipelineData::updateMatrix(const float* viewMatrix, const RenderTransform* transform) {
    float modelMatrix[16]{};
    if (transform) {
        modelMatrix[0]  = transform->m.e11;
        modelMatrix[1]  = transform->m.e21;
        modelMatrix[3]  = transform->m.e31;
        modelMatrix[4]  = transform->m.e12;
        modelMatrix[5]  = transform->m.e22;
        modelMatrix[7]  = transform->m.e32;
        modelMatrix[10] = 1.0f;
        modelMatrix[12] = transform->m.e13;
        modelMatrix[13] = transform->m.e23;
        modelMatrix[15] = transform->m.e33;
    } else {
        modelMatrix[0] = 1.0f;
        modelMatrix[5] = 1.0f;
        modelMatrix[10] = 1.0f;
        modelMatrix[15] = 1.0f;
    }
    // matrix multiply
    for(auto i = 0; i < 4; ++i) {
        for(auto j = 0; j < 4; ++j) {
            float sum = 0.0;
            for (auto k = 0; k < 4; ++k)
                sum += viewMatrix[k*4+i] * modelMatrix[j*4+k];
            uMatrix.transform[j*4+i] = sum;
        }
    }
}

/************************************************************************/
// WgPipelineBindGroup
/************************************************************************/

void WgPipelineBindGroup::bind(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex) {
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, groupIndex, mBindGroup, 0, nullptr);
}

/************************************************************************/
// WgPipelinePipeline
/************************************************************************/

void WgPipelineBase::set(WGPURenderPassEncoder renderPassEncoder) {
    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mRenderPipeline);
}
