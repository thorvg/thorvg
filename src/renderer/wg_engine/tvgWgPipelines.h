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

#ifndef _TVG_WG_PIPELINES_H_
#define _TVG_WG_PIPELINES_H_

#include "tvgWgBindGroups.h"

//*****************************************************************************
// render pipelines
//*****************************************************************************

struct WgPipelineFillShape: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder, WgBindGroupCanvas& groupCanvas, WgBindGroupPaint& groupPaint)
    {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
    }
};

struct WgPipelineFillStroke: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder, WgBindGroupCanvas& groupCanvas, WgBindGroupPaint& groupPaint)
    {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
    }
};

struct WgPipelineSolid: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder, WgBindGroupCanvas& groupCanvas,WgBindGroupPaint& groupPaint, WgBindGroupSolidColor& groupSolid)
    {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupSolid.set(encoder, 2);
    }
};

struct WgPipelineLinear: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder, WgBindGroupCanvas& groupCanvas, WgBindGroupPaint& groupPaint, WgBindGroupLinearGradient& groupLinear)
    {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupLinear.set(encoder, 2);
    }
};

struct WgPipelineRadial: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder, WgBindGroupCanvas& groupCanvas, WgBindGroupPaint& groupPaint, WgBindGroupRadialGradient& groupRadial)
    {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupRadial.set(encoder, 2);
    }
};

struct WgPipelineImage: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder, WgBindGroupCanvas& groupCanvas, WgBindGroupPaint& groupPaint, WgBindGroupPicture& groupPicture)
    {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupPicture.set(encoder, 2);
    }
};

//*****************************************************************************
// compute pipelines
//*****************************************************************************

struct WgPipelineClear: public WgComputePipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPUComputePassEncoder encoder,
             WgBindGroupTextureStorage& groupTexDst)
    {
        set(encoder);
        groupTexDst.set(encoder, 0);
    }
};


struct WgPipelineBlend: public WgComputePipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPUComputePassEncoder encoder,
             WgBindGroupTextureStorage& groupTexSrc,
             WgBindGroupTextureStorage& groupTexDst,
             WgBindGroupBlendMethod& blendMethod)
    {
        set(encoder);
        groupTexSrc.set(encoder, 0);
        groupTexDst.set(encoder, 1);
        blendMethod.set(encoder, 2);
    }
};


struct WgPipelineCompose: public WgComputePipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPUComputePassEncoder encoder,
             WgBindGroupTextureStorage& groupTexSrc,
             WgBindGroupTextureStorage& groupTexMsk,
             WgBindGroupCompositeMethod& groupComposeMethod,
             WgBindGroupOpacity& groupOpacity)
    {
        set(encoder);
        groupTexSrc.set(encoder, 0);
        groupTexMsk.set(encoder, 1);
        groupComposeMethod.set(encoder, 2);
        groupOpacity.set(encoder, 3);
    }
};

//*****************************************************************************
// pipelines
//*****************************************************************************

struct WgPipelines
{
    // render pipelines
    WgPipelineFillShape fillShape;
    WgPipelineFillStroke fillStroke;
    WgPipelineSolid solid;
    WgPipelineLinear linear;
    WgPipelineRadial radial;
    WgPipelineImage image;
    // compute pipelines
    WgPipelineClear computeClear;
    WgPipelineBlend computeBlend;
    WgPipelineCompose computeCompose;

    void initialize(WgContext& context);
    void release();
};

#endif // _TVG_WG_PIPELINES_H_
