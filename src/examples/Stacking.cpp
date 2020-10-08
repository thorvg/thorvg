#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

static tvg::Shape* paints[3];
static int order = 0;

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    canvas->reserve(3);                          //reserve 3 shape nodes (optional)

    //Prepare Round Rectangle
    auto shape1 = tvg::Shape::gen();
    paints[0] = shape1.get();
    shape1->appendRect(0, 0, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape1->fill(0, 255, 0, 255);                //r, g, b, a
    if (canvas->push(move(shape1)) != tvg::Result::Success) return;

    //Prepare Circle
    auto shape2 = tvg::Shape::gen();
    paints[1] = shape2.get();
    shape2->appendRect(100, 100, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape2->fill(255, 255, 0, 255);              //r, g, b, a
    if (canvas->push(move(shape2)) != tvg::Result::Success) return;

    //Prepare Ellipse
    auto shape3 = tvg::Shape::gen();
    paints[2] = shape3.get();
    shape3->appendRect(200, 200, 400, 400, 50, 50);  //x, y, w, h, rx, ry
    shape3->fill(0, 255, 255, 255);              //r, g, b, a
    if (canvas->push(move(shape3)) != tvg::Result::Success) return;
}


void tvgUpdateCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Explicitly clear all retained paint nodes from canvas but not free them.
    if (canvas->clear(false) != tvg::Result::Success) return;

    switch (order) {
        case 0:
            canvas->push(unique_ptr<tvg::Shape>(paints[0]));
            canvas->push(unique_ptr<tvg::Shape>(paints[1]));
            canvas->push(unique_ptr<tvg::Shape>(paints[2]));
            break;
        case 1:
            canvas->push(unique_ptr<tvg::Shape>(paints[1]));
            canvas->push(unique_ptr<tvg::Shape>(paints[2]));
            canvas->push(unique_ptr<tvg::Shape>(paints[0]));
            break;
        case 2:
            canvas->push(unique_ptr<tvg::Shape>(paints[2]));
            canvas->push(unique_ptr<tvg::Shape>(paints[0]));
            canvas->push(unique_ptr<tvg::Shape>(paints[1]));
            break;
    }

    ++order;

    if (order > 2) order = 0;
}



/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);

    /* Push the shape into the Canvas drawing list
       When this shape is into the canvas list, the shape could update & prepare
       internal data asynchronously for coming rendering.
       Canvas keeps this shape node unless user call canvas->clear() */
    tvgDrawCmds(swCanvas.get());
}

void drawSwView(void* data, Eo* obj)
{
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }
}


Eina_Bool timerSwCb(void *data)
{
    tvgUpdateCmds(swCanvas.get());

    Eo* img = (Eo*) data;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);

    return ECORE_CALLBACK_RENEW;
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
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
    }
}


Eina_Bool timerGlCb(void *data)
{
    tvgUpdateCmds(glCanvas.get());

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

    //Threads Count
    auto threads = std::thread::hardware_concurrency();

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {

        elm_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView();
            ecore_timer_add(0.5, timerSwCb, view);
        } else {
            auto view = createGlView();
            ecore_timer_add(0.5, timerGlCb, view);
        }


        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvgEngine);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
