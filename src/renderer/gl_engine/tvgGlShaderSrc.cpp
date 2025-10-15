/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "tvgGlShaderSrc.h"

//***********************************************************************
// service shaders
//***********************************************************************

const char* STENCIL_VERT_SHADER = R"(
uniform float uDepth;
layout(location = 0) in vec2 aLocation;
layout(std140) uniform Matrix { mat4 transform; } uMatrix;

void main() {
    vec4 pos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);
    pos.z = uDepth;
    gl_Position = pos;
}
)";

const char* STENCIL_FRAG_SHADER = R"(
out vec4 FragColor;

void main() {
    FragColor = vec4(0.0);
}
)";

const char* BLIT_VERT_SHADER = R"(
layout(location = 0) in vec2 aLocation;
layout(location = 1) in vec2 aUV;
out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aLocation, 0.0, 1.0);
}
)";

const char* BLIT_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    FragColor = texture(uSrcTexture, vUV);
}
)";

//***********************************************************************
// fill shaders
//***********************************************************************

const char* COLOR_VERT_SHADER = R"(
uniform float uDepth;
layout(location = 0) in vec2 aLocation;
layout(std140) uniform Matrix { mat4 transform; } uMatrix;

void main()
{
    vec4 pos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);
    pos.z = uDepth;
    gl_Position = pos;
}
)";

const char* COLOR_FRAG_SHADER = R"(
layout(std140) uniform ColorInfo {
    vec4 solidColor;
} uColorInfo;
out vec4 FragColor;

void main()
{
   vec4 uColor = uColorInfo.solidColor;
   FragColor =  vec4(uColor.rgb * uColor.a, uColor.a);
}
)";

const char* GRADIENT_VERT_SHADER = R"(
uniform float uDepth;
layout(location = 0) in vec2 aLocation;
out vec2 vPos;
layout(std140) uniform Matrix { mat4 transform; } uMatrix;
layout(std140) uniform InvMatrix { mat4 transform; } uInvMatrix;

void main()
{
    vec4 glPos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);
    glPos.z = uDepth;
    gl_Position = glPos;
    vec4 pos =  uInvMatrix.transform * vec4(aLocation, 0.0, 1.0);
    vPos = pos.xy / pos.w;
}
)";

const char* STR_GRADIENT_FRAG_COMMON_VARIABLES = R"(
const int MAX_STOP_COUNT = 16;
in vec2 vPos;
)";

const char* STR_GRADIENT_FRAG_COMMON_FUNCTIONS = R"(
float gradientStep(float edge0, float edge1, float x)
{
    // linear
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return x;
}
float gradientStop(int index)
{
    if (index >= MAX_STOP_COUNT) index = MAX_STOP_COUNT - 1;
    int i = index / 4;
    int j = index % 4;
    return uGradientInfo.stopPoints[i][j];
}
float gradientWrap(float d)
{
    int spread = int(uGradientInfo.nStops[2]);
    if (spread == 0) return clamp(d, 0.0, 1.0);
    if (spread == 1) { /* Reflect */
        float n = mod(d, 2.0);
        if (n > 1.0) {
            n = 2.0 - n;
        }
        return n;
    }
    if (spread == 2) {  /* Repeat */
        float n = mod(d, 1.0);
        if (n < 0.0) {
            n += 1.0 + n;
        }
        return n;
    }
}
vec4 gradient(float t, float d, float l)
{
    float dist = d * 2.0 / l;
    vec4 col = vec4(0.0);
    int i = 0;
    int count = int(uGradientInfo.nStops[0]);
    if (t <= gradientStop(0)) {
        col = uGradientInfo.stopColors[0];
    } else if (t >= gradientStop(count - 1)) {
        col = uGradientInfo.stopColors[count - 1];
        if (int(uGradientInfo.nStops[2]) == 2 && (1.0 - t) < dist) {
            float dd = (1.0 - t) / dist;
            float alpha =  dd;
            col *= alpha;
            col += uGradientInfo.stopColors[0] * (1. - alpha);
        }
    } else {
        for (i = 0; i < count - 1; ++i) {
            float stopi = gradientStop(i);
            float stopi1 = gradientStop(i + 1);
            if (t >= stopi && t <= stopi1) {
                col = (uGradientInfo.stopColors[i] * (1. - gradientStep(stopi, stopi1, t)));
                col += (uGradientInfo.stopColors[i + 1] * gradientStep(stopi, stopi1, t));
                if (int(uGradientInfo.nStops[2]) == 2 && abs(d) > dist) {
                    if (i == 0 && (t - stopi) < dist) {
                        float dd = (t - stopi) / dist;
                        float alpha = dd;
                        col *= alpha;
                        vec4 nc = uGradientInfo.stopColors[0] * (1.0 - (t - stopi));
                        nc += uGradientInfo.stopColors[count - 1] * (t - stopi);
                        col += nc * (1.0 - alpha);
                    } else if (i == count - 2 && (1.0 - t) < dist) {
                        float dd = (1.0 - t) / dist;
                        float alpha =  dd;
                        col *= alpha;
                        col += (uGradientInfo.stopColors[0]) * (1.0 - alpha);
                    }
                }
                break;
            }
        }
    }
    return col;
}

