/*!
* \file thorvg_capi.h
* \brief Function provides C bindings for thorvg library.
* Please refer to src/examples/Capi.cpp to find thorvg_capi examples.
*
* thorvg_capi module provides a set of functionality that allows to
* implement thorvg C client with following funcionalities
* - Drawing primitive paints: Line, Arc, Curve, Path, Shapes, Polygons
* - Filling: Solid, Linear, Radial Gradient
* - Scene Graph & Affine Transformation (translation, rotation, scale ...)
* - Stroking: Width, Join, Cap, Dash
* - Composition: Blending, Masking, Path Clipping, etc
* - Pictures: SVG, Bitmap
*/

#ifndef __THORVG_CAPI_H__
#define __THORVG_CAPI_H__

/*! Importation of librairies*/
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

/*!
* \def TVG_ENGINE_SW
* Software raster engine type.
*/
#define TVG_ENGINE_SW (1 << 1)
/*!
* \def TVG_ENGINE_GL
* GL raster engine type.
*/
#define TVG_ENGINE_GL (1 << 2)

/*!
* \def TVG_COLORSPACE_ABGR8888
* Colorspace used to fill buffer.
*/
#define TVG_COLORSPACE_ABGR8888 0
/*!
* \def TVG_COLORSPACE_ARGB8888
* Colorspace used to fill buffer.
*/
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

typedef enum {
    TVG_COMPOSITE_METHOD_NONE = 0,
    TVG_COMPOSITE_METHOD_CLIP_PATH,
    TVG_COMPOSITE_METHOD_ALPHA_MASK
} Tvg_Composite_Method;

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
/*!
* \fn Tvg_Result tvg_engine_init(unsigned engine_method, unsigned threads)
* \brief The funciton initialises thorvg library. It must be called before the
* other functions at the beggining of thorvg client.
* \code
* tvg_engine_init(TVG_ENGINE_SW, 0); //Initialize software renderer and use 1 thread
* \endcode
* \param[in] engine_method renderer type
*   - TVG_ENGINE_SW: software renderer
*   - TVG_ENGINE_GL: opengl renderer (not supported yet)
* \param[in] threads number of threads used to perform rendering. If threads = 0, only one
* thread will be used for renderer
* \return Tvg_Result return values:
*   - TVG_RESULT_SUCCESS: if ok.
*   - TVG_RESULT_INSUFFICENT_CONDITION: multiple init calls.
*   - TVG_RESULT_INVALID_ARGUMENT: not known engine_method.
*   - TVG_RESULT_NOT_SUPPORTED: not supported engine_method.
*   - TVG_RESULT_UNKOWN: internal error.
*/
TVG_EXPORT Tvg_Result tvg_engine_init(unsigned engine_method, unsigned threads);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_engine_term(unsigned engine_method)
* \brief The funciton termiates renderer tasks. Used for cleanup.
* It should be called in case of termination of the thorvg library with same
* renderer types as it was passed in tvg_engine_init()
* \code
* tvg_engine_init(TVG_ENGINE_SW, 0);
* //define canvas and shapes, update shapes, general rendering calls
* tvg_engine_term(TVG_ENGINE_SW);
* \endcode
* \param engine_method renderer type
*   - TVG_ENGINE_SW: software renderer
*   - TVG_ENGINE_GL: opengl renderer (not supported yet)
* \return Tvg_Result return values:
*   - TVG_RESULT_SUCCESS: if ok.
*   - TVG_RESULT_INSUFFICENT_CONDITION: multiple terminate calls.
*   - TVG_RESULT_INVALID_ARGUMENT: not known engine_method.
*   - TVG_RESULT_NOT_SUPPORTED: not supported engine_method.
*   - TVG_RESULT_UNKOWN: internal error.
*/
TVG_EXPORT Tvg_Result tvg_engine_term(unsigned engine_method);


/************************************************************************/
/* SwCanvas API                                                         */
/************************************************************************/
/*!
* \fn TVG_EXPORT Tvg_Canvas* tvg_swcanvas_create()
* \brief The function creates a canvas, i.e. an object used for drawing shapes,
* scenes (Tvg_Paint objects)
* \code
* Tvg_Canvas *canvas = NULL;
*
* tvg_engine_init(TVG_ENGINE_SW, 4);
* canvas = tvg_swcavnas_create();
*
* //setup canvas buffer
* uint32_t *buffer = NULL;
* buffer = (uint32_t*) malloc(sizeof(uint32_t) * 100 * 100);
* if (!buffer) return;
*
* tvg_swcanvas_set_target(canvas, buffer, 100, 100, 100, TVG_COLORSPACE_ARGB8888);
*
* //add paints to canvas and setup them before draw calls
*
* tvg_canvas_destroy(canvas);
* tvg_engine_term(TVG_ENGINE_SW);
* \endcode
* \return Tvg_Canvas pointer to the canvas object, or NULL if something went wrong
*/


