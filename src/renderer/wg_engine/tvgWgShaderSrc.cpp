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

const char* cShaderSrc_Stencil = WG_SHADER_SOURCE(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// vertex output
struct VertexOutput {
    @builtin(position) position: vec4f
};

// uniforms
@group(0) @binding(0) var<uniform> uViewMat  : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat : mat4x4f;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // fill output
    var out: VertexOutput;
    out.position = uViewMat * uModelMat * vec4f(in.position.xy, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return vec4f(0.0, 0.0, 0.0, 1.0);
}
);

//************************************************************************
// graphics shader source: solid
//************************************************************************

const char* cShaderSrc_Solid = WG_SHADER_SOURCE(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// BlendSettings
struct BlendSettings {
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
@group(0) @binding(0) var<uniform> uViewMat       : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat      : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : BlendSettings;
@group(2) @binding(0) var<uniform> uSolidColor    : vec4f;

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

    let alpha: f32 = color.a * uBlendSettings.opacity;
    return vec4f(color.rgb * alpha, alpha);
}
);

//************************************************************************
// graphics shader source: linear
//************************************************************************

const char* cShaderSrc_Linear = WG_SHADER_SOURCE(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// BlendSettings
struct BlendSettings {
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
@group(1) @binding(1) var<uniform> uBlendSettings  : BlendSettings;
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
    var t: f32 = dot(pos - st, ba) / dot(ba, ba);

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
        if ((t >= strt) && (t < stop)) {
            let step: f32 = (t - strt) / (stop - strt);
            color = mix(uLinearGradient.stopColors[i], uLinearGradient.stopColors[i+1], step);
        }
    }

    let alpha: f32 = color.a * uBlendSettings.opacity;
    return vec4f(color.rgb * alpha, alpha);
}
);

//************************************************************************
// graphics shader source: radial
//************************************************************************

const char* cShaderSrc_Radial = WG_SHADER_SOURCE(
// vertex input
struct VertexInput {
    @location(0) position: vec2f
};

// BlendSettings
struct BlendSettings {
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
@group(1) @binding(1) var<uniform> uBlendSettings  : BlendSettings;
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
        if ((t >= strt) && (t < stop)) {
            let step: f32 = (t - strt) / (stop - strt);
            color = mix(uRadialGradient.stopColors[i], uRadialGradient.stopColors[i+1], step);
        }
    }

    let alpha: f32 = color.a * uBlendSettings.opacity;
    return vec4f(color.rgb * alpha, alpha);
}
);

//************************************************************************
// graphics shader source: image
//************************************************************************

const char* cShaderSrc_Image = WG_SHADER_SOURCE(
// vertex input
struct VertexInput {
    @location(0) position: vec2f,
    @location(1) texCoord: vec2f
};

// BlendSettings
struct BlendSettings {
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

@group(0) @binding(0) var<uniform> uViewMat       : mat4x4f;
@group(1) @binding(0) var<uniform> uModelMat      : mat4x4f;
@group(1) @binding(1) var<uniform> uBlendSettings : BlendSettings;
@group(2) @binding(0) var uSampler                : sampler;
@group(2) @binding(1) var uTextureView            : texture_2d<f32>;

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
    var format: u32 = uBlendSettings.format;
    if (format == 1u) { /* FMT_ARGB8888 */
        result = color.bgra;
    } else if (format == 2u) { /* FMT_ABGR8888S */
        result = color.rgba;
    } else if (format == 3u) { /* FMT_ARGB8888S */
        result = color.bgra;
    }
    return result * uBlendSettings.opacity;
};
);

//************************************************************************
// compute shaders blend
//************************************************************************


const char* cShaderSrc_MergeMasks = WG_SHADER_SOURCE(
@group(0) @binding(0) var imageMsk0 : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageMsk1 : texture_storage_2d<rgba8unorm, read>;
@group(2) @binding(0) var imageTrg  : texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) id: vec3u) {
    let colorMsk0 = textureLoad(imageMsk0, id.xy);
    let colorMsk1 = textureLoad(imageMsk1, id.xy);
    textureStore(imageTrg, id.xy, colorMsk0 * colorMsk1);
}
);


