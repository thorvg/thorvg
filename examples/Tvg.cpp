/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#define NUM_PER_ROW 9
#define NUM_PER_COL 9

struct UserExample : tvgexam::Example
{
    std::vector<unique_ptr<tvg::Picture>> pictures;
    uint32_t w, h;
    uint32_t size;

    int counter = 0;

    void populate(const char* path) override
    {
        if (counter >= NUM_PER_ROW * NUM_PER_COL) return;

        //ignore if not tvg.
        const char *ext = path + strlen(path) - 3;
        if (strcmp(ext, "tvg")) return;

        auto picture = tvg::Picture::gen();

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

        pictures.push_back(std::move(picture));

        cout << "TVG: " << path << endl;

        counter++;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(255, 255, 255);

        canvas->push(std::move(shape));

        this->w = w;
        this->h = h;
        this->size = w / NUM_PER_ROW;

        this->scandir(EXAMPLE_DIR"/tvg");

        /* This showcase demonstrates the asynchronous loading of tvg.
           For this, pictures are pushed at a certain sync time.
           This allows time for the tvg resources to finish loading;
           otherwise, you can push pictures immediately. */
        for (auto& paint : pictures) {
            canvas->push(std::move(paint));
        }

        pictures.clear();

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, 1280, 1280);
}