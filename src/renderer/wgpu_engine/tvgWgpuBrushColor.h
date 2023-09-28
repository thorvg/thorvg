#pragma once

#ifndef _TVG_WGPU_BRUSH_COLOR_H_
#define _TVG_WGPU_BRUSH_COLOR_H_

#include "tvgWgpuBrush.h"

// struct Matrix
struct WgpuBrushColorUniform_Matrix {
    float matrix[16]{};
};

// struct ColorInfo
struct WgpuBrushColorUniform_ColorInfo {
    float color[4]{};
};

// wgpu brush color uniforms data
struct WgpuBrushColorUniformsData {
    WgpuBrushColorUniform_Matrix    uMatrix;    // @group(0) @binding(0)
    WgpuBrushColorUniform_ColorInfo uColorInfo; // @group(0) @binding(1)
};

// brush color
class WgpuBrushColor: public WgpuBrush {
public:
    void create(WGPUDevice device) override;
    void release() override;
};

#endif