//************************************************************************
// compute shaders blend
//************************************************************************

std::string cBlendHeader = WG_SHADER_SOURCE(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, read>;
@group(2) @binding(0) var imageTgt : texture_storage_2d<rgba8unorm, write>;
@group(3) @binding(0) var<uniform> So : f32;

@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) id: vec3u) {
    let colorSrc = textureLoad(imageSrc, id.xy);
    if (colorSrc.a == 0.0) { return; }
    let colorDst = textureLoad(imageDst, id.xy);

    var Sc: vec3f = colorSrc.rgb;
    var Sa: f32   = colorSrc.a;
    let Dc: vec3f = colorDst.rgb;
    let Da: f32   = colorDst.a;
    var Rc: vec3f = colorDst.rgb;
    var Ra: f32   = 1.0;
);

const std::string cBlendPreProcess_Gradient = WG_SHADER_SOURCE(
    Sc = Sc + Dc * (1.0 - Sa);
    Sa = Sa + Da * (1.0 - Sa);
);
const std::string cBlendPreProcess_Image = WG_SHADER_SOURCE(
    Sc = Sc * So;
    Sa = Sa * So;
);

const std::string cBlendEquation_Normal = WG_SHADER_SOURCE(
    Rc = Sc + Dc * (1.0 - Sa);
    Ra = Sa + Da * (1.0 - Sa);
);
const std::string cBlendEquation_Add = WG_SHADER_SOURCE(
    Rc = Sc + Dc;
);
const std::string cBlendEquation_Screen = WG_SHADER_SOURCE(
    Rc = Sc + Dc - Sc * Dc;
);
const std::string cBlendEquation_Multiply = WG_SHADER_SOURCE(
    Rc = Sc * Dc;
);
const std::string cBlendEquation_Overlay = WG_SHADER_SOURCE(
    Rc.r = select(1.0 - min(1.0, 2 * (1 - Sc.r) * (1 - Dc.r)), min(1.0, 2 * Sc.r * Dc.r), (Dc.r < 0.5));
    Rc.g = select(1.0 - min(1.0, 2 * (1 - Sc.g) * (1 - Dc.g)), min(1.0, 2 * Sc.g * Dc.g), (Dc.g < 0.5));
    Rc.b = select(1.0 - min(1.0, 2 * (1 - Sc.b) * (1 - Dc.b)), min(1.0, 2 * Sc.b * Dc.b), (Dc.b < 0.5));
);
const std::string cBlendEquation_Difference = WG_SHADER_SOURCE(
    Rc = abs(Dc - Sc);
);
const std::string cBlendEquation_Exclusion = WG_SHADER_SOURCE(
    let One = vec3f(1.0, 1.0, 1.0);
    Rc = min(One, Sc + Dc - min(One, 2 * Sc * Dc));
);
const std::string cBlendEquation_SrcOver = WG_SHADER_SOURCE(
    Rc = Sc; Ra = Sa;
);
const std::string cBlendEquation_Darken = WG_SHADER_SOURCE(
    Rc = min(Sc, Dc);
);
const std::string cBlendEquation_Lighten = WG_SHADER_SOURCE(
    Rc = max(Sc, Dc);
);
const std::string cBlendEquation_ColorDodge = WG_SHADER_SOURCE(
    Rc.r = select(Dc.r, (Dc.r * 255.0 / (255.0 - Sc.r * 255.0))/255.0, (1.0 - Sc.r > 0.0));
    Rc.g = select(Dc.g, (Dc.g * 255.0 / (255.0 - Sc.g * 255.0))/255.0, (1.0 - Sc.g > 0.0));
    Rc.b = select(Dc.b, (Dc.b * 255.0 / (255.0 - Sc.b * 255.0))/255.0, (1.0 - Sc.b > 0.0));
);
const std::string cBlendEquation_ColorBurn = WG_SHADER_SOURCE(
    Rc.r = select(1.0 - Dc.r, (255.0 - (255.0 - Dc.r * 255.0) / (Sc.r * 255.0)) / 255.0, (Sc.r > 0.0));
    Rc.g = select(1.0 - Dc.g, (255.0 - (255.0 - Dc.g * 255.0) / (Sc.g * 255.0)) / 255.0, (Sc.g > 0.0));
    Rc.b = select(1.0 - Dc.b, (255.0 - (255.0 - Dc.b * 255.0) / (Sc.b * 255.0)) / 255.0, (Sc.b > 0.0));
);
const std::string cBlendEquation_HardLight = WG_SHADER_SOURCE(
    Rc.r = select(1.0 - min(1.0, 2 * (1 - Sc.r) * (1 - Dc.r)), min(1.0, 2 * Sc.r * Dc.r), (Sc.r < 0.5));
    Rc.g = select(1.0 - min(1.0, 2 * (1 - Sc.g) * (1 - Dc.g)), min(1.0, 2 * Sc.g * Dc.g), (Sc.g < 0.5));
    Rc.b = select(1.0 - min(1.0, 2 * (1 - Sc.b) * (1 - Dc.b)), min(1.0, 2 * Sc.b * Dc.b), (Sc.b < 0.5));
);
const std::string cBlendEquation_SoftLight = WG_SHADER_SOURCE(
    let One = vec3f(1.0, 1.0, 1.0);
    Rc = min(One, (One - 2 * Sc) * Dc * Dc + 2.0 * Sc * Dc);
);

