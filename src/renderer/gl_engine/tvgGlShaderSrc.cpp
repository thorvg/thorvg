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

#define TVG_COMPOSE_SHADER(shader) #shader

const char* COLOR_VERT_SHADER = TVG_COMPOSE_SHADER(
    uniform float uDepth;                                           \n
    layout(location = 0) in vec2 aLocation;                         \n
    layout(std140) uniform Matrix {                                 \n
        mat4 transform;                                             \n
    } uMatrix;                                                      \n
                                                                    \n
    void main()                                                     \n
    {                                                               \n
        vec4 pos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);   \n
        pos.z = uDepth;                                             \n
        gl_Position = pos;                                          \n
    }                                                               \n
);

const char* COLOR_FRAG_SHADER = TVG_COMPOSE_SHADER(
    layout(std140) uniform ColorInfo {                       \n
        vec4 solidColor;                                     \n
    } uColorInfo;                                            \n
    out vec4 FragColor;                                      \n
                                                             \n
    void main()                                              \n
    {                                                        \n
       vec4 uColor = uColorInfo.solidColor;                  \n
       FragColor =  vec4(uColor.rgb * uColor.a, uColor.a);   \n
    }                                                        \n
);

const char* GRADIENT_VERT_SHADER = TVG_COMPOSE_SHADER(
    uniform float uDepth;                                                           \n
    layout(location = 0) in vec2 aLocation;                                         \n
    out vec2 vPos;                                                                  \n
    layout(std140) uniform Matrix {                                                 \n
        mat4 transform;                                                             \n
    } uMatrix;                                                                      \n
    layout(std140) uniform InvMatrix {                                              \n
        mat4 transform;                                                             \n
    } uInvMatrix;                                                                   \n
                                                                                    \n
    void main()                                                                     \n
    {                                                                               \n
        vec4 glPos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);                 \n
        glPos.z = uDepth;                                                           \n
        gl_Position = glPos;                                                        \n
        vec4 pos =  uInvMatrix.transform * vec4(aLocation, 0.0, 1.0);               \n
        vPos = pos.xy / pos.w;                                                      \n
    }                                                                               \n
);


//See: GlRenderer::initShaders()
const char* STR_GRADIENT_FRAG_COMMON_VARIABLES = TVG_COMPOSE_SHADER(
    const int MAX_STOP_COUNT = 16;                                                                          \n
    in vec2 vPos;                                                                                           \n
);

