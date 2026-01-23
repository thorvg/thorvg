/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "tvgGifSaver.h"
#include "tvgStr.h"
#include "tvgGifEncoder.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/
void GifSaver::run(unsigned)
{
    auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
    if (!canvas) return;

    const auto w = static_cast<uint32_t>(vsize[0]);
    const auto h = static_cast<uint32_t>(vsize[1]);
    const auto duration = animation->duration();

    buffer = tvg::realloc<uint32_t*>(buffer, sizeof(uint32_t) * w * h);
    canvas->target(buffer, w, w, h, ColorSpace::ABGR8888S);
    canvas->push(bg);
    canvas->push(animation->picture());

    //use the default fps
    if (fps > 60.0f) fps = 60.0f;   // just in case
    else if (tvg::zero(fps) || fps < 0.0f) {
        fps = (animation->totalFrame() / duration);
    }
    const float delay = 1.0f / fps;
    const int delayTime = std::max(1,static_cast<int>(delay * 100.0f));

    GifEncoder encoder;
    if(encoder.begin(path, w, h) == false) {
        TVGERR("GIF_SAVER", "Failed gif encoding");
        return;
    }
    const bool useGlobalPalette = this->quality == 0;

    if (useGlobalPalette) 
    {
        for (float fno = 0.0f; fno < animation->totalFrame(); fno += fps*0.5f) {
            animation->frame(fno);
            canvas->update();
            if (canvas->draw(true) == tvg::Result::Success)
                canvas->sync();
            encoder.writeGlobalPalette(reinterpret_cast<uint8_t *>(buffer), w*h);
        }
        encoder.buildGlobalPalette();
    }

    for (auto p = 0.0f; p < duration; p += delay) {
        auto frameNo = animation->totalFrame() * (p / duration);
        animation->frame(frameNo);
        canvas->update();
        if (canvas->draw(true) == tvg::Result::Success) {
            canvas->sync();
        }
        if(!encoder.writeFrame(reinterpret_cast<uint8_t*>(buffer), delayTime)) {
            TVGERR("GIF_SAVER", "Failed gif encoding");
            break;
        }
    }

    if (!encoder.end()) {
        TVGERR("GIF_SAVER", "Failed gif encoding");
    }

    if (bg) {
        bg->unref();
        bg = nullptr;
    }
}

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/
GifSaver::~GifSaver()
{
    close();
}

bool GifSaver::close()
{
    this->done();

    if (bg) bg->unref();
    bg = nullptr;

    //animation holds the picture, it must be 1 at the bottom.
    if (animation && animation->picture()->refCnt() <= 1) delete(animation);
    animation = nullptr;

    tvg::free(path);
    path = nullptr;

    tvg::free(buffer);
    buffer = nullptr;

    return true;
}

bool GifSaver::save(TVG_UNUSED Paint* paint, TVG_UNUSED Paint* bg, TVG_UNUSED const char* filename, TVG_UNUSED uint32_t quality)
{
    TVGLOG("GIF_SAVER", "Paint is not supported.");
    return false;
}

bool GifSaver::save(Animation* animation, Paint* bg, const char* filename, uint32_t quality, uint32_t fps)
{
    close();

    this->quality = quality;
    auto picture = animation->picture();
    auto x = 0.0f, y = 0.0f;
    picture->bounds(&x, &y, &vsize[0], &vsize[1]);

    //cut off the negative space
    if (x < 0) vsize[0] += x;
    if (y < 0) vsize[1] += y;

    if (vsize[0] < FLOAT_EPSILON || vsize[1] < FLOAT_EPSILON) {
        TVGLOG("GIF_SAVER", "Saving animation(%p) has zero view size.", animation);
        return false;
    }

    if (!filename) return false;
    this->path = duplicate(filename);

    this->animation = animation;

    if (bg) {
        bg->ref();
        this->bg = bg;
    }
    this->fps = static_cast<float>(fps);

    TaskScheduler::request(this);

    return true;
}