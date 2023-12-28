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

#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

static const char* svg = "<svg height=\"1000\" viewBox=\"0 0 1000 1000\" width=\"1000\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M.10681413.09784845 1000.0527.01592069V1000.0851L.06005738 999.9983Z\" fill=\"#ffffff\" stroke-width=\"3.910218\"/><g fill=\"#252f35\"><g stroke-width=\"3.864492\"><path d=\"M256.61221 100.51736H752.8963V386.99554H256.61221Z\"/><path d=\"M201.875 100.51736H238.366478V386.99554H201.875Z\"/><path d=\"M771.14203 100.51736H807.633508V386.99554H771.14203Z\"/></g><path d=\"M420.82388 380H588.68467V422.805317H420.82388Z\" stroke-width=\"3.227\"/><path d=\"m420.82403 440.7101v63.94623l167.86079 25.5782V440.7101Z\"/><path d=\"M420.82403 523.07258V673.47362L588.68482 612.59701V548.13942Z\"/></g><g fill=\"#222f35\"><path d=\"M420.82403 691.37851 588.68482 630.5019 589 834H421Z\"/><path d=\"m420.82403 852.52249h167.86079v28.64782H420.82403v-28.64782 0 0\"/><path d=\"m439.06977 879.17031c0 0-14.90282 8.49429-18.24574 15.8161-4.3792 9.59153 0 31.63185 0 31.63185h167.86079c0 0 4.3792-22.04032 0-31.63185-3.34292-7.32181-18.24574-15.8161-18.24574-15.8161z\"/></g><g fill=\"#ffffff\"><path d=\"m280 140h15v55l8 10 8-10v-55h15v60l-23 25-23-25z\"/><path d=\"m335 140v80h45v-50h-25v10h10v30h-15v-57h18v-13z\"/></g></svg>";


void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);          //x, y, w, h
    shape->fill(255, 255, 255);                      //r, g, b

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    auto picture = tvg::Picture::gen();
    if (picture->load(svg, strlen(svg), "svg", false) != tvg::Result::Success) return;

    picture->size(WIDTH, HEIGHT);

    canvas->push(std::move(picture));
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

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(swCanvas.get());
}

void drawSwView(void* data, Eo* obj)
{
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(Evas_Object *obj)
{
    static constexpr auto BPP = 4;

    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();
    glCanvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(glCanvas.get());
}

void drawGLview(Evas_Object *obj)
{
    auto gl = elm_glview_gl_api_get(obj);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
    }
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

        elm_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            createSwView();
        } else {
            createGlView();
        }

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
