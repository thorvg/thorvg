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

#ifndef _TVG_GL_PROGRAM_H_
#define _TVG_GL_PROGRAM_H_

#include "tvgGlShader.h"

enum class GlShaderUniform : uint8_t
{
    Depth,
    ViewMatrix,
    Color,
    SourceTexture,
    BlurTexture,
    DestinationTexture,
    MaskTexture,
    Texture,
    Count,
};

enum class GlShaderUniformBlock : uint8_t
{
    Gaussian,
    Viewport,
    DropShadow,
    Params,
    BlendRegion,
    TransformInfo,
    GradientInfo,
    ColorInfo,
    Count,
};

struct GlProgram
{
    GlProgram(const char* vertSrc, const char* fragSrc);
    ~GlProgram();

    int32_t getUniformLocation(GlShaderUniform uniform);
    int32_t getUniformBlockIndex(GlShaderUniformBlock block);
    uint32_t getProgramId();
    bool setUniformBlockBinding(GlShaderUniformBlock block, uint32_t bindingPoint);
    void setSampler(GlShaderUniform uniform, int32_t textureUnit);

private:
    static void unload();

    uint32_t mProgramObj;
    int32_t mUniformLocations[static_cast<uint32_t>(GlShaderUniform::Count)];
    int32_t mUniformBlockIndices[static_cast<uint32_t>(GlShaderUniformBlock::Count)];
    uint32_t mUniformBlockBindings[static_cast<uint32_t>(GlShaderUniformBlock::Count)];
    int32_t mSamplerBindings[static_cast<uint32_t>(GlShaderUniform::Count)];
};

#endif /* _TVG_GL_PROGRAM_H_ */
