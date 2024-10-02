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
// graphics shader source: solid
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
    let Sc = uSolidColor.rgb;
    let Sa = uSolidColor.a;
    let So = uBlendSettings.a;
    return vec4f(Sc * Sa * So, Sa * So);
}
)";

//************************************************************************
// graphics shader source: linear
//************************************************************************

const char* cShaderSrc_Linear = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position : vec4f, @location(0) vScreenCoord : vec4f };

// uniforms
@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(2) @binding(2) var<uniform> uSettingGrad : vec4f;
@group(2) @binding(3) var<uniform> uTransformGrad : mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.vScreenCoord = uTransformGrad * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let pos = in.vScreenCoord.xy;
    let st = uSettingGrad.xy;
    let ed = uSettingGrad.zw;
    let ba = ed - st;
    let t = dot(pos - st, ba) / dot(ba, ba);
    let Sc = textureSample(uTextureGrad, uSamplerGrad, vec2f(t, 0.5));
    let So = uBlendSettings.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: radial
//************************************************************************

const char* cShaderSrc_Radial = R"(
struct VertexInput { @location(0) position: vec2f };
struct VertexOutput { @builtin(position) position : vec4f, @location(0) vScreenCoord : vec4f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSamplerGrad : sampler;
@group(2) @binding(1) var uTextureGrad : texture_2d<f32>;
@group(2) @binding(2) var<uniform> uSettingGrad : vec4f;
@group(2) @binding(3) var<uniform> uTransformGrad : mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.vScreenCoord = uTransformGrad * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let t: f32 = distance(uSettingGrad.zw, in.vScreenCoord.xy) / uSettingGrad.r;
    let Sc = textureSample(uTextureGrad, uSamplerGrad, vec2f(t, 0.5));
    let So = uBlendSettings.a;
    return vec4f(Sc.rgb * Sc.a * So, Sc.a * So);
}
)";

//************************************************************************
// graphics shader source: image
//************************************************************************

const char* cShaderSrc_Image = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) texCoord: vec2f };

@group(0) @binding(0) var<uniform> uViewMat : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat      : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : vec4f;
@group(2) @binding(0) var uSampler     : sampler;
@group(2) @binding(1) var uTextureView : texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    var Sc: vec4f = textureSample(uTextureView, uSampler, in.texCoord.xy);
    let So: f32 = uBlendSettings.a;
    switch u32(uBlendSettings.r) {
        case 0u: { Sc = Sc.rgba; }
        case 1u: { Sc = Sc.bgra; }
        case 2u: { Sc = Sc.rgba; }
        case 3u: { Sc = Sc.bgra; }
        default: {}
    }
    return Sc * So;
};
)";

//************************************************************************
// graphics shader source: scene compose
//************************************************************************

const char* cShaderSrc_Scene_Comp = R"(
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
// graphics shader source: scene blend
//************************************************************************

const char* cShaderSrc_Scene_Blend = R"(
struct VertexInput { @location(0) position: vec2f, @location(1) texCoord: vec2f };
struct VertexOutput { @builtin(position) position: vec4f, @location(0) texCoord: vec2f };

@group(0) @binding(0) var uSamplerSrc : sampler;
@group(0) @binding(1) var uTextureSrc : texture_2d<f32>;
@group(1) @binding(0) var<uniform> So : f32;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position.xy, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return textureSample(uTextureSrc, uSamplerSrc, in.texCoord.xy) * So;
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

//************************************************************************
// compute shader source: blend
//************************************************************************

const char* cShaderSrc_BlendHeader_Solid = R"(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, read>;
@group(2) @binding(0) var imageTgt : texture_storage_2d<rgba8unorm, write>;
@group(3) @binding(0) var<uniform> So : f32;

struct FragData { Sc: vec3f, Sa: f32, Dc: vec3f, Da: f32, skip: bool };
fn getFragData(id: vec2u) -> FragData {
    var data: FragData;
    data.skip = true;
    let colorSrc = textureLoad(imageSrc, id.xy);
    if (colorSrc.a == 0.0) { return data; }
    let colorDst = textureLoad(imageDst, id.xy);
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    data.skip = false;
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f { return R; };
)";


const char* cShaderSrc_BlendHeader_Gradient = R"(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, read>;
@group(2) @binding(0) var imageTgt : texture_storage_2d<rgba8unorm, write>;
@group(3) @binding(0) var<uniform> So : f32;

struct FragData { Sc: vec3f, Sa: f32, Dc: vec3f, Da: f32, skip: bool };
fn getFragData(id: vec2u) -> FragData {
    var data: FragData;
    data.skip = true;
    let colorSrc = textureLoad(imageSrc, id.xy);
    if (colorSrc.a == 0.0) { return data; }
    let colorDst = textureLoad(imageDst, id.xy);
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    data.skip = false;
    data.Sc = data.Sc + data.Dc * (1.0 - data.Sa);
    data.Sa = data.Sa + data.Da * (1.0 - data.Sa);
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f { return R; };
)";

const char* cShaderSrc_BlendHeader_Image = R"(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, read>;
@group(2) @binding(0) var imageTgt : texture_storage_2d<rgba8unorm, write>;
@group(3) @binding(0) var<uniform> So : f32;