//See: GlRenderer::initShaders()
const char* STR_GRADIENT_FRAG_COMMON_FUNCTIONS = TVG_COMPOSE_SHADER(
    float gradientStep(float edge0, float edge1, float x)                                                   \n
    {                                                                                                       \n
        // linear                                                                                           \n
        x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);                                                 \n
        return x;                                                                                           \n
    }                                                                                                       \n
                                                                                                            \n
    float gradientStop(int index)                                                                           \n
    {                                                                                                       \n
        if (index >= MAX_STOP_COUNT) index = MAX_STOP_COUNT - 1;                                            \n
        int i = index / 4;                                                                                  \n
        int j = index % 4;                                                                                  \n
        return uGradientInfo.stopPoints[i][j];                                                              \n
    }                                                                                                       \n
                                                                                                            \n
    float gradientWrap(float d)                                                                             \n
    {                                                                                                       \n
        int spread = int(uGradientInfo.nStops[2]);                                                          \n
        if (spread == 0) return clamp(d, 0.0, 1.0);                                                         \n
                                                                                                            \n
        if (spread == 1) { /* Reflect */                                                                    \n
            float n = mod(d, 2.0);                                                                          \n
            if (n > 1.0) {                                                                                  \n
                n = 2.0 - n;                                                                                \n
            }                                                                                               \n
            return n;                                                                                       \n
        }                                                                                                   \n
        if (spread == 2) {  /* Repeat */                                                                    \n
            float n = mod(d, 1.0);                                                                          \n
            if (n < 0.0) {                                                                                  \n
                n += 1.0 + n;                                                                               \n
            }                                                                                               \n
            return n;                                                                                       \n
        }                                                                                                   \n
    }                                                                                                       \n
                                                                                                            \n
    vec4 gradient(float t, float d, float l)                                                                \n
    {                                                                                                       \n
        float dist = d * 2.0 / l;                                                                           \n
        vec4 col = vec4(0.0);                                                                               \n
        int i = 0;                                                                                          \n
        int count = int(uGradientInfo.nStops[0]);                                                           \n
        if (t <= gradientStop(0)) {                                                                         \n
            col = uGradientInfo.stopColors[0];                                                              \n
        } else if (t >= gradientStop(count - 1)) {                                                          \n
            col = uGradientInfo.stopColors[count - 1];                                                      \n
            if (int(uGradientInfo.nStops[2]) == 2 && (1.0 - t) < dist) {                                    \n
                float dd = (1.0 - t) / dist;                                                                \n
                float alpha =  dd;                                                                          \n
                col *= alpha;                                                                               \n
                col += uGradientInfo.stopColors[0] * (1. - alpha);                                          \n
            }                                                                                               \n
        } else {                                                                                            \n
            for (i = 0; i < count - 1; ++i) {                                                               \n
                float stopi = gradientStop(i);                                                              \n
                float stopi1 = gradientStop(i + 1);                                                         \n
                if (t >= stopi && t <= stopi1) {                                                            \n
                    col = (uGradientInfo.stopColors[i] * (1. - gradientStep(stopi, stopi1, t)));            \n
                    col += (uGradientInfo.stopColors[i + 1] * gradientStep(stopi, stopi1, t));              \n
                    if (int(uGradientInfo.nStops[2]) == 2 && abs(d) > dist) {                               \n
                        if (i == 0 && (t - stopi) < dist) {                                                 \n
                            float dd = (t - stopi) / dist;                                                  \n
                            float alpha = dd;                                                               \n
                            col *= alpha;                                                                   \n
                            vec4 nc = uGradientInfo.stopColors[0] * (1.0 - (t - stopi));                    \n
                            nc += uGradientInfo.stopColors[count - 1] * (t - stopi);                        \n
                            col += nc * (1.0 - alpha);                                                      \n
                        } else if (i == count - 2 && (1.0 - t) < dist) {                                    \n
                            float dd = (1.0 - t) / dist;                                                    \n
                            float alpha =  dd;                                                              \n
                            col *= alpha;                                                                   \n
                            col += (uGradientInfo.stopColors[0]) * (1.0 - alpha);                           \n
                        }                                                                                   \n
                    }                                                                                       \n
                    break;                                                                                  \n
                }                                                                                           \n
            }                                                                                               \n
        }                                                                                                   \n
        return col;                                                                                         \n
    }                                                                                                       \n
                                                                                                            \n
    vec3 ScreenSpaceDither(vec2 vScreenPos)                                                                 \n
    {                                                                                                       \n
        vec3 vDither = vec3(dot(vec2(171.0, 231.0), vScreenPos.xy));                                        \n
        vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0));                                         \n
        return vDither.rgb / 255.0;                                                                         \n
    }                                                                                                       \n
);

//See: GlRenderer::initShaders()
const char* STR_LINEAR_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
    layout(std140) uniform GradientInfo {                                                                   \n
        vec4  nStops;                                                                                       \n
        vec2  gradStartPos;                                                                                 \n
        vec2  gradEndPos;                                                                                   \n
        vec4  stopPoints[MAX_STOP_COUNT / 4];                                                               \n
        vec4  stopColors[MAX_STOP_COUNT];                                                                   \n
    } uGradientInfo;                                                                                        \n
);

//See: GlRenderer::initShaders()
const char* STR_LINEAR_GRADIENT_MAIN = TVG_COMPOSE_SHADER(
    out vec4 FragColor;                                                                                     \n
    void main()                                                                                             \n
    {                                                                                                       \n
        vec2 pos = vPos;                                                                                    \n
        vec2 st = uGradientInfo.gradStartPos;                                                               \n
        vec2 ed = uGradientInfo.gradEndPos;                                                                 \n
        vec2 ba = ed - st;                                                                                  \n
        float d = dot(pos - st, ba) / dot(ba, ba);                                                          \n
        float t = gradientWrap(d);                                                                          \n
        vec4 color = gradient(t, d, length(pos - st));                                                      \n
        FragColor =  vec4(color.rgb * color.a, color.a);                                                    \n
    }                                                                                                       \n
);

