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

#include <thorvg.h>
#include "thorvg_capi.h"

using namespace std;
using namespace tvg;

#ifdef __cplusplus
extern "C" {
#endif

struct _Tvg_Canvas
{
    //Dummy for Direct Casting
};

struct _Tvg_Paint
{
    //Dummy for Direct Casting
};

struct _Tvg_Gradient
{
    //Dummy for Direct Casting
};


/************************************************************************/
/* Engine API                                                           */
/************************************************************************/

TVG_EXPORT Tvg_Result tvg_engine_init(unsigned engine_method, unsigned threads) {
    Result ret = Result::Success;

    if (engine_method & TVG_ENGINE_SW) ret = tvg::Initializer::init(tvg::CanvasEngine::Sw, threads);
    if (ret != Result::Success) return (Tvg_Result) ret;

    if (engine_method & TVG_ENGINE_GL) ret = tvg::Initializer::init(tvg::CanvasEngine::Gl, threads);
    return (Tvg_Result) ret;
}


TVG_EXPORT Tvg_Result tvg_engine_term(unsigned engine_method) {
    Result ret = Result::Success;

    if (engine_method & TVG_ENGINE_SW) ret = tvg::Initializer::term(tvg::CanvasEngine::Sw);
    if (ret != Result::Success) return (Tvg_Result) ret;

    if (engine_method & TVG_ENGINE_GL) ret = tvg::Initializer::term(tvg::CanvasEngine::Gl);
    return (Tvg_Result) ret;
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


TVG_EXPORT Tvg_Result tvg_canvas_clear(Tvg_Canvas* canvas)
{
    if (!canvas) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Canvas*>(canvas)->clear();
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
    delete(paint);
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


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_width(Tvg_Paint* paint, float width)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke(width);
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke(r, g, b, a);
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_dash(Tvg_Paint* paint, const float* dashPattern, uint32_t cnt)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke(dashPattern, cnt);
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_cap(Tvg_Paint* paint, Tvg_Stroke_Cap cap)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke((StrokeCap)cap);
}


TVG_EXPORT Tvg_Result tvg_shape_set_stroke_join(Tvg_Paint* paint, Tvg_Stroke_Join join)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->stroke((StrokeJoin)join);
}


TVG_EXPORT Tvg_Result tvg_shape_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill(r, g, b, a);
}


TVG_EXPORT Tvg_Result tvg_shape_linear_gradient_set(Tvg_Paint* paint, Tvg_Gradient *gradient)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill(unique_ptr<LinearGradient>((LinearGradient*)(gradient)));
}


TVG_EXPORT Tvg_Result tvg_shape_radial_gradient_set(Tvg_Paint* paint, Tvg_Gradient *gradient)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Shape*>(paint)->fill(unique_ptr<RadialGradient>((RadialGradient*)(gradient)));
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


TVG_EXPORT Tvg_Result tvg_picture_get_viewbox(Tvg_Paint* paint, float* x, float* y, float* w, float* h)
{
    if (!paint) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Picture*>(paint)->viewbox(x, y, w, h);
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
    delete(grad);
    return TVG_RESULT_SUCCESS;
}

TVG_EXPORT Tvg_Result tvg_linear_gradient_set(Tvg_Gradient* grad, float x1, float y1, float x2, float y2)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<LinearGradient*>(grad)->linear(x1, y1, x2, y2);
}

TVG_EXPORT Tvg_Result tvg_radial_gradient_set(Tvg_Gradient* grad, float cx, float cy, float radius)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<RadialGradient*>(grad)->radial(cx, cy, radius);
}

TVG_EXPORT Tvg_Result tvg_gradient_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop* color_stop, uint32_t cnt)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Fill*>(grad)->colorStops(reinterpret_cast<const Fill::ColorStop*>(color_stop), cnt);
}

TVG_EXPORT Tvg_Result tvg_gradient_spread(Tvg_Gradient* grad, const Tvg_Stroke_Fill spread)
{
    if (!grad) return TVG_RESULT_INVALID_ARGUMENT;
    return (Tvg_Result) reinterpret_cast<Fill*>(grad)->spread((FillSpread)spread);
}


#ifdef __cplusplus
}
#endif
