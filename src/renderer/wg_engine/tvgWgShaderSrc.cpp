/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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
// graphics shader source: stencil
//************************************************************************

const char* cShaderSrc_Stencil = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position: vec4f };
struct PaintSettings { transform: mat4x4f };

@group(0) @binding(0) var<uniform> uViewMat  : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return vec4f(0.0, 0.0, 0.0, 1.0);
}
)";

//************************************************************************
// graphics shader source: depth
//************************************************************************

const char* cShaderSrc_Depth = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position: vec4f };
struct PaintSettings { transform: mat4x4f };

@group(0) @binding(0) var<uniform> uViewMat  : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
@group(2) @binding(0) var<uniform> uDepth : f32;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.position.z = uDepth;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return vec4f(1.0, 0.5, 0.0, 1.0);
}
)";

//************************************************************************
// graphics shader source: solid normal blend
//************************************************************************

const char* cShaderSrc_Solid = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position: vec4f };
struct PaintSettings { transform: mat4x4f, options: vec4f, color: vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let Sc = uPaintSettings.color;
    let So = uPaintSettings.options.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: linear normal blend
//************************************************************************

const char* cShaderSrc_Linear = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position : vec4f, @location(0) vGradCoord : vec4f };
struct GradSettings  { transform: mat4x4f, coords: vec4f, focal: vec4f };
struct PaintSettings { transform: mat4x4f, options: vec4f, color: vec4f, gradient: GradSettings };

// uniforms
@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.vGradCoord = uPaintSettings.gradient.transform * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let pos = in.vGradCoord.xy;
    let st = uPaintSettings.gradient.coords.xy;
    let ed = uPaintSettings.gradient.coords.zw;
    let ba = ed - st;
    let t = dot(pos - st, ba) / dot(ba, ba);
    let Sc = textureSample(uTextureGrad, uSamplerGrad, vec2f(t, 0.5));
    let So = uPaintSettings.options.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: radial normal blend
//************************************************************************

const char* cShaderSrc_Radial = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position : vec4f, @location(0) vGradCoord : vec4f };
struct GradSettings  { transform: mat4x4f, coords: vec4f, focal: vec4f };
struct PaintSettings { transform: mat4x4f, options: vec4f, color: vec4f, gradient: GradSettings };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.vGradCoord = uPaintSettings.gradient.transform * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // orignal data
    let d0 = in.vGradCoord.xy - uPaintSettings.gradient.coords.xy;
    let d1 = uPaintSettings.gradient.coords.xy - uPaintSettings.gradient.focal.xy;
    let r0 = uPaintSettings.gradient.coords.z;
    let rd = uPaintSettings.gradient.focal.z - uPaintSettings.gradient.coords.z;
    let a = 1.0*dot(d1, d1) - 1.0*rd*rd;
    let b = 2.0*dot(d0, d1) - 2.0*r0*rd;
    let c = 1.0*dot(d0, d0) - 1.0*r0*r0;
    let t = (-b + sqrt(b*b - 4*a*c))/(2*a);
    let Sc = textureSample(uTextureGrad, uSamplerGrad, vec2f(1.0 - t, 0.5));
    let So = uPaintSettings.options.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: image normal blend
//************************************************************************

const char* cShaderSrc_Image = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) vTexCoord: vec2f };
struct PaintSettings { transform: mat4x4f, options: vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
@group(2) @binding(0) var uSampler     : sampler;
@group(2) @binding(1) var uTextureView : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.vTexCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var Sc: vec4f = textureSample(uTextureView, uSampler, in.vTexCoord.xy);
    let So: f32 = uPaintSettings.options.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
};
)";

//************************************************************************
// graphics shader source: scene normal blend
//************************************************************************

const char* cShaderSrc_Scene = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) vTexCoord: vec2f };

