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

    //Image source
    ifstream file(EXAMPLE_DIR"/rawimage_200x300.raw", ios::binary);
    if (!file.is_open()) return;
    auto data = (uint32_t*) malloc(sizeof(uint32_t) * (200 * 300));
    file.read(reinterpret_cast<char*>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    //background
    auto bg = tvg::Shape::gen();
    bg->appendRect(0, 0, 625, HEIGHT);
    bg->fill(50, 50, 50);
    canvas->push(std::move(bg));

    {
        //Shape + Shape Mask Add
        auto shape = tvg::Shape::gen();
        shape->appendCircle(125, 100, 150, 150);
        shape->fill(255, 255, 255);

        auto mask = tvg::Shape::gen();
        mask->appendCircle(125, 100, 50, 50);
        mask->fill(255, 255, 255);

        auto add = tvg::Shape::gen();
        add->appendCircle(175, 100, 50, 50);
        add->fill(255, 255, 255);
        mask->composite(std::move(add), tvg::CompositeMethod::AddMask);
        shape->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(shape));

        //Shape + Shape Mask Subtract
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(375, 100, 150, 150);
        shape2->fill(255, 255, 255, 255);

        auto mask2 = tvg::Shape::gen();
        mask2->appendCircle(375, 100, 50, 50);
        mask2->fill(255, 255, 255, 127);

        auto sub = tvg::Shape::gen();
        sub->appendCircle(400, 100, 50, 50);
        sub->fill(255, 255, 255);
        mask2->composite(std::move(sub), tvg::CompositeMethod::SubtractMask);
        shape2->composite(std::move(mask2), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(shape2));

        //Shape + Shape Mask Intersect
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(625, 100, 50, 50);
        shape3->fill(255, 255, 255, 127);

        auto mask3 = tvg::Shape::gen();
        mask3->appendCircle(625, 100, 50, 50);
        mask3->fill(255, 255, 255, 127);

        auto inter = tvg::Shape::gen();
        inter->appendCircle(650, 100, 50, 50);
        inter->fill(255, 255, 255);
        mask3->composite(std::move(inter), tvg::CompositeMethod::IntersectMask);
        shape3->composite(std::move(mask3), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(shape3));

        //Shape + Shape Mask Difference
        auto shape4 = tvg::Shape::gen();
        shape4->appendCircle(875, 100, 150, 150);
        shape4->fill(255, 255, 255);

        auto mask4 = tvg::Shape::gen();
        mask4->appendCircle(875, 100, 50, 50);
        mask4->fill(255, 255, 255);

        auto diff = tvg::Shape::gen();
        diff->appendCircle(900, 100, 50, 50);
        diff->fill(255, 255, 255);
        mask4->composite(std::move(diff), tvg::CompositeMethod::DifferenceMask);
        shape4->composite(std::move(mask4), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(shape4));
    }
    {
        //Shape + Shape Mask Add
        auto shape = tvg::Shape::gen();
        shape->appendCircle(125, 300, 100, 100);
        shape->fill(255, 255, 255);

        auto mask = tvg::Shape::gen();
        mask->appendCircle(125, 300, 50, 50);
        mask->fill(255, 255, 255);

        auto add = tvg::Shape::gen();
        add->appendCircle(175, 300, 50, 50);
        add->fill(255, 255, 255);
        mask->composite(std::move(add), tvg::CompositeMethod::AddMask);
        shape->composite(std::move(mask), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(shape));

        //Shape + Shape Mask Subtract
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(375, 300, 100, 100);
        shape2->fill(255, 255, 255, 255);

        auto mask2 = tvg::Shape::gen();
        mask2->appendCircle(375, 300, 50, 50);
        mask2->fill(255, 255, 255, 127);

        auto sub = tvg::Shape::gen();
        sub->appendCircle(400, 300, 50, 50);
        sub->fill(255, 255, 255);
        mask2->composite(std::move(sub), tvg::CompositeMethod::SubtractMask);
        shape2->composite(std::move(mask2), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(shape2));

        //Shape + Shape Mask Intersect
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(625, 300, 100, 100);
        shape3->fill(255, 255, 255, 127);

        auto mask3 = tvg::Shape::gen();
        mask3->appendCircle(625, 300, 50, 50);
        mask3->fill(255, 255, 255, 127);

        auto inter = tvg::Shape::gen();
        inter->appendCircle(650, 300, 50, 50);
        inter->fill(255, 255, 255);
        mask3->composite(std::move(inter), tvg::CompositeMethod::IntersectMask);
        shape3->composite(std::move(mask3), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(shape3));

        //Shape + Shape Mask Difference
        auto shape4 = tvg::Shape::gen();
        shape4->appendCircle(875, 300, 100, 100);
        shape4->fill(255, 255, 255);

        auto mask4 = tvg::Shape::gen();
        mask4->appendCircle(875, 300, 50, 50);
        mask4->fill(255, 255, 255);

        auto diff = tvg::Shape::gen();
        diff->appendCircle(900, 300, 50, 50);
        diff->fill(255, 255, 255);
        mask4->composite(std::move(diff), tvg::CompositeMethod::DifferenceMask);
        shape4->composite(std::move(mask4), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(shape4));
    }
    {
        //Rect + Rect Mask Add
        auto shape = tvg::Shape::gen();
        shape->appendRect(75, 450, 150, 150);
        shape->fill(255, 255, 255);

        auto mask = tvg::Shape::gen();
        mask->appendRect(75, 500, 100, 100);
        mask->fill(255, 255, 255);

        auto add = tvg::Shape::gen();
        add->appendRect(125, 450, 100, 100);
        add->fill(255, 255, 255);
        mask->composite(std::move(add), tvg::CompositeMethod::AddMask);
        shape->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(shape));

        //Rect + Rect Mask Subtract
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(325, 450, 150, 150);
        shape2->fill(255, 255, 255);

        auto mask2 = tvg::Shape::gen();
        mask2->appendRect(325, 500, 100, 100);
        mask2->fill(255, 255, 255, 127);

        auto sub = tvg::Shape::gen();
        sub->appendRect(375, 450, 100, 100);
        sub->fill(255, 255, 255);
        mask2->composite(std::move(sub), tvg::CompositeMethod::SubtractMask);
        shape2->composite(std::move(mask2), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(shape2));

        //Rect + Rect Mask Intersect
        auto shape3 = tvg::Shape::gen();
        shape3->appendRect(575, 450, 150, 150);
        shape3->fill(255, 255, 255);

        auto mask3 = tvg::Shape::gen();
        mask3->appendRect(575, 500, 100, 100);
        mask3->fill(255, 255, 255, 127);

        auto inter = tvg::Shape::gen();
        inter->appendRect(625, 450, 100, 100);
        inter->fill(255, 255, 255);
        mask3->composite(std::move(inter), tvg::CompositeMethod::IntersectMask);
        shape3->composite(std::move(mask3), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(shape3));

        //Rect + Rect Mask Difference
        auto shape4 = tvg::Shape::gen();
        shape4->appendRect(825, 450, 150, 150);
        shape4->fill(255, 255, 255);

        auto mask4 = tvg::Shape::gen();
        mask4->appendRect(825, 500, 100, 100);
        mask4->fill(255, 255, 255);

        auto diff = tvg::Shape::gen();
        diff->appendRect(875, 450, 100, 100);
        diff->fill(255, 255, 255);
        mask4->composite(std::move(diff), tvg::CompositeMethod::DifferenceMask);
        shape4->composite(std::move(mask4), tvg::CompositeMethod::AlphaMask);
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

        auto add = tvg::Shape::gen();
        add->appendCircle(150, 750, 50, 50);
        add->fill(255, 255, 255);
        mask->composite(std::move(add), tvg::CompositeMethod::AddMask);
        image->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(image));

         //Transformed Image + Shape Mask Subtract
        auto image2 = tvg::Picture::gen();
        if (image2->load(data, 200, 300, true) != tvg::Result::Success) return;
        image2->translate(400, 650);
        image2->scale(0.5f);
        image2->rotate(45);

        auto mask2 = tvg::Shape::gen();
        mask2->appendCircle(375, 700, 50, 50);
        mask2->fill(255, 255, 255, 127);

        auto sub = tvg::Shape::gen();
        sub->appendCircle(400, 750, 50, 50);
        sub->fill(255, 255, 255);
        mask2->composite(std::move(sub), tvg::CompositeMethod::SubtractMask);
        image2->composite(std::move(mask2), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(image2));

        //Transformed Image + Shape Mask Intersect
        auto image3 = tvg::Picture::gen();
        if (image3->load(data, 200, 300, true) != tvg::Result::Success) return;
        image3->translate(650, 650);
        image3->scale(0.5f);
        image3->rotate(45);

        auto mask3 = tvg::Shape::gen();
        mask3->appendCircle(625, 700, 50, 50);
        mask3->fill(255, 255, 255, 127);

        auto inter = tvg::Shape::gen();
        inter->appendCircle(650, 750, 50, 50);
        inter->fill(255, 255, 255, 127);
        mask3->composite(std::move(inter), tvg::CompositeMethod::IntersectMask);
        image3->composite(std::move(mask3), tvg::CompositeMethod::AlphaMask);
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

        auto diff = tvg::Shape::gen();
        diff->appendCircle(900, 750, 50, 50);
        diff->fill(255, 255, 255);
        mask4->composite(std::move(diff), tvg::CompositeMethod::DifferenceMask);
        image4->composite(std::move(mask4), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(image4));
    }
    {
        //Transformed Image + Shape Mask Add
        auto image = tvg::Picture::gen();
        if (image->load(data, 200, 300, true) != tvg::Result::Success) return;
        image->translate(150, 850);
        image->scale(0.5f);
        image->rotate(45);

        auto mask = tvg::Shape::gen();
        mask->appendCircle(125, 900, 50, 50);
        mask->fill(255, 255, 255);

        auto add = tvg::Shape::gen();
        add->appendCircle(150, 950, 50, 50);
        add->fill(255, 255, 255);
        mask->composite(std::move(add), tvg::CompositeMethod::AddMask);
        image->composite(std::move(mask), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(image));

         //Transformed Image + Shape Mask Subtract
        auto image2 = tvg::Picture::gen();
        if (image2->load(data, 200, 300, true) != tvg::Result::Success) return;
        image2->translate(400, 850);
        image2->scale(0.5f);
        image2->rotate(45);

        auto mask2 = tvg::Shape::gen();
        mask2->appendCircle(375, 900, 50, 50);
        mask2->fill(255, 255, 255, 127);

        auto sub = tvg::Shape::gen();
        sub->appendCircle(400, 950, 50, 50);
        sub->fill(255, 255, 255);
        mask2->composite(std::move(sub), tvg::CompositeMethod::SubtractMask);
        image2->composite(std::move(mask2), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(image2));

        //Transformed Image + Shape Mask Intersect
        auto image3 = tvg::Picture::gen();
        if (image3->load(data, 200, 300, true) != tvg::Result::Success) return;
        image3->translate(650, 850);
        image3->scale(0.5f);
        image3->rotate(45);

        auto mask3 = tvg::Shape::gen();
        mask3->appendCircle(625, 900, 50, 50);
        mask3->fill(255, 255, 255, 127);

        auto inter = tvg::Shape::gen();
        inter->appendCircle(650, 950, 50, 50);
        inter->fill(255, 255, 255, 127);
        mask3->composite(std::move(inter), tvg::CompositeMethod::IntersectMask);
        image3->composite(std::move(mask3), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(image3));

        //Transformed Image + Shape Mask Difference
        auto image4 = tvg::Picture::gen();
        if (image4->load(data, 200, 300, true) != tvg::Result::Success) return;
        image4->translate(900, 850);
        image4->scale(0.5f);
        image4->rotate(45);

        auto mask4 = tvg::Shape::gen();
        mask4->appendCircle(875, 900, 50, 50);
        mask4->fill(255, 255, 255);

        auto diff = tvg::Shape::gen();
        diff->appendCircle(900, 950, 50, 50);
        diff->fill(255, 255, 255);
        mask4->composite(std::move(diff), tvg::CompositeMethod::DifferenceMask);
        image4->composite(std::move(mask4), tvg::CompositeMethod::InvAlphaMask);
        canvas->push(std::move(image4));
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