const std::string cBlendPostProcess_Gradient = WG_SHADER_SOURCE(
);
const std::string cBlendPostProcess_Image = WG_SHADER_SOURCE(
    Rc = mix(Dc, Rc, Sa);
    Ra = mix(Da, Ra, Sa);
);

const std::string cBlendFooter = WG_SHADER_SOURCE(
    textureStore(imageTgt, id.xy, vec4f(Rc, Ra));
}
);

std::string blend_solid_Normal     = cBlendHeader + cBlendEquation_Normal     + cBlendFooter;
std::string blend_solid_Add        = cBlendHeader + cBlendEquation_Add        + cBlendFooter;
std::string blend_solid_Screen     = cBlendHeader + cBlendEquation_Screen     + cBlendFooter;
std::string blend_solid_Multiply   = cBlendHeader + cBlendEquation_Multiply   + cBlendFooter;
std::string blend_solid_Overlay    = cBlendHeader + cBlendEquation_Overlay    + cBlendFooter;
std::string blend_solid_Difference = cBlendHeader + cBlendEquation_Difference + cBlendFooter;
std::string blend_solid_Exclusion  = cBlendHeader + cBlendEquation_Exclusion  + cBlendFooter;
std::string blend_solid_SrcOver    = cBlendHeader + cBlendEquation_SrcOver    + cBlendFooter;
std::string blend_solid_Darken     = cBlendHeader + cBlendEquation_Darken     + cBlendFooter;
std::string blend_solid_Lighten    = cBlendHeader + cBlendEquation_Lighten    + cBlendFooter;
std::string blend_solid_ColorDodge = cBlendHeader + cBlendEquation_ColorDodge + cBlendFooter;
std::string blend_solid_ColorBurn  = cBlendHeader + cBlendEquation_ColorBurn  + cBlendFooter;
std::string blend_solid_HardLight  = cBlendHeader + cBlendEquation_HardLight  + cBlendFooter;
std::string blend_solid_SoftLight  = cBlendHeader + cBlendEquation_SoftLight  + cBlendFooter;

const char* cShaderSrc_Blend_Solid[] {
    blend_solid_Normal.c_str(),
    blend_solid_Add.c_str(),
    blend_solid_Screen.c_str(),
    blend_solid_Multiply.c_str(),
    blend_solid_Overlay.c_str(),
    blend_solid_Difference.c_str(),
    blend_solid_Exclusion.c_str(),
    blend_solid_SrcOver.c_str(),
    blend_solid_Darken.c_str(),
    blend_solid_Lighten.c_str(),
    blend_solid_ColorDodge.c_str(),
    blend_solid_ColorBurn.c_str(),
    blend_solid_HardLight.c_str(),
    blend_solid_SoftLight.c_str()
};


