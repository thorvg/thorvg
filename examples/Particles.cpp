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
        tvg::Paint* obj;
        float x, y;
        float speed;
        float size;
    };

    const float COUNT = 1200.0f;
    std::vector<Particle> raindrops;
    std::vector<Particle> clouds;

    uint32_t w, h;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        srand(100);

        auto city = tvg::Picture::gen();
        city->load(EXAMPLE_DIR"/image/particle.jpg");
        canvas->push(city);

        auto cloud1 = tvg::Picture::gen();
        cloud1->load(EXAMPLE_DIR"/image/clouds.png");
        cloud1->opacity(60);
        canvas->push(cloud1);

        float size;
        cloud1->size(&size, nullptr);
        clouds.push_back({cloud1, 0, 0, 0.25f, size});

        auto cloud2 = cloud1->duplicate();
        cloud2->opacity(30);
        cloud2->translate(400, 100);
        canvas->push(cloud2);

        clouds.push_back({cloud2, 400, 100, 0.125f, size});

        auto cloud3 = cloud1->duplicate();
        cloud3->opacity(20);
        cloud3->translate(1200, 200);
        canvas->push(cloud3);

        clouds.push_back({cloud3, 1200, 200, 0.075f, size});

        auto darkness = tvg::Shape::gen();
        darkness->appendRect(0, 0, w, h);
        darkness->fill(0, 0, 0, 150);
        canvas->push(darkness);

        //rain drops
        size = w / COUNT;
        raindrops.reserve(COUNT);

        for (int i = 0; i < COUNT; ++i) {
            auto shape = tvg::Shape::gen();
            float x = size * i;
            raindrops.push_back({shape, x, float(rand()%h), 10 + float(rand() % 100) * 0.1f, 0 /* unused */});
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
        for (auto& p : raindrops) {
            p.y += p.speed;
            if (p.y > h) {
                p.y -= h;
            }
            p.obj->translate(p.x, p.y);
        }

        for (auto& p : clouds) {
            p.x -= p.speed;
            if (p.x + p.size < 0) {
                p.x = w;
            }
            p.obj->translate(p.x, p.y);
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