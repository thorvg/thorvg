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

#include "tvgGlUniformTexture.h"
#include <string.h>


GlUniformTexture::GlUniformTexture()
{
    mStagingBuffer.reserve(GL_UNIFORM_TEX_WIDTH * 4 * GL_UNIFORM_TEX_MAX_DRAWS);
}


uint32_t GlUniformTexture::pushUniformData(const void* data, uint32_t sizeBytes)
{
    uint32_t drawId = mCurrentRow;
    
    uint32_t rowStartFloats = mCurrentRow * GL_UNIFORM_TEX_WIDTH * 4;
    uint32_t floatsNeeded = rowStartFloats + (sizeBytes / sizeof(float));
    
    if (floatsNeeded > mStagingBuffer.count) {
        uint32_t growBy = floatsNeeded - mStagingBuffer.count;
        if (mStagingBuffer.reserved < floatsNeeded) {
            mStagingBuffer.grow(growBy);
        }
        memset(mStagingBuffer.data + mStagingBuffer.count, 0, growBy * sizeof(float));
        mStagingBuffer.count = floatsNeeded;
    }
    
    memcpy(mStagingBuffer.data + rowStartFloats, data, sizeBytes);
    
    mCurrentRow++;
    mCurrentOffset = 0;
    mNeedsUpload = true;
    
    return drawId;
}


void GlUniformTexture::pushAtOffset(uint32_t offset, const float* data, uint32_t count)
{
    uint32_t rowStartFloats = mCurrentRow * GL_UNIFORM_TEX_WIDTH * 4;
    uint32_t totalOffset = rowStartFloats + offset;
    uint32_t floatsNeeded = totalOffset + count;
    
    if (floatsNeeded > mStagingBuffer.count) {
        uint32_t growBy = floatsNeeded - mStagingBuffer.count;
        if (mStagingBuffer.reserved < floatsNeeded) {
            mStagingBuffer.grow(growBy);
        }
        memset(mStagingBuffer.data + mStagingBuffer.count, 0, growBy * sizeof(float));
        mStagingBuffer.count = floatsNeeded;
    }
    
    memcpy(mStagingBuffer.data + totalOffset, data, count * sizeof(float));
    mNeedsUpload = true;
}


uint32_t GlUniformTexture::finishDrawCall()
{
    uint32_t drawId = mCurrentRow;
    mCurrentRow++;
    mCurrentOffset = 0;
    return drawId;
}

void GlUniformTexture::reset()
{
    mCurrentRow = 0;
    mCurrentOffset = 0;
    mNeedsUpload = false;
}


#define NOISE_LEVEL 0.5f

void GlUniformTexture::stageColorUniforms(uint32_t drawId, const float* matrix, float r, float g, float b, float a)
{
    // Pack 4 draws per row: each draw uses 4 columns (matrix[3] + color[1])
    uint32_t drawsPerRow = GL_UNIFORM_TEX_WIDTH / 4;
    uint32_t row = drawId / drawsPerRow;
    uint32_t colOffset = (drawId % drawsPerRow) * 4;
    
    uint32_t rowStartFloats = row * GL_UNIFORM_TEX_WIDTH * 4;
    uint32_t dataOffset = rowStartFloats + colOffset * 4;  // colOffset * 4 floats per column
    uint32_t floatsNeeded = dataOffset + 16;
    
    if (floatsNeeded > mStagingBuffer.count) {
        uint32_t growBy = floatsNeeded - mStagingBuffer.count;
        if (mStagingBuffer.reserved < floatsNeeded) {
            mStagingBuffer.grow(growBy);
        }
        memset(mStagingBuffer.data + mStagingBuffer.count, 0, growBy * sizeof(float));
        mStagingBuffer.count = floatsNeeded;
    }
    
    float* dst = mStagingBuffer.data + dataOffset;
    
    memcpy(dst, matrix, 12 * sizeof(float));
    
    dst[12] = r;
    dst[13] = g;
    dst[14] = b;
    dst[15] = a;
    
    if (row >= mCurrentRow) mCurrentRow = row + 1;
    mNeedsUpload = true;
}

