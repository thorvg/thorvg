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

    //Create a Scene
    auto scene = tvg::Scene::gen();

    //Prepare Round Rectangle
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape1->fill(0, 255, 0);                     //r, g, b
    scene->push(std::move(shape1));

    //Prepare Circle
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(400, 400, 200, 200);    //cx, cy, radiusW, radiusH
    shape2->fill(255, 255, 0);                   //r, g, b
    scene->push(std::move(shape2));

    //Prepare Ellipse
    auto shape3 = tvg::Shape::gen();
    shape3->appendCircle(600, 600, 150, 100);    //cx, cy, radiusW, radiusH
    shape3->fill(0, 255, 255);                   //r, g, b
    scene->push(std::move(shape3));

    //Create another Scene
    auto scene2 = tvg::Scene::gen();

    //Star
    auto shape4 = tvg::Shape::gen();

    //Appends Paths
    shape4->moveTo(199, 34);
    shape4->lineTo(253, 143);
    shape4->lineTo(374, 160);
    shape4->lineTo(287, 244);
    shape4->lineTo(307, 365);
    shape4->lineTo(199, 309);
    shape4->lineTo(97, 365);
    shape4->lineTo(112, 245);
    shape4->lineTo(26, 161);
    shape4->lineTo(146, 143);
    shape4->close();
    shape4->fill(0, 0, 255);
    scene2->push(std::move(shape4));

    //Circle
    auto shape5 = tvg::Shape::gen();

    auto cx = 550.0f;
    auto cy = 550.0f;
    auto radius = 125.0f;
    auto halfRadius = radius * 0.552284f;

    //Append Paths
    shape5->moveTo(cx, cy - radius);
    shape5->cubicTo(cx + halfRadius, cy - radius, cx + radius, cy - halfRadius, cx + radius, cy);
    shape5->cubicTo(cx + radius, cy + halfRadius, cx + halfRadius, cy + radius, cx, cy+ radius);
    shape5->cubicTo(cx - halfRadius, cy + radius, cx - radius, cy + halfRadius, cx - radius, cy);
    shape5->cubicTo(cx - radius, cy - halfRadius, cx - halfRadius, cy - radius, cx, cy - radius);
    shape5->fill(255, 0, 0);
    scene2->push(std::move(shape5));

    //Push scene2 onto the scene
    scene->push(std::move(scene2));

    //Draw the Scene onto the Canvas
    canvas->push(std::move(scene));
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void initSwView(uint32_t* buffer)
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

void initGlView(Evas_Object *obj)
{
    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();

    //Get the drawing target id
    int32_t targetId;
    auto gl = elm_glview_gl_api_get(obj);
    gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &targetId);

    glCanvas->target(targetId, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(glCanvas.get());
}

void drawGlView(Evas_Object *obj)
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