TVG_EXPORT Tvg_Canvas* tvg_swcanvas_create();
/*!
* \fn TVG_EXPORT Tvg_Result tvg_swcanvas_set_target(Tvg_Canvas* canvas, uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, uint32_t cs)
* \brief The function sets memory buffer used by renderer and define colorspace used for renderering. For optimisation reasons, the library
* does not allocate memory for the output buffer on its own. Buffer is used to integrate thorvg with external libraries. Please refer to examples/Capi.cpp
* where thorvg is integrated with the EFL lib.
* \param[in] canvas The pointer to Tvg_Canvas object.
* \param[in] buffer The uint32_t pointer to allocated memory.
* \param[in] stride The buffer stride - in most cases width.
* \param[in] w The buffer width
* \param[in] h The buffer height
* \param[in] cs The buffer colorspace, defines position of color in raw pixel data.
* - TVG_COLORSPACE_ABGR8888
* - TVG_COLORSPACE_ARGB8888
* \return Tvg_Result return values:
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_MEMORY_CORRUPTION: A canvas is not valid.
* - TVG_RESULT_INVALID_ARGUMENTS: invalid buffer, stride = 0, w or h = 0
*/
TVG_EXPORT Tvg_Result tvg_swcanvas_set_target(Tvg_Canvas* canvas, uint32_t* buffer, uint32_t stride, uint32_t w, uint32_t h, uint32_t cs);


