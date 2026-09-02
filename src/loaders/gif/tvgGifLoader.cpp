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

#include <cmath>
#include "tvgLoader.h"
#include "tvgGifLoader.h"

#define GIF_DISPOSAL_BACKGROUND 2
#define GIF_DISPOSAL_PREVIOUS 3

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void GifLoader::run(unsigned tid)
{
    memset(decoder.canvas, 0, static_cast<size_t>(decoder.width) * static_cast<size_t>(decoder.height) * sizeof(uint32_t));
    decoder.compositeFrame(0);
    lastCompositedFrame = 0;

    surface.setup(reinterpret_cast<pixel_t*>(decoder.canvas), decoder.width, decoder.width, decoder.height, sizeof(uint32_t),
                  decoder.abgr ? ColorSpace::ABGR8888S : ColorSpace::ARGB8888S);
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
    if (freeData) tvg::free(data);
}

bool GifLoader::open(const char* path, TVG_UNUSED const LoaderOps* ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    uint32_t fileSize = 0;
    if (!(data = reinterpret_cast<uint8_t*>(Loader::open(path, fileSize)))) return false;

    freeData = true;

    decoder.abgr = (BitmapLoader::cs != ColorSpace::ARGB8888 && BitmapLoader::cs != ColorSpace::ARGB8888S);
    if (!decoder.load(data, fileSize)) return false;

    w = static_cast<float>(decoder.width);
    h = static_cast<float>(decoder.height);
    segmentEnd = static_cast<float>(decoder.frames.count);
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
        memcpy(this->data, data, size);
        freeData = true;
    } else {
        this->data = reinterpret_cast<unsigned char*>(const_cast<char*>(data));
        freeData = false;
    }

    decoder.abgr = (BitmapLoader::cs != ColorSpace::ARGB8888 && BitmapLoader::cs != ColorSpace::ARGB8888S);
    if (!decoder.load(this->data, size)) return false;

    w = static_cast<float>(decoder.width);
    h = static_cast<float>(decoder.height);
    segmentEnd = static_cast<float>(decoder.frames.count);

    return true;
}

bool GifLoader::read()
{
    if (!Loader::read()) return true;

    TaskScheduler::request(this);

    return true;
}

RenderSurface* GifLoader::bitmap()
{
    done();

    return surface.data ? &surface : nullptr;
}

bool GifLoader::frame(float no)
{
    done();
    if (segmentBegin >= segmentEnd) return false;

    //'no' is segment-relative; map it onto the absolute frame range
    no += segmentBegin;
    if (no < segmentBegin) no = segmentBegin;
    if (no > segmentEnd) no = segmentEnd;

    auto frameIndex = static_cast<uint32_t>(no);
    auto lastFrameIndex = static_cast<uint32_t>(ceilf(segmentEnd)) - 1;
    if (frameIndex > lastFrameIndex) frameIndex = lastFrameIndex;
    if (frameIndex == currentFrameIndex) return false;

    currentFrameIndex = frameIndex;

    if (lastCompositedFrame == NO_FRAME || frameIndex < lastCompositedFrame || frameIndex > lastCompositedFrame + 1 ||
        decoder.frames[lastCompositedFrame].disposal == GIF_DISPOSAL_PREVIOUS) {
        //Reconstruct the canvas by accumulating frames from frame 0.
        memset(decoder.canvas, 0, static_cast<size_t>(decoder.width) * static_cast<size_t>(decoder.height) * sizeof(uint32_t));
        for (auto i = 0u; i <= frameIndex; i++) {
            auto draw = true;
            if (i < frameIndex && decoder.frames[i].disposal == GIF_DISPOSAL_PREVIOUS) draw = false;
            decoder.compositeFrame(i, draw);
        }
    } else {
        decoder.compositeFrame(frameIndex);
    }
    lastCompositedFrame = frameIndex;

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
    if (durationCache < 0.0f) durationCache = decoder.duration(segmentBegin, segmentEnd);
    return durationCache;
}

Result GifLoader::segment(float begin, float end)
{
    if (begin < 0.0f) begin = 0.0f;
    if (end > decoder.frames.count) end = static_cast<float>(decoder.frames.count);
    if (begin > end) return Result::InvalidArguments;

    segmentBegin = begin;
    segmentEnd = end;
    durationCache = -1.0f;

    return Result::Success;
}
