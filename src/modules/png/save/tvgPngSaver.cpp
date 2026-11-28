/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

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

#include <cmath>
#include <memory>
#include "tvgStr.h"
#include "tvgPngEncoder.h"
#include "tvgPngSaver.h"

/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

void PngSaver::run(TVG_UNUSED unsigned tid)
{
    auto canvas = unique_ptr<SwCanvas>(SwCanvas::gen());
    if (!canvas) return;

    auto w = size[0];
    auto h = size[1];

    buffer = tvg::realloc<uint32_t>(buffer, sizeof(uint32_t) * w * h);
    if (!buffer) return;

    canvas->target(buffer, w, w, h, ColorSpace::ABGR8888S);
    if (bg) canvas->add(bg);
    canvas->add(scene);

    if (canvas->draw(true) == Result::Success) canvas->sync();
    else {
        TVGERR("PNG_SAVER", "Failed rendering");
        return;
    }

    if (!pngEncode(path, buffer, w, h, quality)) TVGERR("PNG_SAVER", "Failed png encoding");
}


/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

PngSaver::~PngSaver()
{
    close();
}


bool PngSaver::close()
{
    this->done();

    if (bg) bg->unref();
    bg = nullptr;

    if (scene) scene->unref();
    scene = nullptr;

    tvg::free(path);
    path = nullptr;

    tvg::free(buffer);
    buffer = nullptr;

    return true;
}


bool PngSaver::save(Paint* paint, Paint* bg, const char* filename, uint32_t quality)
{
    close();

    if (!filename) return false;

    float x, y, w, h;
    if (paint->bounds(&x, &y, &w, &h) != Result::Success) {
        TVGLOG("PNG_SAVER", "Saving paint(%p) has no valid bounds.", paint);
        return false;
    }

    //snap the bounds to the pixel grid
    auto left = floorf(x);
    auto top = floorf(y);
    auto right = ceilf(x + w);
    auto bottom = ceilf(y + h);

    //reject invalid(NaN, infinite) or too large sizes
    constexpr float LIMIT = 65536.0f;
    if (!(right - left >= 1.0f) || !(bottom - top >= 1.0f) || right - left > LIMIT || bottom - top > LIMIT) {
        TVGLOG("PNG_SAVER", "Saving paint(%p) has an invalid view size.", paint);
        return false;
    }

    //hold the paint in a scene to shift its origin to the canvas origin without touching its own transform
    auto scene = Scene::gen();
    if (scene->add(paint) != Result::Success) {
        Paint::rel(scene);
        return false;
    }
    scene->ref();
    scene->translate(-left, -top);

    this->scene = scene;
    this->path = duplicate(filename);
    this->size[0] = uint32_t(right - left);
    this->size[1] = uint32_t(bottom - top);
    this->quality = quality;

    if (bg) {
        bg->ref();
        this->bg = bg;
    }

    TaskScheduler::request(this);

    return true;
}


bool PngSaver::save(TVG_UNUSED Animation* animation, TVG_UNUSED Paint* bg, TVG_UNUSED const char* filename, TVG_UNUSED uint32_t quality, TVG_UNUSED uint32_t fps)
{
    TVGLOG("PNG_SAVER", "Animation is not supported.");
    return false;
}
