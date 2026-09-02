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
#include "tvgGifLoader.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

Result GifLoader::load(uint32_t size)
{
    if (!decoder.load(data, size, BitmapLoader::cs.load())) return Result::InvalidArguments;

    w = static_cast<float>(decoder.width);
    h = static_cast<float>(decoder.height);
    segmentEnd = static_cast<float>(decoder.frames.count);

    return Result::Success;
}

void GifLoader::run(TVG_UNUSED unsigned tid)
{
    memset(decoder.output, 0, static_cast<size_t>(decoder.width) * decoder.height * sizeof(uint32_t));
    decoder.compositeFrame(0);
    surface.setup(decoder.output, decoder.width, decoder.width, decoder.height, sizeof(uint32_t), decoder.cs);
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

GifLoader::~GifLoader()
{
    done();
    if (owner != Ownership::Borrow) tvg::free(data);
}

Result GifLoader::open(const char* path, TVG_UNUSED const LoaderOps& ops)
{
#ifdef THORVG_FILE_IO_SUPPORT
    uint32_t fileSize = 0;
    if (!(data = Loader::open(path, fileSize))) return Result::InvalidArguments;

    owner = Ownership::Transfer;

    return load(fileSize);
#else
    return Result::NonSupport;
#endif
}

Result GifLoader::open(const char* data, uint32_t size, const LoaderOps& ops)
{
    if (ops.owner == Ownership::Copy) {
        this->data = tvg::malloc<char>(size);
        memcpy(this->data, (char*)data, size);
    } else {
        this->data = (char*)data;
    }
    owner = ops.owner;
    return load(size);
}

bool GifLoader::read()
{
    if (!Loader::read()) return true;

    TaskScheduler::request(this);

    return true;
}

bool GifLoader::sync()
{
    if (!updated) return false;

    done();

    updated = false;

    return true;
}

RenderSurface* GifLoader::bitmap()
{
    done();

    return surface.data ? &surface : nullptr;
}

bool GifLoader::frame(float no)
{
    const uint8_t GIF_DISPOSAL_PREVIOUS = 3;

    done();

    //'no' is segment-relative; map it onto the absolute frame range
    no += segmentBegin;
    if (no < segmentBegin) no = segmentBegin;
    if (no > segmentEnd) no = segmentEnd;

    auto newIdx = static_cast<uint32_t>(no);
    auto endIdx = static_cast<uint32_t>(ceilf(segmentEnd)) - 1;
    if (newIdx > endIdx) newIdx = endIdx;
    if (newIdx == curIdx) return false;

    if (newIdx < curIdx || newIdx > curIdx + 1 || decoder.frames[curIdx].disposal == GIF_DISPOSAL_PREVIOUS) {
        // Reconstruct the canvas by accumulating frames from frame 0.
        memset(decoder.output, 0, decoder.width * decoder.height * sizeof(uint32_t));
        for (auto i = 0u; i <= newIdx; i++) {
            decoder.compositeFrame(i, (i == newIdx || decoder.frames[i].disposal != GIF_DISPOSAL_PREVIOUS));
        }
    } else {
        decoder.compositeFrame(newIdx);
    }
    curIdx = newIdx;
    updated = true;

    return true;
}

float GifLoader::curFrame()
{
    return curIdx - segmentBegin;
}

float GifLoader::duration()
{
    if (durationCache < 0.0f) {
        durationCache = decoder.duration(segmentBegin, segmentEnd);
    }
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
