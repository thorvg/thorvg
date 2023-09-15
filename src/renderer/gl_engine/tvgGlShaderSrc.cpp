/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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
    layout(location = 0) in vec3 aLocation;                         \n
    layout(std140) uniform Matrix {                                 \n
        mat4 transform;                                             \n
    } uMatrix;                                                      \n
    out float vOpacity;                                             \n
    void main()                                                     \n
    {                                                               \n
        gl_Position =                                               \n
            uMatrix.transform * vec4(aLocation.xy, 0.0, 1.0);       \n
        vOpacity = aLocation.z;                                     \n
    });

const char* COLOR_FRAG_SHADER = TVG_COMPOSE_SHADER(
    layout(std140) uniform ColorInfo {                       \n
        vec4 solidColor;                                     \n
    } uColorInfo;                                            \n
    in float vOpacity;                                       \n
    out vec4 FragColor;                                      \n
    void main()                                              \n
    {                                                        \n
       vec4 uColor = uColorInfo.solidColor;                  \n
       FragColor = vec4(uColor.xyz, uColor.w*vOpacity);      \n
    });

const char* GRADIENT_VERT_SHADER = TVG_COMPOSE_SHADER(
layout(location = 0) in vec3 aLocation;                                         \n
out float vOpacity;                                                             \n
out vec2 vPos;                                                                  \n
layout(std140) uniform Matrix {                                                 \n
    mat4 transform;                                                             \n
} uMatrix;                                                                      \n
                                                                                \n
void main()                                                                     \n
{                                                                               \n
    gl_Position = uMatrix.transform * vec4(aLocation.xy, 0.0, 1.0);             \n
    vOpacity = aLocation.z;                                                     \n
    vPos =  aLocation.xy;                                                       \n
});


std::string STR_GRADIENT_FRAG_COMMON_VARIABLES = TVG_COMPOSE_SHADER(
const int MAX_STOP_COUNT = 4;                                                                           \n
in vec2 vPos;                                                                                           \n
in float vOpacity;                                                                                      \n
);

std::string STR_GRADIENT_FRAG_COMMON_FUNCTIONS = TVG_COMPOSE_SHADER(
float gradientStep(float edge0, float edge1, float x)                                                   \n
{                                                                                                       \n
    // linear                                                                                           \n
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);                                                 \n
    return x;                                                                                           \n
}                                                                                                       \n
                                                                                                        \n
vec4 gradient(float t)                                                                                  \n
{                                                                                                       \n
    vec4 col = vec4(0.0);                                                                               \n
    int i = 0;                                                                                          \n
    int count = int(uGradientInfo.nStops[0]);                                                              \n
    if (t <= uGradientInfo.stopPoints[0])                                                               \n
    {                                                                                                   \n
        col += uGradientInfo.stopColors[0];                                                             \n
    }                                                                                                   \n
    else if (t >= uGradientInfo.stopPoints[count - 1])                                                  \n
    {                                                                                                   \n
        col += uGradientInfo.stopColors[count - 1];                                                     \n
    }                                                                                                   \n
    else                                                                                                \n
    {                                                                                                   \n
        for (i = 0; i < count - 1; ++i)                                                                 \n
        {                                                                                               \n
            if (t > uGradientInfo.stopPoints[i] && t < uGradientInfo.stopPoints[i + 1])                 \n
            {                                                                                           \n
                col += (uGradientInfo.stopColors[i] *                                                   \n
                    (1. - gradientStep(uGradientInfo.stopPoints[i],                                     \n
                                       uGradientInfo.stopPoints[i + 1], t)));                           \n
                col += (uGradientInfo.stopColors[i + 1] *                                               \n
                        gradientStep(uGradientInfo.stopPoints[i], uGradientInfo.stopPoints[i + 1], t)); \n
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
    vec4  stopPoints;                                                                                   \n
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
    //t = smoothstep(0.0, 1.0, clamp(t, 0.0, 1.0));                                                     \n
    t = clamp(t, 0.0, 1.0);                                                                             \n
                                                                                                        \n
    vec4 color = gradient(t);                                                                           \n
                                                                                                        \n
    vec3 noise = 8.0 * uGradientInfo.nStops[1] * ScreenSpaceDither(pos);                                \n
    vec4 finalCol = vec4(color.xyz + noise, color.w);                                                   \n
    FragColor = vec4(finalCol.xyz, finalCol.w* vOpacity);                                               \n
});

std::string STR_RADIAL_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
layout(std140) uniform GradientInfo {                                                                   \n
    vec4  nStops;                                                                                       \n
    vec2  centerPos;                                                                                    \n
    vec2  radius;                                                                                       \n
    vec4  stopPoints;                                                                                   \n
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
    //float t = smoothstep(0.0, 1.0, clamp(d, 0.0, 1.0));                                               \n
    float t = clamp(d, 0.0, 1.0);                                                                       \n
                                                                                                        \n
    vec4 color = gradient(t);                                                                           \n
                                                                                                        \n
    vec3 noise = 8.0 * uGradientInfo.nStops[1] * ScreenSpaceDither(pos);                                \n
    vec4 finalCol = vec4(color.xyz + noise, color.w);                                                   \n
    FragColor = vec4(finalCol.xyz, finalCol.w * vOpacity);                                              \n
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