/************************************************************************/
/* Common Canvas API                                                    */
/************************************************************************/
/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_destroy(Tvg_Canvas* canvas)
* \brief The function clears canvas internal data (e.g all paints stored by canvas)
* and destroy canvas object. There is no need to call tvg_paint_del API manually for
* each paint. This function releases stored paints data. If there is a need to destroy
* paint manually in thorvg client runtime tvg_canvas_clear(canvas, false) API should be
* called, but in that case all shapes should be deleted by thorvg client.
* \code
* static Tvg_Canvas *canvas = NULL;
* static uint32_t *buffer = NULL;
*
* static void _init() {
*   canvas = tvg_swcavnas_create();
*   buffer = (uint32_t*) malloc(sizeof(uint32_t) * 100 * 100);
*   tvg_swcanvas_set_target(canvas, buffer, 100, 100, 100, TVG_COLORSPACE_ARGB8888);
* }
*
* //a task called from main function in a loop
* static void _job(const int cmd) {
*   switch (cmd) {
*     case CMD_EXIT: return 0;
*     case CMD_ADD_RECT:
*       //define valid rectangle shape
*       tvg_canvas_push(canvas, rect);
*       break;
*     case CMD_DEL_RECT:
*       tvg_shape_del(rect);
*       //now to safely delete Tvg_Canvas, tvg_canvas_clear() API have to be used.
*       break;
*     default:
*       break;
*   }
* }
*
* int main(int argc, char **argv) {
*   int cmd = 0;
*   int stop = 1;
*
*   tvg_engine_init(TVG_ENGINE_SW, 4);
*
*   while (stop) {
*      //wait for command e.g. from console
*      stop = _job(cmd);
*   }
*   tvg_canvas_clear(canvas, false);
*   tvg_canvas_destroy(canvas);
*   tvg_engine_term(TVG_ENGINE_SW);
*   return 0;
* }
*
*
* tvg_canvas_destroy(canvas);
* tvg_engine_term()
* \endcode
* \param[in] canvas Tvg_Canvas pointer
* \return Tvg_Result return values:
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_ARGUMENT: if canvas is a NULL pointer.
*/
TVG_EXPORT Tvg_Result tvg_canvas_destroy(Tvg_Canvas* canvas);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_push(Tvg_Canvas* canvas, Tvg_Paint* paint)
* \brief The function inserts paint object in the canvas.
* \param[in] canvas Tvg_Canvas pointer
* \param[in] paint Tvg_Paint pointer
* \return Tvg_Result return values:
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas or if paint are NULL pointers.
* - TVG_RESULT_MEMORY_CORRUPTION: if paint is not a valid Tvg_Paint object.
*/
TVG_EXPORT Tvg_Result tvg_canvas_push(Tvg_Canvas* canvas, Tvg_Paint* paint);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_reserve(Tvg_Canvas* canvas, uint32_t n)
* \brief The function reserves a memory for objects. It might be used to reduce
* reallocations in any cases when number of Tvg_Paints stored in canvas is known
* or it can be estimated.
* \code
* Tvg_Canvas *canvas = NULL;
*
* tvg_engine_init(TVG_ENGINE_SW, 4);
* canvas = tvg_swcavnas_create();
*
* uint32_t *buffer = NULL;
* buffer = (uint32_t*) malloc(sizeof(uint32_t) * 100 * 100);
* if (!buffer) return;
*
* tvg_swcanvas_set_target(canvas, buffer, 100, 100, 100, TVG_COLORSPACE_ARGB8888);
* tvg_canvas_reserve(canvas, 100); //reserve array for 100 paints in canvas.
*
* tvg_canvas_destroy(canvas);
* tvg_engine_term()
* \endcode
* \param[in] canvas Tvg_Canvas pointer.
* \param[in] n uint32_t reserved space.
* \return Tvg_Result return values:
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas is invalid.
*/
TVG_EXPORT Tvg_Result tvg_canvas_reserve(Tvg_Canvas* canvas, uint32_t n);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_clear(Tvg_Canvas* canvas, bool free)
* \brief The function clears a Tvg_Canvas from pushed paints. If free is set to true,
* Tvg_Paints stored in canvas also will be released. If free is set to false,
* all paints should be released manually to avoid memory leaks.
* \param canvas Tvg_Canvas pointer.
* \param free boolean, if equals true, function release all paints stored in given canvas.
* \return
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas is invalid
*/
TVG_EXPORT Tvg_Result tvg_canvas_clear(Tvg_Canvas* canvas, bool free);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_update(Tvg_Canvas* canvas)
* \brief The function update all paints in given canvas. Should be called before draw to
* prepare shapes for rendering.
* \code
* //A frame drawing example. Thread safety and events implementation is skipped to show only thorvg code.
*
* static Tvg_Canvas *canvas = NULL;
* static Tvg_Paint *rect = NULL;
*
* int _frame_render(void) {
*   tvg_canvas_update(canvas);
*   tvg_canvas_draw(canvas);
*   tvg_canvas_sync(canvas);
* }
*
* //event handler from your code or third party library
* void _event_handler(event *event_data) {
*   if (!event_data) return NULL;
*     switch(event_data.type) {
*       case EVENT_RECT_ADD:
*         if (!rect) {
*           tvg_shape_append_rect(shape, 10, 10, 50, 50, 0, 0);
*           tvg_shape_set_stroke_width(shape, 1.0f);
*           tvg_shape_set_stroke_color(shape, 255, 0, 0, 255);
*           tvg_canvas_push(canvas, shape);
*         }
*         break;
*       case EVENT_RECT_MOVE:
*         if (rect) tvg_paint_translate(rect, 10.0, 10.0);
*           break;
*         default:
*           break;
*   }
* }
*
* int main(int argc, char **argv) {
*   //example handler from your code or third party lib
*   event_handler_add(handler, _event_handler);
*
*   //create frame rendering process which calls _frame_render() function.
*   app_loop_begin(_frame_render);
*   app_loop_finish();
*   cleanup();
* }
* \endcode
* \param[in] canvas Tvg_Canvas pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas is invalid]
*/
TVG_EXPORT Tvg_Result tvg_canvas_update(Tvg_Canvas* canvas);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_update_paint(Tvg_Canvas* canvas, Tvg_Paint* paint)
* \brief The funciton updates shape before rendering. If a client application using the
* thorvg library does not update the entire canvas (tvg_canvas_update()) in the frame
* rendering process, Tvg_Paints previosly added to the canvas should be updated manually
* using this function.
* \param[in] canvas Tvg_Canvas pointer
* \param[in] paint Tvg_Paint pointer to update
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas is invalid
*/
TVG_EXPORT Tvg_Result tvg_canvas_update_paint(Tvg_Canvas* canvas, Tvg_Paint* paint);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_draw(Tvg_Canvas* canvas)
* \brief The function start rendering process. All shapes from the given canvas will be rasterized
* to the buffer.
* \param[in] canvas
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INSUFFICIENT_CONDITION: interna rendering errors.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas is invalid.
*/
TVG_EXPORT Tvg_Result tvg_canvas_draw(Tvg_Canvas* canvas);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_canvas_sync(Tvg_Canvas* canvas)
* \brief The function finalize rendering process. It should be called after tvg_canvas_draw(canvas).
* \param[in] canvas
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas is invalids.
*/
TVG_EXPORT Tvg_Result tvg_canvas_sync(Tvg_Canvas* canvas);

