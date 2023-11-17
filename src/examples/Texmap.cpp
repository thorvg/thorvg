/*
 * Copyright (c) 2020 - 2023 the ThorVG project. All rights reserved.

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
    shape->fill(255, 255, 255);

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    //Raw Image
    string path(EXAMPLE_DIR"/rawimage_200x300.raw");

    ifstream file(path, ios::binary);
    if (!file.is_open()) return ;
    auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    //Picture
    auto picture = tvg::Picture::gen();
    if (picture->load(data, 200, 300, true) != tvg::Result::Success) return;

    //Composing Meshes
    tvg::Polygon triangles[4];
    triangles[0].vertex[0] = {{100, 125}, {0, 0}};
    triangles[0].vertex[1] = {{300, 100}, {0.5, 0}};
    triangles[0].vertex[2] = {{200, 550}, {0, 1}};

    triangles[1].vertex[0] = {{300, 100}, {0.5, 0}};
    triangles[1].vertex[1] = {{350, 450}, {0.5, 1}};
    triangles[1].vertex[2] = {{200, 550}, {0, 1}};

    triangles[2].vertex[0] = {{300, 100}, {0.5, 0}};
    triangles[2].vertex[1] = {{500, 200}, {1, 0}};
    triangles[2].vertex[2] = {{350, 450}, {0.5, 1}};

    triangles[3].vertex[0] = {{500, 200}, {1, 0}};
    triangles[3].vertex[1] = {{450, 450}, {1, 1}};
    triangles[3].vertex[2] = {{350, 450}, {0.5, 1}};

    if (picture->mesh(triangles, 4) != tvg::Result::Success) return;

    //Masking + Opacity
    auto picture2 = tvg::cast<tvg::Picture>(picture->duplicate());
    picture2->translate(400, 400);
    picture2->opacity(200);

    auto mask = tvg::Shape::gen();
    mask->appendCircle(700, 700, 200, 200);
    mask->fill(255, 255, 255);
    picture2->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);


    canvas->push(std::move(picture));
    canvas->push(std::move(picture2));

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
