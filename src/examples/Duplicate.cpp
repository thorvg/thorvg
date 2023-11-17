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
#include <fstream>

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Duplicate Shapes
    {
        //Original Shape
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(10, 10, 200, 200);
        shape1->appendRect(220, 10, 100, 100);

        shape1->stroke(3);
        shape1->stroke(0, 255, 0);

        float dashPattern[2] = {4, 4};
        shape1->stroke(dashPattern, 2);
        shape1->fill(255, 0, 0);

        //Duplicate Shape, Switch fill method
        auto shape2 = tvg::cast<tvg::Shape>(shape1->duplicate());
        shape2->translate(0, 220);

        auto fill = tvg::LinearGradient::gen();
        fill->linear(10, 10, 440, 200);

        tvg::Fill::ColorStop colorStops[2];
        colorStops[0] = {0, 0, 0, 0, 255};
        colorStops[1] = {1, 255, 255, 255, 255};
        fill->colorStops(colorStops, 2);

        shape2->fill(std::move(fill));

        //Duplicate Shape 2
        auto shape3 = tvg::cast<tvg::Shape>(shape2->duplicate());
        shape3->translate(0, 440);

        canvas->push(std::move(shape1));
        canvas->push(std::move(shape2));
        canvas->push(std::move(shape3));
    }

    //Duplicate Scene
    {
        //Create a Scene1
        auto scene1 = tvg::Scene::gen();

        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(0, 0, 400, 400, 50, 50);
        shape1->fill(0, 255, 0);
        scene1->push(std::move(shape1));

        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(400, 400, 200, 200);
        shape2->fill(255, 255, 0);
        scene1->push(std::move(shape2));

        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(600, 600, 150, 100);
        shape3->fill(0, 255, 255);
        scene1->push(std::move(shape3));

        scene1->scale(0.25);
        scene1->translate(400, 0);

        //Duplicate Scene1
        auto scene2 = tvg::cast<tvg::Scene>(scene1->duplicate());
        scene2->translate(600, 0);

        canvas->push(std::move(scene1));
        canvas->push(std::move(scene2));
    }

    //Duplicate Picture - svg
    {
        auto picture1 = tvg::Picture::gen();
        picture1->load(EXAMPLE_DIR"/tiger.svg");
        picture1->translate(350, 200);
        picture1->scale(0.25);

        auto picture2 = tvg::cast<tvg::Picture>(picture1->duplicate());
        picture2->translate(550, 250);

        canvas->push(std::move(picture1));
        canvas->push(std::move(picture2));
    }

    //Duplicate Picture - raw
    {
        string path(EXAMPLE_DIR"/rawimage_200x300.raw");
        ifstream file(path, ios::binary);
        if (!file.is_open()) return ;
        uint32_t* data = (uint32_t*)malloc(sizeof(uint32_t) * 200 * 300);
        file.read(reinterpret_cast<char*>(data), sizeof(uint32_t) * 200 * 300);
        file.close();

        auto picture1 = tvg::Picture::gen();
        if (picture1->load(data, 200, 300, true) != tvg::Result::Success) return;
        picture1->scale(0.8);
        picture1->translate(400, 450);

        auto picture2 = tvg::cast<tvg::Picture>(picture1->duplicate());
        picture2->translate(600, 550);
        picture2->scale(0.7);
        picture2->rotate(8);

        canvas->push(std::move(picture1));
        canvas->push(std::move(picture2));

        free(data);
    }
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