/************************************************************************/
/* Paint API                                                            */
/************************************************************************/
/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_del(Tvg_Paint* paint)
* \brief The function releases given paint object. The tvg_canvas_clear(canvas, true) or tvg_canvas_del(canvas)
* releases previously pushed paints internally. If this function is used, tvg_canvas_clear(canvas, false) should
* be used to avoid unexpected behaviours.
* \code
* //example of cleanup function
* Tvg_Paint *rect = NULL; //rectangle shape added in other function
*
* //rectangle delete API
* int rectangle_delete(void) {
*   if (rect) tvg_paint_del(rect);
*   rect = NULL;
* }
*
* int cleanup(void) {
*   tvg_canvas_clear(canvas, false);
*   tvg_canvas_del(canvas);
*   canvas = NULL;
* }
* \endcode
* \param[in] paint Tvg_Paint pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_del(Tvg_Paint* paint);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_scale(Tvg_Paint* paint, float factor)
* \brief The function scales given paint using given factor.
* \param[in] paint Tvg_Paint pointer
* \param[in] factor double scale factor
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INSUFFICENT_CONDITION: if invalid factor is used.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_scale(Tvg_Paint* paint, float factor);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_rotate(Tvg_Paint* paint, float degree)
* \brief The function rotates the givent paint by the given degree.
* \param[in] paint Tvg_Paint pointer
* \param[in] degree double rotation degree
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INSUFFICENT_CONDITION: if invalid degree is used.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid.
*/
TVG_EXPORT Tvg_Result tvg_paint_rotate(Tvg_Paint* paint, float degree);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_translate(Tvg_Paint* paint, float x, float y)
* \brief The function moves the given paint in the X,Y coordinate system.
* \param[in] paint Tvg_Paint pointer
* \param[in] x float x shift
* \param[in] y float y shift
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_translate(Tvg_Paint* paint, float x, float y);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_transform(Tvg_Paint* paint, const Tvg_Matrix* m)
* \brief The function transforms given paint using transformation matrix. It could
* be used to move, rotate in 2d coordinates system and rotate in 3d coordinates system
* \param[in] paint Tvg_Paint pointer
* \param[in] m Tvg_Matrix pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint or m is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_transform(Tvg_Paint* paint, const Tvg_Matrix* m);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_set_opacity(Tvg_Paint* paint, uint8_t opacity)
* \brief The function sets opacity of paint. It could be used in Tvg_Scene to implement
* translucent layers of shapes.
* \param[in] paint Tvg_Paint pointer
* \param[in] opacity uint8_t opacity value
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_set_opacity(Tvg_Paint* paint, uint8_t opacity);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_get_opacity(Tvg_Paint* paint, uint8_t* opacity)
* \brief The function gets opacity of given paint
* \param[in] paint Tvg_Paint pointer
* \param[out] opacity uint8_t pointer to store opacity
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if canvas is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_get_opacity(Tvg_Paint* paint, uint8_t* opacity);


/*!
* \fn TVG_EXPORT Tvg_Paint* tvg_paint_duplicate(Tvg_Paint* paint)
* \brief The function duplicates given paint. Returns newly allocated paint with the
* same properties (stroke, color, path). It could be used to duplicate scenes too.
* \param[in] paint Tvg_Paint pointer
* \return Tvg_Paint pointer to duplicatede paint
*/
TVG_EXPORT Tvg_Paint* tvg_paint_duplicate(Tvg_Paint* paint);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_get_bounds(const Tvg_Paint* paint, float* x, float* y, float* w, float* h)
* \brief The function returns paint path bounds.
* \param[in] paint Tvg_Paint pointer
* \param[out] x float x position
* \param[out] y float y position
* \param[out] w float paint width
* \param[out] h float paint height
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_get_bounds(const Tvg_Paint* paint, float* x, float* y, float* w, float* h);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_paint_set_composite_method(Tvg_Paint* paint, Tvg_Paint* target, Tvg_Composite_Method method)
* \brief The function set composition method
* \param[in] paint Tvg_Paint pointer
* \param[in] target Tvg_Paint pointer to composition object
* \param[in] method Tvg_Composite_Method used composite method
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_paint_set_composite_method(Tvg_Paint* paint, Tvg_Paint* target, Tvg_Composite_Method method);

