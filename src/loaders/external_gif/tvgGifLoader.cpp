/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <memory.h>
#include "tvgGifLoader.h"
#include "tvgMath.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void GifLoader::clear()
{
    if (gifFile) {
        DGifCloseFile(gifFile, nullptr);
        gifFile = nullptr;
    }
    if (freeData && fileData) {
        tvg::free(fileData);
        fileData = nullptr;
    }
    
    tvg::free(canvas);
    canvas = nullptr;
    surface.buf8 = nullptr;
    freeData = false;
}

void GifLoader::compositeFrame(uint32_t frameIndex)
{
    if (!gifFile || frameIndex >= (uint32_t)gifFile->ImageCount) return;
    if (!canvas) return;
    
    SavedImage* frame = &gifFile->SavedImages[frameIndex];
    GifImageDesc* imageDesc = &frame->ImageDesc;
    
    // Get graphics control block for this frame
    GraphicsControlBlock gcb;
    DGifSavedExtensionToGCB(gifFile, frameIndex, &gcb);
    
    // Handle disposal of previous frame
    if (frameIndex > 0)
    {
        GraphicsControlBlock prevGcb;
        DGifSavedExtensionToGCB(gifFile, frameIndex - 1, &prevGcb);
        
        if (prevGcb.DisposalMode == DISPOSE_BACKGROUND)
        {
            // Clear previous frame area to transparent
            SavedImage* prevFrame = &gifFile->SavedImages[frameIndex - 1];
            GifImageDesc* prevDesc = &prevFrame->ImageDesc;
            
            uint32_t* canvas32 = (uint32_t*)canvas;
            
            // Calculate valid bounds
            int startY = (prevDesc->Top < 0) ? 0 : prevDesc->Top;
            int endY = prevDesc->Top + prevDesc->Height;
            if (endY > gifFile->SHeight) endY = gifFile->SHeight;
            
            int startX = (prevDesc->Left < 0) ? 0 : prevDesc->Left;
            int endX = prevDesc->Left + prevDesc->Width;
            if (endX > gifFile->SWidth) endX = gifFile->SWidth;
            
            for (int y = startY; y < endY; y++)
            {
                int canvasIdx = y * gifFile->SWidth + startX;
                int width = endX - startX;
                // Use memset for the row
                if (width > 0) {
                    memset(&canvas32[canvasIdx], 0, width * sizeof(uint32_t));
                }
            }
        }
    }
    
    // Get color map
    ColorMapObject* colorMap = imageDesc->ColorMap ? imageDesc->ColorMap : gifFile->SColorMap;
    if (!colorMap) return;
    
    uint32_t* canvas32 = (uint32_t*)canvas;
    
    // Pre-calculate valid bounds once
    int startY = (imageDesc->Top < 0) ? -imageDesc->Top : 0;
    int endY = imageDesc->Height;
    if (imageDesc->Top + endY > gifFile->SHeight) {
        endY = gifFile->SHeight - imageDesc->Top;
    }
    
    int startX = (imageDesc->Left < 0) ? -imageDesc->Left : 0;
    int endX = imageDesc->Width;
    if (imageDesc->Left + endX > gifFile->SWidth) {
        endX = gifFile->SWidth - imageDesc->Left;
    }
    
    // Early exit if frame is completely out of bounds
    if (imageDesc->Top >= gifFile->SHeight || imageDesc->Left >= gifFile->SWidth) return;
    if (imageDesc->Top + imageDesc->Height <= 0 || imageDesc->Left + imageDesc->Width <= 0) return;
    
    // Composite frame onto canvas
    for (int y = startY; y < endY; y++)
    {
        int canvasY = imageDesc->Top + y;
        int frameIdx = y * imageDesc->Width + startX;
        int canvasIdx = canvasY * gifFile->SWidth + imageDesc->Left + startX;
        
        for (int x = startX; x < endX; x++, frameIdx++, canvasIdx++)
        {
            uint8_t colorIndex = frame->RasterBits[frameIdx];
            
            // Check transparency
            if (gcb.TransparentColor != NO_TRANSPARENT_COLOR && 
                colorIndex == gcb.TransparentColor) {
                continue;
            }
            
            if (colorIndex < colorMap->ColorCount) {
                GifColorType color = colorMap->Colors[colorIndex];
                // Pack BGRA
                canvas32[canvasIdx] = 0xFF000000 | 
                                     (color.Blue << 0) | 
                                     (color.Green << 8) | 
                                     (color.Red << 16);
            }
        }
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

void GifLoader::calculateFrameRate()
{
    if (!gifFile || gifFile->ImageCount == 0) return;
    
    uint32_t totalDelay = 0;
    for (int i = 0; i < gifFile->ImageCount; i++) {
        GraphicsControlBlock gcb;
        DGifSavedExtensionToGCB(gifFile, i, &gcb);
        totalDelay += gcb.DelayTime;
    }
    if (totalDelay > 0) {
        frameRate = (gifFile->ImageCount * 100.0f) / totalDelay;
    } else {
        frameRate = 10.0f;  // Default to 10 fps
    }
}

GifLoader::GifLoader() : FrameModule(FileType::Gif)
{
}

GifLoader::~GifLoader()
{
    clear();
    // Surface points to canvas, which is freed in clear()
}

bool GifLoader::open(const char* path)
{
#ifdef THORVG_FILE_IO_SUPPORT
    int error = 0;
    gifFile = DGifOpenFileName(path, &error);
    if (!gifFile) return false;

    if (DGifSlurp(gifFile) != GIF_OK) {
        clear();
        return false;
    }

    w = static_cast<float>(gifFile->SWidth);
    h = static_cast<float>(gifFile->SHeight);
    segmentEnd = static_cast<float>(gifFile->ImageCount);

    // Calculate frame rate from delays
    calculateFrameRate();

    // Allocate canvas
    canvas = tvg::malloc<uint8_t*>(gifFile->SWidth * gifFile->SHeight * 4);
    if (!canvas) {
        clear();
        return false;
    }

    return true;
#else
    return false;
#endif
}

// Memory reading context for giflib
struct MemoryReader {
    const uint8_t* data;
    uint32_t size;
    uint32_t pos;
};

// Custom InputFunc for reading from memory buffer
static int memoryInputFunc(GifFileType* gif, GifByteType* buffer, int length)
{
    MemoryReader* reader = static_cast<MemoryReader*>(gif->UserData);
    if (!reader || reader->pos >= reader->size) return 0;
    
    int bytesToRead = length;
    if (reader->pos + bytesToRead > reader->size) {
        bytesToRead = reader->size - reader->pos;
    }
    
    if (bytesToRead > 0) {
        memcpy(buffer, reader->data + reader->pos, bytesToRead);
        reader->pos += bytesToRead;
    }
    
    return bytesToRead;
}

bool GifLoader::open(const char* data, uint32_t size, TVG_UNUSED const char* rpath, bool copy)
{
    if (copy) {
        fileData = tvg::malloc<unsigned char*>(size);
        if (!fileData) return false;
        memcpy((unsigned char *)fileData, data, size);
        freeData = true;
    } else {
        fileData = (unsigned char *) data;
        freeData = false;
    }
    fileSize = size;
    
    // Create memory reader context
    MemoryReader* reader = tvg::malloc<MemoryReader*>(sizeof(MemoryReader));
    if (!reader) {
        if (freeData) tvg::free(fileData);
        return false;
    }
    reader->data = fileData;
    reader->size = fileSize;
    reader->pos = 0;
    
    // Open GIF from memory using custom InputFunc
    int error = 0;
    gifFile = DGifOpen(reader, memoryInputFunc, &error);
    if (!gifFile) {
        TVGERR("GIF", "Failed to open GIF from memory: error code %d", error);
        tvg::free(reader);
        if (freeData) tvg::free(fileData);
        return false;
    }
    
    if (DGifSlurp(gifFile) != GIF_OK) {
        TVGERR("GIF", "Failed to read GIF from memory");
        tvg::free(reader);
        clear();
        return false;
    }
    
    // Free reader (gifFile has read all data)
    tvg::free(reader);
    
    w = static_cast<float>(gifFile->SWidth);
    h = static_cast<float>(gifFile->SHeight);
    segmentEnd = static_cast<float>(gifFile->ImageCount);
    
    // Calculate frame rate from delays
    calculateFrameRate();
    
    // Allocate canvas
    canvas = tvg::malloc<uint8_t*>(gifFile->SWidth * gifFile->SHeight * 4);
    if (!canvas) {
        clear();
        return false;
    }
    
    return true;
}

bool GifLoader::read()
{
    if (!LoadModule::read()) return true;
    if (!gifFile || gifFile->ImageCount == 0) return false;
    
    surface.cs = ImageLoader::cs;
    
    // Decode and composite frame 0
    memset(canvas, 0, gifFile->SWidth * gifFile->SHeight * 4);
    compositeFrame(0);
    lastCompositedFrame = 0;
    
    surface.buf8 = canvas;
    surface.stride = gifFile->SWidth;
    surface.w = gifFile->SWidth;
    surface.h = gifFile->SHeight;
    surface.cs = ColorSpace::ARGB8888S;
    surface.channelSize = sizeof(uint32_t);
    
    return true;
}

RenderSurface* GifLoader::bitmap()
{
    return &surface;
}

bool GifLoader::frame(float no)
{
    if (!gifFile || gifFile->ImageCount == 0) return false;
    
    // Clamp frame number
    if (no < 0.0f) no = 0.0f;
    if (no >= gifFile->ImageCount) no = gifFile->ImageCount - 1.0f;
    
    uint32_t frameIndex = static_cast<uint32_t>(no);
    if (frameIndex != currentFrameIndex) {
        currentFrameIndex = frameIndex;
        
        if (canvas) {
            bool needReset = false;
            
            // Check if we need to restart from frame 0
            if (lastCompositedFrame == 0xFFFFFFFF || 
                frameIndex < lastCompositedFrame ||
                frameIndex > lastCompositedFrame + 1) {
                needReset = true;
            }
            
            // Check previous frame's disposal method
            if (!needReset && lastCompositedFrame < (uint32_t)gifFile->ImageCount) {
                GraphicsControlBlock gcb;
                DGifSavedExtensionToGCB(gifFile, lastCompositedFrame, &gcb);
                if (gcb.DisposalMode == DISPOSE_BACKGROUND || 
                    gcb.DisposalMode == DISPOSE_PREVIOUS) {
                    needReset = true;
                }
            }
            
            if (needReset) {
                // Reset and composite from frame 0
                memset(canvas, 0, gifFile->SWidth * gifFile->SHeight * 4);
                for (uint32_t i = 0; i <= frameIndex; i++) {
                    compositeFrame(i);
                }
            } else {
                // Incremental: only composite the new frame
                compositeFrame(frameIndex);
            }
            
            lastCompositedFrame = frameIndex;
        }
        
        return true;
    }
    
    return false;
}

float GifLoader::totalFrame()
{
    return gifFile ? static_cast<float>(gifFile->ImageCount) : 0.0f;
}

float GifLoader::curFrame()
{
    return static_cast<float>(currentFrameIndex);
}

float GifLoader::duration()
{
    if (frameRate > FLOAT_EPSILON && gifFile) {
        return gifFile->ImageCount / frameRate;
    }
    return 0.0f;
}

Result GifLoader::segment(float begin, float end)
{
    if (!gifFile || gifFile->ImageCount == 0) return Result::InsufficientCondition;
    
    if (begin < 0.0f) begin = 0.0f;
    if (end > gifFile->ImageCount) end = static_cast<float>(gifFile->ImageCount);
    if (begin >= end) return Result::InvalidArguments;
    
    segmentBegin = begin;
    segmentEnd = end;
    
    return Result::Success;
}
