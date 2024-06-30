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

#include "tvgWgShaderSrc.h"
#include <string>

//************************************************************************
// shader pipeline fill
//************************************************************************

const char* cShaderSource_PipelineFill = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f
};

// uniforms
@group(0) @binding(0) var<uniform> uViewMat      : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat     : mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> void {
    // nothing to draw, just stencil value
}
)";

//************************************************************************
// shader pipeline solid
//************************************************************************

const char* cShaderSource_PipelineSolid = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// BlendSettigs
struct BlendSettigs {
    format  : u32, // ColorSpace
    dummy0  : f32,
    dummy1  : f32,
    opacity : f32
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f
};

// uniforms
@group(0) @binding(0) var<uniform> uViewMat      : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat     : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettigs : BlendSettigs;
@group(2) @binding(0) var<uniform> uSolidColor   : vec4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // resulting color
    var color = vec4(1.0);

    // get color
    color = uSolidColor;

    let alpha: f32 = color.a * uBlendSettigs.opacity;
    return vec4f(color.rgb*alpha, alpha);
}
)";

//************************************************************************
// shader pipeline linear
//************************************************************************

const char* cShaderSource_PipelineLinear = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// BlendSettigs
struct BlendSettigs {
    format  : u32, // ColorSpace
    dummy0  : f32,
    dummy1  : f32,
    opacity : f32
};

// LinearGradient
const MAX_LINEAR_GRADIENT_STOPS = 32;
const MAX_LINEAR_GRADIENT_STOPS_PACKED = MAX_LINEAR_GRADIENT_STOPS / 4;
struct LinearGradient {
    nStops       : u32,
    spread       : u32,
    dummy0       : u32,
    dummy1       : u32,
    gradStartPos : vec2f,
    gradEndPos   : vec2f,
    stopPoints   : array<vec4f, MAX_LINEAR_GRADIENT_STOPS_PACKED>,
    stopColors   : array<vec4f, MAX_LINEAR_GRADIENT_STOPS>
};

// vertex output
struct VertexOutput {
    @builtin(position) position : vec4f,
    @location(0) vScreenCoord   : vec2f
};

// uniforms
@group(0) @binding(0) var<uniform> uViewMat        : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat       : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettigs   : BlendSettigs;
@group(2) @binding(0) var<uniform> uLinearGradient : LinearGradient;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.vScreenCoord = in.position.xy;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // resulting color
    var color = vec4(1.0);

    let pos: vec2f = in.vScreenCoord;
    let st: vec2f = uLinearGradient.gradStartPos;
    let ed: vec2f = uLinearGradient.gradEndPos;

    let ba: vec2f = ed - st;

    // get interpolation factor
    var t: f32 = abs(dot(pos - st, ba) / dot(ba, ba));

    // fill spread
    switch uLinearGradient.spread {
        /* Pad     */ case 0u: { t = clamp(t, 0.0, 1.0); }
        /* Reflect */ case 1u: { t = select(1.0 - fract(t), fract(t), u32(t) % 2 == 0); }
        /* Repeat  */ case 2u: { t = fract(t); }
        default: { t = t; }
    }

    // get stops count
    let last: i32 = i32(uLinearGradient.nStops) - 1;

    // closer than first stop
    if (t <= uLinearGradient.stopPoints[0][0]) {
        color = uLinearGradient.stopColors[0];
    }
    
    // further than last stop
    if (t >= uLinearGradient.stopPoints[last/4][last%4]) {
        color = uLinearGradient.stopColors[last];
    }

    // look in the middle
    for (var i = 0i; i < last; i++) {
        let strt = uLinearGradient.stopPoints[i/4][i%4];
        let stop = uLinearGradient.stopPoints[(i+1)/4][(i+1)%4];
        if ((t > strt) && (t < stop)) {
            let step: f32 = (t - strt) / (stop - strt);
            color = mix(uLinearGradient.stopColors[i], uLinearGradient.stopColors[i+1], step);
        }
    }

    let alpha: f32 = color.a * uBlendSettigs.opacity;
    return vec4f(color.rgb*alpha, alpha);
}
)";

//************************************************************************
// shader pipeline radial
//************************************************************************

const char* cShaderSource_PipelineRadial = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// BlendSettigs
struct BlendSettigs {
    format  : u32, // ColorSpace
    dummy0  : f32,
    dummy1  : f32,
    opacity : f32
};

