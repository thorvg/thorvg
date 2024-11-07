/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    unique_ptr<tvg::Animation> animation;

    bool hitting(const tvg::Paint* paint, int32_t x, int32_t y, float segment1, float segment2)
    {
        float px, py, pw, ph;
        tvgexam::verify(paint->bounds(&px, &py, &pw, &ph, true));
        if (x >= px && x <= px + pw && y >= py && y <= py + ph) {
            tvgexam::verify(animation->segment(segment1, segment2));
            elapsed = 0.0f;
            return true;
        }
        return false;
    }

    bool clicked(tvg::Canvas* canvas, int32_t x, int32_t y) override
    {
        auto picture = animation->picture();

        //pad1 touch?
        if (auto paint = picture->paint(tvg::Accessor::id("pad1"))) {
            if (hitting(paint, x, y, 0.2222f, 0.3333f)) return true;
        }

        //pad3 touch?
        if (auto paint = picture->paint(tvg::Accessor::id("pad3"))) {
            if (hitting(paint, x, y, 0.4444f, 0.5555f)) return true;
        }

        //pad5 touch?
        if (auto paint = picture->paint(tvg::Accessor::id("pad5"))) {
            if (hitting(paint, x, y, 0.1111f, 0.2222f)) return true;
        }

        //pad7 touch?
        if (auto paint = picture->paint(tvg::Accessor::id("pad7"))) {
            if (hitting(paint, x, y, 0.0000f, 0.1111f)) return true;
        }

        //pad9 touch?
        if (auto paint = picture->paint(tvg::Accessor::id("pad9"))) {
            if (hitting(paint, x, y, 0.3333f, 0.4444f)) return true;
        }

        //bar touch?
        if (auto paint = picture->paint(tvg::Accessor::id("bar"))) {
            if (hitting(paint, x, y, 0.6666f, 1.0f)) return true;
        }

        return false;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        //Animation Controller
        animation = unique_ptr<tvg::Animation>(tvg::Animation::gen());
        auto picture = animation->picture();

        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(50, 50, 50);
        canvas->push(shape);

        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/locker.json"))) return false;

        //image scaling preserving its aspect ratio
        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        float w2, h2;
        picture->size(&w2, &h2);

        if (w2 > h2) {
            scale = w / w2;
            shiftY = (h - h2 * scale) * 0.5f;
        } else {
            scale = h / h2;
            shiftX = (w - w2 * scale) * 0.5f;
        }

        picture->scale(scale);
        picture->translate(shiftX, shiftY);

        canvas->push(picture);

        //Default is a stopped motion
        animation->segment(0.0f, 0.0f);

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        auto progress = tvgexam::progress(elapsed, animation->duration());

        //Default is a stopped motion
        if (progress > 0.95f) {
            animation->segment(0.0f, 0.0f);
            elapsed = progress = 0.0f;
        }

        //Update animation frame only when it's changed
        animation->frame(animation->totalFrame() * progress);
        canvas->update();

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, 1024, 1024, 0);
}