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
    layout(location = 0) in vec2 aLocation;                         \n
    layout(std140) uniform Matrix {                                 \n
        mat4 transform;                                             \n
    } uMatrix;                                                      \n
    void main()                                                     \n
    {                                                               \n
        gl_Position = uMatrix.transform * vec4(aLocation, 0.0, 1.0);\n
    });

const char* COLOR_FRAG_SHADER = TVG_COMPOSE_SHADER(
    layout(std140) uniform ColorInfo {                       \n
        vec4 solidColor;                                     \n
    } uColorInfo;                                            \n
    out vec4 FragColor;                                      \n
    void main()                                              \n
    {                                                        \n
       vec4 uColor = uColorInfo.solidColor;                  \n
       FragColor =  vec4(uColor.rgb * uColor.a, uColor.a);   \n
    });

const char* GRADIENT_VERT_SHADER = TVG_COMPOSE_SHADER(
layout(location = 0) in vec2 aLocation;                                         \n
out vec2 vPos;                                                                  \n
layout(std140) uniform Matrix {                                                 \n
    mat4 transform;                                                             \n
} uMatrix;                                                                      \n
                                                                                \n
void main()                                                                     \n
{                                                                               \n
    gl_Position = uMatrix.transform * vec4(aLocation, 0.0, 1.0);                \n
    vPos =  aLocation;                                                          \n
});


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
                                                                                                        \n
    if (spread == 0) { /* pad */                                                                        \n
        return clamp(d, 0.0, 1.0);                                                                      \n
    }                                                                                                   \n
                                                                                                        \n
    int i = 1;                                                                                          \n
    while (d > 1.0) {                                                                                   \n
        d = d - 1.0;                                                                                    \n
        i *= -1;                                                                                        \n
    }                                                                                                   \n
                                                                                                        \n
    if (spread == 2) {  /* Reflect */                                                                   \n
        return smoothstep(0.0, 1.0, d);                                                                 \n
    }                                                                                                   \n
                                                                                                        \n
    if (i == 1)                                                                                         \n
        return smoothstep(0.0, 1.0, d);                                                                 \n
    else                                                                                                \n
        return smoothstep(1.0, 0.0, d);                                                                 \n
}                                                                                                       \n
                                                                                                        \n
vec4 gradient(float t)                                                                                  \n
{                                                                                                       \n
    vec4 col = vec4(0.0);                                                                               \n
    int i = 0;                                                                                          \n
    int count = int(uGradientInfo.nStops[0]);                                                           \n
    if (t <= gradientStop(0))                                                                           \n
    {                                                                                                   \n
        col += uGradientInfo.stopColors[0];                                                             \n
    }                                                                                                   \n
    else if (t >= gradientStop(count - 1))                                                              \n
    {                                                                                                   \n
        col += uGradientInfo.stopColors[count - 1];                                                     \n
    }                                                                                                   \n
    else                                                                                                \n
    {                                                                                                   \n
        for (i = 0; i < count - 1; ++i)                                                                 \n
        {                                                                                               \n
            float stopi = gradientStop(i);                                                              \n
            float stopi1 = gradientStop(i + 1);                                                         \n
            if (t > stopi && t <stopi1)                                                                 \n
            {                                                                                           \n
                col += (uGradientInfo.stopColors[i] * (1. - gradientStep(stopi, stopi1, t)));           \n
                col += (uGradientInfo.stopColors[i + 1] * gradientStep(stopi, stopi1, t));              \n
                break;                                                                                  \n
            }                                                                                           \n
        }                                                                                               \n
    }                                                                                                   \n
                                                                                                        \n
    return col;                                                                                         \n
}                                                                                                       \n
                                                                                                        \n
vec3 ScreenSpaceDither(vec2 vScreenPos)                                                                 \n
{                                                                                                       \n
    vec3 vDither = vec3(dot(vec2(171.0, 231.0), vScreenPos.xy));                                        \n
    vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0));                                         \n
    return vDither.rgb / 255.0;                                                                         \n
});

std::string STR_LINEAR_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
layout(std140) uniform GradientInfo {                                                                   \n
    vec4  nStops;                                                                                       \n
    vec2  gradStartPos;                                                                                 \n
    vec2  gradEndPos;                                                                                   \n
    vec4  stopPoints[MAX_STOP_COUNT / 4];                                                               \n
    vec4  stopColors[MAX_STOP_COUNT];                                                                   \n
} uGradientInfo ;                                                                                       \n
);

