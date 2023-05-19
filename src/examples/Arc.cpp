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

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Arc Line
    auto shape1 = tvg::Shape::gen();
    shape1->appendArc(150, 150, 80, 10, 180, false);
    shape1->stroke(255, 255, 255, 255);
    shape1->stroke(2);
    if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

    auto shape2 = tvg::Shape::gen();
    shape2->appendArc(400, 150, 80, 0, 300, false);
    shape2->stroke(255, 255, 255, 255);
    shape2->stroke(2);
    if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

    auto shape3 = tvg::Shape::gen();
    shape3->appendArc(600, 150, 80, 300, 60, false);
    shape3->stroke(255, 255, 255, 255);
    shape3->stroke(2);
    if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;

    //Pie Line
    auto shape4 = tvg::Shape::gen();
    shape4->appendArc(150, 400, 80, 10, 180, true);
    shape4->stroke(255, 255, 255, 255);
    shape4->stroke(2);
    if (canvas->push(std::move(shape4)) != tvg::Result::Success) return;

    auto shape5 = tvg::Shape::gen();
    shape5->appendArc(400, 400, 80, 0, 300, true);
    shape5->stroke(255, 255, 255, 255);
    shape5->stroke(2);
    if (canvas->push(std::move(shape5)) != tvg::Result::Success) return;

    auto shape6 = tvg::Shape::gen();
    shape6->appendArc(600, 400, 80, 300, 60, true);
    shape6->stroke(255, 255, 255, 255);
    shape6->stroke(2);
    if (canvas->push(std::move(shape6)) != tvg::Result::Success) return;

    //Pie Fill
    auto shape7 = tvg::Shape::gen();
    shape7->appendArc(150, 650, 80, 10, 180, true);
    shape7->fill(255, 255, 255, 255);
    shape7->stroke(255, 0, 0, 255);
    shape7->stroke(2);
    if (canvas->push(std::move(shape7)) != tvg::Result::Success) return;

    auto shape8 = tvg::Shape::gen();
    shape8->appendArc(400, 650, 80, 0, 300, true);
    shape8->fill(255, 255, 255, 255);
    shape8->stroke(255, 0, 0, 255);
    shape8->stroke(2);
    if (canvas->push(std::move(shape8)) != tvg::Result::Success) return;

    auto shape9 = tvg::Shape::gen();
    shape9->appendArc(600, 650, 80, 300, 60, true);
    shape9->fill(255, 255, 255, 255);
    shape9->stroke(255, 0, 0, 255);
    shape9->stroke(2);
    if (canvas->push(std::move(shape9)) != tvg::Result::Success) return;
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
