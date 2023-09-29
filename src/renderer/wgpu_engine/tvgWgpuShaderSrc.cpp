/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include "tvgWgpuShaderSrc.h"
#include <string>

// brush color shader module source
const char* cShaderSource_BrushColor = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec3f
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
    //return vec4f(1.0, 0.5, 1.0, 1.0);
    return uColorInfo.color;
})";