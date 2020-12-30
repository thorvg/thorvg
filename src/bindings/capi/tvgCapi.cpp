/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.

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

#include <string>
#include <thorvg.h>
#include "thorvg_capi.h"

using namespace std;
using namespace tvg;

#define CCP(A) const_cast<Tvg_Paint*>(A)  //Const-Cast-Paint

#ifdef __cplusplus
extern "C" {
#endif


/************************************************************************/
/* Engine API                                                           */
/************************************************************************/

TVG_EXPORT Tvg_Result tvg_engine_init(unsigned engine_method, unsigned threads)
{
    return (Tvg_Result) Initializer::init(CanvasEngine(engine_method), threads);
}


TVG_EXPORT Tvg_Result tvg_engine_term(unsigned engine_method)
{
    return (Tvg_Result) Initializer::term(CanvasEngine(engine_method));
}

/************************************************************************/
/* Canvas API                                                           */
/************************************************************************/

TVG_EXPORT Tvg_Canvas* tvg_swcanvas_create()
{
    return (Tvg_Canvas*) SwCanvas::gen().release();
}


TVG_EXPORT Tvg_Result tvg_canvas_destroy(Tvg_Canvas* canvas)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    delete(reinterpret_cast<SwCanvas*>(canvas));
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_swcanvas_set_target(Tvg_Canvas* canvas, uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, uint32_t cs)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<SwCanvas*>(canvas)->target(buffer, stride, w, h, static_cast<SwCanvas::Colorspace>(cs));
}


TVG_EXPORT Tvg_Result tvg_canvas_push(Tvg_Canvas* canvas, Tvg_Paint* paint)
{
    if (!canvas || !paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->push(unique_ptr<Paint>((Paint*)paint));
}


TVG_EXPORT Tvg_Result tvg_canvas_reserve(Tvg_Canvas* canvas, uint32_t n)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->reserve(n);
}


TVG_EXPORT Tvg_Result tvg_canvas_clear(Tvg_Canvas* canvas, bool free)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->clear(free);
}


TVG_EXPORT Tvg_Result tvg_canvas_update(Tvg_Canvas* canvas)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->update(nullptr);
}


TVG_EXPORT Tvg_Result tvg_canvas_update_paint(Tvg_Canvas* canvas, Tvg_Paint* paint)
{
    if (!canvas || !paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->update((Paint*) paint);
}


TVG_EXPORT Tvg_Result tvg_canvas_draw(Tvg_Canvas* canvas)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->draw();
}


TVG_EXPORT Tvg_Result tvg_canvas_sync(Tvg_Canvas* canvas)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->sync();
}


/************************************************************************/
/* Paint API                                                            */
/************************************************************************/

TVG_EXPORT Tvg_Result tvg_paint_del(Tvg_Paint* paint)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    delete(reinterpret_cast<Paint*>(paint));
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_paint_scale(Tvg_Paint* paint, float factor)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Paint*>(paint)->scale(factor);
}


TVG_EXPORT Tvg_Result tvg_paint_rotate(Tvg_Paint* paint, float degree)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Paint*>(paint)->rotate(degree);
}


TVG_EXPORT Tvg_Result tvg_paint_translate(Tvg_Paint* paint, float x, float y)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Paint*>(paint)->translate(x, y);
}


TVG_EXPORT Tvg_Result tvg_paint_transform(Tvg_Paint* paint, const Tvg_Matrix* m)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Paint*>(paint)->transform(*(reinterpret_cast<const Matrix*>(m)));
}


TVG_EXPORT Tvg_Paint* tvg_paint_duplicate(Tvg_Paint* paint)
{
    if (!paint) return NULL;
    return (Tvg_Paint*) reinterpret_cast<Paint*>(paint)->duplicate();
}


TVG_EXPORT Tvg_Result tvg_paint_set_opacity(Tvg_Paint* paint, uint8_t opacity)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Paint*>(paint)->opacity(opacity);
}


TVG_EXPORT Tvg_Result tvg_paint_get_opacity(Tvg_Paint* paint, uint8_t* opacity)
{
    if (!paint || !opacity)  return TVG_RESULT_INVALID_ARGUMENT;
    *opacity = reinterpret_cast<Paint*>(paint)->opacity();
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_paint_get_bounds(const Tvg_Paint* paint, float* x, float* y, float* w, float* h)
{
   if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
   return (Tvg_Result) reinterpret_cast<const Paint*>(paint)->bounds(x, y, w, h);
}


/************************************************************************/
/* Shape API                                                            */
/************************************************************************/

TVG_EXPORT Tvg_Paint* tvg_shape_new()
{
    return (Tvg_Paint*) Shape::gen().release();
}


TVG_EXPORT Tvg_Result tvg_shape_reset(Tvg_Paint* paint)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->reset();
}


TVG_EXPORT Tvg_Result tvg_shape_move_to(Tvg_Paint* paint, float x, float y)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->moveTo(x, y);
}


TVG_EXPORT Tvg_Result tvg_shape_line_to(Tvg_Paint* paint, float x, float y)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->lineTo(x, y);
}


