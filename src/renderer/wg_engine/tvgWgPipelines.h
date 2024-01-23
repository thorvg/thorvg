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

struct WgPipelineBlit: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupTextureSampled& groupTexSampled,
             WgBindGroupOpacity& groupOpacity)
    {
        set(encoder);
        groupTexSampled.set(encoder, 0);
        groupOpacity.set(encoder, 1);
    }
};

struct WgPipelineBlitColor: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder, 
             WgBindGroupTextureSampled& groupTexSampled)
    {
        set(encoder);
        groupTexSampled.set(encoder, 0);
    }
};

struct WgPipelineComposition: public WgRenderPipeline
{
    void initialize(WGPUDevice device) override {};
    void initialize(WGPUDevice device, const char* shaderSrc);
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupTextureSampled& groupTexSampledSrc,
             WgBindGroupTextureSampled& groupTexSampledMsk)
    {
        set(encoder);
        groupTexSampledSrc.set(encoder, 0);
        groupTexSampledMsk.set(encoder, 1);
    }
};

//*****************************************************************************
// compute pipelines
//*****************************************************************************

struct WgPipelineBlend: public WgComputePipeline
{
    void initialize(WGPUDevice device) override {};
    void initialize(WGPUDevice device, const char* shaderSrc);
    void use(WGPUComputePassEncoder encoder,
             WgBindGroupStorageTexture& groupTexSrc,
             WgBindGroupStorageTexture& groupTexDst)
    {
        set(encoder);
        groupTexSrc.set(encoder, 0);
        groupTexDst.set(encoder, 1);
    }
};

//*****************************************************************************
// pipelines
//*****************************************************************************

struct WgPipelines
{
    WgPipelineFillShape fillShape;
    WgPipelineFillStroke fillStroke;
    WgPipelineSolid solid;
    WgPipelineLinear linear;
    WgPipelineRadial radial;
    WgPipelineImage image;
    WgPipelineBlit blit;
    WgPipelineBlitColor blitColor;
    // composition pipelines
    WgPipelineComposition compAlphaMask;
    WgPipelineComposition compInvAlphaMask;
    WgPipelineComposition compLumaMask;
    WgPipelineComposition compInvLumaMask;
    WgPipelineComposition compAddMask;
    WgPipelineComposition compSubtractMask;
    WgPipelineComposition compIntersectMask;
    WgPipelineComposition compDifferenceMask;
    // compute pipelines
    WgPipelineBlend computeBlend;

    void initialize(WgContext& context);
    void release();

    WgPipelineComposition* getCompositionPipeline(CompositeMethod method);
};

#endif // _TVG_WG_PIPELINES_H_