@group(0) @binding(0) var uSamplerSrc : sampler;
@group(0) @binding(1) var uTextureSrc : texture_2d<f32>;
@group(1) @binding(0) var<uniform> So : f32;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position.xy, 0.0, 1.0);
    out.vTexCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let Sc = textureSample(uTextureSrc, uSamplerSrc, in.vTexCoord.xy);
    return vec4f(Sc.rgb * So, Sc.a * So);
};
)";

//************************************************************************
// graphics shader source: custom shaders
//************************************************************************

const char* cShaderSrc_Solid_Blend = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(1) vScrCoord: vec2f };
struct PaintSettings { transform: mat4x4f, options: vec4f, color: vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
// @group(2) - empty
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.position = pos;
    out.vScrCoord = vec2f(0.5 + pos.x * 0.5, 0.5 - pos.y * 0.5);
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    // get source data
    let colorSrc = uPaintSettings.color;
    let colorDst = textureSample(uTextureDst, uSamplerDst, in.vScrCoord.xy);
    // fill fragment data
    var data: FragData;
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.So = uPaintSettings.options.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    data.Sc = data.Sa * data.So * data.Sc;
    data.Sa = data.Sa * data.So;
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f { return R; };
)";

const char* cShaderSrc_Linear_Blend = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) vGradCoord : vec4f, @location(1) vScrCoord: vec2f };
struct GradSettings  { transform: mat4x4f, coords: vec4f, focal: vec4f };
struct PaintSettings { transform: mat4x4f, options: vec4f, color: vec4f, gradient: GradSettings };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.position = pos;
    out.vGradCoord = uPaintSettings.gradient.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.vScrCoord = vec2f(0.5 + pos.x * 0.5, 0.5 - pos.y * 0.5);
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    // get source data
    let pos = in.vGradCoord.xy;
    let st = uPaintSettings.gradient.coords.xy;
    let ed = uPaintSettings.gradient.coords.zw;
    let ba = ed - st;
    let t = dot(pos - st, ba) / dot(ba, ba);
    let colorSrc = textureSample(uTextureGrad, uSamplerGrad, vec2f(t, 0.5));
    let colorDst = textureSample(uTextureDst, uSamplerDst, in.vScrCoord.xy);
    // fill fragment data
    var data: FragData;
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.So = uPaintSettings.options.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    data.Sc = mix(data.Dc, data.Sc, data.Sa * data.So);
    data.Sa = mix(data.Da,     1.0, data.Sa * data.So);
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f { return R; };
)";

const char* cShaderSrc_Radial_Blend = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) vGradCoord : vec4f, @location(1) vScrCoord: vec2f };
struct GradSettings  { transform: mat4x4f, coords: vec4f, focal: vec4f };
struct PaintSettings { transform: mat4x4f, options: vec4f, color: vec4f, gradient: GradSettings };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.position = pos;
    out.vGradCoord = uPaintSettings.gradient.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.vScrCoord = vec2f(0.5 + pos.x * 0.5, 0.5 - pos.y * 0.5);
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    let d0 = in.vGradCoord.xy - uPaintSettings.gradient.coords.xy;
    let d1 = uPaintSettings.gradient.coords.xy - uPaintSettings.gradient.focal.xy;
    let r0 = uPaintSettings.gradient.coords.z;
    let rd = uPaintSettings.gradient.focal.z - uPaintSettings.gradient.coords.z;
    let a = 1.0*dot(d1, d1) - 1.0*rd*rd;
    let b = 2.0*dot(d0, d1) - 2.0*r0*rd;
    let c = 1.0*dot(d0, d0) - 1.0*r0*r0;
    let t = (-b + sqrt(b*b - 4*a*c))/(2*a);
    let colorSrc = textureSample(uTextureGrad, uSamplerGrad, vec2f(1.0 - t, 0.5));
    let colorDst = textureSample(uTextureDst, uSamplerDst, in.vScrCoord.xy);
    // fill fragment data
    var data: FragData;
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.So = uPaintSettings.options.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    data.Sc = mix(data.Dc, data.Sc, data.Sa * data.So);
    data.Sa = mix(data.Da,     1.0, data.Sa * data.So);
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f { return R; };
)";

