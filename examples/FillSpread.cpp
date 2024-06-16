/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

    const int colorCnt = 4;
    tvg::Fill::ColorStop colorStops[colorCnt];
    colorStops[0] = {0.0f, 127, 39, 255, 255};
    colorStops[1] = {0.33f, 159, 112, 253, 255};
    colorStops[2] = {0.66f, 253, 191, 96, 255};
    colorStops[3] = {1.0f, 255, 137, 17, 255};

    //Radial grad
    {
        float x1, y1 = 80.0f, r = 120.0f;

        //Pad
        x1 = 20.0f;
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(x1, y1, 2.0f * r, 2.0f * r);

        auto fill1 = tvg::RadialGradient::gen();
        fill1->radial(x1 + r, y1 + r, 40.0f);
        fill1->colorStops(colorStops, colorCnt);
        fill1->spread(tvg::FillSpread::Pad);
        shape1->fill(std::move(fill1));

        if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

        //Reflect
        x1 = 280.0f;
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(x1, y1, 2.0f * r, 2.0f * r);

        auto fill2 = tvg::RadialGradient::gen();
        fill2->radial(x1 + r, y1 + r, 40.0f);
        fill2->colorStops(colorStops, colorCnt);
        fill2->spread(tvg::FillSpread::Reflect);
        shape2->fill(std::move(fill2));

        if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

        //Repeat
        x1 = 540.0f;
        auto shape3 = tvg::Shape::gen();
        shape3->appendRect(x1, y1, 2.0f * r, 2.0f * r);

        auto fill3 = tvg::RadialGradient::gen();
        fill3->radial(x1 + r, y1 + r, 40.0f);
        fill3->colorStops(colorStops, colorCnt);
        fill3->spread(tvg::FillSpread::Repeat);
        shape3->fill(std::move(fill3));

        if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;
    }

    //Linear grad
    {
        float x1, y1 = 480.0f, r = 120.0f;

        //Pad
        x1 = 20.0f;
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(x1, y1, 2.0f * r, 2.0f * r);

        auto fill1 = tvg::LinearGradient::gen();
        fill1->linear(x1, y1, x1 + 50.0f, y1 + 50.0f);
        fill1->colorStops(colorStops, colorCnt);
        fill1->spread(tvg::FillSpread::Pad);
        shape1->fill(std::move(fill1));

        if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

        //Reflect
        x1 = 280.0f;
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(x1, y1, 2.0f * r, 2.0f * r);

        auto fill2 = tvg::LinearGradient::gen();
        fill2->linear(x1, y1, x1 + 50.0f, y1 + 50.0f);
        fill2->colorStops(colorStops, colorCnt);
        fill2->spread(tvg::FillSpread::Reflect);
        shape2->fill(std::move(fill2));

        if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

        //Repeat
        x1 = 540.0f;
        auto shape3 = tvg::Shape::gen();
        shape3->appendRect(x1, y1, 2.0f * r, 2.0f * r);

        auto fill3 = tvg::LinearGradient::gen();
        fill3->linear(x1, y1, x1 + 50.0f, y1 + 50.0f);
        fill3->colorStops(colorStops, colorCnt);
        fill3->spread(tvg::FillSpread::Repeat);
        shape3->fill(std::move(fill3));

        if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;
    }
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
