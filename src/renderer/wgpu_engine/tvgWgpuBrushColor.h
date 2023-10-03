/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#ifndef _TVG_WGPU_BRUSH_COLOR_H_
#define _TVG_WGPU_BRUSH_COLOR_H_

#include "tvgWgpuBrush.h"

// brush color
class WgpuBrushColor;

// struct uMatrix
struct WgpuBrushColorData_Matrix {
    float matrix[16]{};
};

// struct uColorInfo
struct WgpuBrushColorData_ColorInfo {
    float color[4]{};
};

// uniforms data brush color
struct WgpuBrushColorData {
    WgpuBrushColorData_Matrix    uMatrix{};    // @binding(0)
    WgpuBrushColorData_ColorInfo uColorInfo{}; // @binding(1)

    // update matrix
    void updateMatrix(const float* viewMatrix, const RenderTransform* transform);
};

// wgpu brush color uniforms data
class WgpuBrushColorDataBindGroup {
private:
    // webgpu handles
    WGPUBuffer mBufferUniform_uMatrix{};    // @binding(0)
    WGPUBuffer mBufferUniform_uColorInfo{}; // @binding(1)
public:
    // bind group
    WGPUBindGroup mBindGroup{};
public:
    // initialize and release
    void initialize(WGPUDevice device, WgpuBrushColor& brushColor);
    void release();

    // bind
    void bind(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex);
    // update buffers
    void update(WGPUQueue queue, WgpuBrushColorData& data);
};

// brush color
class WgpuBrushColor: public WgpuBrush {
public:
    void initialize(WGPUDevice device) override;
    void release() override;
};

#endif
