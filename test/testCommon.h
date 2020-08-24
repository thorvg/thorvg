#include <iostream>
#include <thread>
#include <Elementary.h>
#include <thorvg.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

/************************************************************************/
/* Common Infrastructure Code                                           */
/************************************************************************/

void tvgSwTest(uint32_t* buffer);
void drawSwView(void* data, Eo* obj);

void win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}

static Eo* createSwView()
{
    static uint32_t buffer[WIDTH * HEIGHT];

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

void initGLview(Evas_Object *obj);
void drawGLview(Evas_Object *obj);

static Eo* createGlView()
{
   elm_config_accel_preference_set("gl");

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Eo* view = elm_glview_add(win);
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
