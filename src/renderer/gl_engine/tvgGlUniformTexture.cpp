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

void GlUniformTexture::stageColorUniforms(uint32_t drawId, const float* matrix, float depth, float r, float g, float b, float a)
{
    uint32_t rowStartFloats = drawId * GL_UNIFORM_TEX_WIDTH * 4;
    uint32_t floatsNeeded = rowStartFloats + 20;
    
    if (floatsNeeded > mStagingBuffer.count) {
        uint32_t growBy = floatsNeeded - mStagingBuffer.count;
        if (mStagingBuffer.reserved < floatsNeeded) {
            mStagingBuffer.grow(growBy);
        }
        memset(mStagingBuffer.data + mStagingBuffer.count, 0, growBy * sizeof(float));
        mStagingBuffer.count = floatsNeeded;
    }
    
    float* row = mStagingBuffer.data + rowStartFloats;
    
    memcpy(row, matrix, 12 * sizeof(float));
    
    row[12] = depth;
    row[13] = 0.0f;
    row[14] = 0.0f;
    row[15] = 0.0f;
    
    row[16] = r;
    row[17] = g;
    row[18] = b;
    row[19] = a;
    
    if (drawId >= mCurrentRow) mCurrentRow = drawId + 1;
    mNeedsUpload = true;
}


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


void GlUniformTexture::debugDumpStagingBuffer() const
{
    printf("\n========== GlUniformTexture Staging Buffer Dump ==========\n");
    printf("Total draw calls staged: %u\n", mCurrentRow);
    printf("Staging buffer size: %u floats (%.2f KB)\n", mStagingBuffer.count, 
           mStagingBuffer.count * sizeof(float) / 1024.0f);
    printf("Texture dimensions: %d x %d texels\n", GL_UNIFORM_TEX_WIDTH, GL_UNIFORM_TEX_MAX_DRAWS);
    printf("===========================================================\n\n");
    
    for (uint32_t i = 0; i < mCurrentRow; ++i) {
        debugDumpDrawCall(i);
    }
}


void GlUniformTexture::debugDumpDrawCall(uint32_t drawId) const
{
    if (drawId >= mCurrentRow) {
        printf("Draw call %u: NOT STAGED\n", drawId);
        return;
    }
    
    uint32_t rowStartFloats = drawId * GL_UNIFORM_TEX_WIDTH * 4;
    const float* row = mStagingBuffer.data + rowStartFloats;
    
    printf("--- Draw Call %u ---\n", drawId);
    
    printf("  Transform Matrix:\n");
    printf("    | %8.4f %8.4f %8.4f |\n", row[0], row[4], row[8]);
    printf("    | %8.4f %8.4f %8.4f |\n", row[1], row[5], row[9]);
    printf("    | %8.4f %8.4f %8.4f |\n", row[2], row[6], row[10]);
    
    printf("  Depth: %.4f\n", row[12]);
    
    printf("  Color: (%.4f, %.4f, %.4f, %.4f)\n", row[16], row[17], row[18], row[19]);
    
    bool hasGradient = (row[28] != 0.0f || row[29] != 0.0f || row[30] != 0.0f);
    
    if (hasGradient || row[28] != 0.0f) {
        printf("  Inverse Matrix:\n");
        printf("    | %8.4f %8.4f %8.4f |\n", row[16], row[20], row[24]);
        printf("    | %8.4f %8.4f %8.4f |\n", row[17], row[21], row[25]);
        printf("    | %8.4f %8.4f %8.4f |\n", row[18], row[22], row[26]);
        
        uint32_t nStops = static_cast<uint32_t>(row[28]);
        float noiseLevel = row[29];
        float spread = row[30];
        float gradType = row[31];
        
        printf("  Gradient Type: %s\n", gradType == 0.0f ? "Linear" : "Radial");
        printf("  nStops: %u, noiseLevel: %.4f, spread: %.1f\n", nStops, noiseLevel, spread);
        
        if (gradType == 0.0f) {
            printf("  Start: (%.4f, %.4f), End: (%.4f, %.4f)\n", row[32], row[33], row[36], row[37]);
        } else {
            printf("  Focal: (%.4f, %.4f), Center: (%.4f, %.4f)\n", row[32], row[33], row[34], row[35]);
            printf("  Focal Radius: %.4f, Outer Radius: %.4f\n", row[36], row[37]);
        }
        
        if (nStops > 0 && nStops <= GL_UNIFORM_TEX_MAX_STOPS) {
            printf("  Stops:\n");
            for (uint32_t s = 0; s < nStops && s < 4; ++s) {
                uint32_t posOffset = 40 + s;
                uint32_t colorOffset = 56 + s * 4;
                printf("    [%u] pos=%.4f color=(%.2f, %.2f, %.2f, %.2f)\n",
                       s, row[posOffset],
                       row[colorOffset], row[colorOffset + 1], row[colorOffset + 2], row[colorOffset + 3]);
            }
            if (nStops > 4) printf("    ... and %u more stops\n", nStops - 4);
        }
    }
    
    printf("\n");
}
