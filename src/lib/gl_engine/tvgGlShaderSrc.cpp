/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
    attribute mediump vec4 aLocation;								\n
    uniform highp mat4 uTransform;									\n
    varying highp float vOpacity;									\n
    void main()														\n
    {																\n
        gl_Position = uTransform * vec4(aLocation.xy, 0.0, 1.0);    \n
        vOpacity = aLocation.z;										\n
    });

const char* COLOR_FRAG_SHADER = TVG_COMPOSE_SHADER(
    uniform highp vec4 uColor;                               \n
    varying highp float vOpacity;                            \n
    void main()                                              \n
    {                                                        \n
       gl_FragColor = vec4(uColor.xyz, uColor.w*vOpacity);   \n
    });

const char* GRADIENT_VERT_SHADER = TVG_COMPOSE_SHADER(
attribute highp vec4 aLocation;                                                 \n
varying highp float vOpacity;                                                   \n
varying highp vec2 vPos;                                                        \n
uniform highp mat4 uTransform;													\n
                                                                                \n
void main()                                                                     \n
{                                                                               \n
    gl_Position = uTransform * vec4(aLocation.xy, 0.0, 1.0);					\n
    vOpacity = aLocation.z;                                                     \n
    vPos = vec2((aLocation.x + 1.0) / 2.0, ((-1.0 * aLocation.y) +1.0) / 2.0);  \n
});


std::string STR_GRADIENT_FRAG_COMMON_VARIABLES = TVG_COMPOSE_SHADER(
precision highp float;                                                                                  \n
const int MAX_STOP_COUNT = 4;                                                                           \n
uniform highp vec2 uSize;                                                                               \n
uniform highp vec2 uCanvasSize;                                                                         \n
uniform float nStops;                                                                                  \n
uniform float noise_level;                                                                              \n
uniform float stopPoints[MAX_STOP_COUNT];                                                               \n
uniform vec4 stopColors[MAX_STOP_COUNT];                                                                \n
varying highp vec2 vPos;                                                                                \n
varying highp float vOpacity;                                                                           \n
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
    int count = int(nStops);                                                                            \n
    if (t <= stopPoints[0])                                                                              \n
    {                                                                                                   \n
        col += stopColors[0];                                                                           \n
    }                                                                                                   \n
    else if (t >= stopPoints[count - 1])                                                                 \n
    {                                                                                                   \n
        col += stopColors[count - 1];                                                                   \n
    }                                                                                                   \n
    else                                                                                                \n
    {                                                                                                   \n
        for (i = 0; i < count - 1; ++i)                                                                 \n
        {                                                                                               \n
            if (t > stopPoints[i] && t < stopPoints[i + 1])                                            \n
            {                                                                                           \n
                col += (stopColors[i] * (1. - gradientStep(stopPoints[i], stopPoints[i + 1], t)));      \n
                col += (stopColors[i + 1] * gradientStep(stopPoints[i], stopPoints[i + 1], t));         \n
                break;                                                                                  \n
            }                                                                                           \n
        }                                                                                               \n
    }                                                                                                   \n
                                                                                                        \n
    return col;                                                                                         \n
}                                                                                                       \n
                                                                                                        \n
highp vec3 ScreenSpaceDither(vec2 vScreenPos)                                                           \n
{                                                                                                       \n
    highp vec3 vDither = vec3(dot(vec2(171.0, 231.0), vScreenPos.xy));                                  \n
    vDither.rgb = fract(vDither.rgb / vec3(103.0, 71.0, 97.0));                                         \n
    return vDither.rgb / 255.0;                                                                         \n
});

std::string STR_LINEAR_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
uniform highp vec2 gradStartPos;                                                                        \n
uniform highp vec2 gradEndPos;                                                                          \n
);

std::string STR_LINEAR_GRADIENT_MAIN = TVG_COMPOSE_SHADER(
void main()                                                                                             \n
{                                                                                                       \n
    highp vec2 pos = vec2(vPos.x * uCanvasSize.x, vPos.y * uCanvasSize.y);                              \n
    highp vec2 spos = vec2(pos.x / uSize.x, pos.y / uSize.y);                                           \n
    highp vec2 st = gradStartPos / (uSize.xy);                                                          \n
    highp vec2 ed = gradEndPos / (uSize.xy);                                                            \n
                                                                                                        \n
    highp vec2 ba = ed - st;                                                                            \n
                                                                                                        \n
    highp float t = dot(spos - st, ba) / dot(ba, ba);                                                   \n
                                                                                                        \n
    //t = smoothstep(0.0, 1.0, clamp(t, 0.0, 1.0));                                                     \n
    t = clamp(t, 0.0, 1.0);                                                                             \n
                                                                                                        \n
    vec4 color = gradient(t);                                                                           \n
                                                                                                        \n
    highp vec3 noise = 8.0 * noise_level * ScreenSpaceDither(pos);                                      \n
    vec4 finalCol = vec4(color.xyz + noise, color.w);                                                   \n
    gl_FragColor = vec4(finalCol.xyz, finalCol.w* vOpacity);                                            \n
});

std::string STR_RADIAL_GRADIENT_VARIABLES = TVG_COMPOSE_SHADER(
    uniform highp vec2 gradStartPos;                                                                    \n
    uniform highp float stRadius;                                                                       \n
);

std::string STR_RADIAL_GRADIENT_MAIN = TVG_COMPOSE_SHADER(
void main()                                                                                             \n
{                                                                                                       \n
    highp vec2 pos = vec2(vPos.x * uCanvasSize.x, vPos.y * uCanvasSize.y);                              \n
    highp vec2 spos = vec2(pos.x / uSize.x, pos.y / uSize.y);                                           \n
                                                                                                        \n
    highp float ba = stRadius;                                                                          \n
    highp float d = distance(gradStartPos, pos);                                                        \n
    d = (d / ba);                                                                                       \n
                                                                                                        \n
    //float t = smoothstep(0.0, 1.0, clamp(d, 0.0, 1.0));                                               \n
    float t = clamp(d, 0.0, 1.0);                                                                       \n
                                                                                                        \n
    vec4 color = gradient(t);                                                                           \n
                                                                                                        \n
    highp vec3 noise = 8.0 * noise_level * ScreenSpaceDither(pos);                                      \n
    vec4 finalCol = vec4(color.xyz + noise, color.w);                                                   \n
    gl_FragColor = vec4(finalCol.xyz, finalCol.w * vOpacity);                                           \n
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
