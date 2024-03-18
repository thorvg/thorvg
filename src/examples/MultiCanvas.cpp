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

#include <iostream>
#include <thread>
#include <vector>
#include <Elementary.h>
#include <thorvg.h>

using namespace std;

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/
#define WIDTH 1024
#define HEIGHT 1024
#define NUM_PER_LINE 7
#define SIZE (WIDTH/NUM_PER_LINE)

static size_t counter = 0;
static std::vector<unique_ptr<tvg::Canvas>> canvases;


void win_del(void *data, Evas_Object *o, void* ev)
{
   elm_exit();
}


void tvgDrawCmds(tvg::Canvas* canvas, const char* path, const char* name)
{
    auto picture = tvg::Picture::gen();

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/%s", path, name);

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
    picture->translate(shiftX, shiftY);

    if (canvas->push(std::move(picture)) != tvg::Result::Success) return;

    cout << "SVG: " << buf << endl;

    counter++;
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

void sw_del(void* data, Evas* evas, Eo* obj, void* ev)
{
    auto buffer = (uint32_t*) data;
    free(buffer);
}


void drawSwView(void* data, Eo* obj)
{
    auto i = reinterpret_cast<size_t>(data);
    auto& canvas = canvases[i];

    if (canvas->draw() == tvg::Result::Success) {
        canvas->sync();
    }
}


void tvgSwTest(const char* name, const char* path, void* data)
{
    //ignore if not svgs.
    const char *ext = name + strlen(name) - 3;
    if (strcmp(ext, "svg")) return;

    Eo* win = (Eo*) data;

    uint32_t* buffer = (uint32_t*) calloc(sizeof(uint32_t), SIZE * SIZE);

    Eo* view = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(view, SIZE, SIZE);
    evas_object_image_data_set(view, buffer);
    evas_object_image_pixels_dirty_set(view, EINA_TRUE);
    evas_object_image_data_update_add(view, 0, 0, SIZE, SIZE);
    evas_object_image_alpha_set(view, EINA_TRUE);
    evas_object_image_pixels_get_callback_set(view, drawSwView, reinterpret_cast<void*>(counter));
    evas_object_event_callback_add(view, EVAS_CALLBACK_DEL, sw_del, buffer);
    evas_object_resize(view, SIZE, SIZE);
    evas_object_move(view, (counter % NUM_PER_LINE) * SIZE, SIZE * (counter / NUM_PER_LINE));
    evas_object_show(view);

    //Create a Canvas
    auto canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, SIZE, SIZE, SIZE, tvg::SwCanvas::ARGB8888);

    tvgDrawCmds(canvas.get(), path, name);

    canvases.push_back(std::move(canvas));
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

struct ObjData
{
    char* path;
    char* name;
    int idx;
};

void gl_del(void* data, Evas* evas, Eo* obj, void* ev)
{
    auto objData = (ObjData*) data;
    delete(objData);
}

void initGLview(Evas_Object *obj)
{
    auto objData = reinterpret_cast<ObjData*>(evas_object_data_get(obj, "objdata"));
    objData->idx = counter;

    //Create a Canvas
    auto canvas = tvg::GlCanvas::gen();

    //Get the drawing target id
    int32_t targetId;
    auto gl = elm_glview_gl_api_get(obj);
    gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &targetId);

    canvas->target(targetId, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(canvas.get(), objData->path, objData->name);

    canvases.push_back(std::move(canvas));
}


void drawGLview(Evas_Object *obj)
{
    auto gl = elm_glview_gl_api_get(obj);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    auto objData = reinterpret_cast<ObjData*>(evas_object_data_get(obj, "objdata"));
    auto& canvas = canvases[objData->idx];

    if (canvas->draw() == tvg::Result::Success) {
        canvas->sync();
    }
}


void tvgGlTest(const char* name, const char* path, void* data)
{
    auto objData = new ObjData;
    objData->name = strdup(name);
    objData->path = strdup(path);

    Eo* win = (Eo*) data;

    Eo* view = elm_glview_add(win);
    elm_glview_mode_set(view, ELM_GLVIEW_ALPHA);
    elm_glview_resize_policy_set(view, ELM_GLVIEW_RESIZE_POLICY_RECREATE);
    elm_glview_render_policy_set(view, ELM_GLVIEW_RENDER_POLICY_ON_DEMAND);
    elm_glview_init_func_set(view, initGLview);
    elm_glview_render_func_set(view, drawGLview);
    evas_object_data_set(view, "objdata", reinterpret_cast<void*>(objData));
    evas_object_event_callback_add(view, EVAS_CALLBACK_DEL, gl_del, objData);
    evas_object_resize(view, SIZE, SIZE);
    evas_object_move(view, (counter % NUM_PER_LINE) * SIZE, SIZE * (counter / NUM_PER_LINE));
    evas_object_show(view);
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

        Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
        evas_object_smart_callback_add(win, "delete,request", win_del, 0);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            eina_file_dir_list(EXAMPLE_DIR"/svg", EINA_TRUE, tvgSwTest, win);
        } else {
            eina_file_dir_list(EXAMPLE_DIR"/svg", EINA_TRUE, tvgGlTest, win);
        }

        evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
        evas_object_show(win);

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
