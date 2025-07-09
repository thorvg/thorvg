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

#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    tvg::Scene* scene1 = nullptr;
    tvg::Scene* scene2 = nullptr;
    tvg::Scene* scene3 = nullptr;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        //background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(255, 255, 255);
        canvas->push(bg);

        float pw, ph;

        //Prepare a scene for post effects
        {
            scene1 = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg/LottieFiles_logo.svg");

            picture->size(&pw, &ph);
            picture->size(pw * 0.5f, ph * 0.5f);
            picture->translate(pw * 0.175f, 0.0f);

            scene1->push(picture);
            canvas->push(scene1);
        }

        //Prepare a scene for post effects
        {
            scene2 = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg/152932619-bd3d6921-72df-4f09-856b-f9743ae32a14.svg");

            picture->size(&pw, &ph);
            picture->translate(pw * 0.45f, ph * 0.45f);
            picture->size(pw * 0.75f, ph * 0.75f);

            scene2->push(picture);
            canvas->push(scene2);
        }

        //Prepare a scene for post effects
        {
            scene3 = tvg::Scene::gen();

            auto picture = tvg::Picture::gen();
            picture->load(EXAMPLE_DIR"/svg//circles1.svg");

            picture->size(&pw, &ph);
            picture->translate(w * 0.3f, h * 0.65f);
            picture->size(pw * 0.75f, ph * 0.75f);

            scene3->push(picture);
            canvas->push(scene3);
        }

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        auto progress = tvgexam::progress(elapsed, 2.5f, true);   //2.5 seconds

        //Clear the previously applied effects
        scene1->push(tvg::SceneEffect::ClearAll);
        //Apply DropShadow post effect (r, g, b, a, angle, distance, sigma of blurness, quality)
        scene1->push(tvg::SceneEffect::DropShadow, 0, 0, 0, 125, 120.0, (double)(20.0f * progress), 3.0, 100);

        scene2->push(tvg::SceneEffect::ClearAll);
        scene2->push(tvg::SceneEffect::DropShadow, 65, 143, 222, (int)(255.0f * progress), 135.0, 10.0, 3.0, 100);

        scene3->push(tvg::SceneEffect::ClearAll);
        scene3->push(tvg::SceneEffect::DropShadow, 0, 0, 0, 125, (double)(360.0f * progress), 20.0, 3.0, 100);

        canvas->update();

        return true;
    }

};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 800, 800, 4, true);
}