/*
 * Copyright (c) 2024 - 2025 the ThorVG project. All rights reserved.
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

#define NUM_PER_ROW 4
#define NUM_PER_COL 3

struct UserExample : tvgexam::Example
{
    unique_ptr<tvg::LottieAnimation> slot0;
    unique_ptr<tvg::LottieAnimation> slot1;
    unique_ptr<tvg::LottieAnimation> slot2;
    unique_ptr<tvg::LottieAnimation> slot3;
    unique_ptr<tvg::LottieAnimation> slot4;
    unique_ptr<tvg::LottieAnimation> slot5;
    unique_ptr<tvg::LottieAnimation> slot6;
    unique_ptr<tvg::LottieAnimation> slot7;
    unique_ptr<tvg::LottieAnimation> slot8;
    unique_ptr<tvg::LottieAnimation> slot9;
    unique_ptr<tvg::LottieAnimation> slot10;
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

        //overriden default slot
        {
            auto progress = tvgexam::progress(elapsed, slot4->duration());
            slot4->frame(slot4->totalFrame() * progress);
        }

        //duplicate slot
        {
            auto progress = tvgexam::progress(elapsed, slot5->duration());
            slot5->frame(slot5->totalFrame() * progress);
        }

        //position slot
        {
            auto progress = tvgexam::progress(elapsed, slot6->duration());
            slot6->frame(slot6->totalFrame() * progress);
        }

        //scale slot
        {
            auto progress = tvgexam::progress(elapsed, slot7->duration());
            slot7->frame(slot7->totalFrame() * progress);
        }

        //rotation slot
        {
            auto progress = tvgexam::progress(elapsed, slot8->duration());
            slot8->frame(slot8->totalFrame() * progress);
        }

        //opacity slot
        {
            auto progress = tvgexam::progress(elapsed, slot9->duration());
            slot9->frame(slot9->totalFrame() * progress);
        }

        //expression slot
        {
            auto progress = tvgexam::progress(elapsed, slot10->duration());
            slot10->frame(slot10->totalFrame() * progress);
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
            auto slotId = slot1->genSlot(slotJson);
            if (!tvgexam::verify(slot1->applySlot(slotId))) return false;

            sizing(picture, 1);

            canvas->push(picture);
        }

        //slot (solid fill)
        {
            slot2 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot2->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample2.json"))) return false;

            const char* slotJson = R"({"ball_color":{"p":{"a":1,"k":[{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":7,"s":[0,0.176,0.867]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":22,"s":[0.867,0,0.533]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":37,"s":[0.867,0,0.533]},{"t":51,"s":[0,0.867,0.255]}]}}})";
            auto slotId = slot2->genSlot(slotJson);
            if (!tvgexam::verify(slot2->applySlot(slotId))) return false;

            sizing(picture, 2);

            canvas->push(picture);
        }

        //slot (image)
        {
            slot3 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot3->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample3.json"))) return false;

            const char* slotJson = R"({"path_img":{"p":{"id":"image_0","w":200,"h":300,"u":"images/","p":"logo.png","e":0}}})";
            auto slotId = slot3->genSlot(slotJson);
            if (!tvgexam::verify(slot3->applySlot(slotId))) return false;

            sizing(picture, 3);

            canvas->push(picture);
        }

        //slot (overriden default slot)
        {
            slot4 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot4->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample4.json"))) return false;

            const char* slotJson = R"({"bg_color":{"p":{"a":0,"k":[1,0.8196,0.2275]}},"check_color":{"p":{"a":0,"k":[0.0078,0.0078,0.0078]}}})";
            auto slotId = slot4->genSlot(slotJson);
            if (!tvgexam::verify(slot4->applySlot(slotId))) return false;

            sizing(picture, 4);

            canvas->push(picture);
        }

        //slot (duplicate slots with default)
        {
            slot5 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot5->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample5.json"))) return false;

            sizing(picture, 5);

            canvas->push(picture);
        }

        //slot (transform: position)
        {
            slot6 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot6->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample6.json"))) return false;

            const char* slotJson = R"({"position_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[100,100],"t":0},{"s":[200,300],"t":100}]}}})";
            auto slotId = slot6->genSlot(slotJson);
            if (!tvgexam::verify(slot6->applySlot(slotId))) return false;

            sizing(picture, 6);

            canvas->push(picture);
        }

        //slot (transform: scale)
        {
            slot7 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot7->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample7.json"))) return false;

            const char* slotJson = R"({"scale_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0,0],"t":0},{"s":[100,100],"t":100}]}}})";
            auto slotId = slot7->genSlot(slotJson);
            if (!tvgexam::verify(slot7->applySlot(slotId))) return false;

            sizing(picture, 7);

            canvas->push(picture);
        }

        //slot (transform: rotation)
        {
            slot8 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot8->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample8.json"))) return false;

            const char* slotJson = R"({"rotation_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[180],"t":100}]}}})";
            auto slotId = slot8->genSlot(slotJson);
            if (!tvgexam::verify(slot8->applySlot(slotId))) return false;

            sizing(picture, 8);

            canvas->push(picture);
        }

        //slot (transform: opacity)
        {
            slot9 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot9->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample9.json"))) return false;

            const char* slotJson = R"({"opacity_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[100],"t":100}]}}})";
            auto slotId = slot9->genSlot(slotJson);
            if (!tvgexam::verify(slot9->applySlot(slotId))) return false;

            sizing(picture, 9);

            canvas->push(picture);
        }

        //slot (expression)
        {
            slot10 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot10->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slotsample10.json"))) return false;

            const char* slotJson = R"({"rect_rotation":{"p":{"x":"var $bm_rt = time * 360;"}},"rect_scale":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}},"rect_position":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}}})";
            auto slotId = slot10->genSlot(slotJson);
            if (!tvgexam::verify(slot10->applySlot(slotId))) return false;

            sizing(picture, 10);

            canvas->push(picture);
        }

        //marker
        {
            marker = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = marker->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/marker_sample.json"))) return false;
            if (!tvgexam::verify(marker->segment("sectionC"))) return false;

            sizing(picture, 11);

            canvas->push(picture);
        }

        return true;
    }

    ~UserExample()
    {
        slot1->delSlot(0);
        slot2->delSlot(0);
        slot3->delSlot(0);
        slot4->delSlot(0);
        slot6->delSlot(0);
        slot7->delSlot(0);
        slot8->delSlot(0);
        slot9->delSlot(0);
        slot10->delSlot(0);
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 1024, 1024, 1);
}