struct FragData { Sc: vec3f, Sa: f32, Dc: vec3f, Da: f32, skip: bool };
fn getFragData(id: vec2u) -> FragData {
    var data: FragData;
    data.skip = true;
    let colorSrc = textureLoad(imageSrc, id.xy);
    if (colorSrc.a == 0.0) { return data; }
    let colorDst = textureLoad(imageDst, id.xy);
    data.Sc = colorSrc.rgb;
    data.Sa = colorSrc.a;
    data.Dc = colorDst.rgb;
    data.Da = colorDst.a;
    data.skip = false;
    return data;
};

fn postProcess(d: FragData, R: vec4f) -> vec4f {
    return mix(vec4(d.Dc, d.Da), R, d.Sa * So);
};
)";

const char* cShaderSrc_Blend_Funcs = R"(
@compute @workgroup_size(8, 8)
fn cs_main_Normal(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let Rc = d.Sc + d.Dc * (1.0 - d.Sa);
    let Ra = d.Sa + d.Da * (1.0 - d.Sa);
    textureStore(imageTgt, id.xy, vec4f(Rc, Ra));
};

@compute @workgroup_size(8, 8)
fn cs_main_Add(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let Rc = d.Sc + d.Dc;
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_Screen(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let Rc = d.Sc + d.Dc - d.Sc * d.Dc;
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_Multiply(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let Rc = d.Sc * d.Dc;
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_Overlay(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    var Rc: vec3f;
    Rc.r = select(1.0 - min(1.0, 2 * (1 - d.Sc.r) * (1 - d.Dc.r)), min(1.0, 2 * d.Sc.r * d.Dc.r), (d.Dc.r < 0.5));
    Rc.g = select(1.0 - min(1.0, 2 * (1 - d.Sc.g) * (1 - d.Dc.g)), min(1.0, 2 * d.Sc.g * d.Dc.g), (d.Dc.g < 0.5));
    Rc.b = select(1.0 - min(1.0, 2 * (1 - d.Sc.b) * (1 - d.Dc.b)), min(1.0, 2 * d.Sc.b * d.Dc.b), (d.Dc.b < 0.5));
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_Difference(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let Rc = abs(d.Dc - d.Sc);
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_Exclusion(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let One = vec3f(1.0, 1.0, 1.0);
    let Rc = min(One, d.Sc + d.Dc - min(One, 2 * d.Sc * d.Dc));
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_Darken(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let Rc = min(d.Sc, d.Dc);
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_Lighten(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let Rc = max(d.Sc, d.Dc);
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_ColorDodge(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    var Rc: vec3f;
    Rc.r = select(d.Dc.r, (d.Dc.r * 255.0 / (255.0 - d.Sc.r * 255.0))/255.0, (1.0 - d.Sc.r > 0.0));
    Rc.g = select(d.Dc.g, (d.Dc.g * 255.0 / (255.0 - d.Sc.g * 255.0))/255.0, (1.0 - d.Sc.g > 0.0));
    Rc.b = select(d.Dc.b, (d.Dc.b * 255.0 / (255.0 - d.Sc.b * 255.0))/255.0, (1.0 - d.Sc.b > 0.0));
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_ColorBurn(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    var Rc: vec3f;
    Rc.r = select(1.0 - d.Dc.r, (255.0 - (255.0 - d.Dc.r * 255.0) / (d.Sc.r * 255.0)) / 255.0, (d.Sc.r > 0.0));
    Rc.g = select(1.0 - d.Dc.g, (255.0 - (255.0 - d.Dc.g * 255.0) / (d.Sc.g * 255.0)) / 255.0, (d.Sc.g > 0.0));
    Rc.b = select(1.0 - d.Dc.b, (255.0 - (255.0 - d.Dc.b * 255.0) / (d.Sc.b * 255.0)) / 255.0, (d.Sc.b > 0.0));
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_HardLight(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    var Rc: vec3f;
    Rc.r = select(1.0 - min(1.0, 2 * (1 - d.Sc.r) * (1 - d.Dc.r)), min(1.0, 2 * d.Sc.r * d.Dc.r), (d.Sc.r < 0.5));
    Rc.g = select(1.0 - min(1.0, 2 * (1 - d.Sc.g) * (1 - d.Dc.g)), min(1.0, 2 * d.Sc.g * d.Dc.g), (d.Sc.g < 0.5));
    Rc.b = select(1.0 - min(1.0, 2 * (1 - d.Sc.b) * (1 - d.Dc.b)), min(1.0, 2 * d.Sc.b * d.Dc.b), (d.Sc.b < 0.5));
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};

@compute @workgroup_size(8, 8)
fn cs_main_SoftLight(@builtin(global_invocation_id) id: vec3u) {
    let d: FragData = getFragData(id.xy);
    if (d.skip) { return; }
    let One = vec3f(1.0, 1.0, 1.0);
    let Rc = min(One, (One - 2 * d.Sc) * d.Dc * d.Dc + 2.0 * d.Sc * d.Dc);
    textureStore(imageTgt, id.xy, postProcess(d, vec4f(Rc, 1.0)));
};
)";