//See: GlRenderer::initShaders()
const char* STR_RADIAL_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
    layout(std140) uniform GradientInfo {                                                                   \n
        vec4  nStops;                                                                                       \n
        vec4  centerPos;                                                                                    \n
        vec2  radius;                                                                                       \n
        vec4  stopPoints[MAX_STOP_COUNT / 4];                                                               \n
        vec4  stopColors[MAX_STOP_COUNT];                                                                   \n
    } uGradientInfo ;                                                                                       \n
);

//See: GlRenderer::initShaders()
const char* STR_RADIAL_GRADIENT_MAIN = TVG_COMPOSE_SHADER(
    out vec4 FragColor;                                                                                     \n
                                                                                                            \n
    mat3 radial_matrix(vec2 p0, vec2 p1)                                                                    \n
    {                                                                                                       \n
        mat3 a = mat3(0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);                                        \n
        mat3 b = mat3(p1.y - p0.y, p0.x - p1.x, 0.0, p1.x - p0.x, p1.y - p0.y, 0.0, p0.x, p0.y, 1.0);       \n
        return a * inverse(b);                                                                              \n
    }                                                                                                       \n
                                                                                                            \n
    vec2 compute_radial_t(vec2 c0, float r0, vec2 c1, float r1, vec2 pos)                                   \n
    {                                                                                                       \n
        const float scalar_nearly_zero = 1.0 / float(1 << 12);                                              \n
        float d_center = distance(c0, c1);                                                                  \n
        float d_radius = r1 - r0;                                                                           \n
        bool radial = d_center < scalar_nearly_zero;                                                        \n
        bool strip = abs(d_radius) < scalar_nearly_zero;                                                    \n
                                                                                                            \n
        if (radial) {                                                                                       \n
            if (strip) return vec2(0.0, -1.0);                                                              \n
                                                                                                            \n
            float scale = 1.0 / d_radius;                                                                   \n
            float scale_sign = sign(d_radius);                                                              \n
            float bias = r0 / d_radius;                                                                     \n
            vec2 pt = (pos - c0) * scale;                                                                   \n
            float t = length(pt) * scale_sign - bias;                                                       \n
            return vec2(t, 1.0);                                                                            \n
        } else if (strip) {                                                                                 \n
            mat3 transform = radial_matrix(c0, c1);                                                         \n
            float r = r0 / d_center;                                                                        \n
            float r_2 = r * r;                                                                              \n
            vec2 pt = (transform * vec3(pos.xy, 1.0)).xy;                                                   \n
            float t = r_2 - pt.y * pt.y;                                                                    \n
                                                                                                            \n
            if (t < 0.0) return vec2(0.0, -1.0);                                                            \n
                                                                                                            \n
            t = pt.x + sqrt(t);                                                                             \n
            return vec2(t, 1.0);                                                                            \n
        } else {                                                                                            \n
            float f = r0 / (r0 - r1);                                                                       \n
            bool is_swapped = abs(f - 1.0) < scalar_nearly_zero;                                            \n
            if (is_swapped) {                                                                               \n
                vec2 c = c0;                                                                                \n
                c0 = c1;                                                                                    \n
                c1 = c;                                                                                     \n
                f = 0.0;                                                                                    \n
            }                                                                                               \n
            vec2 cf = c0 * (1.0 - f) + c1 * f;                                                              \n
            mat3 transform = radial_matrix(cf, c1);                                                         \n
                                                                                                            \n
            float scale_x = abs(1.0 - f);                                                                   \n
            float scale_y = scale_x;                                                                        \n
            float r1 = abs(r1 - r0) / d_center;                                                             \n
            bool is_focal_on_circle = abs(r1 - 1.0) < scalar_nearly_zero;                                   \n
            if (is_focal_on_circle) {                                                                       \n
                scale_x *= 0.5;                                                                             \n
                scale_y *= 0.5;                                                                             \n
            } else {                                                                                        \n
                scale_x *= r1 / (r1 * r1 - 1.0);                                                            \n
                scale_y /= sqrt(abs(r1 * r1 - 1.0));                                                        \n
            }                                                                                               \n
            transform = mat3(scale_x, 0.0, 0.0, 0.0, scale_y, 0.0, 0.0, 0.0, 1.0) * transform;              \n
                                                                                                            \n
            vec2 pt = (transform * vec3(pos.xy, 1.0)).xy;                                                   \n
                                                                                                            \n
            float inv_r1 = 1.0 / r1;                                                                        \n
            float d_radius_sign = sign(1.0 - f);                                                            \n
            bool is_well_behaved = !is_focal_on_circle && r1 > 1.0;                                         \n
                                                                                                            \n
            float x_t = -1.0;                                                                               \n
            if (is_focal_on_circle) x_t = dot(pt, pt) / pt.x;                                               \n
            else if (is_well_behaved) x_t = length(pt) - pt.x * inv_r1;                                     \n
            else {                                                                                          \n
                float temp = pt.x * pt.x - pt.y * pt.y;                                                     \n
                if (temp >= 0.0) {                                                                          \n
                    if (is_swapped || d_radius_sign < 0.0) {                                                \n
                        x_t = -sqrt(temp) - pt.x * inv_r1;                                                  \n
                    } else {                                                                                \n
                        x_t = sqrt(temp) - pt.x * inv_r1;                                                   \n
                    }                                                                                       \n
                }                                                                                           \n
            }                                                                                               \n
                                                                                                            \n
            if (!is_well_behaved && x_t < 0.0) return vec2(0.0, -1.0);                                      \n
                                                                                                            \n
            float t = f + d_radius_sign * x_t;                                                              \n
            if (is_swapped) t = 1.0 - t;                                                                    \n
            return vec2(t, 1.0);                                                                            \n
        }                                                                                                   \n
    }                                                                                                       \n
                                                                                                            \n
    void main()                                                                                             \n
    {                                                                                                       \n
        vec2 pos = vPos;                                                                                    \n
        vec2 res = compute_radial_t(uGradientInfo.centerPos.xy,                                             \n
                                    uGradientInfo.radius.x,                                                 \n
                                    uGradientInfo.centerPos.zw,                                             \n
                                    uGradientInfo.radius.y,                                                 \n
                                    pos);                                                                   \n
        if (res.y < 0.0) {                                                                                  \n
            FragColor = vec4(0.0, 0.0, 0.0, 0.0);                                                           \n
            return;                                                                                         \n
        }                                                                                                   \n
        float t = gradientWrap(res.x);                                                                      \n
        vec4 color = gradient(t, res.x, length(pos - uGradientInfo.centerPos.xy));                          \n
        FragColor =  vec4(color.rgb * color.a, color.a);                                                    \n
    }
);

