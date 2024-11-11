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

#define NUM_PER_ROW 3
#define NUM_PER_COL 3

struct UserExample : tvgexam::Example
{
    unique_ptr<tvg::LottieAnimation> slot0;
    unique_ptr<tvg::LottieAnimation> slot1;
    unique_ptr<tvg::LottieAnimation> slot2;
    unique_ptr<tvg::LottieAnimation> slot3;
    unique_ptr<tvg::LottieAnimation> marker;
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

        //default slot
        {
            auto progress = tvgexam::progress(elapsed, slot0->duration());
            slot0->frame(slot0->totalFrame() * progress);
        }

        //gradient slot
        {
            auto progress = tvgexam::progress(elapsed, slot1->duration());
            slot1->frame(slot1->totalFrame() * progress);
        }

        //solid fill slot
        {
            auto progress = tvgexam::progress(elapsed, slot2->duration());
            slot2->frame(slot2->totalFrame() * progress);
        }

        //image slot
        {
            auto progress = tvgexam::progress(elapsed, slot3->duration());
            slot3->frame(slot3->totalFrame() * progress);
        }

        //marker
        {
            auto progress = tvgexam::progress(elapsed, marker->duration());
            marker->frame(marker->totalFrame() * progress);
        }

        canvas->update();

        return true;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(75, 75, 75);
        canvas->push(bg);

        this->w = w;
        this->h = h;
        this->size = w / NUM_PER_ROW;

        //slot (default)
        {
            slot0 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot0->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample0.json"))) return false;

            sizing(picture, 0);

            canvas->push(picture);
        }

        //slot (gradient)
        {
            slot1 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot1->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample1.json"))) return false;

            const char* slotJson = R"({"gradient_fill":{"p":{"p":2,"k":{"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0,0,1,1]}}}})";
            if (!tvgexam::verify(slot1->override(slotJson))) return false;

            sizing(picture, 1);

            canvas->push(picture);
        }

        //slot (solid fill)
        {
            slot2 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot2->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample2.json"))) return false;

            const char* slotJson = R"({"ball_color":{"p":{"a":1,"k":[{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":7,"s":[0,0.176,0.867]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":22,"s":[0.867,0,0.533]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":37,"s":[0.867,0,0.533]},{"t":51,"s":[0,0.867,0.255]}]}}})";
            if (!tvgexam::verify(slot2->override(slotJson))) return false;

            sizing(picture, 2);

            canvas->push(picture);
        }

        //slot (image)
        {
            slot3 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot3->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample3.json"))) return false;

            const char* slotJson = R"({"path_img":{"id":"image_0","w":200,"h":300,"u":"images/","p":"logo.png","e":0}})";
            if (!tvgexam::verify(slot3->override(slotJson))) return false;

            sizing(picture, 3);

            canvas->push(picture);

            canvas->update();
        }

        //marker
        {
            marker = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = marker->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/marker_sample.json"))) return false;
            if (!tvgexam::verify(marker->segment("sectionC"))) return false;

            sizing(picture, 4);

            canvas->push(picture);
        }

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, 1024, 1024);
}