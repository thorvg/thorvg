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

#include <Elementary.h>
#include <thorvg_capi.h>

#define WIDTH 800
#define HEIGHT 800


/************************************************************************/
/* Capi Test Code                                                       */
/************************************************************************/

static uint32_t* buffer = NULL;
static Tvg_Canvas* canvas = NULL;
static Tvg_Animation* animation = NULL;
static Eo* view = NULL;
static Elm_Transit *transit = NULL;

void testCapi()
{
    canvas = tvg_swcanvas_create();
    tvg_swcanvas_set_target(canvas, buffer, WIDTH, WIDTH, HEIGHT, TVG_COLORSPACE_ARGB8888);
    tvg_swcanvas_set_mempool(canvas, TVG_MEMPOOL_POLICY_DEFAULT);

//////1. Linear gradient shape with a linear gradient stroke
    //Set a shape
    Tvg_Paint* shape1 = tvg_shape_new();
    tvg_shape_move_to(shape1, 25.0f, 25.0f);
    tvg_shape_line_to(shape1, 375.0f, 25.0f);
    tvg_shape_cubic_to(shape1, 500.0f, 100.0f, -500.0f, 200.0f, 375.0f, 375.0f);
    tvg_shape_close(shape1);

    //Prepare a gradient for the fill
    Tvg_Gradient* grad1 = tvg_linear_gradient_new();
    tvg_linear_gradient_set(grad1, 25.0f, 25.0f, 200.0f, 200.0f);
    Tvg_Color_Stop color_stops1[4] =
    {
        {0.00f, 255,   0,   0, 155},
        {0.33f,   0, 255,   0, 100},
        {0.66f, 255,   0, 255, 100},
        {1.00f,   0,   0, 255, 155}
    };
    tvg_gradient_set_color_stops(grad1, color_stops1, 4);
    tvg_gradient_set_spread(grad1, TVG_STROKE_FILL_REFLECT);

    //Prepare a gradient for the stroke
    Tvg_Gradient* grad1_stroke = tvg_gradient_duplicate(grad1);

    //Set a gradient fill
    tvg_shape_set_linear_gradient(shape1, grad1);

    //Set a gradient stroke
    tvg_shape_set_stroke_width(shape1, 20.0f);
    tvg_shape_set_stroke_linear_gradient(shape1, grad1_stroke);
    tvg_shape_set_stroke_join(shape1, TVG_STROKE_JOIN_ROUND);


//////2. Solid transformed shape
    //Set a shape
    const Tvg_Path_Command* cmds;
    uint32_t cmdCnt;
    const Tvg_Point* pts;
    uint32_t ptsCnt;

    Tvg_Paint* shape2 = tvg_shape_new();
    tvg_shape_get_path_commands(shape1, &cmds, &cmdCnt);
    tvg_shape_get_path_coords(shape1, &pts, &ptsCnt);

    tvg_shape_append_path(shape2, cmds, cmdCnt, pts, ptsCnt);
    tvg_shape_set_fill_color(shape2, 255, 255, 255, 128);

    //Transform a shape
    tvg_paint_scale(shape2, 0.3f);
    tvg_paint_translate(shape2, 100.0f, 100.0f);


    //Push shapes 1 and 2 into the canvas
    tvg_canvas_push(canvas, shape1);
    tvg_canvas_push(canvas, shape2);


//////3. Radial gradient shape with a radial dashed stroke
    //Set a shape
    Tvg_Paint* shape3 = tvg_shape_new();
    tvg_shape_append_rect(shape3, 550.0f, 20.0f, 100.0f, 50.0f, 0.0f, 0.0f);
    tvg_shape_append_circle(shape3, 600.0f, 150.0f, 100.0f, 50.0f);
    tvg_shape_append_rect(shape3, 550.0f, 230.0f, 100.0f, 100.0f, 20.0f, 40.0f);

    //Prepare a radial gradient for the fill
    Tvg_Gradient* grad2 = tvg_radial_gradient_new();
    tvg_radial_gradient_set(grad2, 600.0f, 180.0f, 50.0f);
    Tvg_Color_Stop color_stops2[3] =
    {
        {0.0f, 255,   0, 255, 255},
        {0.5f,   0,   0, 255, 255},
        {1.0f,  50,  55, 155, 255}
    };
    tvg_gradient_set_color_stops(grad2, color_stops2, 3);
    tvg_gradient_set_spread(grad2, TVG_STROKE_FILL_PAD);

    //Set a gradient fill
    tvg_shape_set_radial_gradient(shape3, grad2);

    //Prepaer a radial gradient for the stroke
    uint32_t cnt;
    const Tvg_Color_Stop* color_stops2_get;
    tvg_gradient_get_color_stops(grad2, &color_stops2_get, &cnt);

    float cx, cy, radius;
    tvg_radial_gradient_get(grad2, &cx, &cy, &radius);

    Tvg_Gradient* grad2_stroke = tvg_radial_gradient_new();
    tvg_radial_gradient_set(grad2_stroke, cx, cy, radius);
    tvg_gradient_set_color_stops(grad2_stroke, color_stops2_get, cnt);
    tvg_gradient_set_spread(grad2_stroke, TVG_STROKE_FILL_REPEAT);

    //Set a gradient stroke
    tvg_shape_set_stroke_width(shape3, 30.0f);
    tvg_shape_set_stroke_radial_gradient(shape3, grad2_stroke);

    tvg_paint_set_opacity(shape3, 200);

    //Push the shape into the canvas
    tvg_canvas_push(canvas, shape3);


//////4. Scene
    //Set a scene
    Tvg_Paint* scene = tvg_scene_new();

    //Set an arc
    Tvg_Paint* scene_shape1 = tvg_shape_new();
    tvg_shape_append_arc(scene_shape1, 175.0f, 600.0f, 150.0f, -45.0f, 90.0f, 1);
    tvg_shape_append_arc(scene_shape1, 175.0f, 600.0f, 150.0f, 225.0f, -90.0f, 1);
    tvg_shape_set_fill_color(scene_shape1, 0, 0, 255, 150);

    //Set an arc with a dashed stroke
    Tvg_Paint* scene_shape2 = tvg_paint_duplicate(scene_shape1);
    tvg_shape_set_fill_color(scene_shape2, 75, 25, 155, 200);

    //Prapare a dash for the stroke
    float dashPattern[4] = {15.0f, 30.0f, 2.0f, 30.0f};
    tvg_shape_set_stroke_dash(scene_shape2, dashPattern, 4);
    tvg_shape_set_stroke_cap(scene_shape2, TVG_STROKE_CAP_ROUND);
    tvg_shape_set_stroke_color(scene_shape2, 0, 0, 255, 255);
    tvg_shape_set_stroke_width(scene_shape2, 15.0f);

    //Transform a shape
    tvg_paint_scale(scene_shape2, 0.7f);
    tvg_paint_rotate(scene_shape2, -90.0f);
    tvg_paint_translate(scene_shape2, -245.0f, 722.0f);

    //Push the shapes into the scene
    tvg_scene_push(scene, scene_shape1);
    tvg_scene_push(scene, scene_shape2);

    //Push the scene into the canvas
    tvg_canvas_push(canvas, scene);

//////5. Masked picture
    //Set a scene
    Tvg_Paint* pict = tvg_picture_new();
    if (tvg_picture_load(pict, EXAMPLE_DIR"/svg/tiger.svg") != TVG_RESULT_SUCCESS) {
        printf("Problem with loading an svg file\n");
        tvg_paint_del(pict);
    } else {
        float w, h;
        tvg_picture_get_size(pict, &w, &h);
        tvg_picture_set_size(pict, w/2, h/2);
        Tvg_Matrix m = {0.8f, 0.0f, 400.0f, 0.0f, 0.8f, 400.0f, 0.0f, 0.0f, 1.0f};
        tvg_paint_set_transform(pict, &m);

        // Set a composite shape
        Tvg_Paint* comp = tvg_shape_new();
        tvg_shape_append_circle(comp, 600.0f, 600.0f, 100.0f, 100.0f);
        tvg_shape_set_fill_color(comp, 0, 0, 0, 200);
        tvg_paint_set_composite_method(pict, comp, TVG_COMPOSITE_METHOD_INVERSE_ALPHA_MASK);

        //Push the scene into the canvas
        tvg_canvas_push(canvas, pict);
    }

//////6. Animation with a picture
    animation = tvg_animation_new();
    Tvg_Paint* pict_lottie = tvg_animation_get_picture(animation);
    if (tvg_picture_load(pict_lottie, EXAMPLE_DIR"/lottie/sample.json") != TVG_RESULT_SUCCESS) {
        printf("Problem with loading an lottie file\n");
        tvg_animation_del(animation);
    } else {
        tvg_paint_scale(pict_lottie, 3.0f);
        tvg_canvas_push(canvas, pict_lottie);

        float duration;
        tvg_animation_get_duration(animation, &duration);
        elm_transit_duration_set(transit, duration);
        elm_transit_repeat_times_set(transit, -1);
        elm_transit_go(transit);
    }

//////Draw the canvas
    tvg_canvas_draw(canvas);
    tvg_canvas_sync(canvas);
}