TVG_EXPORT Tvg_Result tvg_shape_cubic_to(Tvg_Paint* paint, float cx1, float cy1, float cx2, float cy2, float x, float y)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->cubicTo(cx1, cy1, cx2, cy2, x, y);
}


TVG_EXPORT Tvg_Result tvg_shape_close(Tvg_Paint* paint)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->close();
}


TVG_EXPORT Tvg_Result tvg_shape_append_rect(Tvg_Paint* paint, float x, float y, float w, float h, float rx, float ry)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->appendRect(x, y, w, h, rx, ry);
}

TVG_EXPORT Tvg_Result tvg_shape_append_arc(Tvg_Paint* paint, float cx, float cy, float radius, float startAngle, float sweep, uint8_t pie)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->appendArc(cx, cy, radius, startAngle, sweep, pie);
}

TVG_EXPORT Tvg_Result tvg_shape_append_circle(Tvg_Paint* paint, float cx, float cy, float rx, float ry)
{
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->appendCircle(cx, cy, rx, ry);
}


TVG_EXPORT Tvg_Result tvg_shape_append_path(Tvg_Paint* paint, const Tvg_Path_Command* cmds, uint32_t cmdCnt, const Tvg_Point* pts, uint32_t ptsCnt)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->appendPath((PathCommand*)cmds, cmdCnt, (Point*)pts, ptsCnt);
}

TVG_EXPORT Tvg_Result tvg_shape_get_path_coords(const Tvg_Paint* paint, const Tvg_Point** pts, uint32_t* cnt)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    auto ret = reinterpret_cast<const Shape*>(paint)->pathCoords((const Point**)pts);
    if (cnt) *cnt = ret;
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_shape_get_path_commands(const Tvg_Paint* paint, const Tvg_Path_Command** cmds, uint32_t* cnt)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    auto ret = reinterpret_cast<const Shape*>(paint)->pathCommands((const PathCommand**)cmds);
    if (cnt) *cnt = ret;
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_width(Tvg_Paint* paint, float width)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke(width);
}


TVG_EXPORT Tvg_Result tvg_shape_get_stroke_width(const Tvg_Paint* paint, float* width)
{
    if (!paint || !width) return TVG_RESULT_INVALID_ARGUMENT;
    *width = reinterpret_cast<Shape*>(CCP(paint))->strokeWidth();
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke(r, g, b, a);
}


TVG_EXPORT Tvg_Result tvg_shape_get_stroke_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(CCP(paint))->strokeColor(r, g, b, a);
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_dash(Tvg_Paint* paint, const float* dashPattern, uint32_t cnt)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke(dashPattern, cnt);
}


TVG_EXPORT Tvg_Result tvg_shape_get_stroke_dash(const Tvg_Paint* paint, const float** dashPattern, uint32_t* cnt)
{
    if (!paint || !cnt || !dashPattern) return TVG_RESULT_INVALID_ARGUMENT;
    *cnt = reinterpret_cast<Shape*>(CCP(paint))->strokeDash(dashPattern);
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_cap(Tvg_Paint* paint, Tvg_Stroke_Cap cap)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke((StrokeCap)cap);
}


TVG_EXPORT Tvg_Result tvg_shape_get_stroke_cap(const Tvg_Paint* paint, Tvg_Stroke_Cap* cap)
{
    if (!paint || !cap) return TVG_RESULT_INVALID_ARGUMENT;
    *cap = (Tvg_Stroke_Cap) reinterpret_cast<Shape*>(CCP(paint))->strokeCap();
    return TVG_RESULT_SUCCESS;
}

TVG_EXPORT Tvg_Result tvg_shape_set_stroke_join(Tvg_Paint* paint, Tvg_Stroke_Join join)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke((StrokeJoin)join);
}


TVG_EXPORT Tvg_Result tvg_shape_get_stroke_join(const Tvg_Paint* paint, Tvg_Stroke_Join* join)
{
    if (!paint || !join) return TVG_RESULT_INVALID_ARGUMENT;
    *join = (Tvg_Stroke_Join) reinterpret_cast<Shape*>(CCP(paint))->strokeJoin();
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_shape_set_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill(r, g, b, a);
}


TVG_EXPORT Tvg_Result tvg_shape_get_fill_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(CCP(paint))->fillColor(r, g, b, a);
}


TVG_EXPORT Tvg_Result tvg_shape_set_fill_rule(Tvg_Paint* paint, Tvg_Fill_Rule rule)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill((FillRule)rule);
}


TVG_EXPORT Tvg_Result tvg_shape_get_fill_rule(const Tvg_Paint* paint, Tvg_Fill_Rule* rule)
{
    if (!paint || !rule) return TVG_RESULT_INVALID_ARGUMENT;
    *rule = (Tvg_Fill_Rule) reinterpret_cast<Shape*>(CCP(paint))->fillRule();
    return TVG_RESULT_SUCCESS;
}


