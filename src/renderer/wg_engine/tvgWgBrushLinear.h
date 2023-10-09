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

#ifndef _TVG_WG_BRUSH_LINEAR_H_
#define _TVG_WG_BRUSH_LINEAR_H_

#include "tvgWgBrush.h"

// brush linear
class WgBrushPipelineLinear;

// struct uGradientInfo
#define MAX_LINEAR_GRADIENT_STOPS 4
struct WgBrushLinearGradientInfo {
    alignas(16) float nStops[4]{};
    alignas(16) float startPos[2]{};
    alignas(8)  float endPos[2]{};
    alignas(8)  float stopPoints[MAX_LINEAR_GRADIENT_STOPS]{};
    alignas(16) float stopColors[4 * MAX_LINEAR_GRADIENT_STOPS]{};
};

// uniforms data brush color
struct WgBrushDataLinear: WgBrushData {
    // uColorInfo
    WgBrushLinearGradientInfo uGradientInfo{}; // @binding(1)
    void updateGradient(LinearGradient* linearGradient);
};

// wgpu brush linear uniforms data
class WgBrushBindGroupLinear: public WgBrushBindGroup {
private:
    // data handles
    WGPUBuffer uBufferGradientInfo{}; // @binding(1)
public:
    // initialize and release
    void initialize(WGPUDevice device, WgBrushPipelineLinear& brushPipelineLinear);
    void release();

    // update
    void update(WGPUQueue mQueue, WgBrushDataLinear& brushDataLinear);
};

// brush color
class WgBrushPipelineLinear: public WgBrushPipeline {
public:
    // initialize and release
    void initialize(WGPUDevice device) override;
    void release() override;
};

#endif //_TVG_WG_BRUSH_LINEAR_H_
