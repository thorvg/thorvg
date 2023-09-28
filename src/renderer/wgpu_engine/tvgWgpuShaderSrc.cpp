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

// uMatrix
@group(0) @binding(0) var<uniform> uMatrix: Matrix;
// uColorInfo
@group(0) @binding(1) var<uniform> uColorInfo: ColorInfo;

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uMatrix.transform * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return uColorInfo.color;
})";