std::string STR_LINEAR_GRADIENT_MAIN = TVG_COMPOSE_SHADER(
out vec4 FragColor;                                                                                     \n
void main()                                                                                             \n
{                                                                                                       \n
    vec2 pos = vPos;                                                                                    \n
    vec2 st = uGradientInfo.gradStartPos;                                                               \n
    vec2 ed = uGradientInfo.gradEndPos;                                                                 \n
                                                                                                        \n
    vec2 ba = ed - st;                                                                                  \n
                                                                                                        \n
    float t = dot(pos - st, ba) / dot(ba, ba);                                                          \n
                                                                                                        \n
    t = gradientWrap(t);                                                                                \n
                                                                                                        \n
    vec4 color = gradient(t);                                                                           \n
                                                                                                        \n
    vec3 noise = 8.0 * uGradientInfo.nStops[1] * ScreenSpaceDither(pos);                                \n
    vec4 finalCol = vec4(color.xyz + noise, color.w);                                                   \n
    FragColor =  vec4(finalCol.rgb * finalCol.a, finalCol.a);                                           \n
});

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
                                                                                                        \n
    float ba = uGradientInfo.radius.x;                                                                  \n
    float d = distance(uGradientInfo.centerPos, pos);                                                   \n
    d = (d / ba);                                                                                       \n
                                                                                                        \n
    float t = gradientWrap(d);                                                                          \n
                                                                                                        \n
    vec4 color = gradient(t);                                                                           \n
                                                                                                        \n
    vec3 noise = 8.0 * uGradientInfo.nStops[1] * ScreenSpaceDither(pos);                                \n
    vec4 finalCol = vec4(color.xyz + noise, color.w);                                                   \n
    FragColor =  vec4(finalCol.rgb * finalCol.a, finalCol.a);                                           \n
});

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
    layout (location = 0) in vec2 aLocation;                                                \n
    layout (location = 1) in vec2 aUV;                                                      \n
    layout (std140) uniform Matrix {                                                        \n
        mat4 transform;                                                                     \n
    } uMatrix;                                                                              \n
                                                                                            \n
    out vec2 vUV;                                                                           \n
                                                                                            \n
    void main() {                                                                           \n
        vUV = aUV;                                                                          \n
        gl_Position = uMatrix.transform * vec4(aLocation, 0.0, 1.0);                        \n
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
                                                                                            \n
    in vec2 vUV;                                                                            \n
                                                                                            \n
    out vec4 FragColor;                                                                     \n
                                                                                            \n
    void main() {                                                                           \n
        vec2 uv = vUV;                                                                      \n
        if (uColorInfo.flipY == 1) { uv.y = 1.0 - uv.y; }                                   \n
                                                                                            \n
        vec4 color = texture(uTexture, uv);                                                 \n
        vec4 result = color;                                                                \n
        if (uColorInfo.format == 1) { /* FMT_ARGB8888 */                                    \n
            result.r = color.b * color.a;                                                   \n
            result.g = color.g * color.a;                                                   \n
            result.b = color.r * color.a;                                                   \n
            result.a = color.a;                                                             \n
        } else if (uColorInfo.format == 2) { /* FMT_ABGR8888S */                            \n
            result = color;                                                                 \n
        } else if (uColorInfo.format == 3) { /* FMT_ARGB8888S */                            \n
            result.r = color.b;                                                             \n
            result.g = color.g;                                                             \n
            result.b = color.b;                                                             \n
            result.a = color.a;                                                             \n
        }                                                                                   \n
        float opacity = float(uColorInfo.opacity) / 255.0;                                  \n
        FragColor = result * opacity;                                                       \n
   }                                                                                        \n
);

const char* MASK_VERT_SHADER = TVG_COMPOSE_SHADER(
layout(location = 0) in vec2 aLocation;                 \n
layout(location = 1) in vec2 aUV;                       \n
                                                        \n
out vec2  vUV;                                          \n
                                                        \n
void main() {                                           \n
  vUV = aUV;                                            \n
                                                        \n
  gl_Position = vec4(aLocation, 0.0, 1.0);              \n
}                                                       \n
);


const char* MASK_ALPHA_FRAG_SHADER = TVG_COMPOSE_SHADER(
uniform sampler2D uSrcTexture;                          \n
uniform sampler2D uMaskTexture;                         \n
                                                        \n
in vec2 vUV;                                            \n
                                                        \n
out vec4 FragColor;                                     \n
                                                        \n
void main() {                                           \n
    vec4 srcColor = texture(uSrcTexture, vUV);          \n
    vec4 maskColor = texture(uMaskTexture, vUV);        \n
                                                        \n
    FragColor = srcColor * maskColor.a;                 \n
}                                                       \n
);

const char* MASK_INV_ALPHA_FRAG_SHADER = TVG_COMPOSE_SHADER(
uniform sampler2D uSrcTexture;                              \n
uniform sampler2D uMaskTexture;                             \n
                                                            \n
in vec2 vUV;                                                \n
                                                            \n
out vec4 FragColor;                                         \n
                                                            \n
void main() {                                               \n
    vec4 srcColor = texture(uSrcTexture, vUV);              \n
    vec4 maskColor = texture(uMaskTexture, vUV);            \n
                                                            \n
    FragColor = srcColor *(1.0 - maskColor.a);              \n
}                                                           \n
);