/************************************************************************/
/* Shape API                                                            */
/************************************************************************/
/*!
* \fn TVG_EXPORT Tvg_Paint* tvg_shape_new()
* \brief The function creates a new shape
* \return Tvg_Paint pointer or NULL if something went wrong
*/
TVG_EXPORT Tvg_Paint* tvg_shape_new();


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_reset(Tvg_Paint* paint)
* \brief The function resets shape properties like path, stroke properties, fill color
* \param[in] paint Tvg_Paint pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_reset(Tvg_Paint* paint);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_move_to(Tvg_Paint* paint, float x, float y)
* \brief The funciton moves the current point to the given point.
* \param[in] paint Tvg_paint pointer
* \param[in] x move x coordinate
* \param[in] y move y coordinate
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_move_to(Tvg_Paint* paint, float x, float y);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_line_to(Tvg_Paint* paint, float x, float y)
* \brief The funciton adds straight line from the current point to the given point.
* \param[in] paint Tvg_Paint pointer
* \param[in] x line end x position
* \param[in] y line end y position
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_line_to(Tvg_Paint* paint, float x, float y);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_cubic_to(Tvg_Paint* paint, float cx1, float cy1, float cx2, float cy2, float x, float y)
* \brief The function adds a cubic Bezier curve between the current point and given end point (x, y) specified by control
points (cx1, cy1) and (cx2, cy2).
* \param[in] paint Tvg_Paint pointer
* \param[in] cx1 First control point x coordinate
* \param[in] cy1 First control point y coordinate
* \param[in] cx2 Second control point x coordinate
* \param[in] cy2 Second control point y coordinate
* \param[in] x Line end x coordinate
* \param[in] y Line end y coordinate
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_cubic_to(Tvg_Paint* paint, float cx1, float cy1, float cx2, float cy2, float x, float y);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_close(Tvg_Paint* paint)
* \brief The function closes current path by drawing line to the start position of it.
* \param[in] paint
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_close(Tvg_Paint* paint);
/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_append_rect(Tvg_Paint* paint, float x, float y, float w, float h, float rx, float ry)
* \brief The function appends rectangle in start position specified by x and y parameters, size spiecified by w and h parameters
* and rounded rectangles specified by rx, ry parameteres
* \param[in] paint Tvg_Paint pointer
* \param[in] x start x position
* \param[in] y start y position
* \param[in] w rectangle width
* \param[in] h rectangle height
* \param[in] rx rectangle corner x radius
* \param[in] ry rectangle corner y radius
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_append_rect(Tvg_Paint* paint, float x, float y, float w, float h, float rx, float ry);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_append_circle(Tvg_Paint* paint, float cx, float cy, float rx, float ry)
* \brief The function appends circle with center in (cx, cy) point, x radius (rx) and y radius (ry)
* \param[in] paint Tvg_Paint pointer
* \param[in] cx circle x center
* \param[in] cy circle y center
* \param[in] rx circle x radius
* \param[in] ry circle y radius
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_append_circle(Tvg_Paint* paint, float cx, float cy, float rx, float ry);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_append_arc(Tvg_Paint* paint, float cx, float cy, float radius, float startAngle, float sweep, uint8_t pie)
* \brief The function append an arc to the shape with center in (cx, cy) position. Arc radius is set by radius parameter. Start position of an
* arcus is defined by startAngle variable. Arcus lenght is set by sweep parameter. To draw closed arcus pie parameter should be set to true.
* \param[in] paint Tvg_Paint pointer
* \param[in] cx arcus x center
* \param[in] cy arcus y center
* \param[in] radius radius of an arc
* \param[in] startAngle start angle of an arc in degrees
* \param[in] sweep lenght of an arc in degrees
* \param[in] pie arcus close parameter
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_append_arc(Tvg_Paint* paint, float cx, float cy, float radius, float startAngle, float sweep, uint8_t pie);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_append_path(Tvg_Paint* paint, const Tvg_Path_Command* cmds, uint32_t cmdCnt, const Tvg_Point* pts, uint32_t ptsCnt)
* \brief The function append path specified by path commands and path points
* \param[in] paint Tvg_Paint pointer
* \param[in] cmds array of path commands
* \param[in] cmdCnt lenght of commands array
* \param[in] pts array of command points
* \param[in] ptsCnt lenght of command points array
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_append_path(Tvg_Paint* paint, const Tvg_Path_Command* cmds, uint32_t cmdCnt, const Tvg_Point* pts, uint32_t ptsCnt);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_path_coords(const Tvg_Paint* paint, const Tvg_Point** pts, uint32_t* cnt)
* \brief The function get path coordinates to the Tvg_Point array. Array length is specified by cnt output parameter.
* The function does not allocate any data, it operates on internal memory. There is no need to free pts array.
* \code
* Tvg_Shape *shape = tvg_shape_new();
* Tvg_Point *coords = NULL;
* uint32_t len = 0;
*
* tvg_shape_append_circle(shape, 10, 10, 50, 50);
* tvg_shape_get_path_coords(shape, (const Tvg_Point**)&coords, &coords_len);
* //thorvg aproximates circle by four Bezier lines. In example above cmds array will store their coordinates
* \endcode
* \param[in] paint Tvg_Paint pointer
* \param[out] pts points output array
* \param[out] cnt points array length
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_path_coords(const Tvg_Paint* paint, const Tvg_Point** pts, uint32_t* cnt);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_path_commands(const Tvg_Paint* paint, const Tvg_Path_Command** cmds, uint32_t* cnt)
* \brief The function gets path commands to commands array. Array length is specified by cnt output parameter.
* The function does not allocate any data. There is no need to cmds array.
* \code
* Tvg_Shape *shape = tvg_shape_new();
* Tvg_Point *coords = NULL;
* uint32_t len = 0;
*
* tvg_shape_append_circle(shape, 10, 10, 50, 50);
* tvg_shape_get_path_commands(shape, (const Tvg_Path_Command**)&cmds, &len);
* //thorvg aproximates circle by four Bezier lines. In example above cmds array will store their coordinates
* \endcode
* \param[in] paint Tvg_Paint pointer
* \param[out] cmds commands output array
* \param[out] cnt commands array length
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_path_commands(const Tvg_Paint* paint, const Tvg_Path_Command** cmds, uint32_t* cnt);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_stroke_width(Tvg_Paint* paint, float width)
* \brief The function sets shape's stroke width.
* \param[in] paint Tvg_Paint pointer
* \param[in] width stroke width parameter
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_width(Tvg_Paint* paint, float width);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_stroke_width(const Tvg_Paint* paint, float* width)
* \brief The function gets shape's stroke width
* \param[in] paint Tvg_Paint pointer
* \param[out] width stroke width
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_width(const Tvg_Paint* paint, float* width);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_stroke_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
* \brief The function sets shape's stroke color.
* \param[in] paint Tvg_Paint pointer
* \param[in] r red value
* \param[in] g green value
* \param[in] b blue value
* \param[in] a opacity value
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_stroke_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
* \brief The function gets shape's stroke color
* \param[in] paint Tvg_Paint pointer
* \param[out] r red value
* \param[out] g green value
* \param[out] b blue value
* \param[out] a opacity value
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_stroke_dash(Tvg_Paint* paint, const float* dashPattern, uint32_t cnt)
* \brief The function sets shape's stroke dash mode. Dash pattern is an array of floats with size which have to be
* divisible by 2. Position with index not divisible by 2 defines length of line. Positions divisible by 2 defines
* length of gap
* \code
* //dash pattern examples
* float dashPattern[2] = {20, 10};  // -- - -- - -- -
* float dashPattern[2] = {40, 20};  // ----  ----  ----
* float dashPattern[4] = {10, 20, 30, 40} // -  ---
* \endcode
* \param[in] paint Tvg_Paint pointer
* \param[in] dashPattern array of floats descibing pattern [(line, gap)]
* \param[in] cnt size of an array
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_dash(Tvg_Paint* paint, const float* dashPattern, uint32_t cnt);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_stroke_dash(const Tvg_Paint* paint, const float** dashPattern, uint32_t* cnt)
* \brief The function returns shape's stroke dash pattern and its size.
* \see tvg_shape_set_stroke_dash
* \param[in] paint Tvg_Paint pointer
* \param[out] dashPattern array of floats describing pattern
* \param[out] cnt size of an array
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_dash(const Tvg_Paint* paint, const float** dashPattern, uint32_t* cnt);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_stroke_cap(Tvg_Paint* paint, Tvg_Stroke_Cap cap)
* \brief The function sets the stroke capabilities style to be used for stroking the path.
* \see Tvg_Stroke_Cap
* \see tvg_shape_get_stroke_cap
* \param[in] paint Tvg_Paint pointer
* \param[in] cap stroke capabilities
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_cap(Tvg_Paint* paint, Tvg_Stroke_Cap cap);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_stroke_cap(const Tvg_Paint* paint, Tvg_Stroke_Cap* cap)
* \brief The function gets the stroke capabilities.
* \see Tvg_Stroke_Cap
* \see tvg_shape_set_stroke_cap
* \param[in] paint Tvg_Paint pointer
* \param[out] cap stroke capabilities
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_cap(const Tvg_Paint* paint, Tvg_Stroke_Cap* cap);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_stroke_join(Tvg_Paint* paint, Tvg_Stroke_Join join)
* \brief The function sets the stroke join method.
* \see Tvg_Stroke_Join
* \see tvg_shape_set_stroke_cap
* \param[in] paint Tvg_Paint pointer
* \param[in] join join method
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_stroke_join(Tvg_Paint* paint, Tvg_Stroke_Join join);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_stroke_join(const Tvg_Paint* paint, Tvg_Stroke_Join* join)
* \brief The function gets the stroke join method
* \param[in] paint Tvg_Paint pointer
* \param[out] join join method
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_stroke_join(const Tvg_Paint* paint, Tvg_Stroke_Join* join);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
* \brief The function sets shape's fill color.
* \see tvg_shape_get_fill_color
* \param[in] paint Tvg_Paint pointer
* \param[in] r red value
* \param[in] g green value
* \param[in] b blue value
* \param[in] a alpha value
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_fill_color(Tvg_Paint* paint, uint8_t r, uint8_t g, uint8_t b, uint8_t a);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_fill_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
* \brief The function gets shape's fill color
* \see tvg_shape_set_fill_color
* \param[in] paint Tvg_Paint pointer
* \param[out] r red value
* \param[out] g green value
* \param[out] b blue value
* \param[out] a alpha value
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_fill_color(const Tvg_Paint* paint, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_fill_rule(Tvg_Paint* paint, Tvg_Fill_Rule rule)
* \brief The function sets shape's fill rule.  TVG_FILL_RULE_WINDING is used as default fill rule
* \see \link Wiki https://en.wikipedia.org/wiki/Nonzero-rule \endlink
* \param[in] paint Tvg_Paint pointer
* \param[in] rule fill rule
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_fill_rule(Tvg_Paint* paint, Tvg_Fill_Rule rule);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_fill_rule(const Tvg_Paint* paint, Tvg_Fill_Rule* rule)
* \brief The function gets shape's fill rule.
* \see tvg_shape_get_fill_rule
* \param[in] paint Tvg_Paint pointer
* \param[out] rule shape's fill rule
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_fill_rule(const Tvg_Paint* paint, Tvg_Fill_Rule* rule);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_linear_gradient(Tvg_Paint* paint, Tvg_Gradient* grad)
* \brief The function inserts linear gradient object as an shape fill.
* \code
* Tvg_Gradient* grad = tvg_linear_gradient_new();
* tvg_linear_gradient_set(grad, 700, 700, 800, 800);
* tvg_shape_set_linear_gradient(shape, grad);
* \endcode
* \param[in] paint Tvg_Paint pointer
* \param[in] grad Tvg_Gradient pointer (linear)
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_linear_gradient(Tvg_Paint* paint, Tvg_Gradient* grad);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_set_radial_gradient(Tvg_Paint* paint, Tvg_Gradient* grad)
* \brief The function inserts radial gradient object as an shape fill.
* \code
* Tvg_Gradient* grad = tvg_radial_gradient_new();
* tvg_radial_gradient_set(grad, 550, 550, 50));
* tvg_shape_set_radial_gradient(shape, grad);
* \endcode
* \param[in] paint Tvg_Paint pointer
* \param[in] grad Tvg_Gradient pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_set_radial_gradient(Tvg_Paint* paint, Tvg_Gradient* grad);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_shape_get_gradient(const Tvg_Paint* paint, Tvg_Gradient** grad)
* \brief The function returns gradient previously inserted to given shape. Function deos not
* allocate any data.
* \param[in] paint Tvg_Paint pointer
* \param[out] grad Tvg_Gradient pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_shape_get_gradient(const Tvg_Paint* paint, Tvg_Gradient** grad);