// RadialGradient
const MAX_RADIAL_GRADIENT_STOPS = 32;
const MAX_RADIAL_GRADIENT_STOPS_PACKED = MAX_RADIAL_GRADIENT_STOPS / 4;
struct RadialGradient {
    nStops     : u32,
    spread     : u32,
    dummy0     : u32,
    dummy1     : u32,
    centerPos  : vec2f,
    radius     : vec2f,
    stopPoints : array<vec4f, MAX_RADIAL_GRADIENT_STOPS_PACKED>,
    stopColors : array<vec4f, MAX_RADIAL_GRADIENT_STOPS>
};

// vertex output
struct VertexOutput {
    @builtin(position) position : vec4f,
    @location(0) vScreenCoord   : vec2f
};

// uniforms
@group(0) @binding(0) var<uniform> uViewMat        : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat       : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettigs   : BlendSettigs;
@group(2) @binding(0) var<uniform> uRadialGradient : RadialGradient;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.vScreenCoord = in.position.xy;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // resulting color
    var color = vec4(1.0);

    // get interpolation factor
    var t: f32 = distance(uRadialGradient.centerPos, in.vScreenCoord) / uRadialGradient.radius.x;

    // fill spread
    switch uRadialGradient.spread {
        /* Pad     */ case 0u: { t = clamp(t, 0.0, 1.0); }
        /* Reflect */ case 1u: { t = select(1.0 - fract(t), fract(t), u32(t) % 2 == 0); }
        /* Repeat  */ case 2u: { t = fract(t); }
        default: { t = t; }
    }

    // get stops count
    let last: i32 = i32(uRadialGradient.nStops) - 1;

    // closer than first stop
    if (t <= uRadialGradient.stopPoints[0][0]) {
        color = uRadialGradient.stopColors[0];
    }
    
    // further than last stop
    if (t >= uRadialGradient.stopPoints[last/4][last%4]) {
        color = uRadialGradient.stopColors[last];
    }

    // look in the middle
    for (var i = 0i; i < last; i++) {
        let strt = uRadialGradient.stopPoints[i/4][i%4];
        let stop = uRadialGradient.stopPoints[(i+1)/4][(i+1)%4];
        if ((t > strt) && (t < stop)) {
            let step: f32 = (t - strt) / (stop - strt);
            color = mix(uRadialGradient.stopColors[i], uRadialGradient.stopColors[i+1], step);
        }
    }

    let alpha: f32 = color.a * uBlendSettigs.opacity;
    return vec4f(color.rgb*alpha, alpha);
}
)";

//************************************************************************
// cShaderSource_PipelineImage
//************************************************************************

const char* cShaderSource_PipelineImage = R"(
// vertex input
struct VertexInput {
    @location(0) position: vec2f,
    @location(1) texCoord: vec2f
};

// BlendSettigs
struct BlendSettigs {
    format  : u32, // ColorSpace
    dummy0  : f32,
    dummy1  : f32,
    opacity : f32
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f
};

@group(0) @binding(0) var<uniform> uViewMat      : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat     : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettigs : BlendSettigs;
@group(2) @binding(0) var uSampler               : sampler;
@group(2) @binding(1) var uTextureView           : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var color: vec4f = textureSample(uTextureView, uSampler, in.texCoord.xy);
    var result: vec4f = color;
    var format: u32 = uBlendSettigs.format;
    if (format == 1u) { /* FMT_ARGB8888 */
        result = color.bgra;
    } else if (format == 2u) { /* FMT_ABGR8888S */
        result = color.rgba;
    } else if (format == 3u) { /* FMT_ARGB8888S */
        result = color.bgra;
    }
    return result * uBlendSettigs.opacity;
};
)";

//************************************************************************
// cShaderSource_PipelineComputeBlend
//************************************************************************

// pipeline shader modules clear
const char* cShaderSource_PipelineComputeClear = R"(
@group(0) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, read_write>;

@compute @workgroup_size(8, 8)
fn cs_main( @builtin(global_invocation_id) id: vec3u) {
    textureStore(imageDst, id.xy, vec4f(0.0, 0.0, 0.0, 0.0));
}
)";


// pipeline shader modules blend
const char* cShaderSource_PipelineComputeBlend = R"(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read_write>;
@group(1) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, read_write>;
@group(2) @binding(0) var<uniform> blendMethod : u32;
@group(3) @binding(0) var<uniform> opacity : f32;

