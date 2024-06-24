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

#include <thorvg_lottie.h>
#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

#define NUM_PER_ROW 2
#define NUM_PER_COL 1

struct UserExample : tvgexam::Example
{
    unique_ptr<tvg::LottieAnimation> slotAnimation;
    unique_ptr<tvg::LottieAnimation> markerAnimation;
    uint32_t w, h;
    uint32_t size;

    void sizing(tvg::Picture* picture, uint32_t counter)
    {
        //image scaling preserving its aspect ratio
        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        float w, h;
        picture->size(&w, &h);

        if (w > h) {
            scale = size / w;
            shiftY = (size - h * scale) * 0.5f;
        } else {
            scale = size / h;
            shiftX = (size - w * scale) * 0.5f;
        }

        picture->scale(scale);
        picture->translate((counter % NUM_PER_ROW) * size + shiftX, (counter / NUM_PER_ROW) * (this->h / NUM_PER_COL) + shiftY);
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        //slotsample
        {
            auto progress = tvgexam::progress(elapsed, slotAnimation->duration());
            slotAnimation->frame(markerAnimation->totalFrame() * progress);
        }

        //marker
        {
            auto progress = tvgexam::progress(elapsed, markerAnimation->duration());
            markerAnimation->frame(markerAnimation->totalFrame() * progress);
        }

        canvas->update();

        return true;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(75, 75, 75);

        canvas->push(std::move(shape));

        this->w = w;
        this->h = h;
        this->size = w / NUM_PER_ROW;

        //slotsample
        {
            slotAnimation = tvg::LottieAnimation::gen();
            auto picture = slotAnimation->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample.json"))) return false;

            const char* slotJson = R"({"gradient_fill":{"p":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}})";
            if (!tvgexam::verify(slotAnimation->override(slotJson))) return false;

            sizing(picture, 0);

            canvas->push(tvg::cast(picture));
        }

        //marker
        {
            markerAnimation = tvg::LottieAnimation::gen();
            auto picture = markerAnimation->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/marker_sample.json"))) return false;
            if (!tvgexam::verify(markerAnimation->segment("sectionC"))) return false;

            sizing(picture, 1);

            canvas->push(tvg::cast(picture));
        }

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv);
}