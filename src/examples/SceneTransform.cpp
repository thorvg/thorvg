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

void tvgUpdateCmds(tvg::Canvas* canvas, float progress)
{
    if (!canvas) return;

    if (canvas->clear() != tvg::Result::Success) return;

    //Create a Scene1
    auto scene = tvg::Scene::gen();

    //Prepare Round Rectangle (Scene1)
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(-235, -250, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape1->fill(0, 255, 0);                           //r, g, b
    shape1->stroke(5);                                 //width
    shape1->stroke(255, 255, 255);                     //r, g, b
    scene->push(std::move(shape1));

    //Prepare Circle (Scene1)
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(-165, -150, 200, 200);    //cx, cy, radiusW, radiusH
    shape2->fill(255, 255, 0);                     //r, g, b
    scene->push(std::move(shape2));

    //Prepare Ellipse (Scene1)
    auto shape3 = tvg::Shape::gen();
    shape3->appendCircle(265, 250, 150, 100);      //cx, cy, radiusW, radiusH
    shape3->fill(0, 255, 255);                     //r, g, b
    scene->push(std::move(shape3));

    scene->translate(350, 350);
    scene->scale(0.5);
    scene->rotate(360 * progress);

    //Create Scene2
    auto scene2 = tvg::Scene::gen();

    //Star (Scene2)
    auto shape4 = tvg::Shape::gen();

    //Appends Paths
    shape4->moveTo(0, -114.5);
    shape4->lineTo(54, -5.5);
    shape4->lineTo(175, 11.5);
    shape4->lineTo(88, 95.5);
    shape4->lineTo(108, 216.5);
    shape4->lineTo(0, 160.5);
    shape4->lineTo(-102, 216.5);
    shape4->lineTo(-87, 96.5);
    shape4->lineTo(-173, 12.5);
    shape4->lineTo(-53, -5.5);
    shape4->close();
    shape4->fill(0, 0, 255, 127);
    shape4->stroke(3);                             //width
    shape4->stroke(0, 0, 255);                     //r, g, b
    scene2->push(std::move(shape4));

    //Circle (Scene2)
    auto shape5 = tvg::Shape::gen();

    auto cx = -150.0f;
    auto cy = -150.0f;
    auto radius = 100.0f;
    auto halfRadius = radius * 0.552284f;

    //Append Paths
    shape5->moveTo(cx, cy - radius);
    shape5->cubicTo(cx + halfRadius, cy - radius, cx + radius, cy - halfRadius, cx + radius, cy);
    shape5->cubicTo(cx + radius, cy + halfRadius, cx + halfRadius, cy + radius, cx, cy+ radius);
    shape5->cubicTo(cx - halfRadius, cy + radius, cx - radius, cy + halfRadius, cx - radius, cy);
    shape5->cubicTo(cx - radius, cy - halfRadius, cx - halfRadius, cy - radius, cx, cy - radius);
    shape5->close();
    shape5->fill(255, 0, 0, 127);
    scene2->push(std::move(shape5));

    scene2->translate(500, 350);
    scene2->rotate(360 * progress);

    //Push scene2 onto the scene
    scene->push(std::move(scene2));

    //Draw the Scene onto the Canvas
    canvas->push(std::move(scene));
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
    tvgUpdateCmds(swCanvas.get(), 0);
}

void transitSwCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    tvgUpdateCmds(swCanvas.get(), progress);

    //Update Efl Canvas
    Eo* img = (Eo*) effect;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
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
    tvgUpdateCmds(glCanvas.get(), 0);
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

void transitGlCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    tvgUpdateCmds(glCanvas.get(), progress);
    elm_glview_changed_set((Evas_Object*)effect);
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

        Elm_Transit *transit = elm_transit_add();

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView();
            elm_transit_effect_add(transit, transitSwCb, view, nullptr);
        } else {
            auto view = createGlView();
            elm_transit_effect_add(transit, transitGlCb, view, nullptr);
        }

        elm_transit_duration_set(transit, 2);
        elm_transit_repeat_times_set(transit, -1);
        elm_transit_auto_reverse_set(transit, EINA_TRUE);
        elm_transit_go(transit);

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
