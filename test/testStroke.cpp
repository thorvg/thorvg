#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    //Shape 1
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(50, 50, 200, 200, 0);
    shape1->fill(50, 50, 50, 255);
    shape1->stroke(255, 255, 255, 255);       //color: r, g, b, a
    shape1->stroke(tvg::StrokeJoin::Bevel);   //default is Bevel
    shape1->stroke(10);                       //width: 10px

    if (canvas->push(move(shape1)) != tvg::Result::Success) return;

    //Shape 2
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(300, 50, 200, 200, 0);
    shape2->fill(50, 50, 50, 255);
    shape2->stroke(255, 255, 255, 255);
    shape2->stroke(tvg::StrokeJoin::Round);
    shape2->stroke(10);

    if (canvas->push(move(shape2)) != tvg::Result::Success) return;

    //Shape 3
    auto shape3 = tvg::Shape::gen();
    shape3->appendRect(550, 50, 200, 200, 0);
    shape3->fill(50, 50, 50, 255);
    shape3->stroke(255, 255, 255, 255);
    shape3->stroke(tvg::StrokeJoin::Miter);
    shape3->stroke(10);

    if (canvas->push(move(shape3)) != tvg::Result::Success) return;

    //Shape 4
    auto shape4 = tvg::Shape::gen();
    shape4->appendCircle(150, 450, 100, 100);
    shape4->fill(50, 50, 50, 255);
    shape4->stroke(255, 255, 255, 255);
    shape4->stroke(1);

    if (canvas->push(move(shape4)) != tvg::Result::Success) return;

    //Shape 5
    auto shape5 = tvg::Shape::gen();
    shape5->appendCircle(400, 450, 100, 100);
    shape5->fill(50, 50, 50, 255);
    shape5->stroke(255, 255, 255, 255);
    shape5->stroke(2);

    if (canvas->push(move(shape5)) != tvg::Result::Success) return;

    //Shape 6
    auto shape6 = tvg::Shape::gen();
    shape6->appendCircle(650, 450, 100, 100);
    shape6->fill(50, 50, 50, 255);
    shape6->stroke(255, 255, 255, 255);
    shape6->stroke(4);

    if (canvas->push(move(shape6)) != tvg::Result::Success) return;
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
        createSwView();
    } else {
        createGlView();
    }

    elm_run();
    elm_shutdown();

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvgEngine);
}