#ifdef __ENABLE_FULL_UNIFORM_TEX__
void GlUniformTexture::stageLinearGradientUniforms(uint32_t drawId, const float* matrix, float depth, const float* invMatrix,
                                                        uint32_t nStops, float spread,
                                                        float x1, float y1, float x2, float y2,
                                                        const float* stopPoints, const float* stopColors)
{
    uint32_t rowStartFloats = drawId * GL_UNIFORM_TEX_WIDTH * 4;
    uint32_t floatsNeeded = rowStartFloats + 120;
    
    if (floatsNeeded > mStagingBuffer.count) {
        uint32_t growBy = floatsNeeded - mStagingBuffer.count;
        if (mStagingBuffer.reserved < floatsNeeded) {
            mStagingBuffer.grow(growBy);
        }
        memset(mStagingBuffer.data + mStagingBuffer.count, 0, growBy * sizeof(float));
        mStagingBuffer.count = floatsNeeded;
    }
    
    float* row = mStagingBuffer.data + rowStartFloats;
    uint32_t offset = 0;
    
    memcpy(row + offset, matrix, 12 * sizeof(float));
    offset += 12;
    
    row[offset++] = depth;
    row[offset++] = 0.0f;
    row[offset++] = 0.0f;
    row[offset++] = 0.0f;
    
    memcpy(row + offset, invMatrix, 12 * sizeof(float));
    offset += 12;
    
    row[offset++] = static_cast<float>(nStops);
    row[offset++] = NOISE_LEVEL;
    row[offset++] = spread;
    row[offset++] = 0.0f;
    
    row[offset++] = x1;
    row[offset++] = y1;
    row[offset++] = 0.0f;
    row[offset++] = 0.0f;
    row[offset++] = x2;
    row[offset++] = y2;
    row[offset++] = 0.0f;
    row[offset++] = 0.0f;
    
    uint32_t stopsToCopy = (nStops < GL_UNIFORM_TEX_MAX_STOPS) ? nStops : GL_UNIFORM_TEX_MAX_STOPS;
    memcpy(row + offset, stopPoints, stopsToCopy * sizeof(float));
    offset += 16;
    
    memcpy(row + offset, stopColors, stopsToCopy * 4 * sizeof(float));
    
    if (drawId >= mCurrentRow) mCurrentRow = drawId + 1;
    mNeedsUpload = true;
}


void GlUniformTexture::stageRadialGradientUniforms(uint32_t drawId, const float* matrix, float depth, const float* invMatrix,
                                                        uint32_t nStops, float spread,
                                                        float fx, float fy, float cx, float cy,
                                                        float fr, float r,
                                                        const float* stopPoints, const float* stopColors)
{
    uint32_t rowStartFloats = drawId * GL_UNIFORM_TEX_WIDTH * 4;
    uint32_t floatsNeeded = rowStartFloats + 120;
    
    if (floatsNeeded > mStagingBuffer.count) {
        uint32_t growBy = floatsNeeded - mStagingBuffer.count;
        if (mStagingBuffer.reserved < floatsNeeded) {
            mStagingBuffer.grow(growBy);
        }
        memset(mStagingBuffer.data + mStagingBuffer.count, 0, growBy * sizeof(float));
        mStagingBuffer.count = floatsNeeded;
    }
    
    float* row = mStagingBuffer.data + rowStartFloats;
    uint32_t offset = 0;
    
    memcpy(row + offset, matrix, 12 * sizeof(float));
    offset += 12;
    
    row[offset++] = depth;
    row[offset++] = 0.0f;
    row[offset++] = 0.0f;
    row[offset++] = 0.0f;
    
    memcpy(row + offset, invMatrix, 12 * sizeof(float));
    offset += 12;
    
    row[offset++] = static_cast<float>(nStops);
    row[offset++] = NOISE_LEVEL;
    row[offset++] = spread;
    row[offset++] = 1.0f;
    
    row[offset++] = fx;
    row[offset++] = fy;
    row[offset++] = cx;
    row[offset++] = cy;
    row[offset++] = fr;
    row[offset++] = r;
    row[offset++] = 0.0f;
    row[offset++] = 0.0f;
    
    uint32_t stopsToCopy = (nStops < GL_UNIFORM_TEX_MAX_STOPS) ? nStops : GL_UNIFORM_TEX_MAX_STOPS;
    memcpy(row + offset, stopPoints, stopsToCopy * sizeof(float));
    offset += 16;
    
    memcpy(row + offset, stopColors, stopsToCopy * 4 * sizeof(float));
    
    if (drawId >= mCurrentRow) mCurrentRow = drawId + 1;
    mNeedsUpload = true;
}

#endif //__ENABLE_FULL_UNIFORM_TEX