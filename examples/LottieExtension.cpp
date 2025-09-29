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
#define NUM_PER_COL 4

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
    unique_ptr<tvg::LottieAnimation> slot11;
    unique_ptr<tvg::LottieAnimation> marker;
    unique_ptr<tvg::LottieAnimation> resolver;
    uint32_t slotId1, slotId2, slotId3, slotId4, slotId6, slotId7, slotId8, slotId9, slotId10, slotId11;
    uint32_t w, h;
    uint32_t size;

    void sizing(tvg::Picture* picture, uint32_t counter)
    {
        picture->origin(0.5f, 0.5f);

        //image scaling preserving its aspect ratio
        float w, h;
        picture->size(&w, &h);
        picture->scale((w > h) ? size / w : size / h);
        picture->translate((counter % NUM_PER_ROW) * size + size / 2, (counter / NUM_PER_ROW) * (this->h / NUM_PER_COL) + size / 2);
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        //default slot
        slot0->frame(slot0->totalFrame() * tvgexam::progress(elapsed, slot0->duration()));

        //gradient slot
        slot1->frame(slot1->totalFrame() * tvgexam::progress(elapsed, slot1->duration()));

        //solid fill slot
        slot2->frame(slot2->totalFrame() * tvgexam::progress(elapsed, slot2->duration()));

        //image slot
        slot3->frame(slot3->totalFrame() * tvgexam::progress(elapsed, slot3->duration()));

        //overriden default slot
        slot4->frame(slot4->totalFrame() * tvgexam::progress(elapsed, slot4->duration()));

        //duplicate slot
        slot5->frame(slot5->totalFrame() * tvgexam::progress(elapsed, slot5->duration()));

        //position slot
        slot6->frame(slot6->totalFrame() * tvgexam::progress(elapsed, slot6->duration()));

        //scale slot
        slot7->frame(slot7->totalFrame() * tvgexam::progress(elapsed, slot7->duration()));

        //rotation slot
        slot8->frame(slot8->totalFrame() * tvgexam::progress(elapsed, slot8->duration()));

        //opacity slot
        slot9->frame(slot9->totalFrame() * tvgexam::progress(elapsed, slot9->duration()));

        //expression slot
        slot10->frame(slot10->totalFrame() * tvgexam::progress(elapsed, slot10->duration()));

        //text slot
        slot11->frame(slot11->totalFrame() * tvgexam::progress(elapsed, slot11->duration()));

        //marker
        marker->frame(marker->totalFrame() * tvgexam::progress(elapsed, marker->duration()));

        //asset resolver
        resolver->frame(resolver->totalFrame() * tvgexam::progress(elapsed, resolver->duration()));

        canvas->update();

        return true;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        //The default font for fallback in case
        tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf");

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
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot0.json"))) return false;

            sizing(picture, 0);
            canvas->push(picture);
        }

        //slot (gradient)
        {
            slot1 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot1->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot1.json"))) return false;

            const char* slotJson = R"({"gradient_fill":{"p":{"p":2,"k":{"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0,0,1,1]}}}})";
            slotId1 = slot1->gen(slotJson);
            if (!tvgexam::verify(slot1->apply(slotId1))) return false;

            sizing(picture, 1);
            canvas->push(picture);
        }

        //slot (solid fill)
        {
            slot2 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot2->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot2.json"))) return false;

            const char* slotJson = R"({"ball_color":{"p":{"a":1,"k":[{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":7,"s":[0,0.176,0.867]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":22,"s":[0.867,0,0.533]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":37,"s":[0.867,0,0.533]},{"t":51,"s":[0,0.867,0.255]}]}}})";
            slotId2 = slot2->gen(slotJson);
            if (!tvgexam::verify(slot2->apply(slotId2))) return false;

            sizing(picture, 2);
            canvas->push(picture);
        }

        //slot (image)
        {
            slot3 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot3->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot3.json"))) return false;

            const char* slotJson = R"({"path_img":{"p":{"id":"image_0","w":200,"h":300,"u":"images/","p":"logo.png","e":0}}})";
            slotId3 = slot3->gen(slotJson);
            if (!tvgexam::verify(slot3->apply(slotId3))) return false;

            sizing(picture, 3);
            canvas->push(picture);
        }

        //slot (overriden default slot)
        {
            slot4 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot4->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot4.json"))) return false;

            const char* slotJson = R"({"bg_color":{"p":{"a":0,"k":[1,0.8196,0.2275]}},"check_color":{"p":{"a":0,"k":[0.0078,0.0078,0.0078]}}})";
            slotId4 = slot4->gen(slotJson);
            if (!tvgexam::verify(slot4->apply(slotId4))) return false;

            sizing(picture, 4);
            canvas->push(picture);
        }

        //slot (duplicate slots with default)
        {
            slot5 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot5->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot5.json"))) return false;

            sizing(picture, 5);
            canvas->push(picture);
        }

        //slot (transform: position)
        {
            slot6 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot6->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot6.json"))) return false;

            const char* slotJson = R"({"position_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[100,100],"t":0},{"s":[200,300],"t":100}]}}})";
            slotId6 = slot6->gen(slotJson);
            if (!tvgexam::verify(slot6->apply(slotId6))) return false;

            sizing(picture, 6);
            canvas->push(picture);
        }

        //slot (transform: scale)
        {
            slot7 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot7->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot7.json"))) return false;

            const char* slotJson = R"({"scale_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0,0],"t":0},{"s":[100,100],"t":100}]}}})";
            slotId7 = slot7->gen(slotJson);
            if (!tvgexam::verify(slot7->apply(slotId7))) return false;

            sizing(picture, 7);
            canvas->push(picture);
        }

        //slot (transform: rotation)
        {
            slot8 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot8->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot8.json"))) return false;

            const char* slotJson = R"({"rotation_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[180],"t":100}]}}})";
            slotId8 = slot8->gen(slotJson);
            if (!tvgexam::verify(slot8->apply(slotId8))) return false;

            sizing(picture, 8);
            canvas->push(picture);
        }

        //slot (transform: opacity)
        {
            slot9 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot9->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot9.json"))) return false;

            const char* slotJson = R"({"opacity_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[100],"t":100}]}}})";
            slotId9 = slot9->gen(slotJson);
            if (!tvgexam::verify(slot9->apply(slotId9))) return false;

            sizing(picture, 9);

            canvas->push(picture);
        }

        //slot (expression)
        {
            slot10 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot10->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot10.json"))) return false;

            const char* slotJson = R"({"rect_rotation":{"p":{"x":"var $bm_rt = time * 360;"}},"rect_scale":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}},"rect_position":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}}})";
            slotId10 = slot10->gen(slotJson);
            if (!tvgexam::verify(slot10->apply(slotId10))) return false;

            sizing(picture, 10);
            canvas->push(picture);
        }

        //slot (text)
        {
            slot11 = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot11->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot11.json"))) return false;

            const char* slotJson = R"({"text_doc":{"p":{"k":[{"s":{"f":"Ubuntu Light Italic","t":"ThorVG!","j":0,"s":48,"fc":[1,1,1]},"t":0}]}}})";
            slotId11 = slot11->gen(slotJson);
            if (!tvgexam::verify(slot11->apply(slotId11))) return false;

            sizing(picture, 11);
            canvas->push(picture);
        }

        //marker
        {
            marker = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = marker->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/marker.json"))) return false;
            if (!tvgexam::verify(marker->segment("sectionC"))) return false;

            sizing(picture, 12);
            canvas->push(picture);
        }

        //asset resolver
        {
            resolver = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = resolver->picture();

            if (!tvgexam::verify(picture->resolver([](tvg::Paint* p, const char* src, void* data) {
                if (p->type() != tvg::Type::Picture) return false;     //supposed to be a picture object
                auto ret = static_cast<tvg::Picture*>(p)->load(src);   //load picture resources as demand
                return (ret == (tvg::Result) 0) ? true : false;        //return true if resolving is successful
            }, nullptr))) return false;

            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/resolver.json"))) return false;

            sizing(picture, 13);
            canvas->push(picture);
        }

        return true;
    }

    ~UserExample()
    {
        slot1->del(slotId1);
        slot2->del(slotId2);
        slot3->del(slotId3);
        slot4->del(slotId4);
        slot6->del(slotId6);
        slot7->del(slotId7);
        slot8->del(slotId8);
        slot9->del(slotId9);
        slot10->del(slotId10);
        slot11->del(slotId11);
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 1024, 1024, 1);
}