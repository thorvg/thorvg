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
    tvg::Scene* scene[3] = {nullptr, nullptr, nullptr};

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        for (int i = 0; i < 3; ++i) {
            scene[i] = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg/tiger.svg");
            picture->size(w / 3, h);
            picture->translate((w / 3) * i, 0);

            scene[i]->push(picture);
            canvas->push(scene[i]);
        }

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        auto progress = tvgexam::progress(elapsed, 2.5f, true);   //2.5 seconds

        //Clear the previously applied effects
        scene[0]->push(tvg::SceneEffect::ClearAll);
        //Apply Fill post effect (rgba)
        scene[0]->push(tvg::SceneEffect::Fill, 255, 255, 0, (int)(255.0f * progress));

        scene[1]->push(tvg::SceneEffect::ClearAll);
        scene[1]->push(tvg::SceneEffect::Fill, 255, 255, 255, (int)(255.0f * progress));

        scene[2]->push(tvg::SceneEffect::ClearAll);
        scene[2]->push(tvg::SceneEffect::Fill, 0, 255, 255, (int)(255.0f * progress));

        canvas->update();

        return true;
    }

};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, 1200, 400, 4, true);
}