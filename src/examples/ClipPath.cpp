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

void tvgDrawStar(tvg::Shape* star)
{
    star->moveTo(199, 34);
    star->lineTo(253, 143);
    star->lineTo(374, 160);
    star->lineTo(287, 244);
    star->lineTo(307, 365);
    star->lineTo(199, 309);
    star->lineTo(97, 365);
    star->lineTo(112, 245);
    star->lineTo(26, 161);
    star->lineTo(146, 143);
    star->close();
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;
    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);
    shape->fill(255, 255, 255);
    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    //////////////////////////////////////////////
    auto scene = tvg::Scene::gen();

    auto star1 = tvg::Shape::gen();
    tvgDrawStar(star1.get());
    star1->fill(255, 255, 0);
    star1->strokeFill(255 ,0, 0);
    star1->strokeWidth(10);

    //Move Star1
    star1->translate(-10, -10);

    // color/alpha/opacity are ignored for a clip object - no need to set them
    auto clipStar = tvg::Shape::gen();
    clipStar->appendCircle(200, 230, 110, 110);
    clipStar->translate(10, 10);

    star1->composite(std::move(clipStar), tvg::CompositeMethod::ClipPath);

    auto star2 = tvg::Shape::gen();
    tvgDrawStar(star2.get());
    star2->fill(0, 255, 255);
    star2->strokeFill(0 ,255, 0);
    star2->strokeWidth(10);
    star2->opacity(100);

    //Move Star2
    star2->translate(10, 40);

    // color/alpha/opacity are ignored for a clip object - no need to set them
    auto clip = tvg::Shape::gen();
    clip->appendCircle(200, 230, 130, 130);
    clip->translate(10, 10);

    scene->push(std::move(star1));
    scene->push(std::move(star2));

    //Clipping scene to shape
    scene->composite(std::move(clip), tvg::CompositeMethod::ClipPath);

    canvas->push(std::move(scene));

    //////////////////////////////////////////////
    auto star3 = tvg::Shape::gen();
    tvgDrawStar(star3.get());

    //Fill Gradient
    auto fill = tvg::LinearGradient::gen();
    fill->linear(100, 100, 300, 300);
    tvg::Fill::ColorStop colorStops[2];
    colorStops[0] = {0, 0, 0, 0, 255};
    colorStops[1] = {1, 255, 255, 255, 255};
    fill->colorStops(colorStops, 2);
    star3->fill(std::move(fill));

    star3->strokeFill(255 ,0, 0);
    star3->strokeWidth(10);
    star3->translate(400, 0);

    // color/alpha/opacity are ignored for a clip object - no need to set them
    auto clipRect = tvg::Shape::gen();
    clipRect->appendRect(500, 120, 200, 200);          //x, y, w, h
    clipRect->translate(20, 20);

    //Clipping scene to rect(shape)
    star3->composite(std::move(clipRect), tvg::CompositeMethod::ClipPath);

    canvas->push(std::move(star3));

    //////////////////////////////////////////////
    auto picture = tvg::Picture::gen();
    if (picture->load(EXAMPLE_DIR"/cartman.svg") != tvg::Result::Success) return;

    picture->scale(3);
    picture->translate(50, 400);

    // color/alpha/opacity are ignored for a clip object - no need to set them
    auto clipPath = tvg::Shape::gen();
    clipPath->appendCircle(200, 510, 50, 50);          //x, y, w, h, rx, ry
    clipPath->appendCircle(200, 650, 50, 50);          //x, y, w, h, rx, ry
    clipPath->translate(20, 20);

    //Clipping picture to path
    picture->composite(std::move(clipPath), tvg::CompositeMethod::ClipPath);

    canvas->push(std::move(picture));

    //////////////////////////////////////////////
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(500, 420, 100, 100, 20, 20);
    shape1->fill(255, 0, 255, 160);

    // color/alpha/opacity are ignored for a clip object - no need to set them
    auto clipShape = tvg::Shape::gen();
    clipShape->appendRect(600, 420, 100, 100);

    //Clipping shape1 to clipShape
    shape1->composite(std::move(clipShape), tvg::CompositeMethod::ClipPath);

    canvas->push(std::move(shape1));
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

unique_ptr<tvg::SwCanvas> swCanvas;

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
