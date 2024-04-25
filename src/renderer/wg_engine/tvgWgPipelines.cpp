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

#include "tvgWgPipelines.h"
#include "tvgWgShaderSrc.h"

#define ARRAY_ELEMENTS_COUNT(arr) sizeof(arr)/sizeof(arr[0])

//************************************************************************
// graphics pipelines
//************************************************************************

void WgPipelineFillShape::initialize(WGPUDevice device)
{
    // vertex attributes settings
    WGPUVertexAttribute vertexAttributesPos = { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 };
    WGPUVertexBufferLayout vertexBufferLayouts[] = {
        makeVertexBufferLayout(&vertexAttributesPos, 1, sizeof(float) * 2)
    };

    // bind groups
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupCanvas::getLayout(device),
        WgBindGroupPaint::getLayout(device)
    };

    // stencil function
    WGPUCompareFunction stencilFuncion = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Invert;

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineFill;
    auto shaderLabel = "The shader fill";
    auto pipelineLabel = "The render pipeline fill shape";

    // allocate all pipeline handles
    allocate(device, WgPipelineBlendType::Src,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineFillStroke::initialize(WGPUDevice device)
{
    // vertex and buffers settings
    WGPUVertexAttribute vertexAttributesPos = { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 };
    WGPUVertexBufferLayout vertexBufferLayouts[] = {
        makeVertexBufferLayout(&vertexAttributesPos, 1, sizeof(float) * 2)
    };

    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupCanvas::getLayout(device),
        WgBindGroupPaint::getLayout(device)
    };

    // stencil function
    WGPUCompareFunction stencilFuncion = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Replace;

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineFill;
    auto shaderLabel = "The shader fill";
    auto pipelineLabel = "The render pipeline fill stroke";

    // allocate all pipeline handles
    allocate(device, WgPipelineBlendType::Src,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineSolid::initialize(WGPUDevice device, WgPipelineBlendType blendType)
{
    // vertex and buffers settings
    WGPUVertexAttribute vertexAttributesPos = { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 };
    WGPUVertexBufferLayout vertexBufferLayouts[] = {
        makeVertexBufferLayout(&vertexAttributesPos, 1, sizeof(float) * 2)
    };

    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupCanvas::getLayout(device),
        WgBindGroupPaint::getLayout(device),
        WgBindGroupSolidColor::getLayout(device)
    };

    // stencil function
    WGPUCompareFunction stencilFuncion = WGPUCompareFunction_NotEqual;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineSolid;
    auto shaderLabel = "The shader solid color";
    auto pipelineLabel = "The render pipeline solid color";

    // allocate all pipeline handles
    allocate(device, blendType,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineLinear::initialize(WGPUDevice device, WgPipelineBlendType blendType)
{
    // vertex and buffers settings
    WGPUVertexAttribute vertexAttributesPos = { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 };
    WGPUVertexBufferLayout vertexBufferLayouts[] = {
        makeVertexBufferLayout(&vertexAttributesPos, 1, sizeof(float) * 2)
    };

    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupCanvas::getLayout(device),
        WgBindGroupPaint::getLayout(device),
        WgBindGroupLinearGradient::getLayout(device)
    };

    // stencil function
    WGPUCompareFunction stencilFuncion = WGPUCompareFunction_NotEqual;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineLinear;
    auto shaderLabel = "The shader linear gradient";
    auto pipelineLabel = "The render pipeline linear gradient";

    // allocate all pipeline handles
    allocate(device, blendType,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineRadial::initialize(WGPUDevice device, WgPipelineBlendType blendType)
{
    // vertex and buffers settings
    WGPUVertexAttribute vertexAttributesPos = { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 };
    WGPUVertexBufferLayout vertexBufferLayouts[] = {
        makeVertexBufferLayout(&vertexAttributesPos, 1, sizeof(float) * 2)
    };

    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupCanvas::getLayout(device),
        WgBindGroupPaint::getLayout(device),
        WgBindGroupRadialGradient::getLayout(device)
    };

    // stencil function
    WGPUCompareFunction stencilFuncion = WGPUCompareFunction_NotEqual;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineRadial;
    auto shaderLabel = "The shader radial gradient";
    auto pipelineLabel = "The render pipeline radial gradient";

    // allocate all pipeline handles
    allocate(device, blendType,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineImage::initialize(WGPUDevice device, WgPipelineBlendType blendType)
{
    // vertex and buffers settings
    WGPUVertexAttribute vertexAttributesPos = { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 0 };
    WGPUVertexAttribute vertexAttributesTex = { WGPUVertexFormat_Float32x2, sizeof(float) * 0, 1 };
    WGPUVertexBufferLayout vertexBufferLayouts[] = {
        makeVertexBufferLayout(&vertexAttributesPos, 1, sizeof(float) * 2),
        makeVertexBufferLayout(&vertexAttributesTex, 1, sizeof(float) * 2)
    };

    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupCanvas::getLayout(device),
        WgBindGroupPaint::getLayout(device),
        WgBindGroupPicture::getLayout(device)
    };

    // stencil function
    WGPUCompareFunction stencilFuncion = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineImage;
    auto shaderLabel = "The shader image";
    auto pipelineLabel = "The render pipeline image";

    // allocate all pipeline handles
    allocate(device, blendType,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}

//************************************************************************
// compute pipelines
//************************************************************************

void WgPipelineClear::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTextureStorage::getLayout(device)
    };

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineComputeClear;
    auto shaderLabel = "The compute shader clear";
    auto pipelineLabel = "The compute pipeline clear";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineBlend::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTextureStorage::getLayout(device),
        WgBindGroupTextureStorage::getLayout(device),
        WgBindGroupBlendMethod::getLayout(device)
    };

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineComputeBlend;
    auto shaderLabel = "The compute shader blend";
    auto pipelineLabel = "The compute pipeline blend";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineCompose::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTextureStorage::getLayout(device),
        WgBindGroupTextureStorage::getLayout(device),
        WgBindGroupCompositeMethod::getLayout(device),
        WgBindGroupOpacity::getLayout(device)
    };

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineComputeCompose;
    auto shaderLabel = "The compute shader compose";
    auto pipelineLabel = "The compute pipeline compose";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineComposeBlend::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTexComposeBlend::getLayout(device),
        WgBindGroupCompositeMethod::getLayout(device),
        WgBindGroupBlendMethod::getLayout(device),
        WgBindGroupOpacity::getLayout(device)
    };

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineComputeComposeBlend;
    auto shaderLabel = "The compute shader compose blend";
    auto pipelineLabel = "The compute pipeline compose blend";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineAntiAliasing::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTextureStorage::getLayout(device),
        WgBindGroupTextureStorage::getLayout(device)
    };

    // sheder source and labels
    auto shaderSource = cShaderSource_PipelineComputeAntiAlias;
    auto shaderLabel = "The compute shader anti-aliasing";
    auto pipelineLabel = "The compute pipeline anti-aliasing";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}