vec3 ScreenSpaceDither(vec2 vScreenPos)
{
    vec3 vDither = vec3(dot(vec2(171.0, 231.0), vScreenPos.xy));
    vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0));
    return vDither.rgb / 255.0;
}
)";

const char* STR_LINEAR_GRADIENT_VARIABLES = R"(
layout(std140) uniform GradientInfo {
    vec4  nStops;
    vec2  gradStartPos;
    vec2  gradEndPos;
    vec4  stopPoints[MAX_STOP_COUNT / 4];
    vec4  stopColors[MAX_STOP_COUNT];
} uGradientInfo;
)";

const char* STR_LINEAR_GRADIENT_MAIN = R"(
out vec4 FragColor;
void main()
{
    vec2 pos = vPos;
    vec2 st = uGradientInfo.gradStartPos;
    vec2 ed = uGradientInfo.gradEndPos;
    vec2 ba = ed - st;
    float d = dot(pos - st, ba) / dot(ba, ba);
    float t = gradientWrap(d);
    vec4 color = gradient(t, d, length(pos - st));
    FragColor =  vec4(color.rgb * color.a, color.a);
}
)";

const char* STR_RADIAL_GRADIENT_VARIABLES = R"(
layout(std140) uniform GradientInfo {
    vec4  nStops;
    vec4  centerPos;
    vec2  radius;
    vec4  stopPoints[MAX_STOP_COUNT / 4];
    vec4  stopColors[MAX_STOP_COUNT];
} uGradientInfo;
)";

const char* STR_RADIAL_GRADIENT_MAIN = R"(
out vec4 FragColor;