const char* cShaderSrc_Image_Blend = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) vTexCoord : vec2f, @location(1) vScrCoord: vec2f };
struct PaintSettings { transform: mat4x4f, options: vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uPaintSettings : PaintSettings;
@group(2) @binding(0) var uSamplerSrc : sampler;
@group(2) @binding(1) var uTextureSrc : texture_2d<f32>;
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uPaintSettings.transform * vec4f(in.position.xy, 0.0, 1.0);
    out.position = pos;
    out.vTexCoord = in.texCoord;
    out.vScrCoord = vec2f(0.5 + pos.x * 0.5, 0.5 - pos.y * 0.5);
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    // get source data
    let colorSrc = textureSample(uTextureSrc, uSamplerSrc, in.vTexCoord.xy);
    let colorDst = textureSample(uTextureDst, uSamplerDst, in.vScrCoord.xy);
    // fill fragment data
    var data: FragData;
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.So = uPaintSettings.options.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    data.Sc = data.Sc * data.So;
    data.Sa = data.Sa * data.So;
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f { return mix(vec4(d.Dc, d.Da), R, d.Sa); };
)";

const char* cShaderSrc_Scene_Blend = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) vScrCoord: vec2f };

@group(0) @binding(0) var uSamplerSrc : sampler;
@group(0) @binding(1) var uTextureSrc : texture_2d<f32>;
@group(1) @binding(0) var uSamplerDst : sampler;
@group(1) @binding(1) var uTextureDst : texture_2d<f32>;
@group(2) @binding(0) var<uniform> So : f32;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position.xy, 0.0, 1.0);
    out.vScrCoord = in.texCoord;
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    // get source data
    let colorSrc = textureSample(uTextureSrc, uSamplerSrc, in.vScrCoord.xy);
    let colorDst = textureSample(uTextureDst, uSamplerDst, in.vScrCoord.xy);
    // fill fragment data
    var data: FragData;
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f { return mix(vec4(d.Dc, d.Da), R, d.Sa * So); };
)";

const char* cShaderSrc_BlendFuncs = R"(
@fragment
fn fs_main_Normal(in: VertexOutput) -> @location(0) vec4f {
    // used as debug blend method
    return vec4f(1.0, 0.0, 0.0, 1.0);
}