@compute @workgroup_size(8, 8)
fn cs_main( @builtin(global_invocation_id) id: vec3u) {
    let texSize = textureDimensions(imageSrc);
    if ((id.x >= texSize.x) || (id.y >= texSize.y)) { return; };

    let colorSrc = textureLoad(imageSrc, id.xy);
    let colorDst = textureLoad(imageDst, id.xy);

    let So: f32  = opacity;
    let Sa: f32  = colorSrc.a;
    let Da: f32  = colorDst.a;
    let S: vec4f = colorSrc;
    let D: vec4f = colorDst;
    let One: vec4f = vec4(1.0);

    var color: vec4f = vec4f(0.0, 0.0, 0.0, 0.0);
    switch blendMethod {
        /* Normal     */ case 0u:  { color = (S * So) + (1.0 - Sa * So) * D; }
        /* Add        */ case 1u:  { color = (S + D); }
        /* Screen     */ case 2u:  { color = (S + D) - (S * D); }
        /* Multiply   */ case 3u:  { color = (S * D); }
        /* Overlay    */ case 4u:  { color = (Sa * Da) - 2 * (Da - S) * (Sa - D); }
        /* Difference */ case 5u:  { color = abs(S - D); }
        /* Exclusion  */ case 6u:  { color = S + D - (2 * S * D); }
        /* SrcOver    */ case 7u:  { color = S; }
        /* Darken     */ case 8u:  { color = min(S, D); }
        /* Lighten    */ case 9u:  { color = max(S, D); }
        /* ColorDodge */ case 10u: { color = D / (One - S); }
        /* ColorBurn  */ case 11u: { color = One - (One - D) / S; }
        /* HardLight  */ case 12u: { color = (Sa * Da) - 2.0 * (Da - S) * (Sa - D); }
        /* SoftLight  */ case 13u: { color = (One - 2 * S) * (D * D) + (2 * S * D); }
        default:  { color = (S * So) + (1.0 - Sa * So) * D; }
    }

    textureStore(imageDst, id.xy, color);
}
)";

// pipeline shader modules compose
const char* cShaderSource_PipelineComputeCompose = R"(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read_write>;
@group(1) @binding(0) var imageMsk : texture_storage_2d<rgba8unorm, read_write>;
@group(2) @binding(0) var<uniform> composeMethod : u32;
@group(3) @binding(0) var<uniform> opacity : f32;

@compute @workgroup_size(8, 8)
fn cs_main( @builtin(global_invocation_id) id: vec3u) {
    let texSize = textureDimensions(imageSrc);
    if ((id.x >= texSize.x) || (id.y >= texSize.y)) { return; };

    let colorSrc = textureLoad(imageSrc, id.xy);
    let colorMsk = textureLoad(imageMsk, id.xy);

    var color: vec3f = colorSrc.xyz;
    var alpha: f32   = colorMsk.a;
    switch composeMethod {
        /* None           */ case 0u: { color = colorSrc.xyz; }
        /* ClipPath       */ case 1u: { if (colorMsk.a == 0) { alpha = 0.0; }; }
        /* AlphaMask      */ case 2u: { color = mix(colorMsk.xyz, colorSrc.xyz, colorSrc.a * colorMsk.b); }
        /* InvAlphaMask   */ case 3u: { color = mix(colorSrc.xyz, colorMsk.xyz, colorSrc.a * colorMsk.b); alpha = 1.0 - colorMsk.b; }
        /* LumaMask       */ case 4u: { color = colorSrc.xyz * (0.299 * colorMsk.r + 0.587 * colorMsk.g + 0.114 * colorMsk.b);  }
        /* InvLumaMask    */ case 5u: { color = colorSrc.xyz * (1.0 - (0.299 * colorMsk.r + 0.587 * colorMsk.g + 0.114 * colorMsk.b)); alpha = 1.0 - colorMsk.b; }
        /* AddMask        */ case 6u: { color = colorSrc.xyz * colorSrc.a + colorMsk.xyz * (1.0 - colorSrc.a); }
        /* SubtractMask   */ case 7u: { color = colorSrc.xyz * colorSrc.a - colorMsk.xyz * (1.0 - colorSrc.a); }
        /* IntersectMask  */ case 8u: { color = colorSrc.xyz * min(colorSrc.a, colorMsk.a); }
        /* DifferenceMask */ case 9u: { color = abs(colorSrc.xyz - colorMsk.xyz * (1.0 - colorMsk.a)); }
        default: { color = colorSrc.xyz; }
    }

    textureStore(imageSrc, id.xy, vec4f(color, alpha * opacity));
}
)";

// pipeline shader modules compose blend
const char* cShaderSource_PipelineComputeComposeBlend = R"(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read>;
@group(0) @binding(1) var imageMsk : texture_storage_2d<rgba8unorm, read>;
@group(0) @binding(2) var imageDst : texture_storage_2d<rgba8unorm, read_write>;
@group(1) @binding(0) var<uniform> composeMethod : u32;
@group(2) @binding(0) var<uniform> blendMethod : u32;
@group(3) @binding(0) var<uniform> opacity : f32;

