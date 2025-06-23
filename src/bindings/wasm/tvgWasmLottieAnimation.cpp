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

#include <thorvg.h>
#include <emscripten.h>
#include <emscripten/bind.h>
#include "tvgPicture.h"

using namespace emscripten;
using namespace std;
using namespace tvg;


struct TvgEngineMethod
{
    virtual ~TvgEngineMethod() {}
    virtual Canvas* init() = 0;
    virtual void resize(Canvas* canvas, int w, int h) = 0;
    virtual val output(int w, int h)
    {
        return val(typed_memory_view<uint8_t>(0, nullptr));
    }
};

struct TvgSwEngine : TvgEngineMethod
{
    uint8_t* buffer = nullptr;

    ~TvgSwEngine()
    {
        std::free(buffer);
        Initializer::term();
    }

    Canvas* init() override
    {
        Initializer::init(0);
        return SwCanvas::gen();
    }

    void resize(Canvas* canvas, int w, int h) override
    {
        std::free(buffer);
        buffer = (uint8_t*)std::malloc(w * h * sizeof(uint32_t));
        static_cast<SwCanvas*>(canvas)->target((uint32_t *)buffer, w, w, h, ColorSpace::ABGR8888S);
    }

    val output(int w, int h) override
    {
        return val(typed_memory_view(w * h * 4, buffer));
    }
};

class __attribute__((visibility("default"))) TvgTestApp
{
public:
    ~TvgTestApp()
    {
        delete(cityview);
        delete(darkness);
        delete(canvas);
        delete(engine);
    }

    explicit TvgTestApp()
    {
        this->engine = new TvgSwEngine;
        canvas = this->engine->init();
        cityview = Picture::gen();
        darkness = Shape::gen();
    }

    val size()
    {
        return val(typed_memory_view(2, psize));
    }

    bool load(string data, string mimetype, int width, int height)
    {
        if (!canvas) return false;

        if (data.empty()) return false;

        this->width = 0;
        this->height = 0;

        canvas->remove();
        raindrops.clear();

        cityview->load(data.c_str(), data.size(), mimetype.c_str(), nullptr, false);
        cityview->size(&psize[0], &psize[1]);
        canvas->push(cityview);

        darkness->fill(0, 0, 0, 150);
        canvas->push(darkness);

        //rain drops
        auto size = width / COUNT;
        raindrops.reserve(COUNT);

        for (int i = 0; i < COUNT; ++i) {
            auto shape = tvg::Shape::gen();
            raindrops.push_back({shape, 0, 0, 0, 0});
            shape->appendRect(0, 0, 1, rand() % 20 + size);
            shape->fill(255, 255, 255, 70 + rand() % 100);
            canvas->push(shape);
        }

        resize(width, height);

        return true;
    }

    val render()
    {
        if (!canvas || !cityview) return val(typed_memory_view<uint8_t>(0, nullptr));

        canvas->draw(clear);
        canvas->sync();

        return engine->output(width, height);
    }

    bool update()
    {
        for (auto& p : raindrops) {
            p.y += p.speed;
            if (p.y > height) {
                p.y -= height;
            }
            p.obj->translate(p.x, p.y);
        }

        canvas->update();
        return true;
    }

    bool viewport(float x, float y, float width, float height)
    {
        if (!canvas) return false;
        canvas->viewport(x, y, width, height);
        return true;
    }

    void partial(bool on)
    {
        clear = !on;
    }

    void resize(int width, int height)
    {
        if (!canvas) return;
        if (this->width == width && this->height == height) return;

        canvas->sync();

        this->width = width;
        this->height = height;

        engine->resize(canvas, width, height);

        //darkness
        darkness->reset();
        darkness->appendRect(0, 0, width, height);

        //citiview
        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        if (psize[0] > psize[1]) {
            scale = width / psize[0];
            shiftY = (height - psize[1] * scale) * 0.5f;
        } else {
            scale = height / psize[1];
            shiftX = (width - psize[0] * scale) * 0.5f;
        }
        cityview->scale(scale);
        cityview->translate(shiftX, shiftY);

        auto size = width / COUNT;
        auto i = 0;
        for (auto& p : raindrops) {
            p.x = size * i;
            p.y = float(rand()%height);
            p.speed = (10 + float(rand() % 100) * 0.1f) * 2;
            ++i;
        }
    }

private:
    struct Particle {
        tvg::Paint* obj;
        float x, y;
        float speed;
        float size;
    };

    Picture*                cityview = nullptr;
    Shape*                  darkness = nullptr;

    const float COUNT = 3600.0f;
    std::vector<Particle> raindrops;
    std::vector<Particle> clouds;

    Canvas*                canvas = nullptr;
    TvgEngineMethod*       engine = nullptr;
    uint32_t               width = 0;
    uint32_t               height = 0;
    float                  psize[2];         //cityview size
    bool                   clear = false;
};


EMSCRIPTEN_BINDINGS(thorvg_bindings)
{
    class_<TvgTestApp>("TvgTestApp")
        .constructor()
        .function("size", &TvgTestApp::size)
        .function("render", &TvgTestApp::render)
        .function("load", &TvgTestApp::load)
        .function("update", &TvgTestApp::update)
        .function("viewport", &TvgTestApp::viewport)
        .function("resize", &TvgTestApp::resize)
        .function("partial", &TvgTestApp::partial);
}