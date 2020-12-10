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
    tvg_engine_init(TVG_ENGINE_SW | TVG_ENGINE_GL, 0);

    Tvg_Canvas* canvas = tvg_swcanvas_create();
    tvg_swcanvas_set_target(canvas, buffer, WIDTH, WIDTH, HEIGHT, TVG_COLORSPACE_ARGB8888);

    Tvg_Paint* shape = tvg_shape_new();
    tvg_shape_append_rect(shape, 0, 0, 200, 200, 0, 0);
    tvg_shape_append_circle(shape, 200, 200, 100, 100);
    tvg_shape_append_rect(shape, 100, 100, 300, 300, 100, 100);
    Tvg_Gradient* grad = tvg_linear_gradient_new();
    tvg_linear_gradient_set(grad, 0, 0, 300, 300);
    Tvg_Color_Stop color_stops[4] =
    {
        {.offset=0.0, .r=0, .g=0, .b=0, .a=255},
        {.offset=0.25, .r=255, .g=0, .b=0, .a=255},
        {.offset=0.5, .r=0, .g=255, .b=0, .a=255},
        {.offset=1.0, .r=0, .g=0, .b=255, .a=255}
    };

    Tvg_Paint *shape1 = tvg_shape_new();
    tvg_shape_append_rect(shape1, 500, 500, 100, 100, 30, 30);
    Tvg_Gradient* grad1 = tvg_radial_gradient_new();
    tvg_radial_gradient_set(grad1, 550, 550, 50);
    Tvg_Color_Stop color_stops1[3] =
    {
        {.offset=0.0, .r=0, .g=0, .b=0, .a=255},
        {.offset=0.6, .r=255, .g=0, .b=0, .a=255},
        {.offset=1.0, .r=0, .g=255, .b=255, .a=255}
    };

    Tvg_Paint *shape2 = tvg_shape_new();
    tvg_shape_append_rect(shape2, 400, 0, 800, 400, 20, 20);
    Tvg_Gradient* grad2 = tvg_linear_gradient_new();
    tvg_linear_gradient_set(grad2, 400, 0, 450, 50);
    Tvg_Color_Stop color_stops2[2] =
    {
        {.offset=0.0, .r=0, .g=0, .b=0, .a=255},
        {.offset=1, .r=255, .g=0, .b=0, .a=255},
    };

    tvg_gradient_set_spread(grad2, TVG_STROKE_FILL_REPEAT);

    Tvg_Paint* shape3 = tvg_shape_new();
    tvg_shape_append_rect(shape3, 0, 400, 400, 800, 20, 20);
    Tvg_Gradient* grad3 = tvg_linear_gradient_new();
    tvg_linear_gradient_set(grad3, 0, 400, 50, 450);
    Tvg_Color_Stop color_stops3[2] =
    {
        {.offset=0.0, .r=0, .g=0, .b=0, .a=255},
        {.offset=1, .r=0, .g=255, .b=0, .a=255},
    };

    tvg_gradient_set_spread(grad3, TVG_STROKE_FILL_REFLECT);

    tvg_gradient_set_color_stops(grad, color_stops, 4);
    tvg_gradient_set_color_stops(grad1, color_stops1, 3);
    tvg_gradient_set_color_stops(grad2, color_stops2, 2);
    tvg_gradient_set_color_stops(grad3, color_stops3, 2);
    tvg_shape_set_linear_gradient(shape, grad);
    tvg_shape_set_radial_gradient(shape1, grad1);
    tvg_shape_set_linear_gradient(shape2, grad2);
    tvg_shape_set_linear_gradient(shape3, grad3);

    tvg_canvas_push(canvas, shape);
    tvg_canvas_push(canvas, shape1);
    tvg_canvas_push(canvas, shape2);
    tvg_canvas_push(canvas, shape3);

    Tvg_Paint* shape4 = tvg_shape_new();
    tvg_shape_append_rect(shape4, 700, 700, 100, 100, 20, 20);
    Tvg_Gradient* grad4 = tvg_linear_gradient_new();
    tvg_linear_gradient_set(grad4, 700, 700, 800, 800);
    Tvg_Color_Stop color_stops4[2] =
    {
        {.offset=0.0, .r=0, .g=0, .b=0, .a=255},
        {.offset=1, .r=0, .g=255, .b=0, .a=255},
    };
    tvg_gradient_set_color_stops(grad4, color_stops4, 2);
    tvg_shape_set_linear_gradient(shape4, grad4);

    Tvg_Gradient* grad5 = tvg_linear_gradient_new();
    tvg_linear_gradient_set(grad5, 700, 700, 800, 800);
    Tvg_Color_Stop color_stops5[2] =
    {
        {.offset=0.0, .r=0, .g=0, .b=255, .a=255},
        {.offset=1, .r=0, .g=255, .b=255, .a=255},
    };
    tvg_gradient_set_color_stops(grad5, color_stops5, 2);
    tvg_shape_set_linear_gradient(shape4, grad5);
    tvg_canvas_push(canvas, shape4);

    Tvg_Gradient* grad6 = tvg_radial_gradient_new();
    tvg_radial_gradient_set(grad6, 550, 550, 50);
    Tvg_Color_Stop color_stops6[2] =
    {
        {.offset=0.0, .r=0, .g=125, .b=0, .a=255},
        {.offset=1, .r=125, .g=0, .b=125, .a=255},
    };
    tvg_gradient_set_color_stops(grad6, color_stops6, 2);
    tvg_shape_set_radial_gradient(shape1, grad6);
    tvg_canvas_update(canvas);

    tvg_shape_set_stroke_width(shape,3);
    tvg_shape_set_stroke_color(shape, 125, 0, 125, 255);
    tvg_canvas_update_paint(canvas, shape);

    const Tvg_Path_Command* cmds;
    uint32_t cmdCnt;
    const Tvg_Point* pts;
    uint32_t ptsCnt;

    tvg_shape_get_path_commands(shape, &cmds, &cmdCnt);

    tvg_shape_get_path_coords(shape, &pts, &ptsCnt);

    float x1, y1, x2, y2, radius;
    tvg_linear_gradient_get(grad, &x1, &y1, &x2, &y2);
    tvg_radial_gradient_get(grad6, &x1, &y1, &radius);

    uint32_t cnt;
    const Tvg_Color_Stop* color_stops_get;
    tvg_gradient_get_color_stops(grad5, &color_stops_get, &cnt);

    Tvg_Stroke_Fill spread;
    tvg_gradient_get_spread(grad, &spread);

    //Origin paint for duplicated
    Tvg_Paint* org = tvg_shape_new();
    tvg_shape_append_rect(org, 550, 10, 100, 100, 0, 0);
    tvg_shape_set_stroke_width(org, 3);
    tvg_shape_set_stroke_color(org, 255, 0, 0, 255);
    tvg_shape_set_fill_color(org, 0, 255, 0, 255);

    //Duplicated paint test - should copy rectangle parameters from origin
    Tvg_Paint* dup = tvg_paint_duplicate(org);
    tvg_canvas_push(canvas, dup);

    tvg_paint_del(org);

    //Scene test
    Tvg_Paint* scene = tvg_scene_new();

    Tvg_Paint* scene_shape_1 = tvg_shape_new();
    tvg_shape_append_rect(scene_shape_1, 650, 410, 100, 50, 10, 10);
    tvg_shape_set_fill_color(scene_shape_1, 0, 255, 0, 255);

    Tvg_Paint* scene_shape_2 = tvg_shape_new();
    tvg_shape_append_rect(scene_shape_2, 650, 470, 100, 50, 10, 10);
    tvg_shape_set_fill_color(scene_shape_2, 0, 255, 0, 255);

    tvg_scene_push(scene, scene_shape_1);
    tvg_scene_push(scene, scene_shape_2);
    tvg_paint_set_opacity(scene, 100);

    tvg_canvas_push(canvas, scene);
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
