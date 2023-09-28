#pragma once

#ifndef _TVG_WGPU_BRUSH_H_
#define _TVG_WGPU_BRUSH_H_

#include "tvgWgpuCommon.h"

// webgpu brush
class WgpuBrush {
public:
    // bind group layouts
    WGPUBindGroupLayout mBindGroupLayout{}; // @group(0)
    // pipeline layout
    WGPUPipelineLayout mPipelineLayout{};
    // shader module
    WGPUShaderModule mShaderModule{};
    // render pipeline
    WGPURenderPipeline mRenderPipeline{};
public:
    virtual void create(WGPUDevice device) = 0;
    virtual void release() = 0;
};

#endif // _TVG_WGPU_BRUSH_H_