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

#ifndef _TVG_WG_PIPELINES_H_
#define _TVG_WG_PIPELINES_H_

#include "tvgWgBindGroups.h"

struct WgPipelineFillShape: public WgPipeline {
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupCanvas& groupCanvas,
             WgBindGroupPaint& groupPaint) {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
    }
};

struct WgPipelineFillStroke: public WgPipeline {
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupCanvas& groupCanvas, 
             WgBindGroupPaint& groupPaint) {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
    }
};

struct WgPipelineSolid: public WgPipeline {
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupCanvas& groupCanvas, 
             WgBindGroupPaint& groupPaint,
             WgBindGroupSolidColor& groupSolid) {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupSolid.set(encoder, 2);
    }
};

struct WgPipelineLinear: public WgPipeline {
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupCanvas& groupCanvas, 
             WgBindGroupPaint& groupPaint,
             WgBindGroupLinearGradient& groupLinear) {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupLinear.set(encoder, 2);
    }
};

struct WgPipelineRadial: public WgPipeline {
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupCanvas& groupCanvas, 
             WgBindGroupPaint& groupPaint,
             WgBindGroupRadialGradient& groupRadial) {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupRadial.set(encoder, 2);
    }
};

struct WgPipelineImage: public WgPipeline {
    void initialize(WGPUDevice device) override;
    void use(WGPURenderPassEncoder encoder,
             WgBindGroupCanvas& groupCanvas, 
             WgBindGroupPaint& groupPaint,
             WgBindGroupPicture& groupPicture) {
        set(encoder);
        groupCanvas.set(encoder, 0);
        groupPaint.set(encoder, 1);
        groupPicture.set(encoder, 2);
    }
};

struct WgPipelines {
    WgPipelineFillShape mPipelineFillShape;
    WgPipelineFillStroke mPipelineFillStroke;
    WgPipelineSolid mPipelineSolid;
    WgPipelineLinear mPipelineLinear;
    WgPipelineRadial mPipelineRadial;
    WgPipelineImage mPipelineImage;

    void initialize(WGPUDevice device);
    void release();
};

#endif // _TVG_WG_PIPELINES_H_