const char* IMAGE_VERT_SHADER = TVG_COMPOSE_SHADER(
    uniform float uDepth;                                                                   \n
    layout (location = 0) in vec2 aLocation;                                                \n
    layout (location = 1) in vec2 aUV;                                                      \n
    layout (std140) uniform Matrix {                                                        \n
        mat4 transform;                                                                     \n
    } uMatrix;                                                                              \n
    out vec2 vUV;                                                                           \n
                                                                                            \n
    void main()                                                                             \n
    {                                                                                       \n
        vUV = aUV;                                                                          \n
        vec4 pos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);                           \n
        pos.z = uDepth;                                                                     \n
        gl_Position = pos;                                                                  \n
    }                                                                                       \n
);

const char* IMAGE_FRAG_SHADER = TVG_COMPOSE_SHADER(
    layout(std140) uniform ColorInfo {                                                      \n
        int format;                                                                         \n
        int flipY;                                                                          \n
        int opacity;                                                                        \n
        int dummy;                                                                          \n
    } uColorInfo;                                                                           \n
    uniform sampler2D uTexture;                                                             \n
    in vec2 vUV;                                                                            \n
    out vec4 FragColor;                                                                     \n
                                                                                            \n
    void main()                                                                             \n
    {                                                                                       \n
        vec2 uv = vUV;                                                                      \n
        if (uColorInfo.flipY == 1) { uv.y = 1.0 - uv.y; }                                   \n
        vec4 color = texture(uTexture, uv);                                                 \n
        vec4 result;                                                                        \n
        if (uColorInfo.format == 0) { /* FMT_ABGR8888 */                                    \n
            result = color;                                                                 \n
        } else if (uColorInfo.format == 1) { /* FMT_ARGB8888 */                             \n
            result = color.bgra;                                                            \n
        } else if (uColorInfo.format == 2) { /* FMT_ABGR8888S */                            \n
            result = vec4(color.rgb * color.a, color.a);                                    \n
        } else if (uColorInfo.format == 3) { /* FMT_ARGB8888S */                            \n
            result = vec4(color.bgr * color.a, color.a);                                    \n
        }                                                                                   \n
        FragColor = result * float(uColorInfo.opacity) / 255.0;                             \n
   }                                                                                        \n
);

