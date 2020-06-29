#include <thorvg.h>
#include <iostream>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

unique_ptr<tvg::Paint> tvgDrawCmds()
{
    //Prepare a Shape (Rectangle + Rectangle + Circle + Circle)
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 200, 200, 0);          //x, y, w, h, cornerRadius
    shape1->appendRect(100, 100, 300, 300, 100);    //x, y, w, h, cornerRadius
    shape1->appendCircle(400, 400, 100, 100);       //cx, cy, radiusW, radiusH
    shape1->appendCircle(400, 500, 170, 100);       //cx, cy, radiusW, radiusH
    shape1->fill(255, 255, 0, 255);                 //r, g, b, a

    return move(shape1);
}

/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

void tvgSwTest(uint32_t* buffer)
{
    //Initialize ThorVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

    //Create a Canvas
    auto canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    canvas->push(tvgDrawCmds());
    canvas->draw();
    canvas->sync();

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}

/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> canvas;

void initGLview(Evas_Object *obj)
{
    static constexpr auto BPP = 4;

    //Initialize ThorVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Gl);

    //Create a Canvas
    canvas = tvg::GlCanvas::gen();
    canvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    canvas->push(tvgDrawCmds());
}

void delGLview(Evas_Object *obj)
{
    //Terminate ThorVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Gl);
}

void drawGLview(Evas_Object *obj)
{
    auto gl = elm_glview_gl_api_get(obj);
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

/************************************************************************/
/* Common Infrastructure Code                                           */
/************************************************************************/

void win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}

int main(int argc, char **argv)
{
    bool swEngine = true;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) swEngine = false;
    }

    if (swEngine) cout << "engine: software" << endl;
    else cout << "engine: opengl" << endl;

    elm_init(argc, argv);

    //Show the result using EFL...
    elm_config_accel_preference_set("gl");

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Eo* viewer;

    if (swEngine) {
        static uint32_t buffer[WIDTH * HEIGHT];
        viewer = evas_object_image_filled_add(evas_object_evas_get(win));
        evas_object_image_size_set(viewer, WIDTH, HEIGHT);
        evas_object_image_data_set(viewer, buffer);
        evas_object_size_hint_weight_set(viewer, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(viewer);
        tvgSwTest(buffer);
    //GlEngine
    } else {
        viewer = elm_glview_add(win);
        evas_object_size_hint_weight_set(viewer, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_glview_mode_set(viewer, ELM_GLVIEW_ALPHA);
        elm_glview_resize_policy_set(viewer, ELM_GLVIEW_RESIZE_POLICY_RECREATE);
        elm_glview_render_policy_set(viewer, ELM_GLVIEW_RENDER_POLICY_ON_DEMAND);
        elm_glview_init_func_set(viewer, initGLview);
        elm_glview_del_func_set(viewer, delGLview);
        elm_glview_render_func_set(viewer, drawGLview);
        evas_object_show(viewer);
    }

    elm_win_resize_object_add(win, viewer);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    elm_run();
    elm_shutdown();
}
