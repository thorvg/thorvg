/*
 * Copyright (c) 2020 - 2026 the ThorVG project. All rights reserved.

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

#include "tvgGlRenderTask.h"

#define GL_UNIFORM_TEX_WIDTH 32  // Initial width to fit full gradient rows; can be optimized later.
#define GL_UNIFORM_TEX_MAX_STOPS 16
#define GL_UNIFORM_TEX_UNIT 7
#define GL_UNIFORM_TEX_SLOTS 1

// Dynamic texture management defaults (can be overridden per instance)
#define GL_UNIFORM_TEX_DEFAULT_HEIGHT 256      // Initial texture height (power of two)
#define GL_UNIFORM_TEX_MIN_HEIGHT 64           // Minimum texture height
#define GL_UNIFORM_TEX_GROWTH_FACTOR 2         // Growth multiplier when resizing
#define GL_UNIFORM_TEX_SHRINK_THRESHOLD 0.30f  // Shrink when usage below 30%
#define GL_UNIFORM_TEX_SHRINK_FRAMES 60        // Sustained low usage frames before shrink

struct GlUniformTextureConfig
{
    uint32_t defaultHeight = GL_UNIFORM_TEX_DEFAULT_HEIGHT;
    uint32_t minHeight = GL_UNIFORM_TEX_MIN_HEIGHT;
    uint32_t growthFactor = GL_UNIFORM_TEX_GROWTH_FACTOR;
    float shrinkThreshold = GL_UNIFORM_TEX_SHRINK_THRESHOLD;
    uint32_t shrinkFrames = GL_UNIFORM_TEX_SHRINK_FRAMES;
};

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
struct GlUniformTexture
{
    explicit GlUniformTexture(const GlUniformTextureConfig& config = {});
    ~GlUniformTexture();

    uint32_t pushUniformData(const void* data, uint32_t sizeBytes);

    void pushAtOffset(uint32_t offset, const float* data, uint32_t count);

    uint32_t finishDrawCall();

    void reset();

    void stageColorUniforms(uint32_t drawId, const float* matrix, float r, float g, float b, float a);
    void stageImageUniforms(uint32_t drawId, const float* matrix, float format, float flipY, float opacity);

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

    void ensure();
    // TODO: PBO could enable async texture uploads on GLES3, but requires ping-pong
    // buffers and WebGL2 lacks glMapBufferRange. Not worth the added complexity.
    void upload();
    GLuint getTextureId() const { return textureIds[textureIndex]; }

    uint32_t nextPowerOfTwo(uint32_t n);
    uint32_t computeRequiredHeight(uint32_t rows);
    bool resizeTexture(uint32_t newHeight);
    void updateShrinkHysteresis();

    uint32_t currentRow = 0;
    uint32_t currentOffset = 0;
    Array<float> stagingBuffer;
    bool needsUpload = false;
    GLuint textureIds[GL_UNIFORM_TEX_SLOTS] = {};
    uint32_t textureIndex = 0;

    uint32_t textureHeight = 0;
    uint32_t maxTextureSize = 0;
    uint32_t peakRowsThisFrame = 0;
    uint32_t lowUsageFrameCount = 0;

    uint32_t totalGrowthCount = 0;
    uint32_t totalShrinkCount = 0;
    GlUniformTextureConfig config;
};

#endif
