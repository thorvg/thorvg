#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Prepare first shape, which will not be drawn
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(10, 10, 200, 200, 0, 0);
    shape1->appendRect(220, 10, 100, 100, 0, 0);

    shape1->stroke(3);
    shape1->stroke(0, 255, 0, 255);

    float dashPattern[2] = {4, 4};
    shape1->stroke(dashPattern, 2);
    shape1->fill(255, 0, 0, 255);

    //Create second shape and duplicate parameters
    auto shape2 = shape1->duplicate();

    //Create third shape, duplicate from first and add some new parameters
    auto shape3 = shape1->duplicate();

    //Get access to valid derived class type
    tvg::Shape* shapePtr = reinterpret_cast<tvg::Shape*>(shape3.get());

    //move shape3 to new postion to don't cover second shape
    shape3->translate(0, 220);

    //append new paths
    shapePtr->appendRect(340, 10, 100, 100, 0, 0);
    shapePtr->fill(0, 0, 255, 255);

    //TODO: test gradient
    canvas->push(move(shape2));
    canvas->push(move(shape3));
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
            createSwView();
        } else {
            createGlView();
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