TVG_EXPORT Tvg_Result tvg_shape_set_linear_gradient(Tvg_Paint* paint, Tvg_Gradient* gradient)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill(unique_ptr<LinearGradient>((LinearGradient*)(gradient)));
}


TVG_EXPORT Tvg_Result tvg_shape_set_radial_gradient(Tvg_Paint* paint, Tvg_Gradient* gradient)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill(unique_ptr<RadialGradient>((RadialGradient*)(gradient)));
}


TVG_EXPORT Tvg_Result tvg_shape_get_gradient(const Tvg_Paint* paint, Tvg_Gradient** gradient)
{
   if (!paint || !gradient) return TVG_RESULT_INVALID_ARGUMENT;
   *gradient = (Tvg_Gradient*)(reinterpret_cast<Shape*>(CCP(paint))->fill());
   return TVG_RESULT_SUCCESS;
}


/************************************************************************/
/* Picture API                                                          */
/************************************************************************/

TVG_EXPORT Tvg_Paint* tvg_picture_new()
{
    return (Tvg_Paint*) Picture::gen().release();
}


TVG_EXPORT Tvg_Result tvg_picture_load(Tvg_Paint* paint, const char* path)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Picture*>(paint)->load(path);
}


TVG_EXPORT Tvg_Result tvg_picture_load_raw(Tvg_Paint* paint, uint32_t *data, uint32_t w, uint32_t h, bool copy)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Picture*>(paint)->load(data, w, h, copy);
}


TVG_EXPORT Tvg_Result tvg_picture_get_viewbox(const Tvg_Paint* paint, float* x, float* y, float* w, float* h)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Picture*>(CCP(paint))->viewbox(x, y, w, h);
}


/************************************************************************/
/* Gradient API                                                         */
/************************************************************************/
TVG_EXPORT Tvg_Gradient* tvg_linear_gradient_new()
{
    return (Tvg_Gradient*)LinearGradient::gen().release();
}

TVG_EXPORT Tvg_Gradient* tvg_radial_gradient_new()
{
    return (Tvg_Gradient*)RadialGradient::gen().release();

}

TVG_EXPORT Tvg_Result tvg_gradient_del(Tvg_Gradient* grad)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    delete(reinterpret_cast<Fill*>(grad));
    return TVG_RESULT_SUCCESS;
}

TVG_EXPORT Tvg_Result tvg_linear_gradient_set(Tvg_Gradient* grad, float x1, float y1, float x2, float y2)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<LinearGradient*>(grad)->linear(x1, y1, x2, y2);
}

TVG_EXPORT Tvg_Result tvg_linear_gradient_get(Tvg_Gradient* grad, float* x1, float* y1, float* x2, float* y2)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<LinearGradient*>(grad)->linear(x1, y1, x2, y2);
}

TVG_EXPORT Tvg_Result tvg_radial_gradient_set(Tvg_Gradient* grad, float cx, float cy, float radius)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<RadialGradient*>(grad)->radial(cx, cy, radius);
}

TVG_EXPORT Tvg_Result tvg_radial_gradient_get(Tvg_Gradient* grad, float* cx, float* cy, float* radius)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<RadialGradient*>(grad)->radial(cx, cy, radius);
}

TVG_EXPORT Tvg_Result tvg_gradient_set_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop* color_stop, uint32_t cnt)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Fill*>(grad)->colorStops(reinterpret_cast<const Fill::ColorStop*>(color_stop), cnt);
}

TVG_EXPORT Tvg_Result tvg_gradient_get_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop** color_stop, uint32_t* cnt)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    *cnt = reinterpret_cast<Fill*>(grad)->colorStops(reinterpret_cast<const Fill::ColorStop**>(color_stop));
    return TVG_RESULT_SUCCESS;
}

TVG_EXPORT Tvg_Result tvg_gradient_set_spread(Tvg_Gradient* grad, const Tvg_Stroke_Fill spread)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Fill*>(grad)->spread((FillSpread)spread);
}

TVG_EXPORT Tvg_Result tvg_gradient_get_spread(Tvg_Gradient* grad, Tvg_Stroke_Fill* spread)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    *spread = (Tvg_Stroke_Fill) reinterpret_cast<Fill*>(grad)->spread();
    return TVG_RESULT_SUCCESS;
}

TVG_EXPORT Tvg_Paint* tvg_scene_new()
{
    return (Tvg_Paint*) Scene::gen().release();
}

TVG_EXPORT Tvg_Result tvg_scene_reserve(Tvg_Paint* scene, uint32_t size)
{
    if (!scene) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Scene*>(scene)->reserve(size);
}

TVG_EXPORT Tvg_Result tvg_scene_push(Tvg_Paint* scene, Tvg_Paint* paint)
{
    if (!scene || !paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push(unique_ptr<Paint>((Paint*)paint));
}

TVG_EXPORT Tvg_Result tvg_scene_clear(Tvg_Paint* scene)
{
    if (!scene) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Scene*>(scene)->clear();
}


#ifdef __cplusplus
}
#endif