mat3 radial_matrix(vec2 p0, vec2 p1)
{
    mat3 a = mat3(0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    mat3 b = mat3(p1.y - p0.y, p0.x - p1.x, 0.0, p1.x - p0.x, p1.y - p0.y, 0.0, p0.x, p0.y, 1.0);
    return a * inverse(b);
}

vec2 compute_radial_t(vec2 c0, float r0, vec2 c1, float r1, vec2 pos)
{ 
    const float scalar_nearly_zero = 1.0 / float(1 << 12);
    float d_center = distance(c0, c1);
    float d_radius = r1 - r0;
    bool radial = d_center < scalar_nearly_zero;
    bool strip = abs(d_radius) < scalar_nearly_zero;

    if (radial) { 
        if (strip) return vec2(0.0, -1.0);

        float scale = 1.0 / d_radius;
        float scale_sign = sign(d_radius);
        float bias = r0 / d_radius;
        vec2 pt = (pos - c0) * scale;
        float t = length(pt) * scale_sign - bias;
        return vec2(t, 1.0);
    } else if (strip) {
        mat3 transform = radial_matrix(c0, c1);
        float r = r0 / d_center;
        float r_2 = r * r;
        vec2 pt = (transform * vec3(pos.xy, 1.0)).xy;
        float t = r_2 - pt.y * pt.y;

        if (t < 0.0) return vec2(0.0, -1.0);

        t = pt.x + sqrt(t);
        return vec2(t, 1.0);
    } else { 
        float f = r0 / (r0 - r1);
        bool is_swapped = abs(f - 1.0) < scalar_nearly_zero;
        if (is_swapped) {
            vec2 c = c0;
            c0 = c1;
            c1 = c;
            f = 0.0;
        }
        vec2 cf = c0 * (1.0 - f) + c1 * f;
        mat3 transform = radial_matrix(cf, c1);

        float scale_x = abs(1.0 - f);
        float scale_y = scale_x;
        float r1 = abs(r1 - r0) / d_center;
        bool is_focal_on_circle = abs(r1 - 1.0) < scalar_nearly_zero;
        if (is_focal_on_circle) {
            scale_x *= 0.5;
            scale_y *= 0.5;
        } else {
            scale_x *= r1 / (r1 * r1 - 1.0);
            scale_y /= sqrt(abs(r1 * r1 - 1.0));
        } 
        transform = mat3(scale_x, 0.0, 0.0, 0.0, scale_y, 0.0, 0.0, 0.0, 1.0) * transform;

        vec2 pt = (transform * vec3(pos.xy, 1.0)).xy;

        float inv_r1 = 1.0 / r1;
        float d_radius_sign = sign(1.0 - f);
        bool is_well_behaved = !is_focal_on_circle && r1 > 1.0;

        float x_t = -1.0;
        if (is_focal_on_circle) x_t = dot(pt, pt) / pt.x;
        else if (is_well_behaved) x_t = length(pt) - pt.x * inv_r1;
        else {
            float temp = pt.x * pt.x - pt.y * pt.y;
            if (temp >= 0.0) {
                if (is_swapped || d_radius_sign < 0.0) {
                    x_t = -sqrt(temp) - pt.x * inv_r1;
                } else {
                    x_t = sqrt(temp) - pt.x * inv_r1;
                }
            }
        }

        if (!is_well_behaved && x_t < 0.0) return vec2(0.0, -1.0);

        float t = f + d_radius_sign * x_t;
        if (is_swapped) t = 1.0 - t;
        return vec2(t, 1.0);
    }
}

void main()
{
    vec2 pos = vPos; 
    vec2 res = compute_radial_t(uGradientInfo.centerPos.xy,
                                uGradientInfo.radius.x,
                                uGradientInfo.centerPos.zw,
                                uGradientInfo.radius.y,
                                pos);
    if (res.y < 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    } 
    float t = gradientWrap(res.x);
    vec4 color = gradient(t, res.x, length(pos - uGradientInfo.centerPos.xy));
    FragColor =  vec4(color.rgb * color.a, color.a);
}
)";

const char* IMAGE_VERT_SHADER = R"(
uniform float uDepth;
layout (location = 0) in vec2 aLocation;
layout (location = 1) in vec2 aUV;
layout (std140) uniform Matrix { mat4 transform; } uMatrix;
out vec2 vUV;
 
void main()
{
    vUV = aUV;
    vec4 pos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);
    pos.z = uDepth;
    gl_Position = pos;
}
)";

const char* IMAGE_FRAG_SHADER = R"(
layout(std140) uniform ColorInfo {
    int format;
    int flipY;
    int opacity;
    int dummy;
} uColorInfo;
uniform sampler2D uTexture;
in vec2 vUV;
out vec4 FragColor;

