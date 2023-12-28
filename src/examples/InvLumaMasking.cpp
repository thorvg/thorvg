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
#include <fstream>

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Solid Rectangle
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, 400, 400);
    shape->fill(255, 0, 0);

    //Mask
    auto mask = tvg::Shape::gen();
    mask->appendCircle(200, 200, 125, 125);
    mask->fill(255, 100, 255);

    //Nested Mask
    auto nMask = tvg::Shape::gen();
    nMask->appendCircle(220, 220, 125, 125);
    nMask->fill(255, 200, 255);

    mask->composite(std::move(nMask), tvg::CompositeMethod::InvLumaMask);
    shape->composite(std::move(mask), tvg::CompositeMethod::InvLumaMask);
    canvas->push(std::move(shape));

    //SVG
    auto svg = tvg::Picture::gen();
    if (svg->load(EXAMPLE_DIR"/cartman.svg") != tvg::Result::Success) return;
    svg->opacity(100);
    svg->scale(3);
    svg->translate(50, 400);

    //Mask2
    auto mask2 = tvg::Shape::gen();
    mask2->appendCircle(150, 500, 75, 75);
    mask2->appendRect(150, 500, 200, 200, 30, 30);
    mask2->fill(255, 255, 255);
    svg->composite(std::move(mask2), tvg::CompositeMethod::InvLumaMask);
    if (canvas->push(std::move(svg)) != tvg::Result::Success) return;

    //Star
    auto star = tvg::Shape::gen();
    star->fill(80, 80, 80);
    star->moveTo(599, 34);
    star->lineTo(653, 143);
    star->lineTo(774, 160);
    star->lineTo(687, 244);
    star->lineTo(707, 365);
    star->lineTo(599, 309);
    star->lineTo(497, 365);
    star->lineTo(512, 245);
    star->lineTo(426, 161);
    star->lineTo(546, 143);
    star->close();
    star->stroke(10);
    star->stroke(255, 255, 255);

    //Mask3
    auto mask3 = tvg::Shape::gen();
    mask3->appendCircle(600, 200, 125, 125);
    mask3->fill(0, 255, 255);
    star->composite(std::move(mask3), tvg::CompositeMethod::InvLumaMask);
    if (canvas->push(std::move(star)) != tvg::Result::Success) return;

    //Image
    ifstream file(EXAMPLE_DIR"/rawimage_200x300.raw", ios::binary);
    if (!file.is_open()) return;
    auto data = (uint32_t*) malloc(sizeof(uint32_t) * (200 * 300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    auto image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
    image->translate(500, 400);

    //Mask4
    auto mask4 = tvg::Scene::gen();
    auto mask4_rect = tvg::Shape::gen();
    mask4_rect->appendRect(500, 400, 200, 300);
    mask4_rect->fill(255, 255, 255);
    auto mask4_circle = tvg::Shape::gen();
    mask4_circle->appendCircle(600, 550, 125, 125);
    mask4_circle->fill(128, 0, 128);
    if (mask4->push(std::move(mask4_rect)) != tvg::Result::Success) return;
    if (mask4->push(std::move(mask4_circle)) != tvg::Result::Success) return;
    image->composite(std::move(mask4), tvg::CompositeMethod::InvLumaMask);
    if (canvas->push(std::move(image)) != tvg::Result::Success) return;

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
