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

#include "tvgGlPrograms.h"
#include "tvgGlShaderSrc.h"

//***********************************************************************
// GlProgram
//***********************************************************************

GlProgram::GlProgram(const char* vsrc, const char* fsrc)
{
    // create and compile vertex shader
    vert = shaderCreate(GL_VERTEX_SHADER, vsrc);
    if (!shaderStatusCheck(vert)) return;

    // create and compile fragment shader
    frag = shaderCreate(GL_FRAGMENT_SHADER, fsrc);
    if (!shaderStatusCheck(frag)) return;

    // attach shaders
    prog = GL_CHECK(glCreateProgram());
    GL_CHECK(glAttachShader(prog, vert));
    GL_CHECK(glAttachShader(prog, frag));
    GL_CHECK(glLinkProgram(prog));
    if (!programStatusCheck(prog)) return;
}


GlProgram::~GlProgram()
{
    GL_CHECK(glDeleteProgram(prog));
    GL_CHECK(glDeleteShader(frag));
    GL_CHECK(glDeleteShader(vert));
    prog = 0;
    frag = 0;
    vert = 0;
}


void GlProgram::load()
{
    GL_CHECK(glUseProgram(prog));
}


int32_t GlProgram::getUniformLocation(const char* name)
{
    GL_CHECK(int32_t location = glGetUniformLocation(prog, name));
    return location;
}


int32_t GlProgram::getUniformBlockIndex(const char* name)
{
    GL_CHECK(int32_t location = glGetUniformBlockIndex(prog, name));
    return location;
}


GLuint GlProgram::shaderCreate(GLenum type, const char* src)
{
    const char* srcs[4]{};
    #if defined (THORVG_GL_TARGET_GLES)
    srcs[0] ="#version 300 es\n";
    #else
    srcs[0] ="#version 330 core\n";
    #endif
    srcs[1] = "precision highp float;";
    srcs[2] = "precision highp int;";
    srcs[3] = src;
    // create and compile vertex shader
    GLuint shader = GL_CHECK(glCreateShader(type));
    GL_CHECK(glShaderSource(shader, 4, srcs, nullptr));
    GL_CHECK(glCompileShader(shader));
    return shader;
}


bool GlProgram::shaderStatusCheck(GLuint shader)
{
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        // get max length of info log
        GLsizei maxLength = 0;
        GLsizei length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        // maxLength includes the NULL character
        tvg::Array<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &length, &infoLog[0]);
        assert(false);
        return false;
    }
    return true;
}


bool GlProgram::programStatusCheck(GLuint program)
{
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        // get max length of info log
        GLint maxLength = 0;
        GLsizei length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        // The maxLength includes the NULL character
        tvg::Array<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &length, &infoLog[0]);
        assert(false);
        return false;
    }
    return true;
}

//***********************************************************************
// GlPrograms
//***********************************************************************

#define SHADER_TOTAL_LENGTH 8192

void GlPrograms::init()
{
    // linear gradient
    char linearGradientFragShader[SHADER_TOTAL_LENGTH];
    snprintf(linearGradientFragShader, SHADER_TOTAL_LENGTH, "%s%s%s%s",
        STR_GRADIENT_FRAG_COMMON_VARIABLES,
        STR_LINEAR_GRADIENT_VARIABLES,
        STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
        STR_LINEAR_GRADIENT_MAIN
    );
    // radial gradient
    char radialGradientFragShader[SHADER_TOTAL_LENGTH];
    snprintf(radialGradientFragShader, SHADER_TOTAL_LENGTH, "%s%s%s%s",
        STR_GRADIENT_FRAG_COMMON_VARIABLES,
        STR_RADIAL_GRADIENT_VARIABLES,
        STR_GRADIENT_FRAG_COMMON_FUNCTIONS,
        STR_RADIAL_GRADIENT_MAIN
    );

    // service programs
    stencil = new GlProgram(STENCIL_VERT_SHADER, STENCIL_FRAG_SHADER);
    blit = new GlProgram(BLIT_VERT_SHADER, BLIT_FRAG_SHADER);
    // normal blend programs
    color = new GlProgram(COLOR_VERT_SHADER, COLOR_FRAG_SHADER);
    linear = new GlProgram(GRADIENT_VERT_SHADER, linearGradientFragShader);
    radial = new GlProgram(GRADIENT_VERT_SHADER, radialGradientFragShader);
    image = new GlProgram(IMAGE_VERT_SHADER, IMAGE_FRAG_SHADER);
    inited = true;
};


void GlPrograms::term()
{
    // blend programs
    for (uint32_t i = 0; i < 17; i++) {
        delete color_blend[i];
        delete image_blend[i];
        delete scene_blend[i];
        delete grad_blend[i];
        color_blend[i] = nullptr;
        image_blend[i] = nullptr;
        scene_blend[i] = nullptr;
        grad_blend[i] = nullptr;
    };
    // compose programs
    for (uint32_t i = 0; i < 11; i++) {
        delete compose[i];
        compose[i] = nullptr;
    };
    // normal blend programs
    delete image;
    delete radial;
    delete linear;
    delete color;
    image = radial = linear = color = nullptr;
    // service programs
    delete blit;
    delete stencil;
    blit = stencil = nullptr;
    inited = false;
};


