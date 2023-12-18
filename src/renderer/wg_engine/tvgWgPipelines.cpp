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

#include "tvgWgPipelines.h"
#include "tvgWgShaderSrc.h"

#define ARRAY_ELEMENTS_COUNT(arr) sizeof(arr)/sizeof(arr[0])

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
    allocate(device,
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
    allocate(device,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineSolid::initialize(WGPUDevice device)
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
    allocate(device,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineLinear::initialize(WGPUDevice device)
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
    allocate(device,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineRadial::initialize(WGPUDevice device)
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
    allocate(device,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


void WgPipelineImage::initialize(WGPUDevice device)
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
    allocate(device,
             vertexBufferLayouts, ARRAY_ELEMENTS_COUNT(vertexBufferLayouts),
             bindGroupLayouts, ARRAY_ELEMENTS_COUNT(bindGroupLayouts),
             stencilFuncion, stencilOperation,
             shaderSource, shaderLabel, pipelineLabel);
}


//************************************************************************
// pipelines
//************************************************************************

void WgPipelines::initialize(WGPUDevice device)
{
    mPipelineFillShape.initialize(device);
    mPipelineFillStroke.initialize(device);
    mPipelineSolid.initialize(device);
    mPipelineLinear.initialize(device);
    mPipelineRadial.initialize(device);
    mPipelineImage.initialize(device);
}


void WgPipelines::release()
{
    WgBindGroupCompose::releaseLayout();
    WgBindGroupPicture::releaseLayout();
    WgBindGroupRadialGradient::releaseLayout();
    WgBindGroupLinearGradient::releaseLayout();
    WgBindGroupSolidColor::releaseLayout();
    WgBindGroupCanvas::releaseLayout();
    mPipelineImage.release();
    mPipelineRadial.release();
    mPipelineLinear.release();
    mPipelineSolid.release();
    mPipelineFillStroke.release();
    mPipelineFillShape.release();
}