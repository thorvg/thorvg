/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

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
#include <Elementary.h>

double updateTime = 0;
double accumUpdateTime = 0;
double accumRasterTime = 0;
double accumTotalTime = 0;
uint32_t cnt = 0;

static uint32_t* buffer = nullptr;
static tvg::Canvas* canvas;
static bool updated = false;

void plat_init(int argc, char* argv[])
{
    elm_init(argc, argv);
}

void plat_run(void)
{
    elm_run();
}

void plat_shutdown(void)
{
    elm_shutdown();
}

tvg::Canvas * getCanvas(void)
{
    return canvas;
}

bool getUpdate(void)
{
    return updated;
}

void setUpdate(bool b)
{
    updated = b;
}

static void win_del(void *data, Evas_Object *o, void *ev)
{
    free(buffer);
    elm_exit();
}

static unique_ptr<tvg::SwCanvas> swCanvas;

static void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    canvas = swCanvas.get();
    tvgDrawCmds(swCanvas.get());
}

static void drawSwView(void* data, void* obj)
{
    auto before = ecore_time_get();
    swCanvas->update();
    auto after = ecore_time_get();

    updateTime += (after - before);

    before = ecore_time_get();
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
        updated = false;
    }

    after = ecore_time_get();

    auto rasterTime = after - before;

    ++cnt;

    accumUpdateTime += updateTime;
    accumRasterTime += rasterTime;
    accumTotalTime += (updateTime + rasterTime);

    printf("[%5d]: update = %fs,   raster = %fs,  total = %fs\n", cnt, accumUpdateTime / cnt, accumRasterTime / cnt, accumTotalTime / cnt);

    updateTime = 0;
}


void* createSwView(uint32_t w, uint32_t h)
{
    cout << "tvg engine: software" << endl;

    buffer = static_cast<uint32_t*>(malloc(w * h * sizeof(uint32_t)));

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Evas_Object_Image_Pixels_Get_Cb drawCb = (Evas_Object_Image_Pixels_Get_Cb)drawSwView;

    Eo* view = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(view, w, h);
    evas_object_image_data_set(view, buffer);
    evas_object_image_pixels_get_callback_set(view, drawCb, nullptr);
    evas_object_image_pixels_dirty_set(view, EINA_TRUE);
    evas_object_image_data_update_add(view, 0, 0, w, h);
    evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(view);

    elm_win_resize_object_add(win, view);
    evas_object_geometry_set(win, 0, 0, w, h);
    evas_object_show(win);

    tvgSwTest(buffer);

    return (void*)view;
}

void updateSwView(void* obj)
{
    Eo* img = (Eo*) obj;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
}

<<<<<<< HEAD
Eina_Bool animatorSwCb(void *data)
=======
Eina_Bool animatorSwCb(Eo *data)
>>>>>>> 7e23bbca513363d158c6c03e9c3f2825baca7131
{
    Eo* img = (Eo*) data;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);

    return ECORE_CALLBACK_RENEW;
}

void setAnimatorSw(void * obj)
{
    Eo * view = (Eo*)obj;
    ecore_animator_add(animatorSwCb, view);
}

void* addAnimatorTransit(double duration, int repeat, AnimatCb cb, void * data)
{
<<<<<<< HEAD
    Elm_Transit_Effect_Transition_Cb tcb = (Elm_Transit_Effect_Transition_Cb)cb;
    Elm_Transit* transit = elm_transit_add();
    elm_transit_effect_add(transit, tcb, data, nullptr);
=======
    Elm_Transit* transit = elm_transit_add();
    elm_transit_effect_add(transit, cb, data, nullptr);
>>>>>>> 7e23bbca513363d158c6c03e9c3f2825baca7131
    elm_transit_duration_set(transit, duration);
    elm_transit_repeat_times_set(transit, repeat);
    elm_transit_go(transit);
    return (void*)transit;
}

<<<<<<< HEAD
void setAnimatorTransitAutoReverse(void* tl, bool b)
=======
void setAnimatorTransitAutoReverse(void* tl, bool b);
>>>>>>> 7e23bbca513363d158c6c03e9c3f2825baca7131
{
    Elm_Transit* transit = (Elm_Transit*)tl;
    elm_transit_auto_reverse_set(transit, b ? EINA_TRUE : EINA_FALSE);
}

void delAnimatorTransit(void* tl)
{
    Elm_Transit* transit = (Elm_Transit*)tl;
    elm_transit_del(transit);
}

#ifndef NO_GL_EXAMPLE

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
    canvas = glCanvas.get();
    tvgDrawCmds(glCanvas.get());
}

void drawGLview(Evas_Object *obj)
{
    auto before = ecore_time_get();

    auto gl = elm_glview_gl_api_get(obj);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    glCanvas->update();
    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
        updated = false;
    }

    auto after = ecore_time_get();

    auto rasterTime = after - before;

    ++cnt;

    accumUpdateTime += updateTime;
    accumRasterTime += rasterTime;
    accumTotalTime += (updateTime + rasterTime);

    printf("[%5d]: update = %fs,   raster = %fs,  total = %fs\n", cnt, accumUpdateTime / cnt, accumRasterTime / cnt, accumTotalTime / cnt);

    updateTime = 0;
}

Eina_Bool animatorGlCb(void *data)
{
    elm_glview_changed_set((Evas_Object*)data);

    return ECORE_CALLBACK_RENEW;
}

void setAnimatorGl(void * obj)
{
    Eo * view = (Eo*)obj;
    ecore_animator_add(animatorGlCb, view);
}

void* createGlView(uint32_t w, uint32_t h)
{
    cout << "tvg engine: opengl" << endl;

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
    evas_object_geometry_set(win, 0, 0, w, h);
    evas_object_show(win);

    return (void*)view;
}

void updateGlView(void* obj)
{
}
#endif //NO_GL_EXAMPLE

bool file_dir_list(const char* path, bool recursive, DIR_LIST_CB cb, void * data)
{
<<<<<<< HEAD
    return eina_file_dir_list(path, recursive ? EINA_TRUE : EINA_FALSE, cb, data) ? true : false;
}

struct _timer_st {
    Ecore_Timer * et;
    TimerCb cb;
    void * data;
};

=======
    eina_file_dir_list(path, recursive ? EINA_TRUE : EINA_FALSE, cb, data);
}

>>>>>>> 7e23bbca513363d158c6c03e9c3f2825baca7131
double system_time_get(void)
{
    return ecore_time_get();
}

<<<<<<< HEAD
Eina_Bool timerCb(void *data)
{
    _timer_st * t = (_timer_st*)data;
    t->cb(t->data);
    return ECORE_CALLBACK_RENEW;
}

void* system_timer_add(double s, TimerCb cb, void* data)
{
    _timer_st * t = new _timer_st();
    t->cb = cb;
    t->data = data;
    t->et = ecore_timer_add(s, timerCb, t);

    return (void*)t;
=======
void* system_timer_add(double s, TimerCb cb, void* data)
{
    return (void*)ecore_timer_add(s, cb, data);
>>>>>>> 7e23bbca513363d158c6c03e9c3f2825baca7131
}

void system_timer_del(void* timer)
{
<<<<<<< HEAD
    _timer_st * t = (_timer_st*)timer;
    ecore_timer_del(t->et);
    delete t;
=======
    ecore_timer_del((Ecore_Timer*)timer);
>>>>>>> 7e23bbca513363d158c6c03e9c3f2825baca7131
}
