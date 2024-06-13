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

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);
    shape->fill(75, 75, 75);
    canvas->push(std::move(shape));

    //Load a necessary font data.
    //The loaded font will be released when the Initializer::term() is called.
    //Otherwise, you can immedately unload the font data.
    //Please check Text::unload() APIs.
    if (tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf") != tvg::Result::Success) {
        cout << "TTF is not supported. Did you enable TTF Loader?" << endl;
        return;
    }

    if (tvg::Text::load(EXAMPLE_DIR"/font/NanumGothicCoding.ttf") != tvg::Result::Success) {
        cout << "TTF is not supported. Did you enable TTF Loader?" << endl;
        return;
    }

    //Load from memory
    ifstream file(EXAMPLE_DIR"/font/SentyCloud.ttf", ios::binary);
    if (!file.is_open()) return;
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    auto data = (char*)malloc(size);
    if (!data) return;
    file.read(data, size);
    file.close();
    if (tvg::Text::load("SentyCloud", data, size, "ttf", true) != tvg::Result::Success) {
        cout << "Error while loading TTF from memory. Did you enable TTF Loader?" << endl;
    }
    free(data);

    auto text = tvg::Text::gen();
    text->font("Arial", 80);
    text->text("THORVG Text");
    text->fill(255, 255, 255);
    canvas->push(std::move(text));

    auto text2 = tvg::Text::gen();
    text2->font("Arial", 30, "italic");
    text2->text("Font = \"Arial\", Size = 40, Style=Italic");
    text2->translate(0, 150);
    text2->fill(255, 255, 255);
    canvas->push(std::move(text2));

    auto text3 = tvg::Text::gen();
    text3->font("Arial", 40);
    text3->text("Kerning Test: VA, AV, TJ, JT");
    text3->fill(255, 255, 255);
    text3->translate(0, 225);
    canvas->push(std::move(text3));

    auto text4 = tvg::Text::gen();
    text4->font("Arial", 25);
    text4->text("Purple Text");
    text4->fill(255, 0, 255);
    text4->translate(0, 310);
    canvas->push(std::move(text4));

    auto text5 = tvg::Text::gen();
    text5->font("Arial", 25);
    text5->text("Gray Text");
    text5->fill(150, 150, 150);
    text5->translate(220, 310);
    canvas->push(std::move(text5));

    auto text6 = tvg::Text::gen();
    text6->font("Arial", 25);
    text6->text("Yellow Text");
    text6->fill(255, 255, 0);
    text6->translate(400, 310);
    canvas->push(std::move(text6));

    auto text7 = tvg::Text::gen();
    text7->font("Arial", 15);
    text7->text("Transformed Text - 30'");
    text7->fill(0, 0, 0);
    text7->translate(600, 400);
    text7->rotate(30);
    canvas->push(std::move(text7));

    auto text8 = tvg::Text::gen();
    text8->font("Arial", 15);
    text8->fill(0, 0, 0);
    text8->text("Transformed Text - 90'");
    text8->translate(600, 400);
    text8->rotate(90);
    canvas->push(std::move(text8));

    auto text9 = tvg::Text::gen();
    text9->font("Arial", 15);
    text9->fill(0, 0, 0);
    text9->text("Transformed Text - 180'");
    text9->translate(800, 400);
    text9->rotate(180);
    canvas->push(std::move(text9));

    //gradient texts
    float x, y, w, h;

    auto text10 = tvg::Text::gen();
    text10->font("Arial", 50);
    text10->text("Linear Text");
    text10->translate(0, 350);
    text10->bounds(&x, &y, &w, &h, false);

    //LinearGradient
    auto fill = tvg::LinearGradient::gen();
    fill->linear(x, y + h * 0.5f, x + w, y + h * 0.5f);

    //Gradient Color Stops
    tvg::Fill::ColorStop colorStops[3];
    colorStops[0] = {0, 255, 0, 0, 255};
    colorStops[1] = {0.5, 255, 255, 0, 255};
    colorStops[2] = {1, 255, 255, 255, 255};
    fill->colorStops(colorStops, 3);
    text10->fill(std::move(fill));

    canvas->push(std::move(text10));

    auto text11 = tvg::Text::gen();
    text11->font("NanumGothicCoding", 40);
    text11->text("\xeb\x82\x98\xeb\x88\x94\xea\xb3\xa0\xeb\x94\x95\xec\xbd\x94\xeb\x94\xa9\x28\x55\x54\x46\x2d\x38\x29");
    text11->translate(0, 450);
    text11->bounds(&x, &y, &w, &h, false);

    //LinearGradient
    auto fill2 = tvg::RadialGradient::gen();
    fill2->radial(x + w * 0.5f, y + h * 0.5f, w * 0.5f);

    //Gradient Color Stops
    tvg::Fill::ColorStop colorStops2[3];
    colorStops2[0] = {0, 0, 255, 255, 255};
    colorStops2[1] = {0.5, 255, 255, 0, 255};
    colorStops2[2] = {1, 255, 255, 255, 255};

    fill2->colorStops(colorStops2, 3);
    text11->fill(std::move(fill2));

    canvas->push(std::move(text11));

    auto text12 = tvg::Text::gen();
    text12->font("SentyCloud", 50);
    text12->fill(255, 25, 25);
    text12->text("\xe4\xb8\x8d\xe5\x88\xb0\xe9\x95\xbf\xe5\x9f\x8e\xe9\x9d\x9e\xe5\xa5\xbd\xe6\xb1\x89\xef\xbc\x81");
    text12->translate(0, 525);
    canvas->push(std::move(text12));
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