/************************************************************************/
/* Gradient API                                                         */
/************************************************************************/
/*!
* \fn TVG_EXPORT Tvg_Gradient* tvg_linear_gradient_new()
* \brief The function creates new linear gradient object.
* \code
* Tvg_Paint shape = tvg_shape_new();
* tvg_shape_append_rect(shape, 700, 700, 100, 100, 20, 20);
* Tvg_Gradient* grad = tvg_linear_gradient_new();
* tvg_linear_gradient_set(grad, 700, 700, 800, 800);
* Tvg_Color_Stop color_stops[2] =
* {
*   {.offset=0, .r=0, .g=0, .b=0,   .a=255},
*   {.offset=1, .r=0, .g=255, .b=0, .a=255},
* };
* tvg_gradient_set_color_stops(grad, color_stops, 2);
* tvg_shape_set_linear_gradient(shape, grad);
* \endcode
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Gradient* tvg_linear_gradient_new();


/*!
* \fn TVG_EXPORT Tvg_Gradient* tvg_radial_gradient_new()
* \brief The function creates new gradient object.
* \code
* Tvg_Paint shape = tvg_shape_new();
* tvg_shape_append_rect(shape, 700, 700, 100, 100, 20, 20);
* Tvg_Gradient* grad = tvg_radial_gradient_new();
* tvg_linear_gradient_set(grad, 550, 550, 50);
* Tvg_Color_Stop color_stops[2] =
* {
*   {.offset=0, .r=0, .g=0, .b=0,   .a=255},
*   {.offset=1, .r=0, .g=255, .b=0, .a=255},
* };
* tvg_gradient_set_color_stops(grad, color_stops, 2);
* tvg_shape_set_radial_gradient(shape, grad);
* \endcode
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Gradient* tvg_radial_gradient_new();


/*!
* \fn TVG_EXPORT Tvg_Result tvg_linear_gradient_set(Tvg_Gradient* grad, float x1, float y1, float x2, float y2)
* \brief The function sets start (x1, y1) and end point (x2, y2) of the gradient.
* \param[in] grad Tvg_Gradient pointer
* \param[in] x1 start point x coordinate
* \param[in] y1 start point y coordinate
* \param[in] x2 end point x coordinate
* \param[in] y2 end point y coordinate
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_linear_gradient_set(Tvg_Gradient* grad, float x1, float y1, float x2, float y2);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_linear_gradient_get(Tvg_Gradient* grad, float* x1, float* y1, float* x2, float* y2)
* \brief The function gets linear gradient start and end postion.
* \param[in] grad Tvg_Gradient pointer
* \param[out] x1 start point x coordinate
* \param[out] y1 start point y coordinate
* \param[out] x2 end point x coordinate
* \param[out] y2 end point y coordinate
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_linear_gradient_get(Tvg_Gradient* grad, float* x1, float* y1, float* x2, float* y2);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_radial_gradient_set(Tvg_Gradient* grad, float cx, float cy, float radius)
* \brief The function sets radial gradient center and radius
* \param[in] grad Tvg_Gradient pointer
* \param[in] cx radial gradient center x coordinate
* \param[in] cy radial gradient center y coordinate
* \param[in] radius radial gradient radius value
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_radial_gradient_set(Tvg_Gradient* grad, float cx, float cy, float radius);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_radial_gradient_get(Tvg_Gradient* grad, float* cx, float* cy, float* radius)
* \brief The function gets radial gradient center point ant radius
* \param[in] grad Tvg_Gradient pointer
* \param[out] cx gradient center x coordinate
* \param[out] cy gradient center y coordinate
* \param[out] radius gradient radius value
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_radial_gradient_get(Tvg_Gradient* grad, float* cx, float* cy, float* radius);

/*!
* \fn TVG_EXPORT Tvg_Result tvg_gradient_set_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop* color_stop, uint32_t cnt)
* \brief The function sets lists of color stops for the given gradient
* \see tvg_linear_gradient_new
* \param[in] grad Tvg_Gradient pointer
* \param[in] color_stop color stops list
* \param[in] cnt color stops size
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_gradient_set_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop* color_stop, uint32_t cnt);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_gradient_get_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop** color_stop, uint32_t* cnt)
* \brief The function gets lists of color stops for the given gradient
* \param[in] grad Tvg_Gradient pointer
* \param[out] color_stop color stops list
* \param[out] cnt color stops list size
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_gradient_get_color_stops(Tvg_Gradient* grad, const Tvg_Color_Stop** color_stop, uint32_t* cnt);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_gradient_set_spread(Tvg_Gradient* grad, const Tvg_Stroke_Fill spread)
* \brief The function sets spread fill method for given gradient
* \param[in] grad Tvg_Gradient pointer
* \param[in] spread spread method
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_gradient_set_spread(Tvg_Gradient* grad, const Tvg_Stroke_Fill spread);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_gradient_get_spread(Tvg_Gradient* grad, Tvg_Stroke_Fill* spread)
* \brief The function gets spread fill method for given gradient
* \param[in] grad Tvg_Gradient pointer
* \param[out] spread spread method
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_gradient_get_spread(Tvg_Gradient* grad, Tvg_Stroke_Fill* spread);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_gradient_del(Tvg_Gradient* grad)
* \brief The function deletes given gradient object
* \param[in] grad Tvg_Gradient pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_gradient_del(Tvg_Gradient* grad);

/************************************************************************/
/* Picture API                                                          */
/************************************************************************/
/*!
* \fn TVG_EXPORT Tvg_Paint* tvg_picture_new()
* \brief The function creates new picture object.
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Paint* tvg_picture_new();


/*!
* \fn TVG_EXPORT Tvg_Result tvg_picture_load(Tvg_Paint* paint, const char* path)
* \brief The function loads image into given paint object
* \param[in] paint Tvg_Paint pointer
* \param[in] path absolute path to the image file
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_picture_load(Tvg_Paint* paint, const char* path);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_picture_load_raw(Tvg_Paint* paint, uint32_t *data, uint32_t w, uint32_t h, bool copy)
* \brief The function loads raw image data into given paint object.
* \param[in] paint Tvg_Paint pointer
* \param[in] data raw data pointer
* \param[in] w picture width
* \param[in] h picture height
* \param[in] copy if copy is set to true function copies data into the paint
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_picture_load_raw(Tvg_Paint* paint, uint32_t *data, uint32_t w, uint32_t h, bool copy);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_picture_get_viewbox(const Tvg_Paint* paint, float* x, float* y, float* w, float* h)
* \brief The function returns viewbox coordinates and size for given paint
* \param[in] paint Tvg_Paint pointer
* \param[out] x left top corner x coordinate
* \param[out] y left top corner y coordinate
* \param[out] w viewbox width
* \param[out] h viewbox height
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_picture_get_viewbox(const Tvg_Paint* paint, float* x, float* y, float* w, float* h);

/************************************************************************/
/* Scene API                                                            */
/************************************************************************/
/*!
* \fn TVG_EXPORT Tvg_Paint* tvg_scene_new()
* \brief The function creates new scene object. Scene object is used to group paints
* into one object which can be manipulated using Tvg_Paint API.
* \return Tvg_Paint pointer to newly allocated scene or NULL if something went wrong
*/
TVG_EXPORT Tvg_Paint* tvg_scene_new();


