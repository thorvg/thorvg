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

#ifndef _TVG_WG_PIPELINE_RADIAL_H_
#define _TVG_WG_PIPELINE_RADIAL_H_

#include "tvgWgPipelineBase.h"

// pipeline radial
class WgPipelineRadial;

// struct uGradientInfo
#define MAX_RADIAL_GRADIENT_STOPS 4
struct WgPipelineRadialGradientInfo {
    alignas(16) float nStops[4]{};
    alignas(16) float centerPos[2]{};
    alignas(8)  float radius[2]{};
    alignas(8)  float stopPoints[MAX_RADIAL_GRADIENT_STOPS]{};
    alignas(16) float stopColors[4 * MAX_RADIAL_GRADIENT_STOPS]{};
};

// uniforms data pipeline color
struct WgPipelineDataRadial: WgPipelineData {
    // uColorInfo
    WgPipelineRadialGradientInfo uGradientInfo{}; // @binding(1)
    void updateGradient(RadialGradient* radialGradient);
};

// wgpu pipeline linear uniforms data
class WgPipelineBindGroupRadial: public WgPipelineBindGroup {
private:
    // data handles
    WGPUBuffer uBufferGradientInfo{}; // @binding(1)
public:
    // initialize and release
    void initialize(WGPUDevice device, WgPipelineRadial& pipelinePipelineRadial);
    void release();

    // update
    void update(WGPUQueue mQueue, WgPipelineDataRadial& pipelineDataRadial);
};

// pipeline color
class WgPipelineRadial: public WgPipelineBase {
public:
    // initialize and release
    void initialize(WGPUDevice device) override;
    void release() override;
};

#endif //_TVG_WG_PIPELINE_RADIAL_H_
