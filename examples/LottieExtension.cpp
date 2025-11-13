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
    vector<unique_ptr<tvg::LottieAnimation>> slots;
    unique_ptr<tvg::LottieAnimation> marker;
    unique_ptr<tvg::LottieAnimation> resolver[2];  //picture, text
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
        //slots
        for (auto& slot : slots) {
            slot->frame(slot->totalFrame() * tvgexam::progress(elapsed, slot->duration()));
        }

        //marker
        marker->frame(marker->totalFrame() * tvgexam::progress(elapsed, marker->duration()));

        //asset resolvers
        resolver[0]->frame(resolver[0]->totalFrame() * tvgexam::progress(elapsed, resolver[0]->duration()));
        resolver[1]->frame(resolver[1]->totalFrame() * tvgexam::progress(elapsed, resolver[1]->duration()));

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
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot0.json"))) return false;

            sizing(picture, 0);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (gradient)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot1.json"))) return false;

            const char* slotJson = R"({"gradient_fill":{"p":{"p":2,"k":{"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0,0,1,1]}}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 1);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (solid fill)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot2.json"))) return false;

            const char* slotJson = R"({"ball_color":{"p":{"a":1,"k":[{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":7,"s":[0,0.176,0.867]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":22,"s":[0.867,0,0.533]},{"i":{"x":[0.833],"y":[0.833]},"o":{"x":[0.167],"y":[0.167]},"t":37,"s":[0.867,0,0.533]},{"t":51,"s":[0,0.867,0.255]}]}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 2);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (image)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot3.json"))) return false;

            const char* slotJson = R"({"path_img":{"p":{"id":"image_0","w":200,"h":300,"u":"images/","p":"logo.png","e":0}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 3);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (overriden default slot)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot4.json"))) return false;

            const char* slotJson = R"({"bg_color":{"p":{"a":0,"k":[1,0.8196,0.2275]}},"check_color":{"p":{"a":0,"k":[0.0078,0.0078,0.0078]}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 4);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (duplicate slots with default)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot5.json"))) return false;

            sizing(picture, 5);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (transform: position)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot6.json"))) return false;

            const char* slotJson = R"({"position_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[100,100],"t":0},{"s":[200,300],"t":100}]}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 6);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (transform: scale)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot7.json"))) return false;

            const char* slotJson = R"({"scale_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0,0],"t":0},{"s":[100,100],"t":100}]}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 7);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (transform: rotation)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot8.json"))) return false;

            const char* slotJson = R"({"rotation_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[180],"t":100}]}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 8);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (transform: opacity)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot9.json"))) return false;

            const char* slotJson = R"({"opacity_id":{"p":{"a":1,"k":[{"i":{"x":0.833,"y":0.833},"o":{"x":0.167,"y":0.167},"s":[0],"t":0},{"s":[100],"t":100}]}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 9);

            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (expression)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot10.json"))) return false;

            const char* slotJson = R"({"rect_rotation":{"p":{"x":"var $bm_rt = time * 360;"}},"rect_scale":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}},"rect_position":{"p":{"x":"var $bm_rt = [];$bm_rt[0] = value[0] + Math.cos(2 * Math.PI * time) * 100;$bm_rt[1] = value[1];"}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 10);
            canvas->push(picture);
            slots.push_back(std::move(slot));
        }

        //slot (text)
        {
            auto slot = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = slot->picture();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot11.json"))) return false;

            const char* slotJson = R"({"text_doc":{"p":{"k":[{"s":{"f":"Ubuntu Light Italic","t":"ThorVG!","j":0,"s":48,"fc":[1,1,1]},"t":0}]}}})";
            auto slotId = slot->gen(slotJson);
            if (!tvgexam::verify(slot->apply(slotId))) return false;

            sizing(picture, 11);
            canvas->push(picture);
            slots.push_back(std::move(slot));
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

        //asset resolver (image)
        {
            resolver[0] = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = resolver[0]->picture();

            auto func = [](tvg::Paint* p, const char* src, void* data) {
                if (p->type() != tvg::Type::Picture) return false;
                //The engine may fail to access the source image. This demonstrates how to resolve it using an user vaild source.
                auto assetPath = string(src).replace(0, sizeof(EXAMPLE_DIR"/lottie/extensions/") - 1, EXAMPLE_DIR"/");
                auto ret = static_cast<tvg::Picture*>(p)->load(assetPath.c_str());
                return (ret == (tvg::Result) 0) ? true : false; //return true if the resolving is successful
            };

            //set a resolver prior to load a resource
            if (!tvgexam::verify(picture->resolver(func, nullptr))) return false;
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/resolver1.json"))) return false;

            sizing(picture, 13);
            canvas->push(picture);
        }

        //asset resolver (font)
        {
            resolver[1] = std::unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
            auto picture = resolver[1]->picture();

            auto func = [](tvg::Paint* p, const char* src, void* data) {
                if (p->type() != tvg::Type::Text) return false;
                //The engine may fail to access the source image. This demonstrates how to resolve it using an user vaild source.
                auto assetPath = EXAMPLE_DIR"/" + string(src);
                if (!tvgexam::verify(tvg::Text::load(assetPath.c_str()))) return false;
                auto ret = static_cast<tvg::Text*>(p)->font("SentyCloud");
                return (ret == (tvg::Result) 0) ? true : false; //return true if font loading is successful
            };

            if (!tvgexam::verify(picture->resolver(func, nullptr))) return false;
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/resolver2.json"))) return false;

            sizing(picture, 14);
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
    return tvgexam::main(new UserExample, argc, argv, false, 1024, 1024, 0 /* turn off for expressions */);
}