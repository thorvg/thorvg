#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Test for StrokeJoin & StrokeCap
    auto shape1 = tvg::Shape::gen();
    shape1->moveTo( 20, 50);
    shape1->lineTo(250, 50);
    shape1->lineTo(220, 200);
    shape1->lineTo( 70, 170);
    shape1->lineTo( 70, 30);
    shape1->stroke(255, 0, 0, 255);
    shape1->stroke(10);
    shape1->stroke(tvg::StrokeJoin::Round);
    shape1->stroke(tvg::StrokeCap::Round);
    if (canvas->push(move(shape1)) != tvg::Result::Success) return;

    auto shape2 = tvg::Shape::gen();
    shape2->moveTo(270, 50);
    shape2->lineTo(500, 50);
    shape2->lineTo(470, 200);
    shape2->lineTo(320, 170);
    shape2->lineTo(320, 30);
    shape2->stroke(255, 255, 0, 255);
    shape2->stroke(10);
    shape2->stroke(tvg::StrokeJoin::Bevel);
    shape2->stroke(tvg::StrokeCap::Square);
    if (canvas->push(move(shape2)) != tvg::Result::Success) return;

    auto shape3 = tvg::Shape::gen();
    shape3->moveTo(520, 50);
    shape3->lineTo(750, 50);
    shape3->lineTo(720, 200);
    shape3->lineTo(570, 170);
    shape3->lineTo(570, 30);
    shape3->stroke(0, 255, 0, 255);
    shape3->stroke(10);
    shape3->stroke(tvg::StrokeJoin::Miter);
    shape3->stroke(tvg::StrokeCap::Butt);
    if (canvas->push(move(shape3)) != tvg::Result::Success) return;

    //Test for Stroke Dash
    auto shape4 = tvg::Shape::gen();
    shape4->moveTo( 20, 230);
    shape4->lineTo(250, 230);
    shape4->lineTo(220, 380);
    shape4->lineTo( 70, 330);
    shape4->lineTo( 70, 210);
    shape4->stroke(255, 0, 0, 255);
    shape4->stroke(5);
    shape4->stroke(tvg::StrokeJoin::Round);
    shape4->stroke(tvg::StrokeCap::Round);

    float dashPattern1[2] = {20, 10};
    shape4->stroke(dashPattern1, 2);
    if (canvas->push(move(shape4)) != tvg::Result::Success) return;

    auto shape5 = tvg::Shape::gen();
    shape5->moveTo(270, 230);
    shape5->lineTo(500, 230);
    shape5->lineTo(470, 380);
    shape5->lineTo(320, 330);
    shape5->lineTo(320, 210);
    shape5->stroke(255, 255, 0, 255);
    shape5->stroke(5);
    shape5->stroke(tvg::StrokeJoin::Bevel);
    shape5->stroke(tvg::StrokeCap::Square);

    float dashPattern2[2] = {10, 10};
    shape5->stroke(dashPattern2, 2);
    if (canvas->push(move(shape5)) != tvg::Result::Success) return;

    auto shape6 = tvg::Shape::gen();
    shape6->moveTo(520, 230);
    shape6->lineTo(750, 230);
    shape6->lineTo(720, 380);
    shape6->lineTo(570, 330);
    shape6->lineTo(570, 210);
    shape6->stroke(0, 255, 0, 255);
    shape6->stroke(5);
    shape6->stroke(tvg::StrokeJoin::Miter);
    shape6->stroke(tvg::StrokeCap::Butt);

    float dashPattern3[6] = {10, 10, 1, 8, 1, 10};
    shape6->stroke(dashPattern3, 6);
    if (canvas->push(move(shape6)) != tvg::Result::Success) return;

    //For a comparison with shapes 10-12
    auto shape7 = tvg::Shape::gen();
    shape7->appendArc(70, 400, 160, 10, 70, true);
    shape7->stroke(255, 0, 0, 255);
    shape7->stroke(7);
    shape7->stroke(tvg::StrokeJoin::Round);
    shape7->stroke(tvg::StrokeCap::Round);
    if (canvas->push(move(shape7)) != tvg::Result::Success) return;

    auto shape8 = tvg::Shape::gen();
    shape8->appendArc(320, 400, 160, 10, 70, false);
    shape8->stroke(255, 255, 0, 255);
    shape8->stroke(7);
    shape8->stroke(tvg::StrokeJoin::Bevel);
    shape8->stroke(tvg::StrokeCap::Square);
    if (canvas->push(move(shape8)) != tvg::Result::Success) return;

    auto shape9 = tvg::Shape::gen();
    shape9->appendArc(570, 400, 160, 10, 70, true);
    shape9->stroke(0, 255, 0, 255);
    shape9->stroke(7);
    shape9->stroke(tvg::StrokeJoin::Miter);
    shape9->stroke(tvg::StrokeCap::Butt);
    if (canvas->push(move(shape9)) != tvg::Result::Success) return;

    //Test for Stroke Dash for Arc, Circle, Rect
    auto shape10 = tvg::Shape::gen();
    shape10->appendArc(70, 600, 160, 10, 30, true);
    shape10->appendCircle(70, 700, 20, 60);
    shape10->appendRect(130, 710, 100, 40, 0, 0);
    shape10->stroke(255, 0, 0, 255);
    shape10->stroke(5);
    shape10->stroke(tvg::StrokeJoin::Round);
    shape10->stroke(tvg::StrokeCap::Round);
    shape10->stroke(dashPattern1, 2);
    if (canvas->push(move(shape10)) != tvg::Result::Success) return;

    auto shape11 = tvg::Shape::gen();
    shape11->appendArc(320, 600, 160, 10, 30, false);
    shape11->appendCircle(320, 700, 20, 60);
    shape11->appendRect(380, 710, 100, 40, 0, 0);
    shape11->stroke(255, 255, 0, 255);
    shape11->stroke(5);
    shape11->stroke(tvg::StrokeJoin::Bevel);
    shape11->stroke(tvg::StrokeCap::Square);
    shape11->stroke(dashPattern2, 2);
    if (canvas->push(move(shape11)) != tvg::Result::Success) return;

    auto shape12 = tvg::Shape::gen();
    shape12->appendArc(570, 600, 160, 10, 30, true);
    shape12->appendCircle(570, 700, 20, 60);
    shape12->appendRect(630, 710, 100, 40, 0, 0);
    shape12->stroke(0, 255, 0, 255);
    shape12->stroke(5);
    shape12->stroke(tvg::StrokeJoin::Miter);
    shape12->stroke(tvg::StrokeCap::Butt);
    shape12->stroke(dashPattern3, 6);
    if (canvas->push(move(shape12)) != tvg::Result::Success) return;
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
