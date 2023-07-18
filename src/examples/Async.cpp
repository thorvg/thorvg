/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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

#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/
#define COUNT 50

static double t1, t2, t3, t4;
static unsigned cnt = 0;

bool tvgUpdateCmds(tvg::Canvas* canvas)
{
   if (!canvas) return false;

    auto t = SDL_GetTicks64();

    //Explicitly clear all retained paint nodes.
    if (canvas->clear() != tvg::Result::Success) {
        //Logically wrong! Probably, you missed to call sync() before.
        return false;
    }

    t1 = t;
    t2 = SDL_GetTicks64();

    for (int i = 0; i < COUNT; i++) {
        auto shape = tvg::Shape::gen();

        float x = rand() % (WIDTH/2);
        float y = rand() % (HEIGHT/2);
        float w = 1 + rand() % (int)(WIDTH * 1.3 / 2);
        float h = 1 + rand() %  (int)(HEIGHT * 1.3 / 2);

        shape->appendRect(x, y, w, h);

        //LinearGradient
        auto fill = tvg::LinearGradient::gen();
        fill->linear(x, y, x + w, y + h);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops[3];
        colorStops[0] = {0, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};
        colorStops[1] = {1, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};
        colorStops[2] = {2, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};

        fill->colorStops(colorStops, 3);
        shape->fill(std::move(fill));

        if (canvas->push(std::move(shape)) != tvg::Result::Success) {
            //Did you call clear()? Make it sure if canvas is on rendering
            break;
        }
    }

    t3 = SDL_GetTicks64();

    //Drawing task can be performed asynchronously.
    if (canvas->draw() != tvg::Result::Success) return false;

    return true;
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);
}

bool animSwCb(void* data)
{
    if (!tvgUpdateCmds(swCanvas.get())) return true;

    //Update Efl Canvas
    // Eo* img = (Eo*) data;
    // evas_object_image_pixels_dirty_set(img, EINA_TRUE);
    // evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);

    return true;
}

void drawSwView(void* data)
{
    if (!tvgUpdateCmds(swCanvas.get())) return ;

    //Make it guarantee finishing drawing task.
    swCanvas->sync();

    t4 = SDL_GetTicks64();

    printf("[%5d]: total[%fs] = clear[%fs], update[%fs], render[%fs]\n", ++cnt, t4 - t1, t2 - t1, t3 - t2, t4 - t3);
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(SDL_Window *obj)
{
    static constexpr auto BPP = 4;

    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();
    glCanvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);
}

void drawGLview(SDL_Window *obj)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!tvgUpdateCmds(glCanvas.get())) return ;

    glCanvas->sync();
}

bool animGlCb(void* data)
{
    if (!tvgUpdateCmds(glCanvas.get())) return true;

    //Drawing task can be performed asynchronously.
    glCanvas->draw();

    return true;
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Initialize ThorVG Engine
    if (tvgEngine == tvg::CanvasEngine::Sw) {
        cout << "tvg engine: software" << endl;
    } else {
        cout << "tvg engine: opengl" << endl;
    }

    //Threads Count
    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            createSwView();
        } else {
            createGlView();
        }


        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