const char* MASK_VERT_SHADER = TVG_COMPOSE_SHADER(
    uniform float uDepth;                                   \n
    layout(location = 0) in vec2 aLocation;                 \n
    layout(location = 1) in vec2 aUV;                       \n
    out vec2  vUV;                                          \n
                                                            \n
    void main()                                             \n
    {                                                       \n
        vUV = aUV;                                          \n
        gl_Position = vec4(aLocation, uDepth, 1.0);         \n
    }                                                       \n
);


const char* MASK_ALPHA_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                          \n
    uniform sampler2D uMaskTexture;                         \n
    in vec2 vUV;                                            \n
    out vec4 FragColor;                                     \n
                                                            \n
    void main()                                             \n
    {                                                       \n
        vec4 srcColor = texture(uSrcTexture, vUV);          \n
        vec4 maskColor = texture(uMaskTexture, vUV);        \n
        FragColor = srcColor * maskColor.a;                 \n
    }                                                       \n
);

const char* MASK_INV_ALPHA_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                              \n
    uniform sampler2D uMaskTexture;                             \n
    in vec2 vUV;                                                \n
    out vec4 FragColor;                                         \n
                                                                \n
    void main()                                                 \n
    {                                                           \n
        vec4 srcColor = texture(uSrcTexture, vUV);              \n
        vec4 maskColor = texture(uMaskTexture, vUV);            \n
        FragColor = srcColor *(1.0 - maskColor.a);              \n
    }                                                           \n
);

const char* MASK_LUMA_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                                                                  \n
    uniform sampler2D uMaskTexture;                                                                                 \n
    in vec2 vUV;                                                                                                    \n
    out vec4 FragColor;                                                                                             \n
                                                                                                                    \n
    void main()
    {                                                                                                               \n
        vec4 srcColor = texture(uSrcTexture, vUV);                                                                  \n
        vec4 maskColor = texture(uMaskTexture, vUV);                                                                \n
                                                                                                                    \n
        if (maskColor.a > 0.000001) {                                                                               \n
            maskColor = vec4(maskColor.rgb / maskColor.a, maskColor.a);                                             \n
        }                                                                                                           \n
                                                                                                                    \n
        FragColor = srcColor * dot(maskColor.rgb, vec3(0.2125, 0.7154, 0.0721)) * maskColor.a;                      \n
    }                                                                                                               \n
);

const char* MASK_INV_LUMA_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                                      \n
    uniform sampler2D uMaskTexture;                                                     \n
    in vec2 vUV;                                                                        \n
    out vec4 FragColor;                                                                 \n
                                                                                        \n
    void main()                                                                         \n
    {                                                                                   \n
        vec4 srcColor = texture(uSrcTexture, vUV);                                      \n
        vec4 maskColor = texture(uMaskTexture, vUV);                                    \n
        float luma = dot(maskColor.rgb, vec3(0.2125, 0.7154, 0.0721));                  \n
        FragColor = srcColor * (1.0 - luma);                                            \n
    }                                                                                   \n
);

const char* MASK_ADD_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                      \n
    uniform sampler2D uMaskTexture;                                     \n
    in vec2 vUV;                                                        \n
    out vec4 FragColor;                                                 \n
                                                                        \n
    void main()                                                         \n
    {                                                                   \n
        vec4 srcColor = texture(uSrcTexture, vUV);                      \n
        vec4 maskColor = texture(uMaskTexture, vUV);                    \n
        vec4 color = srcColor + maskColor * (1.0 - srcColor.a);         \n
        FragColor = min(color, vec4(1.0, 1.0, 1.0, 1.0)) ;              \n
    }                                                                   \n
);

