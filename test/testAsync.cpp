#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/
#define COUNT 50

static double t1, t2, t3, t4;
static unsigned cnt = 0;

bool tvgUpdateCmds(tvg::Canvas* canvas)
{
    auto t = ecore_time_get();

    //Explicitly clear all retained paint nodes.
    if (canvas->clear() != tvg::Result::Success) {
        //Logically wrong! Probably, you missed to call sync() before.
        return false;
    }

    t1 = t;
    t2 = ecore_time_get();

    for (int i = 0; i < COUNT; i++) {
        auto shape = tvg::Shape::gen();

        float x = rand() % (WIDTH/2);
        float y = rand() % (HEIGHT/2);
        float w = 1 + rand() % (int)(WIDTH * 1.3 / 2);
        float h = 1 + rand() %  (int)(HEIGHT * 1.3 / 2);

        shape->appendRect(x, y, w, h, rand() % 400);

        //LinearGradient
        auto fill = tvg::LinearGradient::gen();
        fill->linear(x, y, x + w, y + h);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops[3];
        colorStops[0] = {0, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};
        colorStops[1] = {1, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};
        colorStops[2] = {2, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};

        fill->colorStops(colorStops, 3);
        shape->fill(move(fill));

        if (canvas->push(move(shape)) != tvg::Result::Success) {
            //Did you call clear()? Make it sure if canvas is on rendering
            break;
        }
    }

    t3 = ecore_time_get();

    return true;
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
}

Eina_Bool animSwCb(void* data)
{
    if (!tvgUpdateCmds(swCanvas.get())) return ECORE_CALLBACK_RENEW;

    //Drawing task can be performed asynchronously.
    if (swCanvas->draw() != tvg::Result::Success) return false;

    //Update Efl Canvas
    Eo* img = (Eo*) data;
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);

    return ECORE_CALLBACK_RENEW;
}

void drawSwView(void* data, Eo* obj)
{
    //Make it guarantee finishing drawing task.
    swCanvas->sync();

    t4 = ecore_time_get();

    printf("[%5d]: total[%fms] = clear[%fms], update[%fms], render[%fms]\n", ++cnt, t4 - t1, t2 - t1, t3 - t2, t4 - t3);
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

    glCanvas->sync();
}

Eina_Bool animGlCb(void* data)
{
    if (!tvgUpdateCmds(glCanvas.get())) return ECORE_CALLBACK_RENEW;

    //Drawing task can be performed asynchronously.
    glCanvas->draw();

    return ECORE_CALLBACK_RENEW;
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

    if (tvgEngine == tvg::CanvasEngine::Sw) {
        auto view = createSwView();
        evas_object_image_pixels_get_callback_set(view, drawSwView, nullptr);
        ecore_animator_add(animSwCb, view);
    } else {
        auto view = createGlView();
        ecore_animator_add(animGlCb, view);
    }

    elm_run();
    elm_shutdown();

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvgEngine);
}
