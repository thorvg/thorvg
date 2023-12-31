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

    //Prepare Round Rectangle
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape1->fill(0, 255, 0);                     //r, g, b
    if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

    //Prepare Round Rectangle2
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(100, 100, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape2->fill(255, 255, 0);                       //r, g, b
    if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

    //Prepare Round Rectangle3
    auto shape3 = tvg::Shape::gen();
    shape3->appendRect(200, 200, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape3->fill(0, 255, 255);                       //r, g, b
    if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;

    //Prepare Scene
    auto scene = tvg::Scene::gen();

    auto shape4 = tvg::Shape::gen();
    shape4->appendCircle(400, 400, 100, 100);
    shape4->fill(255, 0, 0);
    shape4->strokeWidth(5);
    shape4->strokeFill(255, 255, 255);
    scene->push(std::move(shape4));

    auto shape5 = tvg::Shape::gen();
    shape5->appendCircle(550, 550, 150, 150);
    shape5->fill(255, 0, 255);
    shape5->strokeWidth(5);
    shape5->strokeFill(255, 255, 255);
    scene->push(std::move(shape5));

    if (canvas->push(std::move(scene)) != tvg::Result::Success) return;
}


void tvgUpdateCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Circular list
    auto& list = canvas->paints();
    auto paint = *list.begin();
    list.pop_front();
    list.push_back(paint);
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


Eina_Bool timerSwCb(void *data)
{
    tvgUpdateCmds(swCanvas.get());

    Eo* img = (Eo*) data;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);

    return ECORE_CALLBACK_RENEW;
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


Eina_Bool timerGlCb(void *data)
{
    tvgUpdateCmds(glCanvas.get());

    return ECORE_CALLBACK_RENEW;
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    auto tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Threads Count
    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(threads) == tvg::Result::Success) {

        elm_init(argc, argv);

        Ecore_Timer* timer;

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView();
            timer = ecore_timer_add(0.33, timerSwCb, view);
        } else {
            auto view = createGlView();
            timer = ecore_timer_add(0.33, timerGlCb, view);
        }

        elm_run();

        ecore_timer_del(timer);

        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
