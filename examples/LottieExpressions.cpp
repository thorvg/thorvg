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

#define NUM_PER_ROW 5
#define NUM_PER_COL 5

struct UserExample : tvgexam::Example
{
    std::vector<unique_ptr<tvg::Animation>> animations;
    uint32_t w, h;
    uint32_t size;

    int counter = 0;

    void populate(const char* path) override
    {
        if (counter >= NUM_PER_ROW * NUM_PER_COL) return;

        //ignore if not lottie.
        const char *ext = path + strlen(path) - 4;
        if (strcmp(ext, "json") && strcmp(ext, "lot")) return;

        //Animation Controller
        auto animation = tvg::Animation::gen();
        auto picture = animation->picture();

        if (!tvgexam::verify(picture->load(path))) return;

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

        animations.push_back(unique_ptr<tvg::Animation>(animation));

        cout << "Lottie: " << path << endl;

        counter++;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        for (auto& animation : animations) {
            auto progress = tvgexam::progress(elapsed, animation->duration());
            animation->frame(animation->totalFrame() * progress);
        }

        canvas->update();

        return true;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //The default font for fallback in case
        tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf");

        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(75, 75, 75);

        canvas->push(shape);

        this->w = w;
        this->h = h;
        this->size = w / NUM_PER_ROW;

        this->scandir(EXAMPLE_DIR"/lottie/expressions");

        //Run animation loop
        for (auto& animation : animations) {
            canvas->push(animation->picture());
        }

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 1024, 1024, 0, true);
}