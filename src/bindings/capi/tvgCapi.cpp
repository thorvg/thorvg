/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "config.h"
#include <string>
#include <cstdarg>
#include <thorvg.h>
#include "thorvg_capi.h"
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
#include <thorvg_lottie.h>
#endif

using namespace std;
using namespace tvg;

#ifdef __cplusplus
extern "C" {
#endif


/************************************************************************/
/* Engine API                                                           */
/************************************************************************/

TVG_API Tvg_Result tvg_engine_init(unsigned threads)
{
    return (Tvg_Result) Initializer::init(threads);
}


TVG_API Tvg_Result tvg_engine_term()
{
    return (Tvg_Result) Initializer::term();
}


TVG_API Tvg_Result tvg_engine_version(uint32_t* major, uint32_t* minor, uint32_t* micro, const char** version)
{
    if (version) *version = Initializer::version(major, minor, micro);
    return TVG_RESULT_SUCCESS;
}

/************************************************************************/
/* Canvas API                                                           */
/************************************************************************/

TVG_API Tvg_Canvas* tvg_swcanvas_create()
{
    return (Tvg_Canvas*) SwCanvas::gen();
}


TVG_API Tvg_Canvas* tvg_glcanvas_create()
{
    return (Tvg_Canvas*) GlCanvas::gen();
}


TVG_API Tvg_Canvas* tvg_wgcanvas_create()
{
    return (Tvg_Canvas*) WgCanvas::gen();
}


TVG_API Tvg_Result tvg_canvas_destroy(Tvg_Canvas* canvas)
{
    if (canvas) {
        delete(reinterpret_cast<Canvas*>(canvas));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_swcanvas_set_target(Tvg_Canvas* canvas, uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, Tvg_Colorspace cs)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<SwCanvas*>(canvas)->target(buffer, stride, w, h, static_cast<ColorSpace>(cs));
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_glcanvas_set_target(Tvg_Canvas* canvas, void* context, int32_t id, uint32_t w, uint32_t h, Tvg_Colorspace cs)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<GlCanvas*>(canvas)->target(context, id, w, h, static_cast<ColorSpace>(cs));
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_wgcanvas_set_target(Tvg_Canvas* canvas, void* device, void* instance, void* target, uint32_t w, uint32_t h, Tvg_Colorspace cs, int type)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<WgCanvas*>(canvas)->target(device, instance, target, w, h, static_cast<ColorSpace>(cs), type);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_push(Tvg_Canvas* canvas, Tvg_Paint* paint)
{
    if (canvas && paint) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->push((Paint*)paint);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_push_at(Tvg_Canvas* canvas, Tvg_Paint* target, Tvg_Paint* at)
{
    if (canvas && target && at) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->push((Paint*)target, (Paint*) at);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_remove(Tvg_Canvas* canvas, Tvg_Paint* paint)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->remove((Paint*) paint);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_update(Tvg_Canvas* canvas)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->update(nullptr);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_update_paint(Tvg_Canvas* canvas, Tvg_Paint* paint)
{
    if (canvas && paint) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->update((Paint*) paint);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_draw(Tvg_Canvas* canvas, bool clear)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->draw(clear);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_sync(Tvg_Canvas* canvas)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->sync();
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_canvas_set_viewport(Tvg_Canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (canvas) return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->viewport(x, y, w, h);
    return TVG_RESULT_INVALID_ARGUMENT;
}


/************************************************************************/
/* Paint API                                                            */
/************************************************************************/

TVG_API const Tvg_Paint* tvg_paint_get_parent(const Tvg_Paint* paint)
{
    return (const Tvg_Paint*) reinterpret_cast<const Paint*>(paint)->parent();
}


TVG_API Tvg_Result tvg_paint_del(Tvg_Paint* paint)
{
    if (paint) {
        delete(reinterpret_cast<Paint*>(paint));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API uint8_t tvg_paint_ref(Tvg_Paint* paint)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->ref();
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API uint8_t tvg_paint_unref(Tvg_Paint* paint, bool free)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->unref(free);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API uint8_t tvg_paint_get_ref(const Tvg_Paint* paint)
{
    if (paint) return (Tvg_Result) reinterpret_cast<const Paint*>(paint)->refCnt();
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_scale(Tvg_Paint* paint, float factor)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->scale(factor);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_rotate(Tvg_Paint* paint, float degree)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->rotate(degree);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_translate(Tvg_Paint* paint, float x, float y)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->translate(x, y);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_set_transform(Tvg_Paint* paint, const Tvg_Matrix* m)
{
    if (paint && m) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->transform(*(reinterpret_cast<const Matrix*>(m)));
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_get_transform(Tvg_Paint* paint, Tvg_Matrix* m)
{
    if (paint && m) {
        *reinterpret_cast<Matrix*>(m) = reinterpret_cast<Paint*>(paint)->transform();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Paint* tvg_paint_duplicate(Tvg_Paint* paint)
{
    if (paint) return (Tvg_Paint*) reinterpret_cast<Paint*>(paint)->duplicate();
    return nullptr;
}


TVG_API Tvg_Result tvg_paint_set_opacity(Tvg_Paint* paint, uint8_t opacity)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->opacity(opacity);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_get_opacity(const Tvg_Paint* paint, uint8_t* opacity)
{
    if (paint && opacity) {
        *opacity = reinterpret_cast<const Paint*>(paint)->opacity();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_get_aabb(const Tvg_Paint* paint, float* x, float* y, float* w, float* h)
{
    if (paint) return (Tvg_Result) reinterpret_cast<const Paint*>(paint)->bounds(x, y, w, h);
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_paint_get_obb(const Tvg_Paint* paint, Tvg_Point* pt4)
{
    if (paint) return (Tvg_Result) reinterpret_cast<const Paint*>(paint)->bounds((Point*)pt4);
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_paint_set_mask_method(Tvg_Paint* paint, Tvg_Paint* target, Tvg_Mask_Method method)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->mask((Paint*)target, (MaskMethod)method);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_get_mask_method(const Tvg_Paint* paint, const Tvg_Paint** target, Tvg_Mask_Method* method)
{
    if (paint && target && method) {
        *reinterpret_cast<MaskMethod*>(method) = reinterpret_cast<const Paint*>(paint)->mask(reinterpret_cast<const Paint**>(target));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_set_blend_method(Tvg_Paint* paint, Tvg_Blend_Method method)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->blend((BlendMethod)method);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_get_type(const Tvg_Paint* paint, Tvg_Type* type)
{
    if (paint && type) {
        *type = static_cast<Tvg_Type>(reinterpret_cast<const Paint*>(paint)->type());
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_paint_set_clip(Tvg_Paint* paint, Tvg_Paint* clipper)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Paint*>(paint)->clip((Shape*)(clipper));
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Paint* tvg_paint_get_clip(const Tvg_Paint* paint)
{
   if (paint) return (Tvg_Paint*) reinterpret_cast<const Paint*>(paint)->clip();
   return nullptr;
}


/************************************************************************/
/* Shape API                                                            */
/************************************************************************/

TVG_API Tvg_Paint* tvg_shape_new()
{
    return (Tvg_Paint*) Shape::gen();
}


TVG_API Tvg_Result tvg_shape_reset(Tvg_Paint* paint)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->reset();
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_move_to(Tvg_Paint* paint, float x, float y)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->moveTo(x, y);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_line_to(Tvg_Paint* paint, float x, float y)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->lineTo(x, y);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_cubic_to(Tvg_Paint* paint, float cx1, float cy1, float cx2, float cy2, float x, float y)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->cubicTo(cx1, cy1, cx2, cy2, x, y);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_close(Tvg_Paint* paint)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->close();
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_append_rect(Tvg_Paint* paint, float x, float y, float w, float h, float rx, float ry, bool cw)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->appendRect(x, y, w, h, rx, ry, cw);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_append_circle(Tvg_Paint* paint, float cx, float cy, float rx, float ry, bool cw)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->appendCircle(cx, cy, rx, ry, cw);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_append_path(Tvg_Paint* paint, const Tvg_Path_Command* cmds, uint32_t cmdCnt, const Tvg_Point* pts, uint32_t ptsCnt)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->appendPath((const PathCommand*)cmds, cmdCnt, (const Point*)pts, ptsCnt);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_path(const Tvg_Paint* paint, const Tvg_Path_Command** cmds, uint32_t* cmdsCnt, const Tvg_Point** pts, uint32_t* ptsCnt)
{
    if (paint) return (Tvg_Result) reinterpret_cast<const Shape*>(paint)->path((const PathCommand**)cmds, cmdsCnt, (const Point**)pts, ptsCnt);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_stroke_width(Tvg_Paint* paint, float width)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->strokeWidth(width);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_stroke_width(const Tvg_Paint* paint, float* width)
{
    if (paint && width) {
        *width = reinterpret_cast<const Shape*>(paint)->strokeWidth();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_stroke_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->strokeFill(r, g, b, a);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_stroke_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
{
    if (paint) return (Tvg_Result) reinterpret_cast<const Shape*>(paint)->strokeFill(r, g, b, a);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_stroke_gradient(Tvg_Paint* paint, Tvg_Gradient* gradient)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->strokeFill((Fill*)(gradient));
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_stroke_gradient(const Tvg_Paint* paint, Tvg_Gradient** gradient)
{
    if (paint && gradient) {
        *gradient = (Tvg_Gradient*)(reinterpret_cast<const Shape*>(paint)->strokeFill());
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_stroke_dash(Tvg_Paint* paint, const float* dashPattern, uint32_t cnt, float offset)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->strokeDash(dashPattern, cnt, offset);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_stroke_dash(const Tvg_Paint* paint, const float** dashPattern, uint32_t* cnt, float* offset)
{
    if (paint) {
        *cnt = reinterpret_cast<const Shape*>(paint)->strokeDash(dashPattern, offset);
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_stroke_cap(Tvg_Paint* paint, Tvg_Stroke_Cap cap)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->strokeCap((StrokeCap)cap);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_stroke_cap(const Tvg_Paint* paint, Tvg_Stroke_Cap* cap)
{
    if (paint && cap) {
        *cap = (Tvg_Stroke_Cap) reinterpret_cast<const Shape*>(paint)->strokeCap();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_stroke_join(Tvg_Paint* paint, Tvg_Stroke_Join join)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->strokeJoin((StrokeJoin)join);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_stroke_join(const Tvg_Paint* paint, Tvg_Stroke_Join* join)
{
    if (paint && join) {
        *join = (Tvg_Stroke_Join) reinterpret_cast<const Shape*>(paint)->strokeJoin();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_stroke_miterlimit(Tvg_Paint* paint, float ml)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->strokeMiterlimit(ml);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_stroke_miterlimit(const Tvg_Paint* paint, float* ml)
{
    if (paint && ml) {
        *ml = reinterpret_cast<const Shape*>(paint)->strokeMiterlimit();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_trimpath(Tvg_Paint* paint, float begin, float end, bool simultaneous)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->trimpath(begin, end, simultaneous);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill(r, g, b, a);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_fill_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
{
    if (paint) return (Tvg_Result) reinterpret_cast<const Shape*>(paint)->fill(r, g, b, a);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_fill_rule(Tvg_Paint* paint, Tvg_Fill_Rule rule)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill((FillRule)rule);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_fill_rule(const Tvg_Paint* paint, Tvg_Fill_Rule* rule)
{
    if (paint && rule) {
        *rule = (Tvg_Fill_Rule) reinterpret_cast<const Shape*>(paint)->fillRule();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_paint_order(Tvg_Paint* paint, bool strokeFirst)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->order(strokeFirst);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_set_gradient(Tvg_Paint* paint, Tvg_Gradient* gradient)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill((Fill*)gradient);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_shape_get_gradient(const Tvg_Paint* paint, Tvg_Gradient** gradient)
{
    if (paint && gradient) {
        *gradient = (Tvg_Gradient*)(reinterpret_cast<const Shape*>(paint)->fill());
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}

/************************************************************************/
/* Picture API                                                          */
/************************************************************************/

TVG_API Tvg_Paint* tvg_picture_new()
{
    return (Tvg_Paint*) Picture::gen();
}


TVG_API Tvg_Result tvg_picture_load(Tvg_Paint* paint, const char* path)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Picture*>(paint)->load(path);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_picture_load_raw(Tvg_Paint* paint, uint32_t *data, uint32_t w, uint32_t h, Tvg_Colorspace cs, bool copy)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Picture*>(paint)->load(data, w, h, static_cast<ColorSpace>(cs), copy);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_picture_load_data(Tvg_Paint* paint, const char *data, uint32_t size, const char *mimetype, const char* rpath, bool copy)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Picture*>(paint)->load(data, size, mimetype ? mimetype : "", rpath ? rpath : "", copy);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_picture_set_size(Tvg_Paint* paint, float w, float h)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Picture*>(paint)->size(w, h);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_picture_get_size(const Tvg_Paint* paint, float* w, float* h)
{
    if (paint) return (Tvg_Result) reinterpret_cast<const Picture*>(paint)->size(w, h);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API const Tvg_Paint* tvg_picture_get_paint(Tvg_Paint* paint, uint32_t id)
{
    if (paint) return (Tvg_Paint*) reinterpret_cast<Picture*>(paint)->paint(id);
    return nullptr;
}


/************************************************************************/
/* Gradient API                                                         */
/************************************************************************/

TVG_API Tvg_Gradient* tvg_linear_gradient_new()
{
    return (Tvg_Gradient*)LinearGradient::gen();
}


TVG_API Tvg_Gradient* tvg_radial_gradient_new()
{
    return (Tvg_Gradient*)RadialGradient::gen();
}


TVG_API Tvg_Gradient* tvg_gradient_duplicate(Tvg_Gradient* grad)
{
    if (grad) return (Tvg_Gradient*) reinterpret_cast<Fill*>(grad)->duplicate();
    return nullptr;
}


TVG_API Tvg_Result tvg_gradient_del(Tvg_Gradient* grad)
{
    if (grad) {
        delete(reinterpret_cast<Fill*>(grad));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_linear_gradient_set(Tvg_Gradient* grad, float x1, float y1, float x2, float y2)
{
    if (grad) return (Tvg_Result) reinterpret_cast<LinearGradient*>(grad)->linear(x1, y1, x2, y2);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_linear_gradient_get(Tvg_Gradient* grad, float* x1, float* y1, float* x2, float* y2)
{
    if (grad) return (Tvg_Result) reinterpret_cast<LinearGradient*>(grad)->linear(x1, y1, x2, y2);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_radial_gradient_set(Tvg_Gradient* grad, float cx, float cy, float r, float fx, float fy, float fr)
{
    if (grad) return (Tvg_Result) reinterpret_cast<RadialGradient*>(grad)->radial(cx, cy, r, fx, fy, fr);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_radial_gradient_get(Tvg_Gradient* grad, float* cx, float* cy, float* r, float* fx, float* fy, float* fr)
{
    if (grad) return (Tvg_Result) reinterpret_cast<RadialGradient*>(grad)->radial(cx, cy, r, fx, fy, fr);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_gradient_set_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop* color_stop, uint32_t cnt)
{
    if (grad) return (Tvg_Result) reinterpret_cast<Fill*>(grad)->colorStops(reinterpret_cast<const Fill::ColorStop*>(color_stop), cnt);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_gradient_get_color_stops(const Tvg_Gradient* grad, const Tvg_Color_Stop** color_stop, uint32_t* cnt)
{
    if (grad && color_stop && cnt) {
        *cnt = reinterpret_cast<const Fill*>(grad)->colorStops(reinterpret_cast<const Fill::ColorStop**>(color_stop));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_gradient_set_spread(Tvg_Gradient* grad, const Tvg_Stroke_Fill spread)
{
    if (grad) return (Tvg_Result) reinterpret_cast<Fill*>(grad)->spread((FillSpread)spread);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_gradient_get_spread(const Tvg_Gradient* grad, Tvg_Stroke_Fill* spread)
{
    if (grad && spread) {
        *spread = (Tvg_Stroke_Fill) reinterpret_cast<const Fill*>(grad)->spread();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_gradient_set_transform(Tvg_Gradient* grad, const Tvg_Matrix* m)
{
    if (grad && m) return (Tvg_Result) reinterpret_cast<Fill*>(grad)->transform(*(reinterpret_cast<const Matrix*>(m)));
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_gradient_get_transform(const Tvg_Gradient* grad, Tvg_Matrix* m)
{
    if (grad && m) {
        *reinterpret_cast<Matrix*>(m) = reinterpret_cast<Fill*>(const_cast<Tvg_Gradient*>(grad))->transform();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_gradient_get_type(const Tvg_Gradient* grad, Tvg_Type* type)
{
    if (grad && type) {
        *type = static_cast<Tvg_Type>(reinterpret_cast<const Fill*>(grad)->type());
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


/************************************************************************/
/* Scene API                                                            */
/************************************************************************/

TVG_API Tvg_Paint* tvg_scene_new()
{
    return (Tvg_Paint*) Scene::gen();
}


TVG_API Tvg_Result tvg_scene_push(Tvg_Paint* scene, Tvg_Paint* paint)
{
    if (scene && paint) return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push((Paint*)paint);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_scene_push_at(Tvg_Paint* scene, Tvg_Paint* paint, Tvg_Paint* at)
{
    if (scene && paint && at) return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push((Paint*)paint, (Paint*)at);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_scene_remove(Tvg_Paint* scene, Tvg_Paint* paint)
{
    if (scene) return (Tvg_Result) reinterpret_cast<Scene*>(scene)->remove((Paint*)paint);
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_scene_reset_effects(Tvg_Paint* scene)
{
    if (scene) return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push(SceneEffect::ClearAll);
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_scene_push_drop_shadow(Tvg_Paint* scene, int r, int g, int b, int a, double angle, double distance, double sigma, int quality)
{
    if (scene) {
        Tvg_Result result = (Tvg_Result) reinterpret_cast<Scene*>(scene)->push(SceneEffect::DropShadow, r, g, b, a, angle, distance, sigma, quality);
        return result;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_scene_push_gaussian_blur(Tvg_Paint* scene, float sigma, int direction, int border, int quality)
{
    if (scene) {
        return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push(SceneEffect::GaussianBlur, sigma, direction, border, quality);
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_scene_push_fill(Tvg_Paint* scene, int r, int g, int b, int a)
{
    if (scene) {
        return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push(SceneEffect::Fill, r, g, b, a);
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_scene_push_tint(Tvg_Paint* scene, int black_r, int black_g, int black_b, int white_r, int white_g, int white_b, float intensity)
{
    if (scene) {
        return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push(SceneEffect::Tint, black_r, black_g, black_b, white_r, white_g, white_b, intensity);
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}

TVG_API Tvg_Result tvg_scene_push_tritone(Tvg_Paint* scene, int shadow_r, int shadow_g, int shadow_b, int midtone_r, int midtone_g, int midtone_b, int highlight_r, int highlight_g, int highlight_b)
{
    if (scene) {
        return (Tvg_Result) reinterpret_cast<Scene*>(scene)->push(SceneEffect::Tritone, shadow_r, shadow_g, shadow_b, midtone_r, midtone_g, midtone_b, highlight_r, highlight_g, highlight_b);
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


/************************************************************************/
/* Text API                                                            */
/************************************************************************/

TVG_API Tvg_Paint* tvg_text_new()
{
    return (Tvg_Paint*)Text::gen();
}


TVG_API Tvg_Result tvg_text_set_font(Tvg_Paint* paint, const char* name, float size, const char* style)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Text*>(paint)->font(name, size, style);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_text_set_text(Tvg_Paint* paint, const char* text)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Text*>(paint)->text(text);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_text_set_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Text*>(paint)->fill(r, g, b);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_text_set_gradient(Tvg_Paint* paint, Tvg_Gradient* gradient)
{
    if (paint) return (Tvg_Result) reinterpret_cast<Text*>(paint)->fill((Fill*)(gradient));
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_font_load(const char* path)
{
    return (Tvg_Result) Text::load(path);
}


TVG_API Tvg_Result tvg_font_load_data(const char* name, const char* data, uint32_t size, const char *mimetype, bool copy)
{
    return (Tvg_Result) Text::load(name, data, size, mimetype ? mimetype : "", copy);
}


TVG_API Tvg_Result tvg_font_unload(const char* path)
{
    return (Tvg_Result) Text::unload(path);
}


/************************************************************************/
/* Saver API                                                            */
/************************************************************************/

TVG_API Tvg_Saver* tvg_saver_new()
{
    return (Tvg_Saver*) Saver::gen();
}


TVG_API Tvg_Result tvg_saver_save(Tvg_Saver* saver, Tvg_Paint* paint, const char* path, uint32_t quality)
{
    if (saver && paint && path) return (Tvg_Result) reinterpret_cast<Saver*>(saver)->save((Paint*)paint, path, quality);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_saver_sync(Tvg_Saver* saver)
{
    if (saver) return (Tvg_Result) reinterpret_cast<Saver*>(saver)->sync();
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_saver_del(Tvg_Saver* saver)
{
    if (saver) {
        delete(reinterpret_cast<Saver*>(saver));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


/************************************************************************/
/* Animation API                                                        */
/************************************************************************/

TVG_API Tvg_Animation* tvg_animation_new()
{
    return (Tvg_Animation*) Animation::gen();
}


TVG_API Tvg_Result tvg_animation_set_frame(Tvg_Animation* animation, float no)
{
    if (animation) return (Tvg_Result) reinterpret_cast<Animation*>(animation)->frame(no);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_animation_get_frame(Tvg_Animation* animation, float* no)
{
    if (animation && no) {
        *no = reinterpret_cast<Animation*>(animation)->curFrame();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_animation_get_total_frame(Tvg_Animation* animation, float* cnt)
{
    if (animation && cnt) {
        *cnt = reinterpret_cast<Animation*>(animation)->totalFrame();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Paint* tvg_animation_get_picture(Tvg_Animation* animation)
{
    if (animation) return (Tvg_Paint*) reinterpret_cast<Animation*>(animation)->picture();
    return nullptr;
}


TVG_API Tvg_Result tvg_animation_get_duration(Tvg_Animation* animation, float* duration)
{
    if (animation && duration) {
        *duration = reinterpret_cast<Animation*>(animation)->duration();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_animation_set_segment(Tvg_Animation* animation, float start, float end)
{
    if (animation) return (Tvg_Result) reinterpret_cast<Animation*>(animation)->segment(start, end);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_animation_get_segment(Tvg_Animation* animation, float* start, float* end)
{
    if (animation) return (Tvg_Result) reinterpret_cast<Animation*>(animation)->segment(start, end);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_animation_del(Tvg_Animation* animation)
{
    if (animation) {
        delete(reinterpret_cast<Animation*>(animation));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


/************************************************************************/
/* Accessor API                                                         */
/************************************************************************/

TVG_API Tvg_Accessor* tvg_accessor_new()
{
    return (Tvg_Accessor*) Accessor::gen();
}


TVG_API Tvg_Result tvg_accessor_del(Tvg_Accessor* accessor)
{
    if (accessor) {
        delete(reinterpret_cast<Accessor*>(accessor));
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API Tvg_Result tvg_accessor_set(Tvg_Accessor* accessor, Tvg_Paint* paint, bool (*func)(Tvg_Paint* paint, void* data), void* data)
{
    if (accessor) return (Tvg_Result) reinterpret_cast<Accessor*>(accessor)->set(static_cast<Picture*>(reinterpret_cast<Paint*>(paint)),
                                                [func](const Paint* paint, void* data) { return func((Tvg_Paint*) paint, data); }, data);
    return TVG_RESULT_INVALID_ARGUMENT;
}


TVG_API uint32_t tvg_accessor_generate_id(const char* name)
{
    return Accessor::id(name);
}


/************************************************************************/
/* Lottie Animation API                                                 */
/************************************************************************/

TVG_API Tvg_Animation* tvg_lottie_animation_new()
{
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    return (Tvg_Animation*) LottieAnimation::gen();
#endif
    return nullptr;
}


TVG_API Tvg_Result tvg_lottie_animation_override(Tvg_Animation* animation, const char* slot)
{
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    if (animation) return (Tvg_Result) reinterpret_cast<LottieAnimation*>(animation)->override(slot);
    return TVG_RESULT_INVALID_ARGUMENT;
#endif
    return TVG_RESULT_NOT_SUPPORTED;
}


TVG_API Tvg_Result tvg_lottie_animation_set_marker(Tvg_Animation* animation, const char* marker)
{
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    if (animation) return (Tvg_Result) reinterpret_cast<LottieAnimation*>(animation)->segment(marker);
    return TVG_RESULT_INVALID_ARGUMENT;
#endif
    return TVG_RESULT_NOT_SUPPORTED;
}


TVG_API Tvg_Result tvg_lottie_animation_get_markers_cnt(Tvg_Animation* animation, uint32_t* cnt)
{
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    if (animation && cnt) {
        *cnt = reinterpret_cast<LottieAnimation*>(animation)->markersCnt();
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
#endif
    return TVG_RESULT_NOT_SUPPORTED;
}


TVG_API Tvg_Result tvg_lottie_animation_get_marker(Tvg_Animation* animation, uint32_t idx, const char** name)
{
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    if (animation && name) {
        *name = reinterpret_cast<LottieAnimation*>(animation)->marker(idx);
        if (!(*name)) return TVG_RESULT_INVALID_ARGUMENT;
        return TVG_RESULT_SUCCESS;
    }
    return TVG_RESULT_INVALID_ARGUMENT;
#endif
    return TVG_RESULT_NOT_SUPPORTED;
}


TVG_API Tvg_Result tvg_lottie_animation_tween(Tvg_Animation* animation, float from, float to, float progress)
{
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    if (animation) return (Tvg_Result) reinterpret_cast<LottieAnimation*>(animation)->tween(from, to, progress);
    return TVG_RESULT_INVALID_ARGUMENT;
#endif
    return TVG_RESULT_NOT_SUPPORTED;
}


TVG_API Tvg_Result tvg_lottie_animation_assign(Tvg_Animation* animation, const char* layer, uint32_t ix, const char* var, float val)
{
#ifdef THORVG_LOTTIE_LOADER_SUPPORT
    if (animation) return (Tvg_Result) reinterpret_cast<LottieAnimation*>(animation)->assign(layer, ix, var, val);
    return TVG_RESULT_INVALID_ARGUMENT;
#endif
    return TVG_RESULT_NOT_SUPPORTED;
}

#ifdef __cplusplus
}
#endif