const char* MASK_SUB_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                          \n
    uniform sampler2D uMaskTexture;                                         \n
    in vec2 vUV;                                                            \n
    out vec4 FragColor;                                                     \n
                                                                            \n
    void main()                                                             \n
    {                                                                       \n
        vec4 srcColor = texture(uSrcTexture, vUV);                          \n
        vec4 maskColor = texture(uMaskTexture, vUV);                        \n
        float a = srcColor.a - maskColor.a;                                 \n
                                                                            \n
        if (a < 0.0 || srcColor.a == 0.0) {                                 \n
            FragColor = vec4(0.0, 0.0, 0.0, 0.0);                           \n
        } else {                                                            \n
            vec3 srcRgb = srcColor.rgb / srcColor.a;                        \n
            FragColor = vec4(srcRgb * a, a);                                \n
        }                                                                   \n
    }                                                                       \n
);

const char* MASK_INTERSECT_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                          \n
    uniform sampler2D uMaskTexture;                                         \n
    in vec2 vUV;                                                            \n
    out vec4 FragColor;                                                     \n
                                                                            \n
    void main()                                                             \n
    {                                                                       \n
        vec4 srcColor = texture(uSrcTexture, vUV);                          \n
        vec4 maskColor = texture(uMaskTexture, vUV);                        \n
        FragColor = maskColor * srcColor.a;                                 \n
    }                                                                       \n
);

const char* MASK_DIFF_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                          \n
    uniform sampler2D uMaskTexture;                                         \n
    in vec2 vUV;                                                            \n
    out vec4 FragColor;                                                     \n
                                                                            \n
    void main()                                                             \n
    {                                                                       \n
        vec4 srcColor = texture(uSrcTexture, vUV);                          \n
        vec4 maskColor = texture(uMaskTexture, vUV);                        \n
        float da = srcColor.a - maskColor.a;                                \n
        if (da == 0.0) {                                                    \n
            FragColor = vec4(0.0, 0.0, 0.0, 0.0);                           \n
        } else if (da > 0.0) {                                              \n
            FragColor = srcColor * da;                                      \n
        } else {                                                            \n
            FragColor = maskColor * (-da);                                  \n
        }                                                                   \n
    }                                                                       \n
);

const char* MASK_DARKEN_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                          \n
    uniform sampler2D uMaskTexture;                                         \n
    in vec2 vUV;                                                            \n
    out vec4 FragColor;                                                     \n
                                                                            \n
    void main()                                                             \n
    {                                                                       \n
        vec4 srcColor = texture(uSrcTexture, vUV);                          \n
        vec4 maskColor = texture(uMaskTexture, vUV);                        \n
        if (srcColor.a > 0.0) srcColor.rgb /= srcColor.a;                   \n
        float alpha = min(srcColor.a, maskColor.a);                         \n
        FragColor = vec4(srcColor.rgb * alpha, alpha);                      \n
    }                                                                       \n
);

const char* MASK_LIGHTEN_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                          \n
    uniform sampler2D uMaskTexture;                                         \n
    in vec2 vUV;                                                            \n
    out vec4 FragColor;                                                     \n
                                                                            \n
    void main()                                                             \n
    {                                                                       \n
        vec4 srcColor = texture(uSrcTexture, vUV);                          \n
        vec4 maskColor = texture(uMaskTexture, vUV);                        \n
        if (srcColor.a > 0.0) srcColor.rgb /= srcColor.a;                   \n
        float alpha = max(srcColor.a, maskColor.a);                         \n
        FragColor = vec4(srcColor.rgb * alpha, alpha);                      \n
    }                                                                       \n
);

const char* STENCIL_VERT_SHADER = TVG_COMPOSE_SHADER(
    uniform float uDepth;                                           \n
    layout(location = 0) in vec2 aLocation;                         \n
    layout(std140) uniform Matrix {                                 \n
        mat4 transform;                                             \n
    } uMatrix;                                                      \n
                                                                    \n
    void main()                                                     \n
    {                                                               \n
        vec4 pos = uMatrix.transform * vec4(aLocation, 0.0, 1.0);   \n
        pos.z = uDepth;                                             \n
        gl_Position = pos;                                          \n
    });

