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

namespace tvg
{

constexpr uint32_t GL_UNIFORM_TEX_WIDTH = 16;
constexpr uint32_t GL_UNIFORM_TEX_MAX_STOPS = 16;
constexpr uint32_t GL_UNIFORM_TEX_UNIT = 7;
constexpr uint32_t GL_UNIFORM_TEX_SLOTS = 1;

// Dynamic texture management constants
constexpr uint32_t GL_UNIFORM_TEX_DEFAULT_HEIGHT = 256;      // Initial texture height (power of two)
constexpr uint32_t GL_UNIFORM_TEX_MIN_HEIGHT = 64;           // Minimum texture height
constexpr uint32_t GL_UNIFORM_TEX_GROWTH_FACTOR = 2;         // Growth multiplier when resizing
constexpr float GL_UNIFORM_TEX_SHRINK_THRESHOLD = 0.30f;     // Shrink when usage below 30%
constexpr uint32_t GL_UNIFORM_TEX_SHRINK_FRAMES = 60;        // Sustained low usage frames before shrink

struct GlColorUniformData
{
    float matrix[12];
    float color[4];
};

struct GlUniformTexture
{
    GlUniformTexture();
    ~GlUniformTexture();
    void reset();
    void stageColorUniforms(uint32_t drawId, const float* matrix, float r, float g, float b, float a);
    void ensure();
    // TODO: PBO could enable async texture uploads on GLES3, but requires ping-pong
    // buffers and WebGL2 lacks glMapBufferRange. Not worth the added complexity.
    void upload();
    GLuint getTextureId() const { return textureIds[textureIndex]; }
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
};

}

#endif