const std::string cBlendHeader_Gradient = cBlendHeader + cBlendPreProcess_Gradient;
const std::string cBlendFooter_Gradient = cBlendPostProcess_Gradient + cBlendFooter;
std::string blend_gradient_Normal     = cBlendHeader_Gradient + cBlendEquation_Normal     + cBlendFooter_Gradient;
std::string blend_gradient_Add        = cBlendHeader_Gradient + cBlendEquation_Add        + cBlendFooter_Gradient;
std::string blend_gradient_Screen     = cBlendHeader_Gradient + cBlendEquation_Screen     + cBlendFooter_Gradient;
std::string blend_gradient_Multiply   = cBlendHeader_Gradient + cBlendEquation_Multiply   + cBlendFooter_Gradient;
std::string blend_gradient_Overlay    = cBlendHeader_Gradient + cBlendEquation_Overlay    + cBlendFooter_Gradient;
std::string blend_gradient_Difference = cBlendHeader_Gradient + cBlendEquation_Difference + cBlendFooter_Gradient;
std::string blend_gradient_Exclusion  = cBlendHeader_Gradient + cBlendEquation_Exclusion  + cBlendFooter_Gradient;
std::string blend_gradient_SrcOver    = cBlendHeader_Gradient + cBlendEquation_SrcOver    + cBlendFooter_Gradient;
std::string blend_gradient_Darken     = cBlendHeader_Gradient + cBlendEquation_Darken     + cBlendFooter_Gradient;
std::string blend_gradient_Lighten    = cBlendHeader_Gradient + cBlendEquation_Lighten    + cBlendFooter_Gradient;
std::string blend_gradient_ColorDodge = cBlendHeader_Gradient + cBlendEquation_ColorDodge + cBlendFooter_Gradient;
std::string blend_gradient_ColorBurn  = cBlendHeader_Gradient + cBlendEquation_ColorBurn  + cBlendFooter_Gradient;
std::string blend_gradient_HardLight  = cBlendHeader_Gradient + cBlendEquation_HardLight  + cBlendFooter_Gradient;
std::string blend_gradient_SoftLight  = cBlendHeader_Gradient + cBlendEquation_SoftLight  + cBlendFooter_Gradient;

const char* cShaderSrc_Blend_Gradient[] {
    blend_gradient_Normal.c_str(),
    blend_gradient_Add.c_str(),
    blend_gradient_Screen.c_str(),
    blend_gradient_Multiply.c_str(),
    blend_gradient_Overlay.c_str(),
    blend_gradient_Difference.c_str(),
    blend_gradient_Exclusion.c_str(),
    blend_gradient_SrcOver.c_str(),
    blend_gradient_Darken.c_str(),
    blend_gradient_Lighten.c_str(),
    blend_gradient_ColorDodge.c_str(),
    blend_gradient_ColorBurn.c_str(),
    blend_gradient_HardLight.c_str(),
    blend_gradient_SoftLight.c_str()
};

