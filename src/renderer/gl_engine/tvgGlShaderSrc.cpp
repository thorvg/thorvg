/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include <string>
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


std::string STR_GRADIENT_FRAG_COMMON_VARIABLES = TVG_COMPOSE_SHADER(
    const int MAX_STOP_COUNT = 16;                                                                          \n
    in vec2 vPos;                                                                                           \n
);

std::string STR_GRADIENT_FRAG_COMMON_FUNCTIONS = TVG_COMPOSE_SHADER(
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

std::string STR_LINEAR_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
    layout(std140) uniform GradientInfo {                                                                   \n
        vec4  nStops;                                                                                       \n
        vec2  gradStartPos;                                                                                 \n
        vec2  gradEndPos;                                                                                   \n
        vec4  stopPoints[MAX_STOP_COUNT / 4];                                                               \n
        vec4  stopColors[MAX_STOP_COUNT];                                                                   \n
    } uGradientInfo;                                                                                        \n
);

std::string STR_LINEAR_GRADIENT_MAIN = TVG_COMPOSE_SHADER(
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

std::string STR_RADIAL_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
    layout(std140) uniform GradientInfo {                                                                   \n
        vec4  nStops;                                                                                       \n
        vec2  centerPos;                                                                                    \n
        vec2  radius;                                                                                       \n
        vec4  stopPoints[MAX_STOP_COUNT / 4];                                                               \n
        vec4  stopColors[MAX_STOP_COUNT];                                                                   \n
    } uGradientInfo ;                                                                                       \n
);

std::string STR_RADIAL_GRADIENT_MAIN = TVG_COMPOSE_SHADER(
    out vec4 FragColor;                                                                                     \n
    void main()                                                                                             \n
    {                                                                                                       \n
        vec2 pos = vPos;                                                                                    \n
        float ba = uGradientInfo.radius.x;                                                                  \n
        float d = distance(uGradientInfo.centerPos, pos);                                                   \n
        float t = (d / ba);                                                                                 \n
        t = gradientWrap(t);                                                                                \n
        vec4 color = gradient(t, (d / ba), d);                                                              \n
        FragColor =  vec4(color.rgb * color.a, color.a);                                                    \n
    }
);

std::string STR_LINEAR_GRADIENT_FRAG_SHADER =
STR_GRADIENT_FRAG_COMMON_VARIABLES +
STR_LINEAR_GRADIENT_VARIABLES +
STR_GRADIENT_FRAG_COMMON_FUNCTIONS +
STR_LINEAR_GRADIENT_MAIN;

const char* LINEAR_GRADIENT_FRAG_SHADER = STR_LINEAR_GRADIENT_FRAG_SHADER.c_str();

std::string STR_RADIAL_GRADIENT_FRAG_SHADER =
STR_GRADIENT_FRAG_COMMON_VARIABLES +
STR_RADIAL_GRADIENT_VARIABLES +
STR_GRADIENT_FRAG_COMMON_FUNCTIONS +
STR_RADIAL_GRADIENT_MAIN;

const char* RADIAL_GRADIENT_FRAG_SHADER = STR_RADIAL_GRADIENT_FRAG_SHADER.c_str();


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
        FragColor = srcColor * (0.299 * maskColor.r + 0.587 * maskColor.g + 0.114 * maskColor.b) * maskColor.a;     \n
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
        float luma = (0.299 * maskColor.r + 0.587 * maskColor.g + 0.114 * maskColor.b); \n
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
    void main() {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);
        float opacity = srcColor.a;

        srcColor *= 255.0;
        dstColor *= 255.0;
        vec4 color = vec4(
            255.0 - srcColor.r > 0.0 ? dstColor.r / (255.0 - srcColor.r) : dstColor.r,
            255.0 - srcColor.g > 0.0 ? dstColor.g / (255.0 - srcColor.g) : dstColor.g,
            255.0 - srcColor.b > 0.0 ? dstColor.b / (255.0 - srcColor.b) : dstColor.b,
            255.0 - srcColor.a > 0.0 ? dstColor.a / (255.0 - srcColor.a) : dstColor.a
        );

        FragColor = vec4(color.rgb, 255.0) * opacity / 255.0;
    }
)";

const char* COLOR_BURN_BLEND_FRAG = COMPLEX_BLEND_HEADER R"(
    void main()
    {
        vec4 srcColor = texture(uSrcTexture, vUV);
        vec4 dstColor = texture(uDstTexture, vUV);
        float opacity = srcColor.a;

        if (srcColor.a > 0.0) srcColor.rgb /= srcColor.a;
        if (dstColor.a > 0.0) dstColor.rgb /= dstColor.a;
        vec4 id = vec4(1.0) - dstColor;
        vec4 color = vec4(
            srcColor.r > 0.0 ? (255.0 -  (255.0 - dstColor.r * 255.0) / (srcColor.r * 255.0)) / 255.0 : (1.0 - dstColor.r),
            srcColor.g > 0.0 ? (255.0 -  (255.0 - dstColor.g * 255.0) / (srcColor.g * 255.0)) / 255.0 : (1.0 - dstColor.g),
            srcColor.b > 0.0 ? (255.0 -  (255.0 - dstColor.b * 255.0) / (srcColor.b * 255.0)) / 255.0 : (1.0 - dstColor.b),
            srcColor.a > 0.0 ? (255.0 -  (255.0 - dstColor.a * 255.0) / (srcColor.a * 255.0)) / 255.0 : (1.0 - dstColor.a)
        );

        FragColor = color * srcColor.a;
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
