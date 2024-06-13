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

    //Mask
    auto mask = tvg::Shape::gen();
    mask->appendCircle(200, 200, 125, 125);
    mask->fill(255, 0, 0);

    auto fill = tvg::LinearGradient::gen();
    fill->linear(0, 0, 400, 400);
    tvg::Fill::ColorStop colorStops[2];
    colorStops[0] = {0,0,0,0,255};
    colorStops[1] = {1,255,255,255,255};
    fill->colorStops(colorStops,2);
    shape->fill(std::move(fill));

    shape->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
    canvas->push(std::move(shape));

//-------------------------------------------

    //Star
    auto shape1 = tvg::Shape::gen();
    shape1->moveTo(599, 34);
    shape1->lineTo(653, 143);
    shape1->lineTo(774, 160);
    shape1->lineTo(687, 244);
    shape1->lineTo(707, 365);
    shape1->lineTo(599, 309);
    shape1->lineTo(497, 365);
    shape1->lineTo(512, 245);
    shape1->lineTo(426, 161);
    shape1->lineTo(546, 143);
    shape1->close();

    //Mask
    auto mask1 = tvg::Shape::gen();
    mask1->appendCircle(600, 200, 125, 125);
    mask1->fill(255, 0, 0);

    auto fill1 = tvg::LinearGradient::gen();
    fill1->linear(400, 0, 800, 400);
    tvg::Fill::ColorStop colorStops1[2];
    colorStops1[0] = {0,0,0,0,255};
    colorStops1[1] = {1,1,255,255,255};
    fill1->colorStops(colorStops1,2);
    shape1->fill(std::move(fill1));

    shape1->composite(std::move(mask1), tvg::CompositeMethod::AlphaMask);
    canvas->push(std::move(shape1));

//-------------------------------------------

    //Solid Rectangle
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(0, 400, 400, 400);

    //Mask
    auto mask2 = tvg::Shape::gen();
    mask2->appendCircle(200, 600, 125, 125);
    mask2->fill(255, 0, 0);

    auto fill2 = tvg::LinearGradient::gen();
    fill2->linear(0, 400, 400, 800);
    tvg::Fill::ColorStop colorStops2[2];
    colorStops2[0] = {0,0,0,0,255};
    colorStops2[1] = {1,255,255,255,255};
    fill2->colorStops(colorStops2,2);
    shape2->fill(std::move(fill2));

    shape2->composite(std::move(mask2), tvg::CompositeMethod::InvAlphaMask);
    canvas->push(std::move(shape2));

//-------------------------------------------

    // Star
    auto shape3 = tvg::Shape::gen();
    shape3->moveTo(599, 434);
    shape3->lineTo(653, 543);
    shape3->lineTo(774, 560);
    shape3->lineTo(687, 644);
    shape3->lineTo(707, 765);
    shape3->lineTo(599, 709);
    shape3->lineTo(497, 765);
    shape3->lineTo(512, 645);
    shape3->lineTo(426, 561);
    shape3->lineTo(546, 543);
    shape3->close();

    //Mask
    auto mask3 = tvg::Shape::gen();
    mask3->appendCircle(600, 600, 125, 125);
    mask3->fill(255, 0, 0);

    auto fill3 = tvg::LinearGradient::gen();
    fill3->linear(400, 400, 800, 800);
    tvg::Fill::ColorStop colorStops3[2];
    colorStops3[0] = {0,0,0,0,255};
    colorStops3[1] = {1,1,255,255,255};
    fill3->colorStops(colorStops3,2);
    shape3->fill(std::move(fill3));

    shape3->composite(std::move(mask3), tvg::CompositeMethod::InvAlphaMask);
    canvas->push(std::move(shape3));

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
