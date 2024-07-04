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

#pragma once

#ifndef _TVG_WG_SHADER_SRC_H_
#define _TVG_WG_SHADER_SRC_H_

//*****************************************************************************
// render shader modules
//*****************************************************************************

// pipeline shader modules fill
extern const char* cShaderSource_PipelineFill;
extern const char* cShaderSource_PipelineSolid;
extern const char* cShaderSource_PipelineLinear;
extern const char* cShaderSource_PipelineRadial;
extern const char* cShaderSource_PipelineImage;

//*****************************************************************************
// compute shader modules
//*****************************************************************************

// pipeline shader modules clear, compose and blend
extern const char* cShaderSource_PipelineComputeClear;
extern const char* cShaderSource_PipelineComputeBlendSolid;
extern const char* cShaderSource_PipelineComputeBlendGradient;
extern const char* cShaderSource_PipelineComputeBlendImage;
extern const char* cShaderSource_PipelineComputeCompose;
extern const char* cShaderSource_PipelineComputeComposeBlend;
extern const char* cShaderSource_PipelineComputeAntiAlias;

#endif // _TVG_WG_SHADER_SRC_H_
