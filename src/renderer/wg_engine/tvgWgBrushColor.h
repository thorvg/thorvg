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

#ifndef _TVG_WG_BRUSH_COLOR_H_
#define _TVG_WG_BRUSH_COLOR_H_

#include "tvgWgBrush.h"

// brush color
class WgBrushColor;

// struct uMatrix
struct WgBrushColorData_Matrix {
    float matrix[16]{};
};

// struct uColorInfo
struct WgBrushColorData_ColorInfo {
    float color[4]{};
};

// uniforms data brush color
struct WgBrushColorData {
    WgBrushColorData_Matrix    uMatrix{};    // @binding(0)
    WgBrushColorData_ColorInfo uColorInfo{}; // @binding(1)

    // update matrix
    void updateMatrix(const float* viewMatrix, const RenderTransform* transform);
};

// wgpu brush color uniforms data
class WgBrushColorDataBindGroup {
private:
    // webgpu handles
    WGPUBuffer mBufferUniform_uMatrix{};    // @binding(0)
    WGPUBuffer mBufferUniform_uColorInfo{}; // @binding(1)
public:
    // bind group
    WGPUBindGroup mBindGroup{};
public:
    // initialize and release
    void initialize(WGPUDevice device, WgBrushColor& brushColor);
    void release();

    // bind
    void bind(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex);
    // update buffers
    void update(WGPUQueue queue, WgBrushColorData& data);
};

// brush color
class WgBrushColor: public WgBrush {
public:
    void initialize(WGPUDevice device) override;
    void release() override;
};

#endif // _TVG_WG_BRUSH_COLOR_H_
