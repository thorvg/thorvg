#include <tizenvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH   800
#define HEIGHT  800
#define BPP     4
static Evas_GL_API *glapi;
static unique_ptr<tvg::GlCanvas> canvas;

static void
tvgtest()
{
    //Initialize TizenVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Gl);


    //Create a Canvas
    canvas = tvg::GlCanvas::gen();
    canvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

    //Prepare a Shape (Rectangle + Rectangle + Circle + Circle)
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 200, 200, 0);          //x, y, w, h, cornerRadius
    shape1->appendRect(100, 100, 300, 300, 100);    //x, y, w, h, cornerRadius
    shape1->appendCircle(400, 400, 100, 100);       //cx, cy, radiusW, radiusH
    shape1->appendCircle(400, 500, 170, 100);       //cx, cy, radiusW, radiusH
    shape1->fill(255, 255, 0, 255);                 //r, g, b, a
    shape1->stroke(255, 0, 0, 255);                 //r, g, b, a
    shape1->stroke(10.0f);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    canvas->push(move(shape1));
}

static void
init_gl(Evas_Object *obj)
{
    tvgtest();
}

static void
del_gl(Evas_Object *obj)
{
    //Terminate TizenVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Gl);
}

static void
draw_gl(Evas_Object *obj)
{
    Evas_GL_API *gl = elm_glview_gl_api_get(obj);
    int w, h;
    elm_glview_size_get(obj, &w, &h);
    gl->glViewport(0, 0, w, h);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    gl->glEnable(GL_BLEND);

    canvas->draw();
    canvas->sync();
}

void
win_del(void *data, Evas_Object *o, void *ev)
{
    elm_exit();
}

int main(int argc, char **argv)
{
    //Show the result using EFL...
    elm_init(argc, argv);

    Eo* win = elm_win_util_standard_add(nullptr, "TizenVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    // create a new glview object
    Eo* gl = elm_glview_add(win);
    glapi = elm_glview_gl_api_get(gl);
    evas_object_size_hint_align_set(gl, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_weight_set(gl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    elm_glview_mode_set(gl, ELM_GLVIEW_ALPHA);
    elm_glview_resize_policy_set(gl, ELM_GLVIEW_RESIZE_POLICY_RECREATE);
    elm_glview_render_policy_set(gl, ELM_GLVIEW_RENDER_POLICY_ON_DEMAND);

    evas_object_resize(gl, WIDTH, HEIGHT);

    // initialize callback function gets registered here
    elm_glview_init_func_set(gl, init_gl);
    // delete callback function gets registered here
    elm_glview_del_func_set(gl, del_gl);
    elm_glview_render_func_set(gl, draw_gl);

    evas_object_show(gl);

    elm_object_focus_set(gl, EINA_TRUE);

    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    elm_run();
    elm_shutdown();

    return 0;
}
