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

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Shape 1
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(50, 50, 200, 200);
    shape1->fill(50, 50, 50);
    shape1->stroke(255, 255, 255);            //color: r, g, b
    shape1->stroke(tvg::StrokeJoin::Bevel);   //default is Bevel
    shape1->stroke(10);                       //width: 10px

    if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

    //Shape 2
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(300, 50, 200, 200);
    shape2->fill(50, 50, 50);
    shape2->stroke(255, 255, 255);
    shape2->stroke(tvg::StrokeJoin::Round);
    shape2->stroke(10);

    if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

    //Shape 3
    auto shape3 = tvg::Shape::gen();
    shape3->appendRect(550, 50, 200, 200);
    shape3->fill(50, 50, 50);
    shape3->stroke(255, 255, 255);
    shape3->stroke(tvg::StrokeJoin::Miter);
    shape3->stroke(10);

    if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;

    //Shape 4
    auto shape4 = tvg::Shape::gen();
    shape4->appendCircle(150, 400, 100, 100);
    shape4->fill(50, 50, 50);
    shape4->stroke(255, 255, 255);
    shape4->stroke(1);

    if (canvas->push(std::move(shape4)) != tvg::Result::Success) return;

    //Shape 5
    auto shape5 = tvg::Shape::gen();
    shape5->appendCircle(400, 400, 100, 100);
    shape5->fill(50, 50, 50);
    shape5->stroke(255, 255, 255);
    shape5->stroke(2);

    if (canvas->push(std::move(shape5)) != tvg::Result::Success) return;

    //Shape 6
    auto shape6 = tvg::Shape::gen();
    shape6->appendCircle(650, 400, 100, 100);
    shape6->fill(50, 50, 50);
    shape6->stroke(255, 255, 255);
    shape6->stroke(4);

    if (canvas->push(std::move(shape6)) != tvg::Result::Success) return;

    //Stroke width test
    for (int i = 0; i < 10; ++i) {
        auto hline = tvg::Shape::gen();
        hline->moveTo(50, 550 + (25 * i));
        hline->lineTo(300, 550 + (25 * i));
        hline->stroke(255, 255, 255);            //color: r, g, b
        hline->stroke(i + 1);                    //stroke width
        hline->stroke(tvg::StrokeCap::Round);    //default is Square
        if (canvas->push(std::move(hline)) != tvg::Result::Success) return;

        auto vline = tvg::Shape::gen();
        vline->moveTo(500 + (25 * i), 550);
        vline->lineTo(500 + (25 * i), 780);
        vline->stroke(255, 255, 255);            //color: r, g, b
        vline->stroke(i + 1);                    //stroke width
        vline->stroke(tvg::StrokeCap::Round);    //default is Square
        if (canvas->push(std::move(vline)) != tvg::Result::Success) return;
    }

    //Stroke cap test
    auto line1 = tvg::Shape::gen();
    line1->moveTo(360, 580);
    line1->lineTo(450, 580);
    line1->stroke(255, 255, 255);                //color: r, g, b
    line1->stroke(15);
    line1->stroke(tvg::StrokeCap::Round);

    auto line2 = tvg::cast<tvg::Shape>(line1->duplicate());
    auto line3 = tvg::cast<tvg::Shape>(line1->duplicate());
    if (canvas->push(std::move(line1)) != tvg::Result::Success) return;

    line2->stroke(tvg::StrokeCap::Square);
    line2->translate(0, 50);
    if (canvas->push(std::move(line2)) != tvg::Result::Success) return;

    line3->stroke(tvg::StrokeCap::Butt);
    line3->translate(0, 100);
    if (canvas->push(std::move(line3)) != tvg::Result::Success) return;
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