/*!
* \fn TVG_EXPORT Tvg_Result tvg_scene_reserve(Tvg_Paint* scene, uint32_t size)
* \brief The function reserves a space in given space for specific number of paints
* \see tvg_canvas_reserve
* \param[in] scene Tvg_Paint pointer
* \param[in] size size to allocate
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_scene_reserve(Tvg_Paint* scene, uint32_t size);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_scene_push(Tvg_Paint* scene, Tvg_Paint* paint)
* \brief The function inserts given paint in the specified scene.
* \see tvg_canvas_push
* \param[in] scene Tvg_Paint pointer (scene)
* \param[in] paint Tvg_Paint pointer (paint)
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint or scene is invalid
*/
TVG_EXPORT Tvg_Result tvg_scene_push(Tvg_Paint* scene, Tvg_Paint* paint);


/*!
* \fn TVG_EXPORT Tvg_Result tvg_scene_clear(Tvg_Paint* scene)
* \brief The function claers paints inserted in scene
* \see tvg_canvas_clear
* \param scene Tvg_Paint pointer
* \return Tvg_Result return value
* - TVG_RESULT_SUCCESS: if ok.
* - TVG_RESULT_INVALID_PARAMETERS: if paint is invalid
*/
TVG_EXPORT Tvg_Result tvg_scene_clear(Tvg_Paint* scene);


#ifdef __cplusplus
}
#endif

#endif //_THORVG_CAPI_H_
