#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/
tvg::Shape* pShape = nullptr;

void tvgDrawCmds(tvg::Canvas* canvas)
{
    //Shape1
    auto shape = tvg::Shape::gen();

    /* Acquire shape pointer to access it again.
       instead, you should consider not to interrupt this pointer life-cycle. */
    pShape = shape.get();

    shape->moveTo(0, -114.5);
    shape->lineTo(54, -5.5);
    shape->lineTo(175, 11.5);
    shape->lineTo(88, 95.5);
    shape->lineTo(108, 216.5);
    shape->lineTo(0, 160.5);
    shape->lineTo(-102, 216.5);
    shape->lineTo(-87, 96.5);
    shape->lineTo(-173, 12.5);
    shape->lineTo(-53, -5.5);
    shape->close();
    shape->fill(0, 0, 255, 255);
    shape->stroke(3);
    shape->stroke(255, 255, 255, 255);
    if (canvas->push(move(shape)) != tvg::Result::Success) return;
}

void tvgUpdateCmds(tvg::Canvas* canvas, float progress)
{
    /* Update shape directly.
       You can update only necessary properties of this shape,
       while retaining other properties. */

    //Transform Matrix
    tvg::Matrix m = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    //scale x
    m.e11 = 1 - (progress * 0.5f);

    //scale y
    m.e22 = 1 + (progress * 2.0f);

    //rotation
    constexpr auto PI = 3.141592f;
    auto degree = 45.0f;
    auto radian = degree / 180.0f * PI;
    auto cosVal = cosf(radian);
    auto sinVal = sinf(radian);

    auto t11 = m.e11 * cosVal + m.e12 * sinVal;
    auto t12 = m.e11 * -sinVal + m.e12 * cosVal;
    auto t21 = m.e21 * cosVal + m.e22 * sinVal;
    auto t22 = m.e21 * -sinVal + m.e22 * cosVal;
    auto t31 = m.e31 * cosVal + m.e32 * sinVal;
    auto t32 = m.e31 * -sinVal + m.e32 * cosVal;

    m.e11 = t11;
    m.e12 = t12;
    m.e21 = t21;
    m.e22 = t22;
    m.e31 = t31;
    m.e32 = t32;

    //translate
    m.e31 = progress * 300.0f + 300.0f;
    m.e32 = progress * -100.0f + 300.0f;

    pShape->transform(m);

    //Update shape for drawing (this may work asynchronously)
    canvas->update(pShape);
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
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }
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

    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
    }
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