@fragment
fn fs_main_Multiply(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let Rc = d.Sc * d.Dc;
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_Screen(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let Rc = d.Sc + d.Dc - d.Sc * d.Dc;
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_Overlay(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    var Rc: vec3f;
    Rc.r = select(1.0 - min(1.0, 2 * (1 - d.Sc.r) * (1 - d.Dc.r)), min(1.0, 2 * d.Sc.r * d.Dc.r), (d.Dc.r < 0.5));
    Rc.g = select(1.0 - min(1.0, 2 * (1 - d.Sc.g) * (1 - d.Dc.g)), min(1.0, 2 * d.Sc.g * d.Dc.g), (d.Dc.g < 0.5));
    Rc.b = select(1.0 - min(1.0, 2 * (1 - d.Sc.b) * (1 - d.Dc.b)), min(1.0, 2 * d.Sc.b * d.Dc.b), (d.Dc.b < 0.5));
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_Darken(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let Rc = min(d.Sc, d.Dc);
    return postProcess(d, vec4f(Rc, 1.0));

}

@fragment
fn fs_main_Lighten(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let Rc = max(d.Sc, d.Dc);
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_ColorDodge(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    var Rc: vec3f;
    Rc.r = select(select(0.0, 1.0, d.Dc.r > 0), d.Dc.r / (1 - d.Sc.r), d.Sc.r < 1);
    Rc.g = select(select(0.0, 1.0, d.Dc.g > 0), d.Dc.g / (1 - d.Sc.g), d.Sc.g < 1);
    Rc.b = select(select(0.0, 1.0, d.Dc.b > 0), d.Dc.b / (1 - d.Sc.b), d.Sc.b < 1);
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_ColorBurn(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    var Rc: vec3f;
    Rc.r = select(select(1.0, 0.0, d.Dc.r < 1), 1 - (1 - d.Dc.r) / d.Sc.r, d.Sc.r > 0);
    Rc.g = select(select(1.0, 0.0, d.Dc.g < 1), 1 - (1 - d.Dc.g) / d.Sc.g, d.Sc.g > 0);
    Rc.b = select(select(1.0, 0.0, d.Dc.b < 1), 1 - (1 - d.Dc.b) / d.Sc.b, d.Sc.b > 0);
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_HardLight(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    var Rc: vec3f;
    Rc.r = select(1.0 - min(1.0, 2 * (1 - d.Sc.r) * (1 - d.Dc.r)), min(1.0, 2 * d.Sc.r * d.Dc.r), (d.Sc.r < 0.5));
    Rc.g = select(1.0 - min(1.0, 2 * (1 - d.Sc.g) * (1 - d.Dc.g)), min(1.0, 2 * d.Sc.g * d.Dc.g), (d.Sc.g < 0.5));
    Rc.b = select(1.0 - min(1.0, 2 * (1 - d.Sc.b) * (1 - d.Dc.b)), min(1.0, 2 * d.Sc.b * d.Dc.b), (d.Sc.b < 0.5));
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_SoftLight(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let One = vec3f(1.0, 1.0, 1.0);
    let Rc = min(One, (One - 2 * d.Sc) * d.Dc * d.Dc + 2.0 * d.Sc * d.Dc);
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_Difference(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let One = vec3f(1.0, 1.0, 1.0);
    let Rc = abs(d.Dc - d.Sc);
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_Exclusion(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let One = vec3f(1.0, 1.0, 1.0);
    let Rc = d.Sc + d.Dc - 2 * d.Sc * d.Dc;
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_Add(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    let Rc = d.Sc + d.Dc;
    return postProcess(d, vec4f(Rc, 1.0));
}
)";

//************************************************************************
// graphics shader source: scene compose
//************************************************************************

const char* cShaderSrc_Scene_Compose = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) texCoord: vec2f };

@group(0) @binding(0) var uSamplerSrc : sampler;
@group(0) @binding(1) var uTextureSrc : texture_2d<f32>;
@group(1) @binding(0) var uSamplerMsk : sampler;
@group(1) @binding(1) var uTextureMsk : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position.xy, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main_None(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    return vec4f(src);
};

@fragment
fn fs_main_ClipPath(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src * msk.a);
};

@fragment
fn fs_main_AlphaMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src * msk.a);
};

@fragment
fn fs_main_InvAlphaMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src * (1.0 - msk.a));
};

@fragment
fn fs_main_LumaMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    let luma: f32 = dot(msk.rgb, vec3f(0.2125, 0.7154, 0.0721));
    return vec4f(src * luma);
};

@fragment
fn fs_main_InvLumaMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    let luma: f32 = dot(msk.rgb, vec3f(0.2125, 0.7154, 0.0721));
    return vec4f(src * (1.0 - luma));
};

@fragment
fn fs_main_AddMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src.rgb, src.a + msk.a * (1.0 - src.a));
};

@fragment
fn fs_main_SubtractMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src.rgb, src.a * (1.0 - msk.a));
};

@fragment
fn fs_main_IntersectMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src.rgb, src.a * msk.a);
};

@fragment
fn fs_main_DifferenceMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src.rgb, src.a * (1.0 - msk.a) + msk.a * (1.0 - src.a));
};

@fragment
fn fs_main_LightenMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src.rgb, max(src.a, msk.a));
};

@fragment
fn fs_main_DarkenMask(in: VertexOutput) -> @location(0) vec4f {
    let src: vec4f = textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
    let msk: vec4f = textureSample(uTextureMsk, uSamplerMsk, in.texCoord.xy);
    return vec4f(src.rgb, min(src.a, msk.a));
};
)";