void main()
{
    vec2 uv = vUV;
    if (uColorInfo.flipY == 1) { uv.y = 1.0 - uv.y; }
    vec4 color = texture(uTexture, uv);
    vec4 result;
    if (uColorInfo.format == 0) { /* FMT_ABGR8888 */
        result = color;
    } else if (uColorInfo.format == 1) { /* FMT_ARGB8888 */
        result = color.bgra;
    } else if (uColorInfo.format == 2) { /* FMT_ABGR8888S */
        result = vec4(color.rgb * color.a, color.a);
    } else if (uColorInfo.format == 3) { /* FMT_ARGB8888S */
        result = vec4(color.bgr * color.a, color.a);
    }
    FragColor = result * float(uColorInfo.opacity) / 255.0;
}
)";

//***********************************************************************
// composition shaders
//***********************************************************************

const char* MASK_VERT_SHADER = R"(
uniform float uDepth;
layout(location = 0) in vec2 aLocation;
layout(location = 1) in vec2 aUV;
out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = vec4(aLocation, uDepth, 1.0);
}
)";

const char* MASK_ALPHA_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    FragColor = srcColor * maskColor.a;
}
)";

const char* MASK_INV_ALPHA_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    FragColor = srcColor *(1.0 - maskColor.a);
}
)";

const char* MASK_LUMA_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);

    if (maskColor.a > 0.000001) {
        maskColor = vec4(maskColor.rgb / maskColor.a, maskColor.a);
    }

    FragColor = srcColor * dot(maskColor.rgb, vec3(0.2125, 0.7154, 0.0721)) * maskColor.a;
}
)";

const char* MASK_INV_LUMA_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    float luma = dot(maskColor.rgb, vec3(0.2125, 0.7154, 0.0721));
    FragColor = srcColor * (1.0 - luma);
}
)";

const char* MASK_ADD_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    vec4 color = srcColor + maskColor * (1.0 - srcColor.a);
    FragColor = min(color, vec4(1.0, 1.0, 1.0, 1.0)) ;
}
)";

const char* MASK_SUB_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    float a = srcColor.a - maskColor.a;

    if (a < 0.0 || srcColor.a == 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    } else {
        vec3 srcRgb = srcColor.rgb / srcColor.a;
        FragColor = vec4(srcRgb * a, a);
    }
}
)";

const char* MASK_INTERSECT_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    FragColor = maskColor * srcColor.a;
}
)";

const char* MASK_DIFF_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    float da = srcColor.a - maskColor.a;
    if (da == 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    } else if (da > 0.0) {
        FragColor = srcColor * da;
    } else {
        FragColor = maskColor * (-da);
    }
}
)";

const char* MASK_DARKEN_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    if (srcColor.a > 0.0) srcColor.rgb /= srcColor.a;
    float alpha = min(srcColor.a, maskColor.a);
    FragColor = vec4(srcColor.rgb * alpha, alpha);
}
)";

const char* MASK_LIGHTEN_FRAG_SHADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uMaskTexture;
in vec2 vUV;
out vec4 FragColor;

void main() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 maskColor = texture(uMaskTexture, vUV);
    if (srcColor.a > 0.0) srcColor.rgb /= srcColor.a;
    float alpha = max(srcColor.a, maskColor.a);
    FragColor = vec4(srcColor.rgb * alpha, alpha);
}
)";

//***********************************************************************
// blending shaders
//***********************************************************************

const char* BLEND_SOLID_FRAG_HEADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uDstTexture;

in vec2 vUV;
out vec4 FragColor;

vec3 One = vec3(1.0, 1.0, 1.0);
struct FragData { vec3 Sc; float Sa; float So; vec3 Dc; float Da; };
FragData d;

void getFragData() {
    // get source data
    vec4 colorSrc = texture(uSrcTexture, vUV);
    vec4 colorDst = texture(uDstTexture, vUV);
    // fill fragment data
    d.Sc = colorSrc.rgb;
    d.Sa = colorSrc.a;
    d.So = 1.0;
    d.Dc = colorDst.rgb;
    d.Da = colorDst.a;
}

vec4 postProcess(vec4 R) { return R; }
)";

const char* BLEND_GRADIENT_FRAG_HEADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uDstTexture;

in vec2 vUV;
out vec4 FragColor;

