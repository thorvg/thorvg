/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All rights reserved.

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
#include "thorvg_lottie.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/


#define NUM_PER_ROW 6
#define NUM_PER_COL 6
#define SIZE (WIDTH/NUM_PER_ROW)

static int count = 0;

static tvg::Lottie* lotties[NUM_PER_ROW * NUM_PER_COL] = {nullptr};
static uint32_t totalFrames[NUM_PER_ROW * NUM_PER_COL] = {0};
static uint32_t frames[NUM_PER_ROW * NUM_PER_COL] = {0};

void svgDirCallback(const char* name, const char* path, void* data)
{
    //ignore if not svgs.
    const char *ext = name + strlen(name) - 4;
    if (strcmp(ext, "json")) return;

    string buf = path;
    buf += "/";
    buf += name;//"emoji_shock.json";

    tvg::Lottie* lottie = nullptr;

    if (!lotties[count]) {
        lottie = tvg::Lottie::gen().release();

        if (lottie->load(buf) != tvg::Result::Success) {
             cout << "JSON is not supported. Did you enable JSON Loader? : " << buf << endl;
             return;
        }
        cout << "Lottie: " << buf << endl;
        lotties[count] = lottie;
        lottie->size(SIZE, SIZE);
        lottie->translate((count % NUM_PER_ROW) * SIZE, (count / NUM_PER_ROW) * (HEIGHT / NUM_PER_COL));
    }
    else {
        lottie = lotties[count];
    }

    lottie->totalFrame(&totalFrames[count]);
    frames[count]++;
    if (frames[count] >= totalFrames[count]) frames[count] = 0;
    lottie->frame(frames[count]);

    count++;
}

void tvgUpdateCmds(tvg::Canvas* canvas, float progress)
{
    if (!canvas) return;

    if (canvas->clear() != tvg::Result::Success) return;

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT, 0, 0);    //x, y, w, h, rx, ry
    shape->fill(128, 128, 128, 255);                 //r, g, b, a
    if (canvas->push(move(shape)) != tvg::Result::Success) return;
    eina_file_dir_list(EXAMPLE_DIR"/json", EINA_TRUE, svgDirCallback, (void*)&progress);

    for(int i = 0; i< count; i++) {
        canvas->push(unique_ptr<tvg::Lottie>(static_cast<tvg::Lottie*>(lotties[i]->duplicate())));
    }

    count = 0;
    //lotties.clear();
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void tvgSwTest(uint32_t* buffer)
{

    for (int i = 0; i < NUM_PER_ROW * NUM_PER_COL; i++) lotties[i] = nullptr;
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgUpdateCmds(swCanvas.get(), 0);
}

void transitSwCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    tvgUpdateCmds(swCanvas.get(), progress);

    //Update Efl Canvas
    Eo* img = (Eo*) effect;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
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
    tvgUpdateCmds(glCanvas.get(), 0);
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
    tvgUpdateCmds(glCanvas.get(), progress);
    elm_glview_changed_set((Evas_Object*)effect);
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
            auto view = createSwView();
            elm_transit_effect_add(transit, transitSwCb, view, nullptr);
        } else {
            auto view = createGlView();
            elm_transit_effect_add(transit, transitGlCb, view, nullptr);
        }

        elm_transit_duration_set(transit, 1);
        elm_transit_repeat_times_set(transit, -1);
        elm_transit_auto_reverse_set(transit, EINA_TRUE);
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