const std::string cBlendHeader_Image = cBlendHeader + cBlendPreProcess_Image;
const std::string cBlendFooter_Image = cBlendPostProcess_Image + cBlendFooter;
std::string blend_image_Normal     = cBlendHeader_Image + cBlendEquation_Normal     + cBlendFooter;
std::string blend_image_Add        = cBlendHeader_Image + cBlendEquation_Add        + cBlendFooter_Image;
std::string blend_image_Screen     = cBlendHeader_Image + cBlendEquation_Screen     + cBlendFooter_Image;
std::string blend_image_Multiply   = cBlendHeader_Image + cBlendEquation_Multiply   + cBlendFooter_Image;
std::string blend_image_Overlay    = cBlendHeader_Image + cBlendEquation_Overlay    + cBlendFooter_Image;
std::string blend_image_Difference = cBlendHeader_Image + cBlendEquation_Difference + cBlendFooter_Image;
std::string blend_image_Exclusion  = cBlendHeader_Image + cBlendEquation_Exclusion  + cBlendFooter_Image;
std::string blend_image_SrcOver    = cBlendHeader_Image + cBlendEquation_SrcOver    + cBlendFooter_Image;
std::string blend_image_Darken     = cBlendHeader_Image + cBlendEquation_Darken     + cBlendFooter_Image;
std::string blend_image_Lighten    = cBlendHeader_Image + cBlendEquation_Lighten    + cBlendFooter_Image;
std::string blend_image_ColorDodge = cBlendHeader_Image + cBlendEquation_ColorDodge + cBlendFooter_Image;
std::string blend_image_ColorBurn  = cBlendHeader_Image + cBlendEquation_ColorBurn  + cBlendFooter_Image;
std::string blend_image_HardLight  = cBlendHeader_Image + cBlendEquation_HardLight  + cBlendFooter_Image;
std::string blend_image_SoftLight  = cBlendHeader_Image + cBlendEquation_SoftLight  + cBlendFooter_Image;

const char* cShaderSrc_Blend_Image[] {
    blend_image_Normal.c_str(),
    blend_image_Add.c_str(),
    blend_image_Screen.c_str(),
    blend_image_Multiply.c_str(),
    blend_image_Overlay.c_str(),
    blend_image_Difference.c_str(),
    blend_image_Exclusion.c_str(),
    blend_image_SrcOver.c_str(),
    blend_image_Darken.c_str(),
    blend_image_Lighten.c_str(),
    blend_image_ColorDodge.c_str(),
    blend_image_ColorBurn.c_str(),
    blend_image_HardLight.c_str(),
    blend_image_SoftLight.c_str()
};

//************************************************************************
// compute shaders compose
//************************************************************************

std::string cComposeHeader = WG_SHADER_SOURCE(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageMsk : texture_storage_2d<rgba8unorm, read>;
@group(2) @binding(0) var imageDst : texture_storage_2d<rgba8unorm, read>;
@group(3) @binding(0) var imageTgt : texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) id: vec3u) {
    let colorSrc = textureLoad(imageSrc, id.xy);
    let colorMsk = textureLoad(imageMsk, id.xy);
    let colorDst = textureLoad(imageDst, id.xy);

    var Sc : vec3f = colorSrc.rgb;
    var Sa : f32   = colorSrc.a;
    var Mc : vec3f = colorMsk.rgb;
    var Ma : f32   = colorMsk.a;
    var Dc : vec3f = colorDst.rgb;
    var Da : f32   = colorDst.a;
    var Rc : vec3f = colorDst.rgb;
    var Ra : f32   = colorDst.a;
);

std::string cComposeEquation_None = WG_SHADER_SOURCE(
    Rc = Dc;
    Ra = Da;
);
std::string cComposeEquation_ClipPath = WG_SHADER_SOURCE(
    Rc = Sc * Ma + Dc * (1.0 - Sa * Ma);
    Ra = Sa * Ma + Da * (1.0 - Sa * Ma);
);