/************************************************************************/
/* Animation Code                                                       */
/************************************************************************/

void transitCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    if (!canvas) return;

    float total_frame = 0.0f;
    tvg_animation_get_total_frame(animation, &total_frame);

    float new_frame = total_frame * progress;

    float cur_frame = 0.0f;
    tvg_animation_get_frame(animation, &cur_frame);

    //Update animation frame only when it's changed
    if (tvg_animation_set_frame(animation, new_frame) == TVG_RESULT_SUCCESS) {
        tvg_canvas_update_paint(canvas, tvg_animation_get_picture(animation));
    }

    //Draw the canvas
    tvg_canvas_draw(canvas);
    tvg_canvas_sync(canvas);

    //Update Efl Canvas
    Eo* img = (Eo*) effect;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

void win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}

void resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    int w = 0, h = 0;
    evas_object_geometry_get(obj, NULL, NULL, &w, &h);

    if ((w != WIDTH || h != HEIGHT) && (w != 0 && h != 0))
    {
        evas_object_image_data_set(view, NULL); //prevent evas scale on invalid buffer by rendering thread
        buffer = (uint32_t*) realloc(buffer, sizeof(uint32_t) * w * h);
        tvg_swcanvas_set_target(canvas, buffer, w, w, h, TVG_COLORSPACE_ARGB8888);

        tvg_canvas_update(canvas);
        tvg_canvas_draw(canvas);
        tvg_canvas_sync(canvas);

        evas_object_image_size_set(view, w, h);
        evas_object_image_data_set(view, buffer);
        evas_object_image_pixels_dirty_set(view, EINA_TRUE);
        evas_object_image_data_update_add(view, 0, 0, w, h);
    }
}

int main(int argc, char **argv)
{
    elm_init(argc, argv);
    tvg_engine_init(Tvg_Engine(TVG_ENGINE_SW | TVG_ENGINE_GL), 0);

    buffer = (uint32_t*)malloc(sizeof(uint32_t) * WIDTH * HEIGHT);

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");

    evas_object_smart_callback_add(win, "delete,request", win_del, 0);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, resize_cb, 0);

    view = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(view, WIDTH, HEIGHT);
    evas_object_image_data_set(view, buffer);
    evas_object_image_pixels_dirty_set(view, EINA_TRUE);
    evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);
    evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(view);

    elm_win_resize_object_add(win, view);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    transit = elm_transit_add();

    testCapi();

    elm_transit_effect_add(transit, transitCb, view, nullptr);

    elm_run();
    elm_transit_del(transit);
    
    tvg_canvas_destroy(canvas);
    free(buffer);
    tvg_animation_del(animation);
    tvg_engine_term(Tvg_Engine(TVG_ENGINE_SW | TVG_ENGINE_GL));
    elm_shutdown();

    return 0;
}
