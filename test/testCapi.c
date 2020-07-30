#include <Elementary.h>
#include <thorvg_capi.h>

#define WIDTH 800
#define HEIGHT 800


/************************************************************************/
/* Capi Test Code                                                       */
/************************************************************************/

static uint32_t buffer[WIDTH * HEIGHT];

void testCapi()
{
    tvg_engine_init(TVG_ENGINE_SW | TVG_ENGINE_GL);

    Tvg_Canvas* canvas = tvg_swcanvas_create();
    tvg_swcanvas_set_target(canvas, buffer, WIDTH, WIDTH, HEIGHT);

    Tvg_Paint* shape = tvg_shape_new();
    tvg_shape_append_rect(shape, 0, 0, 200, 200, 0, 0);
    tvg_shape_append_rect(shape, 100, 100, 300, 300, 100, 100);
    tvg_shape_append_circle(shape, 400, 400, 100, 100);
    tvg_shape_append_circle(shape, 400, 500, 170, 100);
    tvg_shape_fill_color(shape, 255, 255, 0, 255);
    tvg_canvas_push(canvas, shape);

    tvg_canvas_draw(canvas);
    tvg_canvas_sync(canvas);

    tvg_canvas_destroy(canvas);

    tvg_engine_term(TVG_ENGINE_SW | TVG_ENGINE_GL);
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

void win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}


int main(int argc, char **argv)
{
    elm_init(argc, argv);

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Eo* view = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(view, WIDTH, HEIGHT);
    evas_object_image_data_set(view, buffer);
    evas_object_image_pixels_dirty_set(view, EINA_TRUE);
    evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);
    evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(view);

    elm_win_resize_object_add(win, view);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    testCapi();

    elm_run();
    elm_shutdown();

    return 0;
}
