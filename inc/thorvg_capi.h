#ifndef __THORVG_CAPI_H__
#define __THORVG_CAPI_H__

#include <stdbool.h>

#ifdef TVG_EXPORT
    #undef TVG_EXPORT
#endif

#ifdef TVG_BUILD
    #define TVG_EXPORT __attribute__ ((visibility ("default")))
#else
    #define TVG_EXPORT
#endif


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Tvg_Canvas Tvg_Canvas;
typedef struct _Tvg_Paint Tvg_Paint;
typedef struct _Tvg_Gradient Tvg_Gradient;

#define TVG_ENGINE_SW (1 << 1)
#define TVG_ENGINE_GL (1 << 2)

#define TVG_COLORSPACE_ABGR8888 0
#define TVG_COLORSPACE_ARGB8888 1

typedef enum {
    TVG_RESULT_SUCCESS = 0,
    TVG_RESULT_INVALID_ARGUMENT,
    TVG_RESULT_INSUFFICIENT_CONDITION,
    TVG_RESULT_FAILED_ALLOCATION,
    TVG_RESULT_MEMORY_CORRUPTION,
    TVG_RESULT_NOT_SUPPORTED,
    TVG_RESULT_UNKNOWN
} Tvg_Result;


typedef enum {
    TVG_PATH_COMMAND_CLOSE = 0,
    TVG_PATH_COMMAND_MOVE_TO,
    TVG_PATH_COMMAND_LINE_TO,
    TVG_PATH_COMMAND_CUBIC_TO
} Tvg_Path_Command;


typedef enum {
    TVG_STROKE_CAP_SQUARE = 0,
    TVG_STROKE_CAP_ROUND,
    TVG_STROKE_CAP_BUTT
} Tvg_Stroke_Cap;


typedef enum {
    TVG_STROKE_JOIN_BEVEL = 0,
    TVG_STROKE_JOIN_ROUND,
    TVG_STROKE_JOIN_MITER
} Tvg_Stroke_Join;


typedef enum {
    TVG_STROKE_FILL_PAD = 0,
    TVG_STROKE_FILL_REFLECT,
    TVG_STROKE_FILL_REPEAT
} Tvg_Stroke_Fill;


typedef enum {
    TVG_FILL_RULE_WINDING = 0,
    TVG_FILL_RULE_EVEN_ODD
} Tvg_Fill_Rule;

typedef struct
{
    float x, y;
} Tvg_Point;


typedef struct
{
    float e11, e12, e13;
    float e21, e22, e23;
    float e31, e32, e33;
} Tvg_Matrix;

typedef struct
{
    float offset;
    uint8_t r, g, b, a;
} Tvg_Color_Stop;

/************************************************************************/
/* Engine API                                                           */
/************************************************************************/
TVG_EXPORT Tvg_Result tvg_engine_init(unsigned engine_method, unsigned threads);
TVG_EXPORT Tvg_Result tvg_engine_term(unsigned engine_method);


/************************************************************************/
/* SwCanvas API                                                         */
/************************************************************************/
TVG_EXPORT Tvg_Canvas* tvg_swcanvas_create();
TVG_EXPORT Tvg_Result tvg_swcanvas_set_target(Tvg_Canvas* canvas, uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, uint32_t cs);


