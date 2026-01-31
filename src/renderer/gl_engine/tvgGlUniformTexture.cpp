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

#include <string.h>
#include <algorithm>
#include "tvgGlUniformTexture.h"


namespace tvg
{

GlUniformTexture::GlUniformTexture()
{
    stagingBuffer.reserve(GL_UNIFORM_TEX_WIDTH * 4 * GL_UNIFORM_TEX_DEFAULT_HEIGHT);
}


GlUniformTexture::~GlUniformTexture()
{
    if (textureIds[0] != 0) glDeleteTextures(GL_UNIFORM_TEX_SLOTS, textureIds);
}


void GlUniformTexture::reset()
{
    if (currentRow > peakRowsThisFrame) {
        peakRowsThisFrame = currentRow;
    }
    if (currentRow > 0) {
        updateShrinkHysteresis();
    }

    currentRow = 0;
    currentOffset = 0;
    needsUpload = false;
    peakRowsThisFrame = 0;
    textureIndex = (textureIndex + 1) % GL_UNIFORM_TEX_SLOTS;
}


void GlUniformTexture::stageColorUniforms(uint32_t drawId, const float* matrix, float r, float g, float b, float a)
{
    uint32_t drawsPerRow = GL_UNIFORM_TEX_WIDTH / 4;
    uint32_t row = drawId / drawsPerRow;
    uint32_t colOffset = (drawId % drawsPerRow) * 4;

    uint32_t rowStartFloats = row * GL_UNIFORM_TEX_WIDTH * 4;
    uint32_t dataOffset = rowStartFloats + colOffset * 4;
    uint32_t floatsNeeded = dataOffset + 16;

    if (floatsNeeded > stagingBuffer.count) {
        uint32_t growBy = floatsNeeded - stagingBuffer.count;
        if (stagingBuffer.reserved < floatsNeeded) {
            stagingBuffer.grow(growBy);
        }
        memset(stagingBuffer.data + stagingBuffer.count, 0, growBy * sizeof(float));
        stagingBuffer.count = floatsNeeded;
    }

    float* dst = stagingBuffer.data + dataOffset;

    memcpy(dst, matrix, 12 * sizeof(float));

    dst[12] = r;
    dst[13] = g;
    dst[14] = b;
    dst[15] = a;

    if (row >= currentRow) currentRow = row + 1;
    needsUpload = true;
}


static uint32_t _nextPowerOfTwo(uint32_t n)
{
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}


uint32_t GlUniformTexture::computeRequiredHeight(uint32_t rows)
{
    uint32_t requiredHeight = _nextPowerOfTwo(rows);
    if (textureHeight > 0) requiredHeight = std::max(requiredHeight, textureHeight * GL_UNIFORM_TEX_GROWTH_FACTOR);
    return std::max(requiredHeight, (uint32_t)GL_UNIFORM_TEX_MIN_HEIGHT);
}


bool GlUniformTexture::resizeTexture(uint32_t newHeight)
{
    if (maxTextureSize == 0) {
        GLint maxSize = 0;
        GL_CHECK(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize));
        maxTextureSize = static_cast<uint32_t>(maxSize);
    }

    if (newHeight > maxTextureSize) {
        TVGERR("GL_ENGINE", "Uniform texture height %u exceeds GL_MAX_TEXTURE_SIZE %u, clamping",
               newHeight, maxTextureSize);
        newHeight = maxTextureSize;
    }

    if (newHeight == textureHeight && textureIds[0] != 0) return false;

    if (textureIds[0] != 0) {
        GL_CHECK(glDeleteTextures(GL_UNIFORM_TEX_SLOTS, textureIds));
        memset(textureIds, 0, sizeof(textureIds));
    }

    GL_CHECK(glGenTextures(GL_UNIFORM_TEX_SLOTS, textureIds));

    for (uint32_t i = 0; i < GL_UNIFORM_TEX_SLOTS; ++i) {
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureIds[i]));
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, GL_UNIFORM_TEX_WIDTH, newHeight, 0, GL_RGBA, GL_FLOAT, nullptr));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    }

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    uint32_t oldHeight = textureHeight;
    textureHeight = newHeight;

    if (newHeight > oldHeight) {
        totalGrowthCount++;
        TVGLOG("GL_ENGINE", "Uniform texture grown: %u -> %u rows (growth #%u)", oldHeight, newHeight, totalGrowthCount);
    } else {
        totalShrinkCount++;
        TVGLOG("GL_ENGINE", "Uniform texture shrunk: %u -> %u rows (shrink #%u)", oldHeight, newHeight, totalShrinkCount);
    }

    return true;
}


void GlUniformTexture::updateShrinkHysteresis()
{
    if (textureHeight == 0) return;

    float usage = static_cast<float>(peakRowsThisFrame) / textureHeight;

    if (usage < GL_UNIFORM_TEX_SHRINK_THRESHOLD) {
        lowUsageFrameCount++;

        if (lowUsageFrameCount >= GL_UNIFORM_TEX_SHRINK_FRAMES) {
            uint32_t targetHeight = _nextPowerOfTwo(peakRowsThisFrame);
            targetHeight = std::max(targetHeight, (uint32_t)GL_UNIFORM_TEX_MIN_HEIGHT);

            if (targetHeight < textureHeight / 2) {
                resizeTexture(targetHeight);
            }

            lowUsageFrameCount = 0;
        }
    } else {
        lowUsageFrameCount = 0;
    }
}


void GlUniformTexture::ensure()
{
    if (maxTextureSize == 0) {
        GLint maxSize = 0;
        GL_CHECK(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize));
        maxTextureSize = static_cast<uint32_t>(maxSize);
    }

    if (textureIds[0] == 0) {
        textureHeight = GL_UNIFORM_TEX_DEFAULT_HEIGHT;
        resizeTexture(textureHeight);
    }
}


void GlUniformTexture::upload()
{
    if (!needsUpload || currentRow == 0) return;

    if (currentRow > textureHeight) {
        uint32_t newHeight = computeRequiredHeight(currentRow);
        resizeTexture(newHeight);
    }

    ensure();

    if (currentRow > peakRowsThisFrame) {
        peakRowsThisFrame = currentRow;
    }

    GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureIds[textureIndex]));
    GL_CHECK(glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0, 0,
        GL_UNIFORM_TEX_WIDTH,
        currentRow,
        GL_RGBA,
        GL_FLOAT,
        stagingBuffer.data));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    needsUpload = false;
}

}
