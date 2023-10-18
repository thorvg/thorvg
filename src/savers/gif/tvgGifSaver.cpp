/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#include "gif.h"
#include "tvgGifSaver.h"

#ifdef _WIN32
    #include <malloc.h>
#elif defined(__linux__)
    #include <alloca.h>
#else
    #include <stdlib.h>
#endif

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

    delete(animation);
    animation = nullptr;

    free(path);
    path = nullptr;

    free(buffer);
    buffer = nullptr;

    return true;
}


bool GifSaver::save(TVG_UNUSED Paint* paint, TVG_UNUSED const string& path, TVG_UNUSED bool compress)
{
    TVGLOG("GIF_SAVER", "Paint is not supported.");
    return false;
}


bool GifSaver::save(Animation* animation, const string& path, TVG_UNUSED uint32_t quality, uint32_t fps)
{
    close();

    auto picture = animation->picture();
    float x, y;
    x = y = 0;
    picture->bounds(&x, &y, &vsize[0], &vsize[1], false);

    //cut off the negative space
    if (x < 0) vsize[0] += x;
    if (y < 0) vsize[1] += y;

    if (vsize[0] < FLT_EPSILON || vsize[1] < FLT_EPSILON) {
        TVGLOG("GIF_SAVER", "Saving animation(%p) has zero view size.", animation);
        return false;
    }

    this->path = strdup(path.c_str());
    if (!this->path) return false;

    this->animation = animation;
    this->fps = fps;

    TaskScheduler::request(this);

    return true;
}


void GifSaver::run(unsigned tid)
{
    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) return;

    //Do not share the memory pool since this canvas could be running on a thread.
    canvas->mempool(SwCanvas::Individual);

    auto w = static_cast<uint32_t>(vsize[0]);
    auto h = static_cast<uint32_t>(vsize[1]);

    buffer = (uint32_t*)realloc(buffer, sizeof(uint32_t) * w * h);
    canvas->target(buffer, w, w, h, tvg::SwCanvas::ABGR8888);
    canvas->push(tvg::cast(animation->picture()));

    //use the default fps
    if (fps > 60.0f) fps = 60.0f;   // just in case

    if (mathZero(fps)) {
        fps = (animation->totalFrame() / animation->duration());
    }

    auto delay = (1.0f / fps);

    GifWriter writer;
    GifBegin(&writer, path, w, h, uint32_t(delay * 100.f));

    auto duration = animation->duration();

    for (auto p = 0.0f; p < duration; p += delay) {
        auto frameNo = animation->totalFrame() * (p / duration);
        animation->frame(frameNo);
        canvas->update();
        if (canvas->draw() == tvg::Result::Success) { 
            canvas->sync();
        }
        GifWriteFrame(&writer, reinterpret_cast<uint8_t*>(buffer), w, h, uint32_t(delay * 100.0f));
    }

    GifEnd(&writer);
}

