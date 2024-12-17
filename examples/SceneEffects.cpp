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

#define SIZE 400

struct UserExample : tvgexam::Example
{
    tvg::Scene* blur[3] = {nullptr, nullptr, nullptr};   //(for direction both, horizontal, vertical)
    tvg::Scene* fill = nullptr;
    tvg::Scene* tint = nullptr;
    tvg::Scene* trintone = nullptr;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //blur scene
        for (int i = 0; i < 3; ++i) {
            blur[i] = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg/tiger.svg");
            picture->size(SIZE, SIZE);
            picture->translate(SIZE * i, 0);

            blur[i]->push(picture);
            canvas->push(blur[i]);
        }

        //fill scene
        {
            fill = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg/tiger.svg");
            picture->size(SIZE, SIZE);
            picture->translate(0, SIZE);

            fill->push(picture);
            canvas->push(fill);
        }

        //tint scene
        {
            tint = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg/tiger.svg");
            picture->size(SIZE, SIZE);
            picture->translate(SIZE, SIZE);

            tint->push(picture);
            canvas->push(tint);
        }

        //trinton scene
        {
            trintone = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg/tiger.svg");
            picture->size(SIZE, SIZE);
            picture->translate(SIZE * 2, SIZE);

            trintone->push(picture);
            canvas->push(trintone);
        }


        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        auto progress = tvgexam::progress(elapsed, 2.5f, true);   //2.5 seconds

        //Apply GaussianBlur post effect (sigma, direction, border option, quality)
        for (int i = 0; i < 3; ++i) {
            blur[i]->push(tvg::SceneEffect::ClearAll);
            blur[i]->push(tvg::SceneEffect::GaussianBlur, 10.0f * progress, i, 0, 100);
        }

        //Apply Fill post effect (rgba)
        fill->push(tvg::SceneEffect::ClearAll);
        fill->push(tvg::SceneEffect::Fill, 0, (int)(progress * 255), 0, (int)(255.0f * progress));

        //Apply Tint post effect (black:rgb, white:rgb, intensity)
        tint->push(tvg::SceneEffect::ClearAll);
        tint->push(tvg::SceneEffect::Tint, 0, 0, 0, 0, (int)(progress * 255), 0, progress * 100.0f);

        //Apply Trintone post effect (shadow:rgb, midtone:rgb, highlight:rgb)
        trintone->push(tvg::SceneEffect::ClearAll);
        trintone->push(tvg::SceneEffect::Tritone, 0, (int)(progress * 255), 0, 199, 110, 36, 255, 255, 255);

        canvas->update();

        return true;
    }

};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, SIZE * 3, SIZE * 2, 4, true);
}