/************************************************************************/
/* Common Canvas API                                                    */
/************************************************************************/
TVG_EXPORT Tvg_Result tvg_canvas_destroy(Tvg_Canvas* canvas);
TVG_EXPORT Tvg_Result tvg_canvas_push(Tvg_Canvas* canvas, Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_canvas_reserve(Tvg_Canvas* canvas, uint32_t n);
TVG_EXPORT Tvg_Result tvg_canvas_clear(Tvg_Canvas* canvas, bool free);
TVG_EXPORT Tvg_Result tvg_canvas_update(Tvg_Canvas* canvas);
TVG_EXPORT Tvg_Result tvg_canvas_update_paint(Tvg_Canvas* canvas, Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_canvas_draw(Tvg_Canvas* canvas);
TVG_EXPORT Tvg_Result tvg_canvas_sync(Tvg_Canvas* canvas);


/************************************************************************/
/* Paint API                                                            */
/************************************************************************/
TVG_EXPORT Tvg_Result tvg_paint_del(Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_paint_scale(Tvg_Paint* paint, float factor);
TVG_EXPORT Tvg_Result tvg_paint_rotate(Tvg_Paint* paint, float degree);
TVG_EXPORT Tvg_Result tvg_paint_translate(Tvg_Paint* paint, float x, float y);
TVG_EXPORT Tvg_Result tvg_paint_transform(Tvg_Paint* paint, const Tvg_Matrix* m);
TVG_EXPORT Tvg_Result tvg_paint_set_opacity(Tvg_Paint* paint, uint8_t opacity);
TVG_EXPORT Tvg_Result tvg_paint_get_opacity(Tvg_Paint* paint, uint8_t* opacity);
TVG_EXPORT Tvg_Paint* tvg_paint_duplicate(Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_paint_get_bounds(const Tvg_Paint* paint, float* x, float* y, float* w, float* h);

/************************************************************************/
/* Shape API                                                            */
/************************************************************************/
TVG_EXPORT Tvg_Paint* tvg_shape_new();
TVG_EXPORT Tvg_Result tvg_shape_reset(Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_shape_move_to(Tvg_Paint* paint, float x, float y);
TVG_EXPORT Tvg_Result tvg_shape_line_to(Tvg_Paint* paint, float x, float y);
TVG_EXPORT Tvg_Result tvg_shape_cubic_to(Tvg_Paint* paint, float cx1, float cy1, float cx2, float cy2, float x, float y);
TVG_EXPORT Tvg_Result tvg_shape_close(Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_shape_append_rect(Tvg_Paint* paint, float x, float y, float w, float h, float rx, float ry);
TVG_EXPORT Tvg_Result tvg_shape_append_circle(Tvg_Paint* paint, float cx, float cy, float rx, float ry);
TVG_EXPORT Tvg_Result tvg_shape_append_arc(Tvg_Paint* paint, float cx, float cy, float radius, float startAngle, float sweep, uint8_t pie);
TVG_EXPORT Tvg_Result tvg_shape_append_path(Tvg_Paint* paint, const Tvg_Path_Command* cmds, uint32_t cmdCnt, const Tvg_Point* pts, uint32_t ptsCnt);
TVG_EXPORT Tvg_Result tvg_shape_get_path_coords(const Tvg_Paint* paint, const Tvg_Point** pts, uint32_t* cnt);
TVG_EXPORT Tvg_Result tvg_shape_get_path_commands(const Tvg_Paint* paint, const Tvg_Path_Command** cmds, uint32_t* cnt);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_width(Tvg_Paint* paint, float width);
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_width(const Tvg_Paint* paint, float* width);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_dash(Tvg_Paint* paint, const float* dashPattern, uint32_t cnt);
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_dash(const Tvg_Paint* paint, const float** dashPattern, uint32_t* cnt);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_cap(Tvg_Paint* paint, Tvg_Stroke_Cap cap);
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_cap(const Tvg_Paint* paint, Tvg_Stroke_Cap* cap);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_join(Tvg_Paint* paint, Tvg_Stroke_Join join);
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_join(const Tvg_Paint* paint, Tvg_Stroke_Join* join);
TVG_EXPORT Tvg_Result tvg_shape_set_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
TVG_EXPORT Tvg_Result tvg_shape_get_fill_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
TVG_EXPORT Tvg_Result tvg_shape_set_fill_rule(Tvg_Paint* paint, Tvg_Fill_Rule rule);
TVG_EXPORT Tvg_Result tvg_shape_get_fill_rule(const Tvg_Paint* paint, Tvg_Fill_Rule* rule);
TVG_EXPORT Tvg_Result tvg_shape_set_linear_gradient(Tvg_Paint* paint, Tvg_Gradient* grad);
TVG_EXPORT Tvg_Result tvg_shape_set_radial_gradient(Tvg_Paint* paint, Tvg_Gradient* grad);
TVG_EXPORT Tvg_Result tvg_shape_get_gradient(const Tvg_Paint* paint, Tvg_Gradient** grad);

/************************************************************************/
/* Gradient API                                                         */
/************************************************************************/
TVG_EXPORT Tvg_Gradient* tvg_linear_gradient_new();
TVG_EXPORT Tvg_Gradient* tvg_radial_gradient_new();
TVG_EXPORT Tvg_Result tvg_linear_gradient_set(Tvg_Gradient* grad, float x1, float y1, float x2, float y2);
TVG_EXPORT Tvg_Result tvg_linear_gradient_get(Tvg_Gradient* grad, float* x1, float* y1, float* x2, float* y2);
TVG_EXPORT Tvg_Result tvg_radial_gradient_set(Tvg_Gradient* grad, float cx, float cy, float radius);
TVG_EXPORT Tvg_Result tvg_radial_gradient_get(Tvg_Gradient* grad, float* cx, float* cy, float* radius);
TVG_EXPORT Tvg_Result tvg_gradient_set_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop* color_stop, uint32_t cnt);
TVG_EXPORT Tvg_Result tvg_gradient_get_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop** color_stop, uint32_t* cnt);
TVG_EXPORT Tvg_Result tvg_gradient_set_spread(Tvg_Gradient* grad, const Tvg_Stroke_Fill spread);
TVG_EXPORT Tvg_Result tvg_gradient_get_spread(Tvg_Gradient* grad, Tvg_Stroke_Fill* spread);
TVG_EXPORT Tvg_Result tvg_gradient_del(Tvg_Gradient* grad);

/************************************************************************/
/* Picture API                                                          */
/************************************************************************/
TVG_EXPORT Tvg_Paint* tvg_picture_new();
TVG_EXPORT Tvg_Result tvg_picture_load(Tvg_Paint* paint, const char* path);
TVG_EXPORT Tvg_Result tvg_picture_load_raw(Tvg_Paint* paint, uint32_t *data, uint32_t w, uint32_t h, bool copy);
TVG_EXPORT Tvg_Result tvg_picture_get_viewbox(const Tvg_Paint* paint, float* x, float* y, float* w, float* h);

/************************************************************************/
/* Scene API                                                            */
/************************************************************************/
TVG_EXPORT Tvg_Paint* tvg_scene_new();
TVG_EXPORT Tvg_Result tvg_scene_reserve(Tvg_Paint* scene, uint32_t size);
TVG_EXPORT Tvg_Result tvg_scene_push(Tvg_Paint* scene, Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_scene_clear(Tvg_Paint* scene);


#ifdef __cplusplus
}
#endif

#endif //_THORVG_CAPI_H_
