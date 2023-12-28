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

#include <fstream>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void bgColor(tvg::Canvas* canvas)
{
    //Background
    auto bg = tvg::Shape::gen();
    bg->appendRect(0, 0, WIDTH, HEIGHT, 0, 0);    //x, y, w, h, rx, ry
    bg->fill(200, 200, 255);                      //r, g, b
    canvas->push(std::move(bg));
}

void goWild(tvg::Canvas* canvas)
{
    float top = 100.0f;
    float bot = 700.0f;

    auto path = tvg::Shape::gen();
    path->moveTo(300, top / 2);
    path->lineTo(100, bot);
    path->lineTo(350, 400);
    path->lineTo(420, bot);
    path->lineTo(430, top * 2);
    path->lineTo(500, bot);
    path->lineTo(460, top * 2);
    path->lineTo(750, bot);
    path->lineTo(460, top / 2);
    path->close();

    path->fill(150, 150, 255);         // fill color
    path->strokeWidth(20);             // stroke width
    path->strokeFill(120, 120, 255);   // stroke color

    // path->strokeJoin(tvg::StrokeJoin::Round);
    // path->strokeJoin(tvg::StrokeJoin::Bevel);
    path->strokeJoin(tvg::StrokeJoin::Miter);

    path->strokeMiterlimit(10);
    static float ml = path->strokeMiterlimit();
    cout << "Set stroke miterlimit to " << ml << endl;
    canvas->push(std::move(path)); // push the path into the canvas

}

void blueprint(tvg::Canvas* canvas)
{
    // Load png file from path.
    std::string path = EXAMPLE_DIR"/stroke-miterlimit.png";

    std::unique_ptr<tvg::Picture> picture = tvg::Picture::gen();
    if (picture->load(path) != tvg::Result::Success) {
        cout << "Cannot load the picture: " << path << endl;
        return;
    }

    picture->opacity(42);
    picture->translate(24, 0);
    if (canvas->push(std::move(picture)) != tvg::Result::Success) {
        cout << "Cannot push image to canvas: " << path << endl;
    }
}

void svg(tvg::Canvas* canvas)
{
    //SVG stroke-miterlimit test.
    std::string svg_text (R"SVG(
<svg viewBox="0 0 38 30">
  <!-- Impact of the default miter limit -->
  <path
    stroke="black"
    fill="none"
    stroke-linejoin="miter"
    id="p1"
    d="M1,9 l7   ,-3 l7   ,3
       m2,0 l3.5 ,-3 l3.5 ,3
       m2,0 l2   ,-3 l2   ,3
       m2,0 l0.75,-3 l0.75,3
       m2,0 l0.5 ,-3 l0.5 ,3" />

  <!-- Impact of the smallest miter limit (1) -->
  <path
    stroke="black"
    fill="none"
    stroke-linejoin="miter"
    stroke-miterlimit="1"
    id="p2"
    d="M1,19 l7   ,-3 l7   ,3
       m2, 0 l3.5 ,-3 l3.5 ,3
       m2, 0 l2   ,-3 l2   ,3
       m2, 0 l0.75,-3 l0.75,3
       m2, 0 l0.5 ,-3 l0.5 ,3" />

  <!-- Impact of a large miter limit (8) -->
  <path
    stroke="black"
    fill="none"
    stroke-linejoin="miter"
    stroke-miterlimit="8"
    id="p3"
    d="M1,29 l7   ,-3 l7   ,3
       m2, 0 l3.5 ,-3 l3.5 ,3
       m2, 0 l2   ,-3 l2   ,3
       m2, 0 l0.75,-3 l0.75,3
       m2, 0 l0.5 ,-3 l0.5 ,3" />

  <!-- the following pink lines highlight the position of the path for each stroke -->
  <path
    stroke="pink"
    fill="none"
    stroke-width="0.05"
    d="M1, 9 l7,-3 l7,3 m2,0 l3.5,-3 l3.5,3 m2,0 l2,-3 l2,3 m2,0 l0.75,-3 l0.75,3 m2,0 l0.5,-3 l0.5,3
      M1,19 l7,-3 l7,3 m2,0 l3.5,-3 l3.5,3 m2,0 l2,-3 l2,3 m2,0 l0.75,-3 l0.75,3 m2,0 l0.5,-3 l0.5,3
      M1,29 l7,-3 l7,3 m2,0 l3.5,-3 l3.5,3 m2,0 l2,-3 l2,3 m2,0 l0.75,-3 l0.75,3 m2,0 l0.5,-3 l0.5,3" />
</svg>
)SVG");

    std::size_t svg_text_size = svg_text.size();
    std::unique_ptr<tvg::Picture> picture = tvg::Picture::gen();

    if (picture->load(svg_text.data(), svg_text_size, "svg", "", true) != tvg::Result::Success) {
        cout << "Couldn't load svg text data." << endl;
        return;
    }

    picture->scale(20);
    canvas->push(std::move(picture));
}


void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (canvas) {
        bgColor(canvas);
        goWild(canvas);
        blueprint(canvas);
        svg(canvas);
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
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888S);

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