@compute @workgroup_size(8, 8)
fn cs_main( @builtin(global_invocation_id) id: vec3u) {
    let texSize = textureDimensions(imageSrc);
    if ((id.x >= texSize.x) || (id.y >= texSize.y)) { return; };

    let colorSrc = textureLoad(imageSrc, id.xy);
    let colorMsk = textureLoad(imageMsk, id.xy);
    if ((length(colorSrc) == 0.0) || (length(colorMsk) == 0.0)) { return; };
    let colorDst = textureLoad(imageDst, id.xy);

    var result: vec3f = colorSrc.xyz;
    var alpha: f32   = colorMsk.a;
    let luma: f32    = dot(colorMsk.xyz, vec3f(0.299, 0.587, 0.114));
    switch composeMethod {
        /* None           */ case 0u: { result = colorSrc.xyz; }
        /* ClipPath       */ case 1u: { if (colorMsk.a == 0) { alpha = 0.0; }; }
        /* AlphaMask      */ case 2u: { result = colorSrc.xyz; alpha = colorSrc.a * colorMsk.a; }
        /* InvAlphaMask   */ case 3u: { result = colorSrc.xyz; alpha = colorSrc.a * (1.0 - colorMsk.a); }
        /* LumaMask       */ case 4u: { result = colorSrc.xyz; alpha = colorSrc.a * luma; }
        /* InvLumaMask    */ case 5u: { result = colorSrc.xyz; alpha = colorSrc.a * (1.0 - luma); }
        /* AddMask        */ case 6u: { result = colorSrc.xyz * colorSrc.a + colorMsk.xyz * (1.0 - colorSrc.a); }
        /* SubtractMask   */ case 7u: { result = colorSrc.xyz * colorSrc.a - colorMsk.xyz * (1.0 - colorSrc.a); }
        /* IntersectMask  */ case 8u: { result = colorSrc.xyz * min(colorSrc.a, colorMsk.a); }
        /* DifferenceMask */ case 9u: { result = abs(colorMsk.xyz - colorSrc.xyz * (1.0 - colorMsk.a)); }
        default: { result = colorSrc.xyz; }
    }

    let So: f32  = opacity;
    let Sa: f32  = alpha;
    let Da: f32  = colorDst.a;
    let S: vec4f = vec4(result, alpha);
    let D: vec4f = colorDst;
    let One: vec4f = vec4(1.0);

    var color: vec4f = vec4f(0.0, 0.0, 0.0, 0.0);
    switch blendMethod {
        /* Normal     */ case 0u:  { color = (S * So) + (1.0 - Sa * So) * D; }
        /* Add        */ case 1u:  { color = (S + D); }
        /* Screen     */ case 2u:  { color = (S + D) - (S * D); }
        /* Multiply   */ case 3u:  { color = (S * D); }
        /* Overlay    */ case 4u:  { color = (Sa * Da) - 2 * (Da - S) * (Sa - D); }
        /* Difference */ case 5u:  { color = abs(S - D); }
        /* Exclusion  */ case 6u:  { color = S + D - (2 * S * D); }
        /* SrcOver    */ case 7u:  { color = S; }
        /* Darken     */ case 8u:  { color = min(S, D); }
        /* Lighten    */ case 9u:  { color = max(S, D); }
        /* ColorDodge */ case 10u: { color = D / (One - S); }
        /* ColorBurn  */ case 11u: { color = One - (One - D) / S; }
        /* HardLight  */ case 12u: { color = (Sa * Da) - 2.0 * (Da - S) * (Sa - D); }
        /* SoftLight  */ case 13u: { color = (One - 2 * S) * (D * D) + (2 * S * D); }
        default:  { color = (S * So) + (1.0 - Sa * So) * D; }
    }

    textureStore(imageDst, id.xy, color);
}
)";

// pipeline shader modules anti-aliasing
const char* cShaderSource_PipelineComputeAntiAlias = R"(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read_write>;
@group(1) @binding(0) var imageDst : texture_storage_2d<bgra8unorm, write>;

@compute @workgroup_size(8, 8)
fn cs_main( @builtin(global_invocation_id) id: vec3u) {
    let texSizeSrc = textureDimensions(imageSrc);
    let texSizeDst = textureDimensions(imageDst);
    if ((id.x >= texSizeDst.x) || (id.y >= texSizeDst.y)) { return; };

    let samples = u32(texSizeSrc.x / texSizeDst.x);
    var color = vec4f(0);
    for (var i = 0u; i < samples; i++) {
        for (var j = 0u; j < samples; j++) {
            color += textureLoad(imageSrc, vec2(id.x * samples + j, id.y * samples + i));
        }
    }

    textureStore(imageDst, id.xy, color / f32(samples * samples));
}
)";
