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

void WgPipelineFillShapeWinding::initialize(WGPUDevice device)
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
    WGPUCompareFunction stencilFunctionFront = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperationFront = WGPUStencilOperation_IncrementWrap;
    WGPUCompareFunction stencilFunctionBack = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperationBack = WGPUStencilOperation_DecrementWrap;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineFill;
    auto shaderLabel = "The shader fill";
    auto pipelineLabel = "The render pipeline fill shape winding";

    // allocate all pipeline handles
    allocate(device, WgPipelineBlendType::SrcOver, WGPUColorWriteMask_None,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunctionFront, stencilOperationFront, stencilFunctionBack, stencilOperationBack,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineFillShapeEvenOdd::initialize(WGPUDevice device)
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
    WGPUCompareFunction stencilFunctionFront = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperationFront = WGPUStencilOperation_Invert;
    WGPUCompareFunction stencilFunctionBack = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperationBack = WGPUStencilOperation_Invert;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineFill;
    auto shaderLabel = "The shader fill";
    auto pipelineLabel = "The render pipeline fill shape Even Odd";

    // allocate all pipeline handles
    allocate(device, WgPipelineBlendType::SrcOver, WGPUColorWriteMask_None,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunctionFront, stencilOperationFront, stencilFunctionBack, stencilOperationBack,
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
    WGPUCompareFunction stencilFunction = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Replace;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineFill;
    auto shaderLabel = "The shader fill";
    auto pipelineLabel = "The render pipeline fill stroke";

    // allocate all pipeline handles
    allocate(device, WgPipelineBlendType::SrcOver, WGPUColorWriteMask_None,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunction, stencilOperation, stencilFunction, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineClipMask::initialize(WGPUDevice device)
{
    // vertex and buffers settings
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
    WGPUCompareFunction stencilFunction = WGPUCompareFunction_NotEqual;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineFill;
    auto shaderLabel = "The shader fill";
    auto pipelineLabel = "The render pipeline clip mask";

    // allocate all pipeline handles
    allocate(device, WgPipelineBlendType::SrcOver, WGPUColorWriteMask_All,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunction, stencilOperation, stencilFunction, stencilOperation,
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
    WGPUCompareFunction stencilFunction = WGPUCompareFunction_NotEqual;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineSolid;
    auto shaderLabel = "The shader solid color";
    auto pipelineLabel = "The render pipeline solid color";

    // allocate all pipeline handles
    allocate(device, blendType, WGPUColorWriteMask_All,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunction, stencilOperation, stencilFunction, stencilOperation,
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
    WGPUCompareFunction stencilFunction = WGPUCompareFunction_NotEqual;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineLinear;
    auto shaderLabel = "The shader linear gradient";
    auto pipelineLabel = "The render pipeline linear gradient";

    // allocate all pipeline handles
    allocate(device, blendType, WGPUColorWriteMask_All,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunction, stencilOperation, stencilFunction, stencilOperation,
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
    WGPUCompareFunction stencilFunction = WGPUCompareFunction_NotEqual;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineRadial;
    auto shaderLabel = "The shader radial gradient";
    auto pipelineLabel = "The render pipeline radial gradient";

    // allocate all pipeline handles
    allocate(device, blendType, WGPUColorWriteMask_All,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunction, stencilOperation, stencilFunction, stencilOperation,
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
    WGPUCompareFunction stencilFunction = WGPUCompareFunction_Always;
    WGPUStencilOperation stencilOperation = WGPUStencilOperation_Zero;

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineImage;
    auto shaderLabel = "The shader image";
    auto pipelineLabel = "The render pipeline image";

    // allocate all pipeline handles
    allocate(device, blendType, WGPUColorWriteMask_All,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFunction, stencilOperation, stencilFunction, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}

//************************************************************************
// compute pipelines
//************************************************************************

void WgPipelineClear::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTextureStorageRgba::getLayout(device)
    };

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineComputeClear;
    auto shaderLabel = "The compute shader clear";
    auto pipelineLabel = "The compute pipeline clear";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineBlend::initialize(WGPUDevice device, const char *shaderSource)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTextureStorageRgba::getLayout(device),
        WgBindGroupTextureStorageRgba::getLayout(device),
        WgBindGroupBlendMethod::getLayout(device),
        WgBindGroupOpacity::getLayout(device)
    };

    // shader source and labels
    auto shaderLabel = "The compute shader blend";
    auto pipelineLabel = "The compute pipeline blend";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineBlendMask::initialize(WGPUDevice device, const char *shaderSource)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTexComposeBlend::getLayout(device),
        WgBindGroupBlendMethod::getLayout(device),
        WgBindGroupOpacity::getLayout(device)
    };

    // shader source and labels
    auto shaderLabel = "The compute shader blend mask";
    auto pipelineLabel = "The compute pipeline blend mask";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineMaskCompose::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTexMaskCompose::getLayout(device),
    };

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineComputeMaskCompose;
    auto shaderLabel = "The compute shader mask compose";
    auto pipelineLabel = "The compute pipeline mask compose";

    // allocate all pipeline handles
    allocate(device,
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineCompose::initialize(WGPUDevice device)
{
    // bind groups and layouts
    WGPUBindGroupLayout bindGroupLayouts[] = {
        WgBindGroupTexComposeBlend::getLayout(device),
        WgBindGroupCompositeMethod::getLayout(device),
        WgBindGroupBlendMethod::getLayout(device),
        WgBindGroupOpacity::getLayout(device)
    };

    // shader source and labels
    auto shaderSource = cShaderSource_PipelineComputeCompose;
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
        WgBindGroupTextureStorageRgba::getLayout(device),
        WgBindGroupTextureStorageBgra::getLayout(device)
    };

    // shader source and labels
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
    fillShapeWinding.initialize(context.device);
    fillShapeEvenOdd.initialize(context.device);
    fillStroke.initialize(context.device);
    clipMask.initialize(context.device);
    for (uint8_t type = (uint8_t)WgPipelineBlendType::SrcOver; type <= (uint8_t)WgPipelineBlendType::Custom; type++) {
        solid[type].initialize(context.device, (WgPipelineBlendType)type);
        linear[type].initialize(context.device, (WgPipelineBlendType)type);
        radial[type].initialize(context.device, (WgPipelineBlendType)type);
        image[type].initialize(context.device, (WgPipelineBlendType)type);
    }
    // compute pipelines
    computeClear.initialize(context.device);
    computeBlendSolid.initialize(context.device, cShaderSource_PipelineComputeBlendSolid);
    computeBlendGradient.initialize(context.device, cShaderSource_PipelineComputeBlendGradient);
    computeBlendImage.initialize(context.device, cShaderSource_PipelineComputeBlendImage);
    computeBlendSolidMask.initialize(context.device, cShaderSource_PipelineComputeBlendSolidMask);
    computeBlendGradientMask.initialize(context.device, cShaderSource_PipelineComputeBlendGradientMask);
    computeBlendImageMask.initialize(context.device, cShaderSource_PipelineComputeBlendImageMask);
    computeMaskCompose.initialize(context.device);
    computeCompose.initialize(context.device);
    computeAntiAliasing.initialize(context.device);
    // store pipelines to context
    context.pipelines = this;
}


void WgPipelines::release()
{
    WgBindGroupTexMaskCompose::releaseLayout();
    WgBindGroupTexComposeBlend::releaseLayout();
    WgBindGroupTextureSampled::releaseLayout();
    WgBindGroupTextureStorageBgra::releaseLayout();
    WgBindGroupTextureStorageRgba::releaseLayout();
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
    computeCompose.release();
    computeMaskCompose.release();
    computeBlendImageMask.release();
    computeBlendGradientMask.release();
    computeBlendSolidMask.release();
    computeBlendImage.release();
    computeBlendGradient.release();
    computeBlendSolid.release();
    computeClear.release();
    // fill pipelines
    for (uint8_t type = (uint8_t)WgPipelineBlendType::SrcOver; type <= (uint8_t)WgPipelineBlendType::Custom; type++) {
        image[type].release();
        radial[type].release();
        linear[type].release();
        solid[type].release();
    }
    clipMask.release();
    fillStroke.release();
    fillShapeEvenOdd.release();
    fillShapeWinding.release();
}


bool WgPipelines::isBlendMethodSupportsHW(BlendMethod blendMethod)
{
    switch (blendMethod) {
        case BlendMethod::SrcOver:
        case BlendMethod::Normal:
            return true;
        default: return false;
    };
}


WgPipelineBlendType WgPipelines::blendMethodToBlendType(BlendMethod blendMethod)
{
    switch (blendMethod) {
        case BlendMethod::SrcOver: return WgPipelineBlendType::SrcOver;
        case BlendMethod::Normal: return WgPipelineBlendType::Normal;
        default: return WgPipelineBlendType::Custom;
    };
}
