/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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

    ifstream file(EXAMPLE_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*) malloc(sizeof(uint32_t) * (200 * 300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    auto scene = tvg::Scene::gen();

    auto image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;

    auto clip = tvg::Shape::gen();
    clip->appendRect(0, 0, 200, 200, 30, 30);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    auto mask = tvg::Shape::gen();
    mask->appendRect(0, 100, 100, 100, 0, 0);
    mask->fill(255, 0, 0, 155);
    scene->composite(move(mask), tvg::CompositeMethod::AlphaMask);

    canvas->push(move(scene));


    scene = tvg::Scene::gen();

    image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;

    clip = tvg::Shape::gen();
    clip->appendRect(0, 0, 200, 200, 90, 90);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    mask = tvg::Shape::gen();
    mask->appendRect(0, 90, 110, 110, 0, 0);
    mask->fill(255, 0, 0, 255);
    scene->composite(move(mask), tvg::CompositeMethod::InvAlphaMask);

    canvas->push(move(scene));


    scene = tvg::Scene::gen();

    image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->translate(200, 0);

    clip = tvg::Shape::gen();
    clip->appendRect(200, 0, 200, 200, 30, 30);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    mask = tvg::Shape::gen();
    mask->appendRect(200, 100, 100, 100, 0, 0);
    mask->fill(255, 0, 0, 155);
    scene->composite(move(mask), tvg::CompositeMethod::AlphaMask);

    canvas->push(move(scene));


    scene = tvg::Scene::gen();

    image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->translate(200, 0);

    clip = tvg::Shape::gen();
    clip->appendRect(200, 0, 200, 200, 90, 90);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    mask = tvg::Shape::gen();
    mask->appendRect(200, 90, 110, 110, 0, 0);
    mask->fill(255, 0, 0, 255);
    scene->composite(move(mask), tvg::CompositeMethod::InvAlphaMask);

    canvas->push(move(scene));


    scene = tvg::Scene::gen();

    image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->scale(2);

    clip = tvg::Shape::gen();
    clip->appendRect(0, 200, 200, 200, 30, 30);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    mask = tvg::Shape::gen();
    mask->appendRect(0, 300, 100, 100, 0, 0);
    mask->fill(255, 0, 0, 155);
    scene->composite(move(mask), tvg::CompositeMethod::AlphaMask);

    canvas->push(move(scene));


    scene = tvg::Scene::gen();

    image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->scale(2);

    clip = tvg::Shape::gen();
    clip->appendRect(0, 200, 200, 200, 90, 90);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    mask = tvg::Shape::gen();
    mask->appendRect(0, 290, 110, 110, 0, 0);
    mask->fill(255, 0, 0, 255);
    scene->composite(move(mask), tvg::CompositeMethod::InvAlphaMask);

    canvas->push(move(scene));


    scene = tvg::Scene::gen();

    image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->translate(0, 400);
    image->scale(0.49);

    clip = tvg::Shape::gen();
    clip->appendRect(0, 400, 98, 98, 30, 30);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    mask = tvg::Shape::gen();
    mask->appendRect(0, 450, 50, 50, 0, 0);
    mask->fill(255, 0, 0, 155);
    scene->composite(move(mask), tvg::CompositeMethod::AlphaMask);

    canvas->push(move(scene));


    scene = tvg::Scene::gen();

    image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->translate(0, 400);
    image->scale(0.49);

    clip = tvg::Shape::gen();
    clip->appendRect(0, 400, 98, 98, 90, 90);
    clip->fill(255, 0, 0, 255);
    image->composite(move(clip), tvg::CompositeMethod::ClipPath);
    scene->push(move(image));

    mask = tvg::Shape::gen();
    mask->appendRect(0, 445, 60, 60, 0, 0);
    mask->fill(255, 0, 0, 255);
    scene->composite(move(mask), tvg::CompositeMethod::InvAlphaMask);

    canvas->push(move(scene));

    free(data);
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
    //Drawing task can be performed asynchronously.
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
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
