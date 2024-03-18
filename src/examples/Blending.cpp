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

#include <fstream>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Normal
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 400, 400, 50, 50);
    shape1->fill(0, 255, 255);
    shape1->blend(tvg::BlendMethod::Normal);
    if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

    //Add
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(400, 400, 200, 200);
    shape2->fill(255, 255, 0, 170);
    shape2->blend(tvg::BlendMethod::Add);
    if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

    //Multiply
    auto shape3 = tvg::Shape::gen();
    shape3->appendCircle(400, 400, 250, 100);
    shape3->fill(255, 255, 255, 100);
    shape3->blend(tvg::BlendMethod::Multiply);
    if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;

    //Overlay
    auto shape4 = tvg::Shape::gen();
    shape4->moveTo(199, 234);
    shape4->lineTo(253, 343);
    shape4->lineTo(374, 360);
    shape4->lineTo(287, 444);
    shape4->lineTo(307, 565);
    shape4->lineTo(199, 509);
    shape4->lineTo(97, 565);
    shape4->lineTo(112, 445);
    shape4->lineTo(26, 361);
    shape4->lineTo(146, 343);
    shape4->close();
    shape4->fill(255, 0, 200, 200);
    shape4->blend(tvg::BlendMethod::Overlay);
    if (canvas->push(std::move(shape4)) != tvg::Result::Success) return;

    //Difference
    auto shape5 = tvg::Shape::gen();
    shape5->appendCircle(300, 600, 200, 200);

    //LinearGradient
    auto fill = tvg::LinearGradient::gen();
    fill->linear(300, 600, 200, 200);

    //Gradient Color Stops
    tvg::Fill::ColorStop colorStops[2];
    colorStops[0] = {0, 0, 0, 0, 255};
    colorStops[1] = {1, 255, 255, 255, 100};
    fill->colorStops(colorStops, 2);

    shape5->fill(std::move(fill));
    shape5->blend(tvg::BlendMethod::Difference);
    if (canvas->push(std::move(shape5)) != tvg::Result::Success) return;

    //Exclusion
    auto shape6 = tvg::Shape::gen();
    shape6->appendCircle(300, 800, 150, 150);

    //RadialGradient
    auto fill2 = tvg::RadialGradient::gen();
    fill2->radial(300, 800, 150);
    fill2->colorStops(colorStops, 2);

    shape6->fill(std::move(fill2));
    shape6->blend(tvg::BlendMethod::Exclusion);
    if (canvas->push(std::move(shape6)) != tvg::Result::Success) return;

    //Screen
    auto shape7 = tvg::Shape::gen();
    shape7->appendCircle(600, 650, 200, 150);
    shape7->blend(tvg::BlendMethod::Screen);
    shape7->fill(0, 0, 255);
    if (canvas->push(std::move(shape7)) != tvg::Result::Success) return;

    //SrcOver
    auto shape8 = tvg::Shape::gen();
    shape8->appendRect(550, 600, 150, 150);
    shape8->blend(tvg::BlendMethod::SrcOver);
    shape8->fill(10, 255, 155, 50);
    if (canvas->push(std::move(shape8)) != tvg::Result::Success) return;

    //Darken
    auto shape9 = tvg::Shape::gen();
    shape9->appendRect(600, 650, 350, 250);
    shape9->blend(tvg::BlendMethod::Darken);
    shape9->fill(10, 255, 155);
    if (canvas->push(std::move(shape9)) != tvg::Result::Success) return;

    //Prepare Transformed Image
    string path(EXAMPLE_DIR"/image/rawimage_200x300.raw");

    ifstream file(path, ios::binary);
    if (!file.is_open()) return ;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    //Lighten
    auto picture = tvg::Picture::gen();
    if (picture->load(data, 200, 300, true, true) != tvg::Result::Success) return;
    picture->translate(800, 700);
    picture->rotate(40);
    picture->blend(tvg::BlendMethod::Lighten);
    canvas->push(std::move(picture));

    //ColorDodge
    auto shape10 = tvg::Shape::gen();
    shape10->appendRect(0, 0, 200, 200, 50, 50);
    shape10->blend(tvg::BlendMethod::ColorDodge);
    shape10->fill(255, 255, 255, 250);
    if (canvas->push(std::move(shape10)) != tvg::Result::Success) return;

    //ColorBurn
    auto picture2 = tvg::Picture::gen();
    if (picture2->load(data, 200, 300, true, true) != tvg::Result::Success) return;
    picture2->translate(600, 250);
    picture2->blend(tvg::BlendMethod::ColorBurn);
    picture2->opacity(150);
    canvas->push(std::move(picture2));

    //HardLight
    auto picture3 = tvg::Picture::gen();
    if (picture3->load(data, 200, 300, true, true) != tvg::Result::Success) return;
    picture3->translate(700, 150);
    picture3->blend(tvg::BlendMethod::HardLight);
    canvas->push(std::move(picture3));

    //SoftLight
    auto picture4 = tvg::Picture::gen();
    if (picture4->load(data, 200, 300, true, true) != tvg::Result::Success) return;
    picture4->translate(350, 600);
    picture4->rotate(90);
    picture4->blend(tvg::BlendMethod::SoftLight);
    canvas->push(std::move(picture4));

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
            createSwView(1024, 1024);
        } else {
            createGlView(1024, 1024);
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