vec3 One = vec3(1.0, 1.0, 1.0);
struct FragData { vec3 Sc; float Sa; float So; vec3 Dc; float Da; };
FragData d;

void getFragData() {
    // get source data
    vec4 colorSrc = texture(uSrcTexture, vUV);
    vec4 colorDst = texture(uDstTexture, vUV);
    // fill fragment data
    d.Sc = colorSrc.rgb;
    d.Sa = colorSrc.a;
    d.So = 1.0;
    d.Dc = colorDst.rgb;
    d.Da = colorDst.a;
    if (d.Sa > 0.0) {d.Sc = d.Sc / d.Sa; }
    d.Sc = mix(d.Dc, d.Sc, d.Sa * d.So);
    d.Sa = mix(d.Da,  1.0, d.Sa * d.So);
}

vec4 postProcess(vec4 R) { return R; }
)";

const char* BLEND_IMAGE_FRAG_HEADER = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uDstTexture;

in vec2 vUV;
out vec4 FragColor;

vec3 One = vec3(1.0, 1.0, 1.0);
struct FragData { vec3 Sc; float Sa; float So; vec3 Dc; float Da; };
FragData d;

void getFragData() {
    // get source data
    vec4 colorSrc = texture(uSrcTexture, vUV);
    vec4 colorDst = texture(uDstTexture, vUV);
    // fill fragment data
    d.Sc = colorSrc.rgb;
    d.Sa = colorSrc.a;
    d.So = 1.0;
    d.Dc = colorDst.rgb;
    d.Da = colorDst.a;
    if (d.Sa > 0.0) { d.Sc = d.Sc / d.Sa; }
}

vec4 postProcess(vec4 R) { return R; }
)";

const char* BLEND_SCENE_FRAG_HEADER = R"(
layout(std140) uniform ColorInfo {
    int format;
    int flipY;
    int opacity;
    int dummy;
} uColorInfo;
uniform sampler2D uSrcTexture;
uniform sampler2D uDstTexture;

in vec2 vUV;
out vec4 FragColor;

vec3 One = vec3(1.0, 1.0, 1.0);
struct FragData { vec3 Sc; float Sa; float So; vec3 Dc; float Da; };
FragData d;

void getFragData() {
    // get source data
    vec4 colorSrc = texture(uSrcTexture, vUV);
    vec4 colorDst = texture(uDstTexture, vUV);
    // fill fragment data
    d.Sc = colorSrc.rgb;
    d.Sa = colorSrc.a;
    d.So = float(uColorInfo.opacity) / 255.0;
    d.Dc = colorDst.rgb;
    d.Da = colorDst.a;
    if (d.Sa > 0.0) {d.Sc = d.Sc / d.Sa; }
}

vec4 postProcess(vec4 R) { return mix(vec4(d.Dc, d.Da), R, d.Sa * d.So); }
)";

const char* BLEND_FRAG_HSL = R"(
// RGB to HSL conversion
vec3 rgbToHsl(vec3 color) {
    float minVal = min(color.r, min(color.g, color.b));
    float maxVal = max(color.r, max(color.g, color.b));
    float delta = maxVal - minVal;

    float h = 0.0;
    if (delta > 0.0) {
             if (maxVal == color.r) { h = (color.g - color.b) / delta - trunc(h / 6.0) * 6.0; }
        else if (maxVal == color.g) { h = (color.b - color.r) / delta + 2.0;
        } else                      { h = (color.r - color.g) / delta + 4.0; }
        h = h * 60.0;
        if (h < 0.0) { h += 360.0; }
    }

    float l = (maxVal + minVal) * 0.5;
    float s = delta > 0.0 ? delta / (1.0 - abs(2.0 * l - 1.0)) : 0.0;
    
    return vec3(h, s, l);
}