const char* STENCIL_FRAG_SHADER = TVG_COMPOSE_SHADER(
    out vec4 FragColor;                                             \n
                                                                    \n
    void main()                                                     \n
    {                                                               \n
        FragColor = vec4(0.0);                                      \n
    }                                                               \n
);

const char* BLIT_VERT_SHADER = TVG_COMPOSE_SHADER(
    layout(location = 0) in vec2 aLocation;                         \n
    layout(location = 1) in vec2 aUV;                               \n
    out vec2 vUV;                                                   \n
                                                                    \n
    void main()                                                     \n
    {                                                               \n
        vUV = aUV;                                                  \n
        gl_Position = vec4(aLocation, 0.0, 1.0);                    \n
    }
);

const char* BLIT_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform sampler2D uSrcTexture;                                  \n
    in vec2 vUV;                                                    \n
    out vec4 FragColor;                                             \n
                                                                    \n
    void main()                                                     \n
    {                                                               \n
        FragColor = texture(uSrcTexture, vUV);                      \n
    }
);

#define COMPLEX_BLEND_HEADER R"( \
    uniform sampler2D uSrcTexture; \
    uniform sampler2D uDstTexture; \
    \
    in vec2 vUV; \
    out vec4 FragColor; \
)"

const char* MULTIPLY_BLEND_FRAG = COMPLEX_BLEND_HEADER  R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);
        FragColor = srcColor * dstColor;
    }
)";

#define SCREEN_BLEND_FUNC R"( \
    vec4 screenBlend(vec4 srcColor, vec4 dstColor) \
    { \
        return dstColor + srcColor - (dstColor * srcColor); \
    } \
)"

#define HARD_LIGHT_BLEND_FUNC R"( \
    vec4 hardLightBlend(vec4 srcColor, vec4 dstColor) \
    { \
        return vec4(srcColor.r < 0.5 ? 2.0 * srcColor.r * dstColor.r : 1.0 - 2.0 * (1.0 - srcColor.r) * (1.0 - dstColor.r), \
                    srcColor.g < 0.5 ? 2.0 * srcColor.g * dstColor.g : 1.0 - 2.0 * (1.0 - srcColor.g) * (1.0 - dstColor.g), \
                    srcColor.b < 0.5 ? 2.0 * srcColor.b * dstColor.b : 1.0 - 2.0 * (1.0 - srcColor.b) * (1.0 - dstColor.b), \
                    1.0); \
    } \
)"

#define SOFT_LIGHT_BLEND_FUNC R"( \
    float softLightD(float v) \
    { \
        if (v <= 0.25) return ((16.0 * v - 12.0) * v + 4.0) * v; \
        else return sqrt(v); \
    } \
)"

const char* SCREEN_BLEND_FRAG = COMPLEX_BLEND_HEADER SCREEN_BLEND_FUNC R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);
        FragColor = screenBlend(srcColor, dstColor);
    }
)";

const char* OVERLAY_BLEND_FRAG = COMPLEX_BLEND_HEADER HARD_LIGHT_BLEND_FUNC R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);
        FragColor = hardLightBlend(dstColor, srcColor);
    }
)";

const char* COLOR_DODGE_BLEND_FRAG = COMPLEX_BLEND_HEADER R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);

        FragColor = vec4(
            srcColor.r < 1.0 ? dstColor.r / (1.0 - srcColor.r) : (dstColor.r > 0.0 ? 1.0 : 0.0),
            srcColor.g < 1.0 ? dstColor.g / (1.0 - srcColor.g) : (dstColor.g > 0.0 ? 1.0 : 0.0),
            srcColor.b < 1.0 ? dstColor.b / (1.0 - srcColor.b) : (dstColor.b > 0.0 ? 1.0 : 0.0),
            1.0
        );
    }
)";

const char* COLOR_BURN_BLEND_FRAG = COMPLEX_BLEND_HEADER R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);

        FragColor = vec4(
            srcColor.r > 0.0 ? (1.0 - (1.0 - dstColor.r) / srcColor.r) : (dstColor.r < 1.0 ? 0.0 : 1.0),
            srcColor.g > 0.0 ? (1.0 - (1.0 - dstColor.g) / srcColor.g) : (dstColor.g < 1.0 ? 0.0 : 1.0),
            srcColor.b > 0.0 ? (1.0 - (1.0 - dstColor.b) / srcColor.b) : (dstColor.b < 1.0 ? 0.0 : 1.0),
            1.0
        );
    }
)";

