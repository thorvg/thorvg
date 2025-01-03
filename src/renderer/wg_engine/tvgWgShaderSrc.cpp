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

#define WG_SHADER_SOURCE(...) #__VA_ARGS__

//************************************************************************
// graphics shader source: stencil
//************************************************************************

const char* cShaderSrc_Stencil = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position: vec4f };

@group(0) @binding(0) var<uniform> uViewMat  : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
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

@group(0) @binding(0) var<uniform> uViewMat  : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(2) @binding(0) var<uniform> uDepth : f32;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
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

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat      : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var<uniform> uSolidColor : vec4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let Sc = uSolidColor;
    let So = uBlendSettings.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: linear normal blend
//************************************************************************

const char* cShaderSrc_Linear = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position : vec4f, @location(0) vGradCoord : vec4f };
struct GradSettings { settings: vec4f, focal: vec4f };

// uniforms
@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(2) @binding(2) var<uniform> uSettingGrad : GradSettings;
@group(2) @binding(3) var<uniform> uTransformGrad : mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.vGradCoord = uTransformGrad * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let pos = in.vGradCoord.xy;
    let st = uSettingGrad.settings.xy;
    let ed = uSettingGrad.settings.zw;
    let ba = ed - st;
    let t = dot(pos - st, ba) / dot(ba, ba);
    let Sc = textureSample(uTextureGrad, uSamplerGrad, vec2f(t, 0.5));
    let So = uBlendSettings.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: radial normal blend
//************************************************************************

const char* cShaderSrc_Radial = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position : vec4f, @location(0) vGradCoord : vec4f };
struct GradSettings { settings: vec4f, focal: vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(2) @binding(2) var<uniform> uSettingGrad : GradSettings;
@group(2) @binding(3) var<uniform> uTransformGrad : mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.vGradCoord = uTransformGrad * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // orignal data
    let d0 = in.vGradCoord.xy - uSettingGrad.settings.xy;
    let d1 = uSettingGrad.settings.xy - uSettingGrad.focal.xy;
    let r0 = uSettingGrad.settings.z;
    let rd = uSettingGrad.focal.z - uSettingGrad.settings.z;
    let a = 1.0*dot(d1, d1) - 1.0*rd*rd;
    let b = 2.0*dot(d0, d1) - 2.0*r0*rd;
    let c = 1.0*dot(d0, d0) - 1.0*r0*r0;
    let t = (-b + sqrt(b*b - 4*a*c))/(2*a);
    let Sc = textureSample(uTextureGrad, uSamplerGrad, vec2f(1.0 - t, 0.5));
    let So = uBlendSettings.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: image normal blend
//************************************************************************

const char* cShaderSrc_Image = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) vTexCoord: vec2f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat      : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSampler     : sampler;
@group(2) @binding(1) var uTextureView : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.vTexCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var Sc: vec4f = textureSample(uTextureView, uSampler, in.vTexCoord.xy);
    let So: f32 = uBlendSettings.a;
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

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var<uniform> uSolidColor : vec4f;
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.position = pos;
    out.vScrCoord = vec2f(0.5 + pos.x * 0.5, 0.5 - pos.y * 0.5);
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    // get source data
    let colorSrc = uSolidColor;
    let colorDst = textureSample(uTextureDst, uSamplerDst, in.vScrCoord.xy);
    // fill fragment data
    var data: FragData;
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.So = uBlendSettings.a;
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
struct GradSettings { settings: vec4f, focal: vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(2) @binding(2) var<uniform> uSettingGrad : vec4f;
@group(2) @binding(3) var<uniform> uTransformGrad : mat4x4f;
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.position = pos;
    out.vGradCoord = uTransformGrad * vec4f(in.position.xy, 0.0, 1.0);
    out.vScrCoord = vec2f(0.5 + pos.x * 0.5, 0.5 - pos.y * 0.5);
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    // get source data
    let pos = in.vGradCoord.xy;
    let st = uSettingGrad.xy;
    let ed = uSettingGrad.zw;
    let ba = ed - st;
    let t = dot(pos - st, ba) / dot(ba, ba);
    let colorSrc = textureSample(uTextureGrad, uSamplerGrad, vec2f(t, 0.5));
    let colorDst = textureSample(uTextureDst, uSamplerDst, in.vScrCoord.xy);
    // fill fragment data
    var data: FragData;
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.So = uBlendSettings.a;
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
struct GradSettings { settings: vec4f, focal: vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(2) @binding(2) var<uniform> uSettingGrad : GradSettings;
@group(2) @binding(3) var<uniform> uTransformGrad : mat4x4f;
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.position = pos;
    out.vGradCoord = uTransformGrad * vec4f(in.position.xy, 0.0, 1.0);
    out.vScrCoord = vec2f(0.5 + pos.x * 0.5, 0.5 - pos.y * 0.5);
    return out;
}

struct FragData { Sc: vec3f, Sa: f32, So: f32, Dc: vec3f, Da: f32 };
fn getFragData(in: VertexOutput) -> FragData {
    let d0 = in.vGradCoord.xy - uSettingGrad.settings.xy;
    let d1 = uSettingGrad.settings.xy - uSettingGrad.focal.xy;
    let r0 = uSettingGrad.settings.z;
    let rd = uSettingGrad.focal.z - uSettingGrad.settings.z;
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
    data.So = uBlendSettings.a;
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

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat      : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSamplerSrc     : sampler;
@group(2) @binding(1) var uTextureSrc : texture_2d<f32>;
@group(3) @binding(0) var uSamplerDst : sampler;
@group(3) @binding(1) var uTextureDst : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let pos = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
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
    data.So = uBlendSettings.a;
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
    Rc.r = select(d.Dc.r, (d.Dc.r * 255.0 / (255.0 - d.Sc.r * 255.0))/255.0, (1.0 - d.Sc.r > 0.0));
    Rc.g = select(d.Dc.g, (d.Dc.g * 255.0 / (255.0 - d.Sc.g * 255.0))/255.0, (1.0 - d.Sc.g > 0.0));
    Rc.b = select(d.Dc.b, (d.Dc.b * 255.0 / (255.0 - d.Sc.b * 255.0))/255.0, (1.0 - d.Sc.b > 0.0));
    return postProcess(d, vec4f(Rc, 1.0));
}

@fragment
fn fs_main_ColorBurn(in: VertexOutput) -> @location(0) vec4f {
    let d: FragData = getFragData(in);
    var Rc: vec3f;
    Rc.r = select(1.0 - d.Dc.r, (255.0 - (255.0 - d.Dc.r * 255.0) / (d.Sc.r * 255.0)) / 255.0, (d.Sc.r > 0.0));
    Rc.g = select(1.0 - d.Dc.g, (255.0 - (255.0 - d.Dc.g * 255.0) / (d.Sc.g * 255.0)) / 255.0, (d.Sc.g > 0.0));
    Rc.b = select(1.0 - d.Dc.b, (255.0 - (255.0 - d.Dc.b * 255.0) / (d.Sc.b * 255.0)) / 255.0, (d.Sc.b > 0.0));
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

const char* cShaderSrc_MergeMasks = R"(
@group(0) @binding(0) var imageMsk0 : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageMsk1 : texture_storage_2d<rgba8unorm, read>;
@group(2) @binding(0) var imageTrg  : texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) id: vec3u) {
    let colorMsk0 = textureLoad(imageMsk0, id.xy);
    let colorMsk1 = textureLoad(imageMsk1, id.xy);
    textureStore(imageTrg, id.xy, colorMsk0 * colorMsk1);
}
)";