// HSL to RGB conversion
vec3 hslToRgb(vec3 color) {
    float h = color.x;
    float s = color.y;
    float l = color.z;

    float C = (1.0 - abs(2.0 * l - 1.0)) * s;
    float h_prime = h / 60.0;
    float X = C * (1.0 - abs(h_prime - 2.0 * trunc(h_prime / 2.0) - 1.0));
    float m = l - C / 2.0;

    vec3 rgb = vec3(0.0);
         if (h_prime >= 0.0 && h_prime < 1.0) { rgb = vec3(C, X, 0.0); }
    else if (h_prime >= 1.0 && h_prime < 2.0) { rgb = vec3(X, C, 0.0); }
    else if (h_prime >= 2.0 && h_prime < 3.0) { rgb = vec3(0.0, C, X); }
    else if (h_prime >= 3.0 && h_prime < 4.0) { rgb = vec3(0.0, X, C); }
    else if (h_prime >= 4.0 && h_prime < 5.0) { rgb = vec3(X, 0.0, C); }
    else                                      { rgb = vec3(C, 0.0, X); }

    return rgb + vec3(m);
}
)";

const char* NORMAL_BLEND_FRAG = R"(
void main()
{
    FragColor = texture(uSrcTexture, vUV);
}
)";

const char* MULTIPLY_BLEND_FRAG = R"(
void main()
{
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        Rc = d.Sc * min(One, d.Dc / d.Da);
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* SCREEN_BLEND_FRAG = R"(
void main()
{
    getFragData();
    vec3 Rc = d.Sc + d.Dc - d.Sc * d.Dc;
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* OVERLAY_BLEND_FRAG = R"(
void main()
{
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        Rc.r = Dc.r < 0.5 ? min(1.0, 2.0 * d.Sc.r * Dc.r) : 1.0 - min(1.0, 2.0 * (1.0 - d.Sc.r) * (1.0 - Dc.r));
        Rc.g = Dc.g < 0.5 ? min(1.0, 2.0 * d.Sc.g * Dc.g) : 1.0 - min(1.0, 2.0 * (1.0 - d.Sc.g) * (1.0 - Dc.g));
        Rc.b = Dc.b < 0.5 ? min(1.0, 2.0 * d.Sc.b * Dc.b) : 1.0 - min(1.0, 2.0 * (1.0 - d.Sc.b) * (1.0 - Dc.b));
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* DARKEN_BLEND_FRAG = R"(
void main()
{
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        Rc = min(d.Sc, min(One, d.Dc / d.Da));
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* LIGHTEN_BLEND_FRAG = R"(
void main()
{
    getFragData();
    vec3 Rc = max(d.Sc, d.Dc);
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* COLOR_DODGE_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        Rc.r = Dc.r > 0.0 ? d.Sc.r < 1.0 ? min(1.0, Dc.r / (1.0 - d.Sc.r)) : 1.0 : 0.0;
        Rc.g = Dc.g > 0.0 ? d.Sc.g < 1.0 ? min(1.0, Dc.g / (1.0 - d.Sc.g)) : 1.0 : 0.0;
        Rc.b = Dc.b > 0.0 ? d.Sc.b < 1.0 ? min(1.0, Dc.b / (1.0 - d.Sc.b)) : 1.0 : 0.0;
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* COLOR_BURN_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        Rc.r = d.Sc.r > 0.0 ? 1.0 - min(1.0, (1.0 - Dc.r) / d.Sc.r) : Dc.r < 1.0 ? 0.0 : 1.0;
        Rc.g = d.Sc.g > 0.0 ? 1.0 - min(1.0, (1.0 - Dc.g) / d.Sc.g) : Dc.g < 1.0 ? 0.0 : 1.0;
        Rc.b = d.Sc.b > 0.0 ? 1.0 - min(1.0, (1.0 - Dc.b) / d.Sc.b) : Dc.b < 1.0 ? 0.0 : 1.0;
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* HARD_LIGHT_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        Rc.r = d.Sc.r < 0.5 ? min(1.0, 2.0 * d.Sc.r * Dc.r) : 1.0 - min(1.0, 2.0 * (1.0 - d.Sc.r) * (1.0 - Dc.r));
        Rc.g = d.Sc.g < 0.5 ? min(1.0, 2.0 * d.Sc.g * Dc.g) : 1.0 - min(1.0, 2.0 * (1.0 - d.Sc.g) * (1.0 - Dc.g));
        Rc.b = d.Sc.b < 0.5 ? min(1.0, 2.0 * d.Sc.b * Dc.b) : 1.0 - min(1.0, 2.0 * (1.0 - d.Sc.b) * (1.0 - Dc.b));
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* SOFT_LIGHT_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        Rc = min(One, (One - 2.0 * d.Sc) * Dc * Dc + 2.0 * d.Sc * Dc);
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* DIFFERENCE_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = abs(d.Dc - d.Sc);
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* EXCLUSION_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc + d.Dc - 2.0 * d.Sc * d.Dc;
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* HUE_BLEND_FRAG = R"(
void main()
{
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        vec3 Sc = d.Sc;

        vec3 Shsl = rgbToHsl(Sc);
        vec3 Dhsl = rgbToHsl(Dc);
        Rc = hslToRgb(vec3(Shsl.r, Dhsl.g, Dhsl.b)); // sh, ds, dl
        
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* SATURATION_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        vec3 Sc = d.Sc;

        vec3 Shsl = rgbToHsl(Sc);
        vec3 Dhsl = rgbToHsl(Dc);
        Rc = hslToRgb(vec3(Dhsl.r, Shsl.g, Dhsl.b)); // dh, ss, dl
        
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* COLOR_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        vec3 Sc = d.Sc;

        vec3 Shsl = rgbToHsl(Sc);
        vec3 Dhsl = rgbToHsl(Dc);
        Rc = hslToRgb(vec3(Shsl.r, Shsl.g, Dhsl.b)); // sh, ss, dl
        
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* LUMINOSITY_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = d.Sc;
    if (d.Da > 0.0) {
        vec3 Dc = min(One, d.Dc / d.Da);
        vec3 Sc = d.Sc;

        vec3 Shsl = rgbToHsl(Sc);
        vec3 Dhsl = rgbToHsl(Dc);
        Rc = hslToRgb(vec3(Dhsl.r, Dhsl.g, Shsl.b)); // dh, ds, sl
        
        Rc = mix(d.Sc, Rc, d.Da);
    }
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

const char* ADD_BLEND_FRAG = R"(
void main() {
    getFragData();
    vec3 Rc = min(One, d.Sc + d.Dc);
    FragColor = postProcess(vec4(Rc, 1.0));
}
)";

//***********************************************************************
// effect shaders
//***********************************************************************

const char* EFFECT_VERTEX = R"(
layout(location = 0) in vec2 aLocation;
out vec2 vUV;

void main()
{
    vUV = aLocation * 0.5 + 0.5;
    gl_Position = vec4(aLocation, 0.0, 1.0);
}
)";

const char* GAUSSIAN_VERTICAL = R"(
uniform sampler2D uSrcTexture;
layout(std140) uniform Gaussian {
    float sigma;
    float scale;
    float extend;
    float dummy0;
} uGaussian;

layout(std140) uniform Viewport {
    vec4 vp;
} uViewport;

in vec2 vUV;
out vec4 FragColor;

float gaussian(float x, float sigma) {
    float exponent = -x * x / (2.0 * sigma * sigma);
    return exp(exponent) / (sqrt(2.0 * 3.141592) * sigma);
}

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uSrcTexture, 0));
    vec4 colorSum = vec4(0.0);
    float sigma = uGaussian.sigma * uGaussian.scale;
    float weightSum = 0.0;
    int radius = int(uGaussian.extend);
    
    for (int y = -radius; y <= radius; ++y) {
        vec2 offset = vec2(0.0, float(y) * texelSize.y);
        vec2 coord = vUV + offset;
        float pixCoord = uViewport.vp.y - coord.y / texelSize.y;
        float weight = pixCoord < uViewport.vp.w ? gaussian(float(y), sigma) : 0.0;
        colorSum += texture(uSrcTexture, coord) * weight;
        weightSum += weight;
    }
    
    FragColor = colorSum / weightSum;
} 
)";