const char* HARD_LIGHT_BLEND_FRAG = COMPLEX_BLEND_HEADER HARD_LIGHT_BLEND_FUNC R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);
        FragColor = hardLightBlend(srcColor, dstColor);
    }
)";

const char* SOFT_LIGHT_BLEND_FRAG = COMPLEX_BLEND_HEADER SOFT_LIGHT_BLEND_FUNC R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);

        FragColor = vec4(
            srcColor.r <= 0.5 ? dstColor.r - (1.0 - 2.0 * srcColor.r) * dstColor.r * (1.0 - dstColor.r) : dstColor.r + (2.0 * srcColor.r - 1.0) * (softLightD(dstColor.r) - dstColor.r),
            srcColor.g <= 0.5 ? dstColor.g - (1.0 - 2.0 * srcColor.g) * dstColor.g * (1.0 - dstColor.g) : dstColor.g + (2.0 * srcColor.g - 1.0) * (softLightD(dstColor.g) - dstColor.g),
            srcColor.b <= 0.5 ? dstColor.b - (1.0 - 2.0 * srcColor.b) * dstColor.b * (1.0 - dstColor.b) : dstColor.b + (2.0 * srcColor.b - 1.0) * (softLightD(dstColor.b) - dstColor.b),
            1.0
            );
    }
)";

const char* DIFFERENCE_BLEND_FRAG = COMPLEX_BLEND_HEADER R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);

        FragColor = abs(dstColor - srcColor);
    }
)";

const char* EXCLUSION_BLEND_FRAG = COMPLEX_BLEND_HEADER R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);

        FragColor = dstColor + srcColor - (2.0 * dstColor * srcColor);
    }
)";

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
    int level;
    float sigma;
    float scale;
    float extend;
} uGaussian;

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
        float weight = gaussian(float(y), sigma);
        vec2 offset = vec2(0.0, float(y) * texelSize.y);
        colorSum += texture(uSrcTexture, vUV + offset) * weight;
        weightSum += weight;
    }
    
    FragColor = colorSum / weightSum;
} 
)";

const char* GAUSSIAN_HORIZONTAL = R"(
uniform sampler2D uSrcTexture;
layout(std140) uniform Gaussian {
    int level;
    float sigma;
    float scale;
    float extend;
} uGaussian;

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
        float weight = gaussian(float(y), sigma);
        vec2 offset = vec2(float(y) * texelSize.x, 0.0);
        colorSum += texture(uSrcTexture, vUV + offset) * weight;
        weightSum += weight;
    }
    
    FragColor = colorSum / weightSum;
} 
)";

const char* EFFECT_DROPSHADOW = R"(
uniform sampler2D uSrcTexture;
uniform sampler2D uBlrTexture;
layout(std140) uniform DropShadow {
    int level;
    float sigma;
    float scale;
    float extend;
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
    vec4 black = uParams.params[0];
    vec4 white = uParams.params[1];
    float intens = uParams.params[2].r;
    FragColor = mix(orig, mix(black, white, luma), intens) * orig.a;
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
    vec4 shadow = uParams.params[0];
    vec4 midtone = uParams.params[1];
    vec4 highlight = uParams.params[2];

    vec4 tmp = vec4(
        luma >= 0.5f ? mix(midtone.r, highlight.r, (luma - 0.5f)*2.0f) : mix(shadow.r, midtone.r, luma * 2.0f),
        luma >= 0.5f ? mix(midtone.g, highlight.g, (luma - 0.5f)*2.0f) : mix(shadow.g, midtone.g, luma * 2.0f),
        luma >= 0.5f ? mix(midtone.b, highlight.b, (luma - 0.5f)*2.0f) : mix(shadow.b, midtone.b, luma * 2.0f),
        luma >= 0.5f ? mix(midtone.a, highlight.a, (luma - 0.5f)*2.0f) : mix(shadow.a, midtone.a, luma * 2.0f)
    );

    //blender
    if (highlight.a > 0.0f) tmp = mix(tmp, orig, highlight.a);
    FragColor = tmp * orig.a;
} 
)";
