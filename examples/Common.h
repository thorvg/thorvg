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
#include <Elementary.h>
#include <thorvg.h>

using namespace std;

static uint32_t WIDTH = 800;
static uint32_t HEIGHT = 800;
static uint32_t* buffer = nullptr;


/************************************************************************/
/* Common Infrastructure Code                                           */
/************************************************************************/

void tvgSwTest(uint32_t* buffer);
void drawSwView(void* data, Eo* obj);

void win_del(void *data, Evas_Object *o, void *ev)
{
    free(buffer);
    elm_exit();
}

static Eo* createSwView(uint32_t w = WIDTH, uint32_t h = HEIGHT)
{
    cout << "tvg engine: software" << endl;

    WIDTH = w;
    HEIGHT = h;

    buffer = static_cast<uint32_t*>(malloc(w * h * sizeof(uint32_t)));

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Eo* view = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(view, WIDTH, HEIGHT);
    evas_object_image_data_set(view, buffer);
    evas_object_image_pixels_get_callback_set(view, drawSwView, nullptr);
    evas_object_image_pixels_dirty_set(view, EINA_TRUE);
    evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);
    evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(view);

    elm_win_resize_object_add(win, view);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    tvgSwTest(buffer);

    return view;
}

#ifndef NO_GL_EXAMPLE

void initGLview(Evas_Object *obj);
void drawGLview(Evas_Object *obj);

static Eo* createGlView(uint32_t w = WIDTH, uint32_t h = HEIGHT)
{
    cout << "tvg engine: opengl" << endl;

    WIDTH = w;
    HEIGHT = h;

    elm_config_accel_preference_set("gl");

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Eo* view = elm_glview_version_add(win, EVAS_GL_GLES_3_X);
    evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_glview_mode_set(view, ELM_GLVIEW_ALPHA);
    elm_glview_resize_policy_set(view, ELM_GLVIEW_RESIZE_POLICY_RECREATE);
    elm_glview_render_policy_set(view, ELM_GLVIEW_RENDER_POLICY_ON_DEMAND);
    elm_glview_init_func_set(view, initGLview);
    elm_glview_render_func_set(view, drawGLview);
    evas_object_show(view);

    elm_win_resize_object_add(win, view);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    return view;
}

#endif //NO_GL_EXAMPLE

