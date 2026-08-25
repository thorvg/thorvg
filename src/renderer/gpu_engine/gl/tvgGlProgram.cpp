/*
 * Copyright (c) 2020 - 2026 ThorVG project. All rights reserved.

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

#include "tvgGlProgram.h"

static constexpr int32_t UNKNOWN_LOCATION = -2;
static constexpr uint32_t UNKNOWN_BINDING = UINT32_MAX;

static const char* const UNIFORM_NAMES[] = {
    "uDepth",
    "uViewMatrix",
    "uSrcTexture",
    "uBlrTexture",
    "uDstTexture",
    "uMaskTexture",
    "uTexture",
};

static const char* const UNIFORM_BLOCK_NAMES[] = {
    "Gaussian",
    "Viewport",
    "DropShadow",
    "Params",
    "BlendRegion",
    "TransformInfo",
    "GradientInfo",
    "ColorInfo",
};

static_assert(sizeof(UNIFORM_NAMES) / sizeof(UNIFORM_NAMES[0]) == static_cast<uint32_t>(GlShaderUniform::Count), "Missing uniform name");
static_assert(sizeof(UNIFORM_BLOCK_NAMES) / sizeof(UNIFORM_BLOCK_NAMES[0]) == static_cast<uint32_t>(GlShaderUniformBlock::Count), "Missing uniform block name");

GlProgram::GlProgram(const char* vertSrc, const char* fragSrc)
{
    for (auto& location : mUniformLocations)
        location = UNKNOWN_LOCATION;
    for (auto& index : mUniformBlockIndices)
        index = UNKNOWN_LOCATION;
    for (auto& binding : mUniformBlockBindings)
        binding = UNKNOWN_BINDING;
    for (auto& binding : mSamplerBindings)
        binding = -1;

    auto shader = GlShader(vertSrc, fragSrc);

    // Create the program object
    uint32_t progObj = glCreateProgram();
    assert(progObj);

    glAttachShader(progObj, shader.getVertexShader());
    glAttachShader(progObj, shader.getFragmentShader());

    // Link the program
    glLinkProgram(progObj);

    // Check the link status
    GLint linked;
    glGetProgramiv(progObj, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(progObj, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            auto infoLog = tvg::malloc<char>(sizeof(char) * infoLen);
            glGetProgramInfoLog(progObj, infoLen, NULL, infoLog);
            TVGERR("GL_ENGINE", "Error linking shader: %s", infoLog);
            tvg::free(infoLog);

        }
        glDeleteProgram(progObj);
        progObj = 0;
        assert(0);
    }
    mProgramObj = progObj;
}


GlProgram::~GlProgram()
{
    glDeleteProgram(mProgramObj);
}

int32_t GlProgram::getUniformLocation(GlShaderUniform uniform)
{
    auto index = static_cast<uint32_t>(uniform);
    auto& location = mUniformLocations[index];
    if (location != UNKNOWN_LOCATION) return location;

    GL_CHECK(location = glGetUniformLocation(mProgramObj, UNIFORM_NAMES[index]));
    return location;
}

int32_t GlProgram::getUniformBlockIndex(GlShaderUniformBlock block)
{
    auto type = static_cast<uint32_t>(block);
    auto& index = mUniformBlockIndices[type];
    if (index != UNKNOWN_LOCATION) return index;

    uint32_t blockIndex;
    GL_CHECK(blockIndex = glGetUniformBlockIndex(mProgramObj, UNIFORM_BLOCK_NAMES[type]));
    index = blockIndex == GL_INVALID_INDEX ? -1 : static_cast<int32_t>(blockIndex);
    return index;
}

uint32_t GlProgram::getProgramId()
{
    return mProgramObj;
}

bool GlProgram::setUniformBlockBinding(GlShaderUniformBlock block, uint32_t bindingPoint)
{
    auto blockIndex = getUniformBlockIndex(block);
    if (blockIndex < 0) return false;

    auto& binding = mUniformBlockBindings[static_cast<uint32_t>(block)];
    if (binding != UNKNOWN_BINDING) return binding == bindingPoint;

    GL_CHECK(glUniformBlockBinding(mProgramObj, static_cast<uint32_t>(blockIndex), bindingPoint));
    binding = bindingPoint;
    return true;
}

void GlProgram::setSampler(GlShaderUniform uniform, int32_t textureUnit)
{
    auto location = getUniformLocation(uniform);
    if (location < 0) return;

    auto& binding = mSamplerBindings[static_cast<uint32_t>(uniform)];
    if (binding == textureUnit) return;

    GL_CHECK(glUniform1iv(location, 1, &textureUnit));
    binding = textureUnit;
}
