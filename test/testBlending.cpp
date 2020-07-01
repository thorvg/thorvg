#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    canvas->reserve(5);

    //Prepare Round Rectangle
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 400, 400, 50);      //x, y, w, h, cornerRadius
    shape1->fill(0, 255, 0, 255);                //r, g, b, a
    if (canvas->push(move(shape1)) != tvg::Result::Success) return;

    //Prepare Circle
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(400, 400, 200, 200);    //cx, cy, radiusW, radiusH
    shape2->fill(170, 170, 0, 170);              //r, g, b, a
    if (canvas->push(move(shape2)) != tvg::Result::Success) return;

    //Prepare Ellipse
    auto shape3 = tvg::Shape::gen();
    shape3->appendCircle(400, 400, 250, 100);    //cx, cy, radiusW, radiusH
    shape3->fill(100, 100, 100, 100);            //r, g, b, a
    if (canvas->push(move(shape3)) != tvg::Result::Success) return;

    //Prepare Star
    auto shape4 = tvg::Shape::gen();
    shape4->moveTo(199, 234);
    shape4->lineTo(253, 343);
    shape4->lineTo(374, 360);
    shape4->lineTo(287, 444);
    shape4->lineTo(307, 565);
    shape4->lineTo(199, 509);
    shape4->lineTo(97, 565);
    shape4->lineTo(112, 445);
    shape4->lineTo(26, 361);
    shape4->lineTo(146, 343);
    shape4->close();
    shape4->fill(200, 0, 200, 200);
    if (canvas->push(move(shape4)) != tvg::Result::Success) return;

    //Prepare Opaque Ellipse
    auto shape5 = tvg::Shape::gen();
    shape5->appendCircle(600, 650, 200, 150);
    shape5->fill(0, 0, 255, 255);
    if (canvas->push(move(shape5)) != tvg::Result::Success) return;
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