// masking fragment programs
const char* GlPrograms::composeFragShaders[11] = {
    MASK_ALPHA_FRAG_SHADER,     // None
    MASK_ALPHA_FRAG_SHADER,     // Alpha
    MASK_INV_ALPHA_FRAG_SHADER, // InvAlpha,
    MASK_LUMA_FRAG_SHADER,      // Luma
    MASK_INV_LUMA_FRAG_SHADER,  // InvLuma
    MASK_ADD_FRAG_SHADER,       // Add
    MASK_SUB_FRAG_SHADER,       // Subtract
    MASK_INTERSECT_FRAG_SHADER, // Intersect
    MASK_DIFF_FRAG_SHADER,      // Difference
    MASK_LIGHTEN_FRAG_SHADER,   // Lighten
    MASK_DARKEN_FRAG_SHADER     // Darken
};


// blending functions
const char* GlPrograms::blendFuncs[17] = {
    NORMAL_BLEND_FRAG,      // Normal
    MULTIPLY_BLEND_FRAG,    // Multiply
    SCREEN_BLEND_FRAG,      // Screen
    OVERLAY_BLEND_FRAG,     // Overlay
    DARKEN_BLEND_FRAG,      // Darken
    LIGHTEN_BLEND_FRAG,     // Lighten
    COLOR_DODGE_BLEND_FRAG, // ColorDodge
    COLOR_BURN_BLEND_FRAG,  // ColorBurn
    HARD_LIGHT_BLEND_FRAG,  // HardLight
    SOFT_LIGHT_BLEND_FRAG,  // SoftLight
    DIFFERENCE_BLEND_FRAG,  // Difference
    EXCLUSION_BLEND_FRAG,   // Exclusion
    HUE_BLEND_FRAG,         // Hue
    SATURATION_BLEND_FRAG,  // Saturation
    COLOR_BLEND_FRAG,       // Color
    LUMINOSITY_BLEND_FRAG,  // Luminosity
    ADD_BLEND_FRAG          // Add
};


GlProgram* GlPrograms::getCompose(MaskMethod method)
{
    uint32_t index = (uint32_t)method;
    if (!compose[index])
        compose[index] = new GlProgram(MASK_VERT_SHADER, composeFragShaders[index]);
    return compose[index];
};


const char* GlPrograms::getBlendHelpers(BlendMethod method)
{
    if ((method == BlendMethod::Hue) ||
        (method == BlendMethod::Saturation) ||
        (method == BlendMethod::Color) ||
        (method == BlendMethod::Luminosity))
        return BLEND_FRAG_HSL;
    return "";
}


GlProgram* GlPrograms::getBlendColor(BlendMethod method)
{
    uint32_t index = (uint32_t)method;
    if (!color_blend[index]) {
        char fragShader[SHADER_TOTAL_LENGTH];
        snprintf(fragShader, SHADER_TOTAL_LENGTH, "%s%s%s", BLEND_SOLID_FRAG_HEADER, getBlendHelpers(method), blendFuncs[index]);
        color_blend[index] = new GlProgram(BLIT_VERT_SHADER, fragShader);
    }
    return color_blend[index];
};


GlProgram* GlPrograms::getBlendGrad(BlendMethod method)
{
    uint32_t index = (uint32_t)method;
    if (!grad_blend[index]) {
        char fragShader[SHADER_TOTAL_LENGTH];
        snprintf(fragShader, SHADER_TOTAL_LENGTH, "%s%s%s", BLEND_GRADIENT_FRAG_HEADER, getBlendHelpers(method), blendFuncs[index]);
        grad_blend[index] = new GlProgram(BLIT_VERT_SHADER, fragShader);
    }
    return grad_blend[index];
};


GlProgram* GlPrograms::getBlendImage(BlendMethod method)
{
    uint32_t index = (uint32_t)method;
    if (!image_blend[index]) {
        char fragShader[SHADER_TOTAL_LENGTH];
        snprintf(fragShader, SHADER_TOTAL_LENGTH, "%s%s%s", BLEND_IMAGE_FRAG_HEADER, getBlendHelpers(method), blendFuncs[index]);
        image_blend[index] = new GlProgram(BLIT_VERT_SHADER, fragShader);
    }
    return image_blend[index];
};


GlProgram* GlPrograms::getBlendScene(BlendMethod method)
{
    uint32_t index = (uint32_t)method;
    if (!scene_blend[index]) {
        char fragShader[SHADER_TOTAL_LENGTH];
        snprintf(fragShader, SHADER_TOTAL_LENGTH, "%s%s%s", BLEND_SCENE_FRAG_HEADER, getBlendHelpers(method), blendFuncs[index]);
        scene_blend[index] = new GlProgram(BLIT_VERT_SHADER, fragShader);
    }
    return scene_blend[index];
};
