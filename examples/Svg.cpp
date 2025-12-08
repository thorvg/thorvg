/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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
#include "thorvg.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

#define NUM_PER_ROW 9
#define NUM_PER_COL 9

struct UserExample : tvgexam::Example
{
    tvg::Scene* root = tvg::Scene::gen();
    tvg::Shape* bg = tvg::Shape::gen();
    std::vector<tvg::Picture*> pictures;
    uint32_t initWidth, initHeight;
    uint32_t beforeWidth = 0, beforeHeight = 0;
    uint32_t size;

    int counter = 0;

    void populate(const char* path) override
    {
        if (counter >= NUM_PER_ROW * NUM_PER_COL) return;

        //ignore if not svg.
        const char *ext = path + strlen(path) - 3;
        if (strcmp(ext, "svg")) return;

        auto picture = tvg::Picture::gen();
        picture->origin(0.5f, 0.5f);

        if (!tvgexam::verify(picture->load(path))) return;

        //image scaling preserving its aspect ratio
        float w, h;
        picture->size(&w, &h);
        picture->scale((w > h) ? size / w : size / h);
        picture->translate((counter % NUM_PER_ROW) * size + size / 2, (counter / NUM_PER_ROW) * (initHeight / NUM_PER_COL) + size / 2);

        pictures.push_back(picture);

        cout << "SVG: " << path << endl;

        counter++;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        //The default font for fallback in case
        tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf");

        //Background
        bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(255, 255, 255);

        canvas->push(bg);

        beforeWidth = initWidth = w;
        beforeHeight = initHeight = h;
        this->size = w / NUM_PER_ROW;
        this->scandir(EXAMPLE_DIR"/svg");

        /* This showcase demonstrates the asynchronous loading of tvg.
           For this, pictures are pushed at a certain sync time.
           This allows time for the tvg resources to finish loading;
           otherwise, you can push pictures immediately. */
        for (auto& paint : pictures) {
            root->push(paint);
        }

        canvas->push(root);

        pictures.clear();
        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed, uint32_t width, uint32_t height) override
    {
        if (beforeWidth == width && beforeHeight == height) return false;

        bg->reset();
        bg->appendRect(0, 0, width, height);

        auto scale = 1.0f;
        if (width > height) scale = (float)height / (float)initHeight;
        else scale = (float)width / (float)initWidth;

        root->scale(scale);
        root->translate((width - initWidth * scale) * 0.5f, (height - initHeight * scale) * 0.5f);

        beforeWidth = width;
        beforeHeight = height;
        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 1280, 1280);
}

