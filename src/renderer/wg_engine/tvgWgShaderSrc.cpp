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

#include "tvgWgShaderSrc.h"
#include <string>

//************************************************************************
// cShaderSource_PipelineEmpty
//************************************************************************

const char* cShaderSource_PipelineEmpty = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// Matrix
struct Matrix {
    transform: mat4x4f
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f
};

// uMatrix
@group(0) @binding(0) var<uniform> uMatrix: Matrix;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uMatrix.transform * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> void {
    // nothing to draw, just stencil value
})";

//************************************************************************
// cShaderSource_PipelineSolid
//************************************************************************

const char* cShaderSource_PipelineSolid = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// Matrix
struct Matrix {
    transform: mat4x4f
};

// ColorInfo
struct ColorInfo {
    color: vec4f
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f
};

// uMatrix
@group(0) @binding(0) var<uniform> uMatrix: Matrix;
// uColorInfo
@group(0) @binding(1) var<uniform> uColorInfo: ColorInfo;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uMatrix.transform * vec4f(in.position.xy, 0.0, 1.0);
    //out.position = vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return uColorInfo.color;
})";

//************************************************************************
// cShaderSource_PipelineLinear
//************************************************************************

const char* cShaderSource_PipelineLinear = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// Matrix
struct Matrix {
    transform: mat4x4f
};

// GradientInfo
const MAX_STOP_COUNT = 4;
struct GradientInfo {
    nStops       : vec4f,
    gradStartPos : vec2f,
    gradEndPos   : vec2f,
    stopPoints   : vec4f,
    stopColors   : array<vec4f, MAX_STOP_COUNT>
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) vScreenCoord: vec2f
};

// uMatrix
@group(0) @binding(0) var<uniform> uMatrix: Matrix;
// uGradientInfo
@group(0) @binding(1) var<uniform> uGradientInfo: GradientInfo;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uMatrix.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.vScreenCoord = in.position.xy;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let pos: vec2f = in.vScreenCoord;
    let st: vec2f = uGradientInfo.gradStartPos;
    let ed: vec2f = uGradientInfo.gradEndPos;

    let ba: vec2f = ed - st;

    // get interpolation factor
    let t: f32 = clamp(dot(pos - st, ba) / dot(ba, ba), 0.0, 1.0);

    // get stops count
    let last: i32 = i32(uGradientInfo.nStops[0]) - 1;

    // resulting color
    var color = vec4(1.0);

    // closer than first stop
    if (t <= uGradientInfo.stopPoints[0]) {
        color = uGradientInfo.stopColors[0];
    }
    
    // further than last stop
    if (t >= uGradientInfo.stopPoints[last]) {
        color = uGradientInfo.stopColors[last];
    }

    // look in the middle
    for (var i = 0i; i < last; i++) {
        let strt = uGradientInfo.stopPoints[i];
        let stop = uGradientInfo.stopPoints[i+1];
        if ((t > strt) && (t < stop)) {
            let step: f32 = (t - strt) / (stop - strt);
            color = mix(uGradientInfo.stopColors[i], uGradientInfo.stopColors[i+1], step);
        }
    }

    return color;
})";

//************************************************************************
// cShaderSource_PipelineRadial
//************************************************************************

const char* cShaderSource_PipelineRadial = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// Matrix
struct Matrix {
    transform: mat4x4f
};

// GradientInfo
const MAX_STOP_COUNT = 4;
struct GradientInfo {
    nStops     : vec4f,
    centerPos  : vec2f,
    radius     : vec2f,
    stopPoints : vec4f,
    stopColors : array<vec4f, MAX_STOP_COUNT>
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) vScreenCoord: vec2f
};

// uMatrix
@group(0) @binding(0) var<uniform> uMatrix: Matrix;
// uGradientInfo
@group(0) @binding(1) var<uniform> uGradientInfo: GradientInfo;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uMatrix.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.vScreenCoord = in.position.xy;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // get interpolation factor
    let t: f32 = clamp(distance(uGradientInfo.centerPos, in.vScreenCoord) / uGradientInfo.radius.x, 0.0, 1.0);

    // get stops count
    let last: i32 = i32(uGradientInfo.nStops[0]) - 1;

    // resulting color
    var color = vec4(1.0);

    // closer than first stop
    if (t <= uGradientInfo.stopPoints[0]) {
        color = uGradientInfo.stopColors[0];
    }
    
    // further than last stop
    if (t >= uGradientInfo.stopPoints[last]) {
        color = uGradientInfo.stopColors[last];
    }

    // look in the middle
    for (var i = 0i; i < last; i++) {
        let strt = uGradientInfo.stopPoints[i];
        let stop = uGradientInfo.stopPoints[i+1];
        if ((t > strt) && (t < stop)) {
            let step: f32 = (t - strt) / (stop - strt);
            color = mix(uGradientInfo.stopColors[i], uGradientInfo.stopColors[i+1], step);
        }
    }

    return color;
})";

const char* cShaderSource_PipelineImage = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f,
    @location(1) texCoords: vec2f
};

// Matrix
struct Matrix {
    transform: mat4x4f
};

// ColorInfo
struct ColorInfo {
    format: u32,
    dummy0: f32,
    dummy1: f32,
    opacity: f32
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoords: vec2f,
};

// uMatrix
@group(0) @binding(0) var<uniform> uMatrix: Matrix;
// uColorInfo
@group(0) @binding(1) var<uniform> uColorInfo: ColorInfo;
// uSamplerBase
@group(0) @binding(2) var uSamplerBase: sampler;
// uTextureViewBase
@group(0) @binding(3) var uTextureViewBase: texture_2d<f32>;


@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uMatrix.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.texCoords = in.texCoords;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var color: vec4f = textureSample(uTextureViewBase, uSamplerBase, in.texCoords.xy);
    var result: vec4f = color;
    var format: u32 = uColorInfo.format;
    if (format == 1u) { /* FMT_ARGB8888 */
        result = color.bgra;
    } else if (format == 2u) { /* FMT_ABGR8888S */
        result = vec4(color.rgb * color.a, color.a);
    } else if (format == 3u) { /* FMT_ARGB8888S */
        result = vec4(color.bgr * color.a, color.a);
    }
    return vec4f(result.rgb, result.a * uColorInfo.opacity);
})";
