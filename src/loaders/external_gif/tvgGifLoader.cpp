/*
 * Copyright (c) 2026 the ThorVG project. All rights reserved.
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

//Feeds in-memory GIF data to giflib through DGifOpen()'s read callback
struct MemoryReader
{
    const uint8_t* data;
    uint32_t size;
    uint32_t pos;
};

static int _memoryInputFunc(GifFileType* gif, GifByteType* buffer, int length)
{
    auto reader = static_cast<MemoryReader*>(gif->UserData);
    if (!reader || reader->pos >= reader->size) return 0;

    int bytesToRead = length;
    if (reader->pos + bytesToRead > reader->size) bytesToRead = reader->size - reader->pos;

    if (bytesToRead > 0) {
        memcpy(buffer, reader->data + reader->pos, bytesToRead);
        reader->pos += bytesToRead;
    }

    return bytesToRead;
}

void GifLoader::clear()
{
    if (gifFile) {
        DGifCloseFile(gifFile, nullptr);
        gifFile = nullptr;
    }
    if (freeData) tvg::free(data);
    data = nullptr;

    tvg::free(canvas);
    canvas = nullptr;
    surface.buf8 = nullptr;
    freeData = false;
}

void GifLoader::disposeBackground(const GifImageDesc* prevDesc)
{
    auto canvas32 = reinterpret_cast<uint32_t*>(canvas);

    int startY = (prevDesc->Top < 0) ? 0 : prevDesc->Top;
    int endY = prevDesc->Top + prevDesc->Height;
    if (endY > gifFile->SHeight) endY = gifFile->SHeight;

    int startX = (prevDesc->Left < 0) ? 0 : prevDesc->Left;
    int endX = prevDesc->Left + prevDesc->Width;
    if (endX > gifFile->SWidth) endX = gifFile->SWidth;

    for (int y = startY; y < endY; y++) {
        size_t canvasIdx = static_cast<size_t>(y) * gifFile->SWidth + startX;
        int width = endX - startX;
        if (width > 0) memset(&canvas32[canvasIdx], 0, width * sizeof(uint32_t));
    }
}

void GifLoader::blitRow(const uint8_t* raster, size_t frameIdx, size_t canvasIdx, int count, const ColorMapObject* colorMap, int transparent)
{
    auto canvas32 = reinterpret_cast<uint32_t*>(canvas);

    for (int x = 0; x < count; x++, frameIdx++, canvasIdx++) {
        uint8_t colorIndex = raster[frameIdx];

        if (transparent != NO_TRANSPARENT_COLOR && colorIndex == transparent) continue;
        if (colorIndex >= colorMap->ColorCount) continue;

        GifColorType color = colorMap->Colors[colorIndex];
        canvas32[canvasIdx] = abgr ? (0xFF000000 | (static_cast<uint32_t>(color.Red) << 0) | (static_cast<uint32_t>(color.Green) << 8) | (static_cast<uint32_t>(color.Blue) << 16))
                                   : (0xFF000000 | (static_cast<uint32_t>(color.Blue) << 0) | (static_cast<uint32_t>(color.Green) << 8) | (static_cast<uint32_t>(color.Red) << 16));
    }
}

void GifLoader::blitFrame(const SavedImage* frame, const GifImageDesc* imageDesc, const GraphicsControlBlock& gcb)
{
    ColorMapObject* colorMap = imageDesc->ColorMap ? imageDesc->ColorMap : gifFile->SColorMap;
    if (!colorMap) return;

    if (imageDesc->Top >= gifFile->SHeight || imageDesc->Left >= gifFile->SWidth) return;
    if (imageDesc->Top + imageDesc->Height <= 0 || imageDesc->Left + imageDesc->Width <= 0) return;

    int startY = (imageDesc->Top < 0) ? -imageDesc->Top : 0;
    int endY = imageDesc->Height;
    if (imageDesc->Top + endY > gifFile->SHeight) endY = gifFile->SHeight - imageDesc->Top;

    int startX = (imageDesc->Left < 0) ? -imageDesc->Left : 0;
    int endX = imageDesc->Width;
    if (imageDesc->Left + endX > gifFile->SWidth) endX = gifFile->SWidth - imageDesc->Left;

    for (int y = startY; y < endY; y++) {
        int canvasY = imageDesc->Top + y;
        size_t frameIdx = static_cast<size_t>(y) * imageDesc->Width + startX;
        size_t canvasIdx = static_cast<size_t>(canvasY) * gifFile->SWidth + imageDesc->Left + startX;
        blitRow(frame->RasterBits, frameIdx, canvasIdx, endX - startX, colorMap, gcb.TransparentColor);
    }
}

void GifLoader::compositeFrame(uint32_t frameIndex, bool draw)
{
    if (!gifFile || frameIndex >= static_cast<uint32_t>(gifFile->ImageCount)) return;
    if (!canvas) return;

    SavedImage* frame = &gifFile->SavedImages[frameIndex];
    GifImageDesc* imageDesc = &frame->ImageDesc;

    GraphicsControlBlock gcb;
    DGifSavedExtensionToGCB(gifFile, frameIndex, &gcb);

    if (frameIndex > 0) {
        GraphicsControlBlock prevGcb;
        DGifSavedExtensionToGCB(gifFile, frameIndex - 1, &prevGcb);
        if (prevGcb.DisposalMode == DISPOSE_BACKGROUND) {
            disposeBackground(&gifFile->SavedImages[frameIndex - 1].ImageDesc);
        }
    }

    if (draw) blitFrame(frame, imageDesc, gcb);
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
        //DelayTime is in centiseconds (0.01s units), so 100.0f converts the total delay to seconds
        frameRate = (gifFile->ImageCount * 100.0f) / totalDelay;
    } else {
        TVGLOG("GIF", "No frame delay provided. Falling back to 10 fps.");
        frameRate = 10.0f;
    }
}

GifLoader::GifLoader() : AnimLoader(FileType::Gif)
{
}

GifLoader::~GifLoader()
{
    clear();
    // Surface points to canvas, which is freed in clear()
}

bool GifLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    int error = 0;
    gifFile = DGifOpenFileName(path, &error);
    if (!gifFile) return false;

    if (DGifSlurp(gifFile) != GIF_OK) {
        clear();
        return false;
    }

    abgr = (ImageLoader::cs != ColorSpace::ARGB8888 && ImageLoader::cs != ColorSpace::ARGB8888S);

    w = static_cast<float>(gifFile->SWidth);
    h = static_cast<float>(gifFile->SHeight);
    segmentEnd = static_cast<float>(gifFile->ImageCount);

    calculateFrameRate();

    auto canvasSize = static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(uint32_t);
    canvas = tvg::malloc<uint8_t>(canvasSize);
    if (!canvas) {
        clear();
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool GifLoader::open(const char* data, uint32_t size, TVG_UNUSED const LoaderOps* ops, bool copy)
{
    if (copy) {
        this->data = tvg::malloc<unsigned char>(size);
        if (!this->data) return false;
        memcpy((unsigned char*)this->data, data, size);
        freeData = true;
    } else {
        this->data = (unsigned char*)data;
        freeData = false;
    }
    this->size = size;

    //The reader only needs to live while DGifSlurp() synchronously reads the in-memory data
    MemoryReader reader = {this->data, this->size, 0};

    int error = 0;
    gifFile = DGifOpen(&reader, _memoryInputFunc, &error);
    if (!gifFile) {
        TVGERR("GIF", "Failed to open GIF from memory: error code %d", error);
        clear();
        return false;
    }

    auto ret = DGifSlurp(gifFile);
    gifFile->UserData = nullptr;  //reader is about to go out of scope
    if (ret != GIF_OK) {
        TVGERR("GIF", "Failed to read GIF from memory");
        clear();
        return false;
    }

    abgr = (ImageLoader::cs != ColorSpace::ARGB8888 && ImageLoader::cs != ColorSpace::ARGB8888S);

    w = static_cast<float>(gifFile->SWidth);
    h = static_cast<float>(gifFile->SHeight);
    segmentEnd = static_cast<float>(gifFile->ImageCount);

    calculateFrameRate();

    auto canvasSize = static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(uint32_t);
    canvas = tvg::malloc<uint8_t>(canvasSize);
    if (!canvas) {
        clear();
        return false;
    }

    return true;
}

bool GifLoader::read()
{
    if (!Loader::read()) return true;
    if (!gifFile || gifFile->ImageCount == 0) return false;

    //Decode and composite frame 0
    memset(canvas, 0, static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(uint32_t));
    compositeFrame(0);
    lastCompositedFrame = 0;

    surface.buf8 = canvas;
    surface.stride = static_cast<uint32_t>(w);
    surface.w = static_cast<uint32_t>(w);
    surface.h = static_cast<uint32_t>(h);
    surface.cs = abgr ? ColorSpace::ABGR8888S : ColorSpace::ARGB8888S;
    surface.channelSize = sizeof(uint32_t);

    return true;
}

bool GifLoader::needsReset(uint32_t frameIndex)
{
    if (lastCompositedFrame == 0xFFFFFFFF || frameIndex < lastCompositedFrame || frameIndex > lastCompositedFrame + 1) {
        return true;
    }
    if (lastCompositedFrame < static_cast<uint32_t>(gifFile->ImageCount)) {
        GraphicsControlBlock gcb;
        DGifSavedExtensionToGCB(gifFile, lastCompositedFrame, &gcb);
        if (gcb.DisposalMode == DISPOSE_PREVIOUS) return true;
    }
    return false;
}

void GifLoader::compositeFromStart(uint32_t frameIndex)
{
    memset(canvas, 0, static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(uint32_t));
    for (uint32_t i = 0; i < frameIndex; i++) {
        GraphicsControlBlock gcb;
        DGifSavedExtensionToGCB(gifFile, i, &gcb);
        compositeFrame(i, gcb.DisposalMode != DISPOSE_PREVIOUS);
    }
    compositeFrame(frameIndex);
}

bool GifLoader::frame(float no)
{
    if (!gifFile || gifFile->ImageCount == 0) return false;

    //'no' is segment-relative; map it onto the absolute frame range
    no += segmentBegin;
    if (no < segmentBegin) no = segmentBegin;
    if (no > segmentEnd - 1.0f) no = segmentEnd - 1.0f;

    auto frameIndex = static_cast<uint32_t>(no);
    if (frameIndex == currentFrameIndex) return false;

    currentFrameIndex = frameIndex;

    if (canvas) {
        if (needsReset(frameIndex)) compositeFromStart(frameIndex);
        else compositeFrame(frameIndex);
        lastCompositedFrame = frameIndex;
    }

    return true;
}

float GifLoader::totalFrame()
{
    return segmentEnd - segmentBegin;
}

float GifLoader::curFrame()
{
    return currentFrameIndex - segmentBegin;
}

float GifLoader::duration()
{
    if (frameRate > FLOAT_EPSILON) return (segmentEnd - segmentBegin) / frameRate;
    return 0.0f;
}

Result GifLoader::segment(float begin, float end)
{
    if (!gifFile || gifFile->ImageCount == 0) return Result::InsufficientCondition;

    if (begin < 0.0f) begin = 0.0f;
    if (end > gifFile->ImageCount) end = static_cast<float>(gifFile->ImageCount);
    if (begin > end) return Result::InvalidArguments;

    segmentBegin = begin;
    segmentEnd = end;

    return Result::Success;
}