std::string cComposeEquation_AlphaMask = WG_SHADER_SOURCE(
    Rc = Sc * Ma + Dc * (1.0 - Sa * Ma);
    Ra = Sa * Ma + Da * (1.0 - Sa * Ma);
);
std::string cComposeEquation_InvAlphaMask = WG_SHADER_SOURCE(
    Rc = Sc * (1.0 - Ma) + Dc * (1.0 - Sa * (1.0 - Ma));
    Ra = Sa * (1.0 - Ma) + Da * (1.0 - Sa * (1.0 - Ma));
);
std::string cComposeEquation_LumaMask = WG_SHADER_SOURCE(
    let luma: f32 = dot(Mc, vec3f(0.2125, 0.7154, 0.0721));
    Rc = Sc * luma + Dc * (1.0 - Sa * luma);
    Ra = Sa * luma + Da * (1.0 - Sa * luma);
);
std::string cComposeEquation_InvLumaMask = WG_SHADER_SOURCE(
    let luma: f32 = dot(Mc, vec3f(0.2125, 0.7154, 0.0721));
    Rc = Sc * (1.0 - luma) + Dc * (1.0 - Sa * (1.0 - luma));
    Ra = Sa * (1.0 - luma) + Da * (1.0 - Sa * (1.0 - luma));
);
std::string cComposeEquation_AddMask = WG_SHADER_SOURCE(
    Rc = Sc;
    Ra = Sa + Ma * (1.0 - Sa);
);
std::string cComposeEquation_SubtractMask = WG_SHADER_SOURCE(
    Rc = Sc;
    Ra = Sa * (1.0 - Ma);
);
std::string cComposeEquation_IntersectMask = WG_SHADER_SOURCE(
    Rc = Sc;
    Ra = Sa * Ma;
);
std::string cComposeEquation_DifferenceMask = WG_SHADER_SOURCE(
    Rc = Sc;
    Ra = Sa * (1.0 - Ma) + Ma * (1.0 - Sa);
);
std::string cComposeEquation_LightenMask = WG_SHADER_SOURCE(
    Rc = Sc;
    Ra = select(Ma, Sa, Sa > Ma);
);
std::string cComposeEquation_DarkenMask = WG_SHADER_SOURCE(
    Rc = Sc;
    Ra = select(Sa, Ma, Sa > Ma);
);

std::string cComposeFooter = WG_SHADER_SOURCE(
    textureStore(imageTgt, id.xy, vec4f(Rc, Ra));
}
);

std::string compose_None           = cComposeHeader + cComposeEquation_None           + cComposeFooter;
std::string compose_ClipPath       = cComposeHeader + cComposeEquation_ClipPath       + cComposeFooter;
std::string compose_AlphaMask      = cComposeHeader + cComposeEquation_AlphaMask      + cComposeFooter;
std::string compose_InvAlphaMask   = cComposeHeader + cComposeEquation_InvAlphaMask   + cComposeFooter;
std::string compose_LumaMask       = cComposeHeader + cComposeEquation_LumaMask       + cComposeFooter;
std::string compose_InvLumaMask    = cComposeHeader + cComposeEquation_InvLumaMask    + cComposeFooter;
std::string compose_AddMask        = cComposeHeader + cComposeEquation_AddMask        + cComposeFooter;
std::string compose_SubtractMask   = cComposeHeader + cComposeEquation_SubtractMask   + cComposeFooter;
std::string compose_IntersectMask  = cComposeHeader + cComposeEquation_IntersectMask  + cComposeFooter;
std::string compose_DifferenceMask = cComposeHeader + cComposeEquation_DifferenceMask + cComposeFooter;
std::string compose_LightenMask    = cComposeHeader + cComposeEquation_LightenMask    + cComposeFooter;
std::string compose_DarkenMask     = cComposeHeader + cComposeEquation_DarkenMask     + cComposeFooter;

const char* cShaderSrc_Compose[12] {
    compose_None.c_str(),
    compose_ClipPath.c_str(),
    compose_AlphaMask.c_str(),
    compose_InvAlphaMask.c_str(),
    compose_LumaMask.c_str(),
    compose_InvLumaMask.c_str(),
    compose_AddMask.c_str(),
    compose_SubtractMask.c_str(),
    compose_IntersectMask.c_str(),
    compose_DifferenceMask.c_str(),
    compose_LightenMask.c_str(),
    compose_DarkenMask.c_str()
};

const char* cShaderSrc_Copy = WG_SHADER_SOURCE(
@group(0) @binding(0) var imageSrc : texture_storage_2d<rgba8unorm, read>;
@group(1) @binding(0) var imageDst : texture_storage_2d<bgra8unorm, write>;

@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) id: vec3u) {
    textureStore(imageDst, id.xy, textureLoad(imageSrc, id.xy));
}
);