const char* MASK_LUMA_FRAG_SHADER = TVG_COMPOSE_SHADER(
uniform sampler2D uSrcTexture;                                                                          \n
uniform sampler2D uMaskTexture;                                                                         \n
                                                                                                        \n
in vec2 vUV;                                                                                            \n
                                                                                                        \n
out vec4 FragColor;                                                                                     \n
                                                                                                        \n
void main() {                                                                                           \n
    vec4 srcColor = texture(uSrcTexture, vUV);                                                          \n
    vec4 maskColor = texture(uMaskTexture, vUV);                                                        \n
                                                                                                        \n
    if (maskColor.a > 0.000001) {                                                                       \n
        maskColor = vec4(maskColor.rgb / maskColor.a, maskColor.a);                                     \n
    }                                                                                                   \n
                                                                                                        \n
    FragColor =                                                                                         \n
        srcColor * (0.299 * maskColor.r + 0.587 * maskColor.g + 0.114 * maskColor.b) * maskColor.a;     \n
}                                                                                                       \n
);

const char* MASK_INV_LUMA_FRAG_SHADER = TVG_COMPOSE_SHADER(
uniform sampler2D uSrcTexture;                                                      \n
uniform sampler2D uMaskTexture;                                                     \n
                                                                                    \n
in vec2 vUV;                                                                        \n
                                                                                    \n
out vec4 FragColor;                                                                 \n
                                                                                    \n
void main() {                                                                       \n
    vec4 srcColor = texture(uSrcTexture, vUV);                                      \n
    vec4 maskColor = texture(uMaskTexture, vUV);                                    \n
                                                                                    \n
    float luma = (0.299 * maskColor.r + 0.587 * maskColor.g + 0.114 * maskColor.b); \n
    luma *= maskColor.a;                                                            \n
    FragColor = srcColor * (1.0 - luma);                                            \n
}                                                                                   \n
);

const char* MASK_ADD_FRAG_SHADER = TVG_COMPOSE_SHADER(
uniform sampler2D uSrcTexture;                                      \n
uniform sampler2D uMaskTexture;                                     \n
                                                                    \n
in vec2 vUV;                                                        \n
                                                                    \n
out vec4 FragColor;                                                 \n
                                                                    \n
void main() {                                                       \n
    vec4 srcColor = texture(uSrcTexture, vUV);                      \n
    vec4 maskColor = texture(uMaskTexture, vUV);                    \n
                                                                    \n
    vec4 color = srcColor + maskColor * (1.0 - srcColor.a);         \n
                                                                    \n
    FragColor = min(color, vec4(1.0, 1.0, 1.0, 1.0)) ;              \n
}                                                                   \n
);

const char* MASK_SUB_FRAG_SHADER = TVG_COMPOSE_SHADER(
uniform sampler2D uSrcTexture;                                          \n
uniform sampler2D uMaskTexture;                                         \n
                                                                        \n
in vec2 vUV;                                                            \n
                                                                        \n
out vec4 FragColor;                                                     \n
                                                                        \n
void main() {                                                           \n
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
                                                                        \n
in vec2 vUV;                                                            \n
                                                                        \n
out vec4 FragColor;                                                     \n
                                                                        \n
void main() {                                                           \n
    vec4 srcColor = texture(uSrcTexture, vUV);                          \n
    vec4 maskColor = texture(uMaskTexture, vUV);                        \n
                                                                        \n
                                                                        \n
    FragColor = maskColor * srcColor.a;                                 \n
}                                                                       \n
);

const char* MASK_DIFF_FRAG_SHADER = TVG_COMPOSE_SHADER(
uniform sampler2D uSrcTexture;                                          \n
uniform sampler2D uMaskTexture;                                         \n
                                                                        \n
in vec2 vUV;                                                            \n
                                                                        \n
out vec4 FragColor;                                                     \n
                                                                        \n
void main() {                                                           \n
    vec4 srcColor = texture(uSrcTexture, vUV);                          \n
    vec4 maskColor = texture(uMaskTexture, vUV);                        \n
                                                                        \n
    float da = srcColor.a - maskColor.a;                                \n
                                                                        \n
    if (da == 0.0) {                                                    \n
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);                           \n
    } else if (da > 0.0) {                                              \n
        FragColor = srcColor * da;                                      \n
    } else {                                                            \n
        FragColor = maskColor * (-da);                                  \n
    }                                                                   \n
}                                                                       \n
);

const char* STENCIL_VERT_SHADER = TVG_COMPOSE_SHADER(
    layout(location = 0) in vec2 aLocation;                         \n
    layout(std140) uniform Matrix {                                 \n
        mat4 transform;                                             \n
    } uMatrix;                                                      \n
    void main()                                                     \n
    {                                                               \n
        gl_Position =                                               \n
            uMatrix.transform * vec4(aLocation, 0.0, 1.0);          \n
    });

const char* STENCIL_FRAG_SHADER = TVG_COMPOSE_SHADER(
    out vec4 FragColor;                                             \n
    void main() { FragColor = vec4(0.0); }                          \n
);
