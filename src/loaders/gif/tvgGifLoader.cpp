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

#include "tvgLoader.h"
#include "tvgGifLoader.h"

#define GIF_DISPOSAL_BACKGROUND 2
#define GIF_DISPOSAL_PREVIOUS 3

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void GifLoader::clear()
{
    if (freeData) tvg::free(data);
    data = nullptr;
    surface.buf8 = nullptr;

    decoder.clear();
    currentFrameIndex = 0;
    lastCompositedFrame = 0xFFFFFFFF;
}

void GifLoader::run(unsigned tid)
{
    if (!decoder.data || decoder.frameCount == 0) return;

    if (currentFrameIndex < decoder.frameCount) {
        memset(decoder.canvas, 0, static_cast<size_t>(decoder.width) * static_cast<size_t>(decoder.height) * 4);

        decoder.compositeFrame(0);
        lastCompositedFrame = 0;

        surface.buf8 = decoder.canvas;
        surface.stride = decoder.width;
        surface.w = decoder.width;
        surface.h = decoder.height;
        surface.cs = decoder.abgr ? ColorSpace::ABGR8888S : ColorSpace::ARGB8888S;
        surface.channelSize = sizeof(uint32_t);
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

GifLoader::GifLoader() : AnimLoader(FileType::Gif)
{
    segmentEnd = 0.0f;
}

GifLoader::~GifLoader()
{
    done();
    clear();
}

bool GifLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    uint32_t fileSize = 0;
    if (!(data = (uint8_t*)Loader::open(path, fileSize))) return false;

    size = fileSize;

    decoder.abgr = (ImageLoader::cs != ColorSpace::ARGB8888 && ImageLoader::cs != ColorSpace::ARGB8888S);
    if (!decoder.load(data, size)) {
        clear();
        return false;
    }

    w = static_cast<float>(decoder.width);
    h = static_cast<float>(decoder.height);
    segmentEnd = static_cast<float>(decoder.frameCount);
    freeData = true;
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

    decoder.abgr = (ImageLoader::cs != ColorSpace::ARGB8888 && ImageLoader::cs != ColorSpace::ARGB8888S);
    if (!decoder.load(this->data, size)) {
        clear();
        return false;
    }

    w = static_cast<float>(decoder.width);
    h = static_cast<float>(decoder.height);
    segmentEnd = static_cast<float>(decoder.frameCount);

    return true;
}

bool GifLoader::read()
{
    if (!Loader::read()) return true;

    if (!data || decoder.frameCount == 0) return false;

    surface.cs = ImageLoader::cs;

    TaskScheduler::request(this);

    return true;
}

RenderSurface* GifLoader::bitmap()
{
    this->done();
    return ImageLoader::bitmap();
}

bool GifLoader::needsReset(uint32_t frameIndex)
{
    if (lastCompositedFrame == 0xFFFFFFFF || frameIndex < lastCompositedFrame || frameIndex > lastCompositedFrame + 1) {
        return true;
    }
    if (lastCompositedFrame < decoder.frameCount) {
        if (decoder.frames[lastCompositedFrame].disposal == GIF_DISPOSAL_PREVIOUS) return true;
    }
    return false;
}

void GifLoader::compositeFromStart(uint32_t frameIndex)
{
    memset(decoder.canvas, 0, static_cast<size_t>(decoder.width) * static_cast<size_t>(decoder.height) * 4);
    for (uint32_t i = 0; i <= frameIndex; i++) {
        bool draw = true;
        if (i < frameIndex && decoder.frames[i].disposal == GIF_DISPOSAL_PREVIOUS) draw = false;
        decoder.compositeFrame(i, draw);
    }
}

bool GifLoader::frame(float no)
{
    if (decoder.frameCount == 0) return false;

    //'no' is segment-relative; map it onto the absolute frame range
    no += segmentBegin;
    if (no < segmentBegin) no = segmentBegin;
    if (no > segmentEnd - 1.0f) no = segmentEnd - 1.0f;

    uint32_t frameIndex = static_cast<uint32_t>(no);
    if (frameIndex == currentFrameIndex) return false;

    currentFrameIndex = frameIndex;

    if (decoder.canvas) {
        if (needsReset(frameIndex)) compositeFromStart(frameIndex);
        else decoder.compositeFrame(frameIndex);
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
    if (decoder.frameRate > FLOAT_EPSILON) return (segmentEnd - segmentBegin) / decoder.frameRate;
    return 0.0f;
}

Result GifLoader::segment(float begin, float end)
{
    if (decoder.frameCount == 0) return Result::InsufficientCondition;

    if (begin < 0.0f) begin = 0.0f;
    if (end > decoder.frameCount) end = static_cast<float>(decoder.frameCount);
    if (begin > end) return Result::InvalidArguments;

    segmentBegin = begin;
    segmentEnd = end;

    return Result::Success;
}
