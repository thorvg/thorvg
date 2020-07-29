/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef __THORVG_CAPI_H__
#define __THORVG_CAPI_H__

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

#define TVG_ENGINE_SW (1 << 1)
#define TVG_ENGINE_GL (1 << 2)


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


/************************************************************************/
/* Engine API                                                           */
/************************************************************************/
TVG_EXPORT Tvg_Result tvg_engine_init(unsigned engine_method);
TVG_EXPORT Tvg_Result tvg_engine_term(unsigned engine_method);


/************************************************************************/
/* SwCanvas API                                                         */
/************************************************************************/
TVG_EXPORT Tvg_Canvas* tvg_swcanvas_create();
TVG_EXPORT Tvg_Result tvg_swcanvas_set_target(Tvg_Canvas* canvas, uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h);


/************************************************************************/
/* Common Canvas API                                                    */
/************************************************************************/
TVG_EXPORT Tvg_Result tvg_canvas_destroy(Tvg_Canvas* canvas);
TVG_EXPORT Tvg_Result tvg_canvas_push(Tvg_Canvas* canvas, Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_canvas_reserve(Tvg_Canvas* canvas, uint32_t n);
TVG_EXPORT Tvg_Result tvg_canvas_clear(Tvg_Canvas* canvas);
TVG_EXPORT Tvg_Result tvg_canvas_update(Tvg_Canvas* canvas);
TVG_EXPORT Tvg_Result tvg_canvas_update_paint(Tvg_Canvas* canvas, Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_canvas_draw(Tvg_Canvas* canvas, unsigned char async);
TVG_EXPORT Tvg_Result tvg_canvas_sync(Tvg_Canvas* canvas);


/************************************************************************/
/* Shape API                                                            */
/************************************************************************/
TVG_EXPORT Tvg_Paint* tvg_shape_new();
TVG_EXPORT Tvg_Result tvg_shape_del(Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_shape_reset(Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_shape_move_to(Tvg_Paint* paint, float x, float y);
TVG_EXPORT Tvg_Result tvg_shape_line_to(Tvg_Paint* paint, float x, float y);
TVG_EXPORT Tvg_Result tvg_shape_cubic_to(Tvg_Paint* paint, float cx1, float cy1, float cx2, float cy2, float x, float y);
TVG_EXPORT Tvg_Result tvg_shape_close(Tvg_Paint* paint);
TVG_EXPORT Tvg_Result tvg_shape_append_rect(Tvg_Paint* paint, float x, float y, float w, float h, float rx, float ry);
TVG_EXPORT Tvg_Result tvg_shape_append_circle(Tvg_Paint* paint, float cx, float cy, float rx, float ry);
TVG_EXPORT Tvg_Result tvg_shape_append_path(Tvg_Paint* paint, const Tvg_Path_Command* cmds, uint32_t cmdCnt, const Tvg_Point* pts, uint32_t ptsCnt);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_width(Tvg_Paint* paint, float width);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_dash(Tvg_Paint* paint, const float* dashPattern, uint32_t cnt);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_cap(Tvg_Paint* paint, Tvg_Stroke_Cap cap);
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_join(Tvg_Paint* paint, Tvg_Stroke_Join join);
TVG_EXPORT Tvg_Result tvg_shape_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
TVG_EXPORT Tvg_Result tvg_shape_scale(Tvg_Paint* paint, float factor);
TVG_EXPORT Tvg_Result tvg_shape_rotate(Tvg_Paint* paint, float degree);
TVG_EXPORT Tvg_Result tvg_shape_translate(Tvg_Paint* paint, float x, float y);
TVG_EXPORT Tvg_Result tvg_shape_transform(Tvg_Paint* paint, const Tvg_Matrix* m);

#ifdef __cplusplus
}
#endif

#endif //_THORVG_CAPI_H_