//************************************************************************
// graphics shader source: texture blit
//************************************************************************

const char* cShaderSrc_Blit = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) texCoord: vec2f };

@group(0) @binding(0) var uSamplerSrc : sampler;
@group(0) @binding(1) var uTextureSrc : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position.xy, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy);
};
)";

//************************************************************************
// compute shader source: merge clip path masks
//************************************************************************

const char* cShaderSrc_GaussianBlur = R"(
@group(0) @binding(0) var imageSrc : texture_2d<f32>;
@group(1) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, write>;
@group(2) @binding(0) var<uniform> settings: array<vec4f, 3>;
@group(3) @binding(0) var<uniform> viewport: vec4f;

const N: u32 = 128;
const M: u32 = N * 3;
var<workgroup> buff: array<vec4f, M>;

fn gaussian(x: f32, sigma: f32) -> f32 {
    let a = 0.39894f / sigma;
    let b = -(x * x) / (2.0 * sigma * sigma);
    return a * exp(b);
}

@compute @workgroup_size(N, 1)
fn cs_main_horz(@builtin(global_invocation_id) gid: vec3u,
                @builtin(local_invocation_id)  lid: vec3u) {
    // settings decode
    let sigma = settings[0].x;
    let scale = settings[0].y;
    let size = i32(settings[0].z);

    // viewport decode
    let xmin = i32(viewport.x);
    let ymin = i32(viewport.y);
    let xmax = i32(viewport.z);
    let ymax = i32(viewport.w);

    // tex coord
    let uid = vec2u(gid.x + u32(xmin), gid.y + u32(ymin));
    let iid = vec2i(uid);

    // load source to local workgroup memory
    buff[lid.x + N*0] = textureLoad(imageSrc, uid - vec2u(N, 0), 0);
    buff[lid.x + N*1] = textureLoad(imageSrc, uid + vec2u(0, 0), 0);
    buff[lid.x + N*2] = textureLoad(imageSrc, uid + vec2u(N, 0), 0);
    workgroupBarrier();

    // apply filter
    var weight = gaussian(0.0, sigma);
    var color = weight * buff[lid.x + N];
    var sum = weight;

    for (var i: i32 = 1; i < size; i++) {
        let ii = i32(f32(i) * scale);
        weight = gaussian(f32(i) * scale, sigma);
        let poffset = min(iid.x + ii, xmax) - iid.x;
        let noffset = max(iid.x - ii, xmin) - iid.x;
        color += (weight * buff[i32(lid.x + N) + poffset]);
        color += (weight * buff[i32(lid.x + N) + noffset]);
        sum += (2.0 * weight);
    }

    // store result
    textureStore(imageDst, uid, color / sum);
}

@compute @workgroup_size(1, N)
fn cs_main_vert(@builtin(global_invocation_id) gid: vec3u,
                @builtin(local_invocation_id)  lid: vec3u) {
    // settings decode
    let sigma = settings[0].x;
    let scale = settings[0].y;
    let size = i32(settings[0].z);

    // viewport decode
    let xmin = i32(viewport.x);
    let ymin = i32(viewport.y);
    let xmax = i32(viewport.z);
    let ymax = i32(viewport.w);

    // tex coord
    let uid = vec2u(gid.x + u32(xmin), gid.y + u32(ymin));
    let iid = vec2i(uid);

    // load source to local workgroup memory
    buff[lid.y + N*0] = textureLoad(imageSrc, uid - vec2u(0, N), 0);
    buff[lid.y + N*1] = textureLoad(imageSrc, uid + vec2u(0, 0), 0);
    buff[lid.y + N*2] = textureLoad(imageSrc, uid + vec2u(0, N), 0);
    workgroupBarrier();

    // apply filter
    var weight = gaussian(0.0, sigma);
    var color = weight * buff[lid.y + N];
    var sum = weight;

    for (var i: i32 = 1; i < size; i++) {
        let ii = i32(f32(i) * scale);
        weight = gaussian(f32(i) * scale, sigma);
        let poffset = min(iid.y + ii, ymax) - iid.y;
        let noffset = max(iid.y - ii, ymin) - iid.y;
        color += (weight * buff[i32(lid.y + N) + poffset]);
        color += (weight * buff[i32(lid.y + N) + noffset]);
        sum += (2.0 * weight);
    }

    // store result
    textureStore(imageDst, uid, color / sum);
}
)";

