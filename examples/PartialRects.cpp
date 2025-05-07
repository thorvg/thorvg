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
    struct Particle {
        tvg::Shape* shape;
        float x, y;
        float dur;
        bool dir;
    };

    const int COUNT = 1200;

    tvg::Shape* obstacle1 = nullptr;
    tvg::Shape* obstacle2 = nullptr;
    std::vector<Particle> rects;

    uint32_t w, h;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        srand(100);

#if 1
    {
        auto bg = tvg::Picture::gen();
        bg->load(EXAMPLE_DIR"/image/partial.jpg");
        canvas->push(bg);
    }
#endif

#if 1
    {
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h, 200, 200);
        bg->fill(0, 150, 0, 50);
        canvas->push(bg);
    }
#endif

#if 1
    {
        auto bg = tvg::Shape::gen();
        bg->appendCircle(w/2, h/2, w/2, h/2);
        bg->fill(125, 125, 125, 125);
        canvas->push(bg);
    }
#endif

#if 1
    {
        auto bg = tvg::Shape::gen();
        bg->moveTo(w/2, 0);
        bg->lineTo(0, h/2);
        bg->lineTo(w/2, h);
        bg->lineTo(w, h/2);
        bg->close();
        bg->fill(150, 0, 0, 50);
        canvas->push(bg);
    }
#endif

#if 0
    {
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(200, 200, 200, 200);
        canvas->push(bg);
    }
#endif

        obstacle1 = tvg::Shape::gen();
        obstacle1->appendRect(0, 0, w / 4, h / 4);
        obstacle1->fill(200, 200, 0, 170);
        canvas->push(obstacle1);

        obstacle2 = tvg::Shape::gen();
        obstacle2->appendRect(0, 0, w / 2, h / 3);
        obstacle2->fill(200, 200, 0, 170);
        canvas->push(obstacle2);

        //rects
        auto size = w / COUNT;

        for (int i = 0; i < COUNT; ++i) {
            auto shape = tvg::Shape::gen();
            float x = size * i;
            rects.push_back({shape, x, 0.0f, float(rand() % 1000) * 0.01f, 0});
            shape->appendRect(0, 0, size, size);
            shape->fill(rand() % 255, rand() % 255, rand() % 255);
            canvas->push(shape);
        }

        this->w = w;
        this->h = h;

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        auto progress = tvgexam::progress(elapsed, 2.0f, true);  //play time 2 sec.

        obstacle1->translate((w/2) - (w/ 8), (h - (h / 4)) * progress);
        obstacle2->translate((w/2) - (w/2) * progress , (h/3));

        auto size = w / COUNT;
        for (auto& p : rects) {
            float y;
            auto progress = tvgexam::progress(elapsed, p.dur, true);  //play time 2 sec.

            if (p.dir == 0) {
                y = h * progress;
                if (y + size > h) {
                    y = h - size;
                    p.dir = !p.dir;
                }
            } else {
                y = h - h * progress;
                if (y < 0) {
                    y = 0;
                    p.dir = !p.dir;
                }
            }
            p.shape->translate(p.x, y);
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