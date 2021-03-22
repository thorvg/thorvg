#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/
tvg::Shape* pShape = nullptr;

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    tvg::Fill::ColorStop colorStops1[3];
    colorStops1[0] = {0, 255, 0, 0, 150};
    colorStops1[1] = {0.5, 0, 0, 255, 150};
    colorStops1[2] = {1, 127, 0, 127, 150};

    tvg::Fill::ColorStop colorStops2[2];
    colorStops2[0] = {0.3, 255, 0, 0, 255};
    colorStops2[1] = {1, 50, 0, 255, 155};

    tvg::Fill::ColorStop colorStops3[2];
    colorStops3[0] = {0, 0, 0, 255, 155};
    colorStops3[1] = {1, 0, 255, 0, 155};

    float dashPattern1[2] = {15, 15};

    // linear gradient stroke + linear gradient fill
    auto shape1 = tvg::Shape::gen();
    shape1->moveTo(150, 100);
    shape1->lineTo(200, 100);
    shape1->lineTo(200, 150);
    shape1->lineTo(300, 150);
    shape1->lineTo(250, 200);
    shape1->lineTo(200, 200);
    shape1->lineTo(200, 250);
    shape1->lineTo(150, 300);
    shape1->lineTo(150, 200);
    shape1->lineTo(100, 200);
    shape1->lineTo(100, 150);
    shape1->close();

    shape1->stroke(0, 255, 0, 255);
    shape1->stroke(20);
    shape1->stroke(tvg::StrokeJoin::Miter);
    shape1->stroke(tvg::StrokeCap::Butt);

    auto fillStroke1 = tvg::LinearGradient::gen();
    fillStroke1->linear(100, 100, 250, 250);
    fillStroke1->colorStops(colorStops1, 3);
    shape1->stroke(move(fillStroke1));

    auto fill1 = tvg::LinearGradient::gen();
    fill1->linear(100, 100, 250, 250);
    fill1->colorStops(colorStops1, 3);
    shape1->fill(move(fill1));

    if (canvas->push(move(shape1)) != tvg::Result::Success) return;

    // radial gradient stroke + duplicate 
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(600, 175, 100, 60);
    shape2->stroke(80);

    auto fillStroke2 = tvg::RadialGradient::gen();
    fillStroke2->radial(600, 175, 100);
    fillStroke2->colorStops(colorStops2, 2);
    shape2->stroke(move(fillStroke2));

    auto shape3 = unique_ptr<tvg::Shape>(static_cast<tvg::Shape*>(shape2->duplicate()));
    shape3->translate(0, 200);

    auto fillStroke3 = tvg::LinearGradient::gen();
    fillStroke3->linear(500, 115, 700, 235);
    fillStroke3->colorStops(colorStops3, 2);
    shape3->stroke(move(fillStroke3));

    auto shape4 = unique_ptr<tvg::Shape>(static_cast<tvg::Shape*>(shape2->duplicate()));
    shape4->translate(0, 400);

    if (canvas->push(move(shape2)) != tvg::Result::Success) return;
    if (canvas->push(move(shape3)) != tvg::Result::Success) return;
    if (canvas->push(move(shape4)) != tvg::Result::Success) return;

    // dashed gradient stroke
    auto shape5 = tvg::Shape::gen();
    shape5->appendRect(100, 500, 300, 300, 50, 80);

    shape5->stroke(20);
    shape5->stroke(dashPattern1, 2);
    shape5->stroke(tvg::StrokeCap::Butt);
    auto fillStroke5 = tvg::LinearGradient::gen();
    fillStroke5->linear(150, 450, 450, 750);
    fillStroke5->colorStops(colorStops3, 2);
    shape5->stroke(move(fillStroke5));

    auto fill5 = tvg::LinearGradient::gen();
    fill5->linear(150, 450, 450, 750);
    fill5->colorStops(colorStops3, 2);
    shape5->fill(move(fill5));
    pShape = shape5.get();

    if (canvas->push(move(shape5)) != tvg::Result::Success) return;
    pShape->scale(0.8);
    if (canvas->update(pShape) != tvg::Result::Success) return;
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
