/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

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
#include <vector>
#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    tvg::Shape* bg1 = nullptr;
    tvg::Shape* bg2 = nullptr;
    std::vector<tvg::Shape*> rects;

    uint32_t w, h;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        srand(100);

        //background
        auto bg = tvg::Picture::gen();
        if (!tvgexam::verify(bg->load(EXAMPLE_DIR"/image/partial.jpg"))) return false;
        canvas->push(bg);

        bg1 = tvg::Shape::gen();
        bg1->appendRect(0, 0, w / 2, h / 2);
        bg1->fill(200, 200, 0, 100);
        canvas->push(bg1);

        bg2 = tvg::Shape::gen();
        bg2->appendRect(0, 0, w / 2, h / 2);
        bg2->fill(200, 0, 200, 100);
        canvas->push(bg2);

        //rects
        auto cnt = 100;
        auto size = w / cnt;

        for (int i = 0; i < cnt; ++i) {
            auto shape = tvg::Shape::gen();
            shape->appendRect(size * i, rand() % (h - size), size, size);
            shape->fill(rand() % 255, rand() % 255, rand() % 255);
            canvas->push(shape);
            rects.push_back(shape);
        }

        this->w = w;
        this->h = h;

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        auto progress = tvgexam::progress(elapsed, 2.0f, true);  //play time 2 sec.

        bg1->translate((w/2) * progress, 0);
        bg2->translate((w/2) - (w/2) * progress , (h/2));

        int i = 0;
        auto size = w / 100;
        for (auto p : rects) {
            p->reset();
            p->appendRect(size * i++, rand() % (h - size), size, size);
        }

        canvas->update();

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 2400, 1440, 0, true);
}