const char* cShaderSrc_Effects = R"(
@group(0) @binding(0) var imageSrc : texture_2d<f32>;
@group(0) @binding(1) var imageSdw : texture_2d<f32>;
@group(1) @binding(0) var imageTrg : texture_storage_2d<rgba8unorm, write>;
@group(2) @binding(0) var<uniform> settings: array<vec4f, 3>;
@group(3) @binding(0) var<uniform> viewport: vec4f;

@compute @workgroup_size(128, 1)
fn cs_main_drop_shadow(@builtin(global_invocation_id) gid: vec3u) {
    // decode viewport and settings
    let vmin = vec2u(viewport.xy);
    let vmax = vec2u(viewport.zw);
    let voff = vec2i(settings[2].xy);

    // tex coord
    let uid = gid.xy + vmin;
    let oid = clamp(vec2u(vec2i(uid) - voff), vmin, vmax);

    let orig = textureLoad(imageSrc, uid, 0);
    let blur = textureLoad(imageSdw, oid, 0);
    let shad = settings[1] * blur.a;
    let color = orig + shad * (1.0 - orig.a);
    textureStore(imageTrg, uid.xy, color);
}
    
@compute @workgroup_size(128, 1)
fn cs_main_fill(@builtin(global_invocation_id) gid: vec3u) {
    // decode viewport and settings
    let vmin = vec2u(viewport.xy);
    let vmax = vec2u(viewport.zw);

    // tex coord
    let uid = min(gid.xy + vmin, vmax);

    let orig = textureLoad(imageSrc, uid, 0);
    let fill = settings[0];
    let color = fill * orig.a * fill.a;
    textureStore(imageTrg, uid.xy, color);
}

@compute @workgroup_size(128, 1)
fn cs_main_tint(@builtin(global_invocation_id) gid: vec3u) {
    let vmin = vec2u(viewport.xy);
    let vmax = vec2u(viewport.zw);
    let uid = min(gid.xy + vmin, vmax);
    let orig = textureLoad(imageSrc, uid, 0);
    let luma: f32 = dot(orig.rgb, vec3f(0.2126, 0.7152, 0.0722));
    let color = vec4f(mix(orig.rgb, mix(settings[0].rgb, settings[1].rgb, luma), settings[2].r) * orig.a, orig.a);
    textureStore(imageTrg, uid.xy, color);
}

@compute @workgroup_size(128, 1)
fn cs_main_tritone(@builtin(global_invocation_id) gid: vec3u) {
    let vmin = vec2u(viewport.xy);
    let vmax = vec2u(viewport.zw);
    let uid = min(gid.xy + vmin, vmax);
    let orig = textureLoad(imageSrc, uid, 0);
    let luma: f32 = dot(orig.rgb, vec3f(0.2126, 0.7152, 0.0722));
    let is_bright: bool = luma >= 0.5f;
    let t = select(luma * 2.0f, (luma - 0.5) * 2.0f, is_bright);
    var tmp = vec4f(mix(select(settings[0].rgb, settings[1].rgb, is_bright), select(settings[1].rgb, settings[2].rgb, is_bright), t), 1.0f);
    if (settings[2].a > 0.0f) {
        tmp = mix(tmp, orig, settings[2].a);
    }
    textureStore(imageTrg, uid.xy, tmp * orig.a);
}
)";