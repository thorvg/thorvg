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
        float speed;
    };

    const float COUNT = 1200.0f;
    std::vector<Particle> rects;

    uint32_t w, h;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        srand(100);

        auto city = tvg::Picture::gen();
        city->load(EXAMPLE_DIR"/image/particle.jpg");
        canvas->push(city);

        auto night = tvg::Shape::gen();
        night->appendRect(0, 0, w, h);
        night->fill(0, 0, 0, 220);
        canvas->push(night);

        //rain drops
        auto size = w / COUNT;
        rects.reserve(COUNT);

        for (int i = 0; i < COUNT; ++i) {
            auto shape = tvg::Shape::gen();
            float x = size * i;
            rects.push_back({shape, x, float(rand()%h), 10 + float(rand() % 100) * 0.1f});
            shape->appendRect(0, 0, 1, rand() % 15 + size);
            shape->fill(255, 255, 255, 55 + rand() % 100);
            canvas->push(shape);
        }

        this->w = w;
        this->h = h;

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;
        for (auto& p : rects) {
            p.y += p.speed;
            if (p.y > h) {
                p.y -= h;
            }
            p.shape->translate(p.x, p.y);
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
    return tvgexam::main(new UserExample, argc, argv, false, 2440, 1280, 0, true);
}