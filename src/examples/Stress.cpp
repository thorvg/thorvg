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

#include <vector>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

#define NUM_PER_LINE 16
#define SIZE 80

static bool rendered = false;
static int xCnt = 0;
static int yCnt = 0;
static int frame = 0;
static std::vector<tvg::Picture*> pictures;
static double t1, t2, t3, t4;

void svgDirCallback(const char* name, const char* path, void* data)
{
    if (yCnt > NUM_PER_LINE) return;        //Load maximum to NUM_PER_LINE

    //ignore if not svgs.
    const char *ext = name + strlen(name) - 3;
    if (strcmp(ext, "svg")) return;

    auto picture = tvg::Picture::gen();

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "/%s/%s", path, name);

    if (picture->load(buf) != tvg::Result::Success) return;

    //image scaling preserving its aspect ratio
    float scale;
    float shiftX = 0.0f, shiftY = 0.0f;
    float w, h;
    picture->size(&w, &h);

    if (w > h) {
        scale = SIZE / w;
        shiftY = (SIZE - h * scale) * 0.5f;
    } else {
        scale = SIZE / h;
        shiftX = (SIZE - w * scale) * 0.5f;
    }

    picture->scale(scale);
    picture->translate((xCnt % NUM_PER_LINE) * SIZE + shiftX, SIZE * (xCnt / NUM_PER_LINE) + shiftY);
    ++xCnt;

    //Duplicates
    for (int i = 0; i < NUM_PER_LINE - 1; i++) {
        tvg::Picture* dup = static_cast<tvg::Picture*>(picture->duplicate());
        dup->translate((xCnt % NUM_PER_LINE) * SIZE + shiftX, SIZE * (xCnt / NUM_PER_LINE) + shiftY);
        pictures.push_back(dup);
        ++xCnt;
    }

    cout << "SVG: " << buf << endl;
    pictures.push_back(picture.release());

    ++yCnt;
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);          //x, y, w, h
    shape->fill(255, 255, 255);                      //r, g, b

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    eina_file_dir_list(EXAMPLE_DIR, EINA_TRUE, svgDirCallback, canvas);

    /* This showcase shows you asynchrounous loading of svg.
       For this, pushing pictures at a certian sync time.
       This means it earns the time to finish loading svg resources,
       otherwise you can push pictures immediately. */
    for (auto picture : pictures) {
        canvas->push(tvg::cast<tvg::Picture>(picture));
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
    t3 = ecore_time_get();

    //Drawing task can be performed asynchronously.
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }

    t4 = ecore_time_get();
    printf("[%5d]: total[%fs] update[%fs], render[%fs]\n", ++frame, t4 - t1, t2 - t1, t4 - t3);

    rendered = true;
}

void transitSwCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    if (!rendered) return;

    t1 = ecore_time_get();

    for (auto picture : pictures) {
        picture->rotate(progress * 360);
        swCanvas->update(picture);
    }

    t2 = ecore_time_get();

    //Update Efl Canvas
    auto img = (Eo*) effect;
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);

    rendered = false;
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

void transitGlCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    for (auto picture : pictures) {
        picture->rotate(progress * 360);
        glCanvas->update(picture);
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

        Elm_Transit *transit = elm_transit_add();

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView(1280, 1280);
            elm_transit_effect_add(transit, transitSwCb, view, nullptr);
        } else {
            auto view = createGlView(1280, 1280);
            elm_transit_effect_add(transit, transitGlCb, view, nullptr);
        }

        elm_transit_duration_set(transit, 2);
        elm_transit_repeat_times_set(transit, -1);
        elm_transit_go(transit);

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
