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

#ifndef _TVG_GL_UNIFORM_TEXTURE_H_
#define _TVG_GL_UNIFORM_TEXTURE_H_

#include "tvgArray.h"

#define GL_UNIFORM_TEX_WIDTH 16
#define GL_UNIFORM_TEX_MAX_DRAWS 4096
#define GL_UNIFORM_TEX_MAX_STOPS 16
#define GL_UNIFORM_TEX_UNIT 7


struct GlColorUniformData
{
    float matrix[12];
    float color[4];
};
#ifdef __ENABLE_FULL_UNIFORM_TEX__
struct GlGradientUniformData
{
    float matrix[12];
    float depth;
    float reserved[3];
    float invMatrix[12];
    float nStops;
    float noiseLevel;
    float spread;
    float gradientType;
    float gradientParams[8];
    float stopPoints[GL_UNIFORM_TEX_MAX_STOPS];
    float stopColors[GL_UNIFORM_TEX_MAX_STOPS * 4];
};
#endif //__ENABLE_FULL_UNIFORM_TEX__
class GlUniformTexture
{
public:
    GlUniformTexture();
    ~GlUniformTexture() = default;

    uint32_t pushUniformData(const void* data, uint32_t sizeBytes);

    void pushAtOffset(uint32_t offset, const float* data, uint32_t count);

    uint32_t finishDrawCall();

    void reset();

    bool needsUpload() const { return mNeedsUpload; }

    void markUploaded() { mNeedsUpload = false; }

    uint32_t getDrawCallCount() const { return mCurrentRow; }

    uint32_t getWidth() const { return GL_UNIFORM_TEX_WIDTH; }

    void stageColorUniforms(uint32_t drawId, const float* matrix, float r, float g, float b, float a);

    void stageLinearGradientUniforms(uint32_t drawId, const float* matrix, float depth, const float* invMatrix,
                                          uint32_t nStops, float spread,
                                          float x1, float y1, float x2, float y2,
                                          const float* stopPoints, const float* stopColors);

    void stageRadialGradientUniforms(uint32_t drawId, const float* matrix, float depth, const float* invMatrix,
                                          uint32_t nStops, float spread,
                                          float fx, float fy, float cx, float cy,
                                          float fr, float r,
                                          const float* stopPoints, const float* stopColors);

    void debugDumpStagingBuffer() const;

    void debugDumpDrawCall(uint32_t drawId) const;

    const float* getStagingData() const { return mStagingBuffer.data; }

    uint32_t getStagingSize() const { return mStagingBuffer.count; }

private:
    uint32_t mCurrentRow = 0;
    uint32_t mCurrentOffset = 0;
    Array<float> mStagingBuffer;
    bool mNeedsUpload = false;
};

#endif