const char* GAUSSIAN_HORIZONTAL = R"(
uniform sampler2D uSrcTexture;
layout(std140) uniform Gaussian {
    float sigma;
    float scale;
    float extend;
    float dummy0;
} uGaussian;

layout(std140) uniform Viewport {
    vec4 vp;
} uViewport;

in vec2 vUV;
out vec4 FragColor;

float gaussian(float x, float sigma) {
    float exponent = -x * x / (2.0 * sigma * sigma);
    return exp(exponent) / (sqrt(2.0 * 3.141592) * sigma);
}

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uSrcTexture, 0));
    vec4 colorSum = vec4(0.0);
    float sigma = uGaussian.sigma * uGaussian.scale;
    float weightSum = 0.0;
    int radius = int(uGaussian.extend);
    
    for (int y = -radius; y <= radius; ++y) {
        vec2 offset = vec2(float(y) * texelSize.x, 0.0);
        vec2 coord = vUV + offset;
        float pixCoord = uViewport.vp.x + coord.x / texelSize.x;
        float weight = pixCoord < uViewport.vp.z ? gaussian(float(y), sigma) : 0.0;
        colorSum += texture(uSrcTexture, coord) * weight;
        weightSum += weight;
    }
    
    FragColor = colorSum / weightSum;
} 
)";

