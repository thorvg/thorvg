/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

    //Image source
    ifstream file(EXAMPLE_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    auto data = (uint32_t*) malloc(sizeof(uint32_t) * (200 * 300));
    file.read(reinterpret_cast<char*>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    {
        //Shape + Shape Mask Add
        auto shape = tvg::Shape::gen();
        shape->appendCircle(125, 100, 50, 50);
        shape->fill(255, 255, 255);

        auto mask = tvg::Shape::gen();
        mask->appendCircle(175, 100, 50, 50);
        mask->fill(255, 255, 255);
        shape->composite(std::move(mask), tvg::CompositeMethod::AddMask);

        canvas->push(std::move(shape));

        //Shape + Shape Mask Subtract
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(375, 100, 50, 50);
        shape2->fill(255, 255, 255);

        auto mask2 = tvg::Shape::gen();
        mask2->appendCircle(400, 100, 50, 50);
        mask2->fill(255, 255, 255);

        shape2->composite(std::move(mask2), tvg::CompositeMethod::SubtractMask);

        canvas->push(std::move(shape2));

        //Shape + Shape Mask Intersect
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(625, 100, 50, 50);
        shape3->fill(255, 255, 255);

        auto mask3 = tvg::Shape::gen();
        mask3->appendCircle(650, 100, 50, 50);
        mask3->fill(255, 255, 255);
        shape3->composite(std::move(mask3), tvg::CompositeMethod::IntersectMask);

        canvas->push(std::move(shape3));

        //Shape + Shape Mask Difference
        auto shape4 = tvg::Shape::gen();
        shape4->appendCircle(875, 100, 50, 50);
        shape4->fill(255, 255, 255);

        auto mask4 = tvg::Shape::gen();
        mask4->appendCircle(900, 100, 50, 50);
        mask4->fill(255, 255, 255);
        shape4->composite(std::move(mask4), tvg::CompositeMethod::DifferenceMask);

        canvas->push(std::move(shape4));
    }

    {
        //Shape + Image Mask Add
        auto shape = tvg::Shape::gen();
        shape->appendCircle(150, 250, 50, 50);
        shape->fill(255, 255, 255);

        auto mask = tvg::Picture::gen();
        if (mask->load(data, 200, 300, true) != tvg::Result::Success) return;
        mask->translate(100, 250);
        mask->scale(0.5f);
        shape->composite(std::move(mask), tvg::CompositeMethod::AddMask);
        canvas->push(std::move(shape));

        //Shape + Image Mask Subtract
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(400, 250, 50, 50);
        shape2->fill(255, 255, 255);

        auto mask2 = tvg::Picture::gen();
        if (mask2->load(data, 200, 300, true) != tvg::Result::Success) return;
        mask2->translate(350, 250);
        mask2->scale(0.5f);
        shape2->composite(std::move(mask2), tvg::CompositeMethod::SubtractMask);

        canvas->push(std::move(shape2));

        //Shape + Image Mask Intersect
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(650, 250, 50, 50);
        shape3->fill(255, 255, 255);

        auto mask3 = tvg::Picture::gen();
        if (mask3->load(data, 200, 300, true) != tvg::Result::Success) return;
        mask3->translate(600, 250);
        mask3->scale(0.5f);
        shape3->composite(std::move(mask3), tvg::CompositeMethod::IntersectMask);

        canvas->push(std::move(shape3));

        //Shape + Image Mask Difference
        auto shape4 = tvg::Shape::gen();
        shape4->appendCircle(900, 250, 50, 50);
        shape4->fill(255, 255, 255);

        auto mask4 = tvg::Picture::gen();
        if (mask4->load(data, 200, 300, true) != tvg::Result::Success) return;
        mask4->translate(850, 250);
        mask4->scale(0.5f);
        shape4->composite(std::move(mask4), tvg::CompositeMethod::DifferenceMask);

        canvas->push(std::move(shape4));
    }

    {
        //Rect + Rect Mask Add
        auto shape = tvg::Shape::gen();
        shape->appendRect(75, 500, 100, 100);
        shape->fill(255, 255, 255);

        auto mask = tvg::Shape::gen();
        mask->appendRect(125, 450, 100, 100);
        mask->fill(255, 255, 255);
        shape->composite(std::move(mask), tvg::CompositeMethod::AddMask);

        canvas->push(std::move(shape));

        //Rect + Rect Mask Subtract
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(325, 500, 100, 100);
        shape2->fill(255, 255, 255);

        auto mask2 = tvg::Shape::gen();
        mask2->appendRect(375, 450, 100, 100);
        mask2->fill(255, 255, 255);
        shape2->composite(std::move(mask2), tvg::CompositeMethod::SubtractMask);

        canvas->push(std::move(shape2));

        //Rect + Rect Mask Intersect
        auto shape3 = tvg::Shape::gen();
        shape3->appendRect(575, 500, 100, 100);
        shape3->fill(255, 255, 255);

        auto mask3 = tvg::Shape::gen();
        mask3->appendRect(625, 450, 100, 100);
        mask3->fill(255, 255, 255);
        shape3->composite(std::move(mask3), tvg::CompositeMethod::IntersectMask);

        canvas->push(std::move(shape3));

        //Rect + Rect Mask Difference
        auto shape4 = tvg::Shape::gen();
        shape4->appendRect(825, 500, 100, 100);
        shape4->fill(255, 255, 255);

        auto mask4 = tvg::Shape::gen();
        mask4->appendRect(875, 450, 100, 100);
        mask4->fill(255, 255, 255);
        shape4->composite(std::move(mask4), tvg::CompositeMethod::DifferenceMask);

        canvas->push(std::move(shape4));
    }

    {
        //Transformed Image + Shape Mask Add
        auto image = tvg::Picture::gen();
        if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
        image->translate(150, 650);
        image->scale(0.5f);
        image->rotate(45);

        auto mask = tvg::Shape::gen();
        mask->appendCircle(125, 700, 50, 50);
        mask->fill(255, 255, 255);
        image->composite(std::move(mask), tvg::CompositeMethod::AddMask);
        canvas->push(std::move(image));

         //Transformed Image + Shape Mask Subtract
        auto image2 = tvg::Picture::gen();
        if (image2->load(data, 200, 300, true) != tvg::Result::Success) return;
        image2->translate(400, 650);
        image2->scale(0.5f);
        image2->rotate(45);

        auto mask2 = tvg::Shape::gen();
        mask2->appendCircle(375, 700, 50, 50);
        mask2->fill(255, 255, 255);
        image2->composite(std::move(mask2), tvg::CompositeMethod::SubtractMask);
        canvas->push(std::move(image2));

        //Transformed Image + Shape Mask Intersect
        auto image3 = tvg::Picture::gen();
        if (image3->load(data, 200, 300, true) != tvg::Result::Success) return;
        image3->translate(650, 650);
        image3->scale(0.5f);
        image3->rotate(45);

        auto mask3 = tvg::Shape::gen();
        mask3->appendCircle(625, 700, 50, 50);
        mask3->fill(255, 255, 255);
        image3->composite(std::move(mask3), tvg::CompositeMethod::IntersectMask);
        canvas->push(std::move(image3));

        //Transformed Image + Shape Mask Difference
        auto image4 = tvg::Picture::gen();
        if (image4->load(data, 200, 300, true) != tvg::Result::Success) return;
        image4->translate(900, 650);
        image4->scale(0.5f);
        image4->rotate(45);

        auto mask4 = tvg::Shape::gen();
        mask4->appendCircle(875, 700, 50, 50);
        mask4->fill(255, 255, 255);
        image4->composite(std::move(mask4), tvg::CompositeMethod::DifferenceMask);
        canvas->push(std::move(image4));
    }

    {
        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops[2];
        colorStops[0] = {0, 255, 0, 0, 255};
        colorStops[1] = {1, 255, 255, 255, 255};

        //Shape + Shape Mask Add
        auto shape = tvg::Shape::gen();
        shape->appendRect(75, 850, 100, 100, 10, 10);

        //LinearGradient
        auto fill = tvg::LinearGradient::gen();
        fill->linear(75, 850, 175, 950);
        fill->colorStops(colorStops, 2);
        shape->fill(std::move(fill));

        auto mask = tvg::Shape::gen();
        mask->appendCircle(175, 900, 50, 50);
        mask->fill(255, 255, 255);
        shape->composite(std::move(mask), tvg::CompositeMethod::AddMask);

        canvas->push(std::move(shape));

        //Shape + Shape Mask Subtract
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(300, 850, 100, 100, 10, 10);

        //LinearGradient
        auto fill2 = tvg::LinearGradient::gen();
        fill2->linear(300, 850, 400, 950);
        fill2->colorStops(colorStops, 2);
        shape2->fill(std::move(fill2));

        auto mask2 = tvg::Shape::gen();
        mask2->appendCircle(400, 900, 50, 50);
        mask2->fill(255, 255, 255);

        shape2->composite(std::move(mask2), tvg::CompositeMethod::SubtractMask);

        canvas->push(std::move(shape2));

        //Shape + Shape Mask Intersect
        auto shape3 = tvg::Shape::gen();
        shape3->appendRect(550, 850, 100, 100, 10, 10);

        //LinearGradient
        auto fill3 = tvg::LinearGradient::gen();
        fill3->linear(550, 850, 650, 950);
        fill3->colorStops(colorStops, 2);
        shape3->fill(std::move(fill3));

        auto mask3 = tvg::Shape::gen();
        mask3->appendCircle(650, 900, 50, 50);
        mask3->fill(255, 255, 255);
        shape3->composite(std::move(mask3), tvg::CompositeMethod::IntersectMask);

        canvas->push(std::move(shape3));

        //Shape + Shape Mask Difference
        auto shape4 = tvg::Shape::gen();
        shape4->appendRect(800, 850, 100, 100, 10, 10);
        shape4->fill(255, 255, 255);

        //LinearGradient
        auto fill4 = tvg::LinearGradient::gen();
        fill4->linear(800, 850, 900, 950);
        fill4->colorStops(colorStops, 2);
        shape4->fill(std::move(fill4));


        auto mask4 = tvg::Shape::gen();
        mask4->appendCircle(900, 900, 50, 50);
        mask4->fill(255, 255, 255);
        shape4->composite(std::move(mask4), tvg::CompositeMethod::DifferenceMask);

        canvas->push(std::move(shape4));
    }

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
            createSwView(1024, 1024);
        } else {
            createGlView(1024, 1024);
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
