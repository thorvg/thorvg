#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    //Shape
    auto shape = tvg::Shape::gen();
    shape->appendRect(-100, -100, 200, 200, 0);
    shape->fill(255, 255, 255, 255);
    canvas->push(move(shape));
}

void tvgUpdateCmds(tvg::Canvas* canvas, float progress)
{
    //Explicitly clear all retained paint nodes.
    canvas->clear();

    //Shape
    auto shape = tvg::Shape::gen();
    shape->appendRect(-100, -100, 200, 200, (100 * progress));
    shape->fill(rand()%255, rand()%255, rand()%255, 255);
    shape->translate(800 * progress, 800 * progress);
    shape->scale(1 - 0.75 * progress);
    shape->rotate(360 * progress);

    canvas->push(move(shape));
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(swCanvas.get());
}

void transitSwCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    tvgUpdateCmds(swCanvas.get(), progress);

    //Update Efl Canvas
    Eo* img = (Eo*) effect;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
}

void drawSwView(void* data, Eo* obj)
{
    swCanvas->draw();
    swCanvas->sync();
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(Evas_Object *obj)
{
    static constexpr auto BPP = 4;

    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();
    glCanvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(glCanvas.get());
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

    glCanvas->draw();
    glCanvas->sync();
}

void transitGlCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    tvgUpdateCmds(glCanvas.get(), progress);
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Initialize ThorVG Engine
    if (tvgEngine == tvg::CanvasEngine::Sw) {
        cout << "tvg engine: software" << endl;
    } else {
        cout << "tvg engine: opengl" << endl;
    }

    //Initialize ThorVG Engine
    tvg::Initializer::init(tvgEngine);

    elm_init(argc, argv);

    elm_config_accel_preference_set("gl");

    Elm_Transit *transit = elm_transit_add();

    if (tvgEngine == tvg::CanvasEngine::Sw) {
        auto view = createSwView();
        elm_transit_effect_add(transit, transitSwCb, view, nullptr);
    } else {
        auto view = createGlView();
        elm_transit_effect_add(transit, transitGlCb, view, nullptr);
    }

    elm_transit_duration_set(transit, 2);
    elm_transit_repeat_times_set(transit, -1);
    elm_transit_auto_reverse_set(transit, EINA_TRUE);
    elm_transit_go(transit);

    elm_run();
    elm_shutdown();

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvgEngine);
}