const char* EFFECT_DROPSHADOW = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uBlrTexture;
layout(std140) uniform DropShadow {
    float sigma;
    float scale;
    float extend;
    float dummy0;
    vec4 color;
    vec2 offset;
} uDropShadow;

in vec2 vUV;
out vec4 FragColor;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uSrcTexture, 0));
    vec2 offset = uDropShadow.offset * texelSize;
    vec4 orig = texture(uSrcTexture, vUV);
    vec4 blur = texture(uBlrTexture, vUV + offset);
    vec4 shad = uDropShadow.color * blur.a;
    FragColor = orig + shad * (1.0 - orig.a);
} 
)";

const char* EFFECT_FILL = R"(
uniform sampler2D uSrcTexture;
layout(std140) uniform Params {
    vec4 params[3];
} uParams;

in vec2 vUV;
out vec4 FragColor;

void main()
{
    vec4 orig = texture(uSrcTexture, vUV);
    vec4 fill = uParams.params[0];
    FragColor = fill * orig.a * fill.a;
} 
)";

const char* EFFECT_TINT = R"(
uniform sampler2D uSrcTexture;
layout(std140) uniform Params {
    vec4 params[3];
} uParams;

in vec2 vUV;
out vec4 FragColor;

void main()
{
    vec4 orig = texture(uSrcTexture, vUV);
    float luma = dot(orig.rgb, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(mix(orig.rgb, mix(uParams.params[0].rgb, uParams.params[1].rgb, luma), uParams.params[2].r) * orig.a, orig.a);
} 
)";

const char* EFFECT_TRITONE = R"(
uniform sampler2D uSrcTexture;
layout(std140) uniform Params {
    vec4 params[3];
} uParams;

in vec2 vUV;
out vec4 FragColor;

void main()
{
    vec4 orig = texture(uSrcTexture, vUV);
    float luma = dot(orig.rgb, vec3(0.2126, 0.7152, 0.0722));
    bool isBright = luma >= 0.5f;
    float t = isBright ? (luma - 0.5f) * 2.0f : luma * 2.0f;
    vec3 from = isBright ? uParams.params[1].rgb : uParams.params[0].rgb;
    vec3 to = isBright ? uParams.params[2].rgb : uParams.params[1].rgb;
    vec4 tmp = vec4(mix(from, to, t), 1.0f);

    if (uParams.params[2].a > 0.0f) tmp = mix(tmp, orig, uParams.params[2].a);
    FragColor = tmp * orig.a;
} 
)";