//************************************************************************
// pipelines
//************************************************************************

void WgPipelines::initialize(WgContext& context)
{
    // fill pipelines
    fillShape.initialize(context.device);
    fillStroke.initialize(context.device);
    for (uint8_t type = (uint8_t)WgPipelineBlendType::Src; type <= (uint8_t)WgPipelineBlendType::Max; type++) {
        solid[type].initialize(context.device, (WgPipelineBlendType)type);
        linear[type].initialize(context.device, (WgPipelineBlendType)type);
        radial[type].initialize(context.device, (WgPipelineBlendType)type);
        image[type].initialize(context.device, (WgPipelineBlendType)type);
    }
    // compute pipelines
    computeClear.initialize(context.device);
    computeBlend.initialize(context.device);
    computeCompose.initialize(context.device);
    computeComposeBlend.initialize(context.device);
    computeAntiAliasing.initialize(context.device);
    // store pipelines to context
    context.pipelines = this;
}


void WgPipelines::release()
{
    WgBindGroupTexComposeBlend::layout = nullptr;
    WgBindGroupTextureSampled::releaseLayout();
    WgBindGroupTextureStorage::releaseLayout();
    WgBindGroupTexture::releaseLayout();
    WgBindGroupOpacity::releaseLayout();
    WgBindGroupPicture::releaseLayout();
    WgBindGroupRadialGradient::releaseLayout();
    WgBindGroupLinearGradient::releaseLayout();
    WgBindGroupSolidColor::releaseLayout();
    WgBindGroupPaint::releaseLayout();
    WgBindGroupCanvas::releaseLayout();
    // compute pipelines
    computeAntiAliasing.release();
    computeComposeBlend.release();
    computeCompose.release();
    computeBlend.release();
    computeClear.release();
    // fill pipelines
    for (uint8_t type = (uint8_t)WgPipelineBlendType::Src; type <= (uint8_t)WgPipelineBlendType::Max; type++) {
        image[type].release();
        radial[type].release();
        linear[type].release();
        solid[type].release();
    }
    fillStroke.release();
    fillShape.release();
}


bool WgPipelines::isBlendMethodSupportsHW(BlendMethod blendMethod)
{
    switch (blendMethod) {
        case BlendMethod::SrcOver:
        case BlendMethod::Normal:
        case BlendMethod::Add:
        case BlendMethod::Multiply:
        case BlendMethod::Darken:
        case BlendMethod::Lighten:
            return true;
        default: return false;
    };
}


WgPipelineBlendType WgPipelines::blendMethodToBlendType(BlendMethod blendMethod)
{
    switch (blendMethod) {
        case BlendMethod::SrcOver: return WgPipelineBlendType::Src;
        case BlendMethod::Normal: return WgPipelineBlendType::Normal;
        case BlendMethod::Add: return WgPipelineBlendType::Add;
        case BlendMethod::Multiply: return WgPipelineBlendType::Mult;
        case BlendMethod::Darken: return WgPipelineBlendType::Min;
        case BlendMethod::Lighten: return WgPipelineBlendType::Max;
        default: return WgPipelineBlendType::Src;
    };
}
