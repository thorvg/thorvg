/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

    //BG 
    auto bg = tvg::Shape::gen();
    bg->appendRect(0, 0, WIDTH, HEIGHT);
    bg->fill(100, 100, 100);
    canvas->push(std::move(bg));

    //Create a Scene
    auto scene = tvg::Scene::gen();
    scene->blend(tvg::BlendMethod::Add);

    //Prepare Circle
    auto shape1 = tvg::Shape::gen();
    shape1->appendCircle(400, 400, 250, 250);
    shape1->fill(255, 255, 0);
    scene->push(std::move(shape1));

    //Round rectangle
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(450, 100, 200, 200, 50, 50);
    shape2->fill(0, 255, 0);
    shape2->strokeWidth(10);
    shape2->strokeFill(255, 255, 255);
    scene->push(std::move(shape2));

    //Draw the Scene onto the Canvas
    canvas->push(std::move(scene));

    //Create a Scene 2
    auto scene2 = tvg::Scene::gen();
    scene2->opacity(127);              //Apply opacity to scene (0 - 255)
    scene2->blend(tvg::BlendMethod::Overlay);
    scene2->scale(1.2);

    //Star
    auto shape3 = tvg::Shape::gen();

    //Appends Paths
    shape3->moveTo(199, 34);
    shape3->lineTo(253, 143);
    shape3->lineTo(374, 160);
    shape3->lineTo(287, 244);
    shape3->lineTo(307, 365);
    shape3->lineTo(199, 309);
    shape3->lineTo(97, 365);
    shape3->lineTo(112, 245);
    shape3->lineTo(26, 161);
    shape3->lineTo(146, 143);
    shape3->close();
    shape3->fill(0, 0, 255);
    shape3->strokeWidth(10);
    shape3->strokeFill(255, 255, 255);
    shape3->opacity(127);

    scene2->push(std::move(shape3));

    //Circle
    auto shape4 = tvg::Shape::gen();

    auto cx = 150.0f;
    auto cy = 150.0f;
    auto radius = 50.0f;
    auto halfRadius = radius * 0.552284f;

    //Append Paths
    shape4->moveTo(cx, cy - radius);
    shape4->cubicTo(cx + halfRadius, cy - radius, cx + radius, cy - halfRadius, cx + radius, cy);
    shape4->cubicTo(cx + radius, cy + halfRadius, cx + halfRadius, cy + radius, cx, cy+ radius);
    shape4->cubicTo(cx - halfRadius, cy + radius, cx - radius, cy + halfRadius, cx - radius, cy);
    shape4->cubicTo(cx - radius, cy - halfRadius, cx - halfRadius, cy - radius, cx, cy - radius);
    shape4->close();
    shape4->fill(255, 0, 0);
    shape4->strokeWidth(10);
    shape4->strokeFill(0, 0, 255);
    shape4->opacity(200);
    shape4->scale(3);
    scene2->push(std::move(shape4));

    //Draw the Scene onto the Canvas
    canvas->push(std::move(scene2));
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

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            createSwView();
        } else {
            createGlView();
        }

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
