#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawStar(tvg::Shape* star)
{
    star->moveTo(199, 34);
    star->lineTo(253, 143);
    star->lineTo(374, 160);
    star->lineTo(287, 244);
    star->lineTo(307, 365);
    star->lineTo(199, 309);
    star->lineTo(97, 365);
    star->lineTo(112, 245);
    star->lineTo(26, 161);
    star->lineTo(146, 143);
    star->close();
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;
    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT, 0, 0);
    shape->fill(255, 255, 255, 255);
    if (canvas->push(move(shape)) != tvg::Result::Success) return;

    //////////////////////////////////////////////
    auto scene = tvg::Scene::gen();
    scene->reserve(2);

    auto star1 = tvg::Shape::gen();
    tvgDrawStar(star1.get());
    star1->fill(255, 255, 0, 255);
    star1->stroke(255 ,0, 0, 128);
    star1->stroke(10);

    //Move Star1
    star1->translate(-10, -10);

    auto clipStar = tvg::Shape::gen();
    clipStar->appendCircle(200, 230, 110, 110);
    clipStar->fill(255, 255, 255, 255); // clip object must have alpha.
    clipStar->translate(10, 10);

    star1->composite(move(clipStar), tvg::CompMethod::ClipPath);

    auto star2 = tvg::Shape::gen();
    tvgDrawStar(star2.get());
    star2->fill(0, 255, 255, 64);
    star2->stroke(0 ,255, 0, 128);
    star2->stroke(10);

    //Move Star2
    star2->translate(10, 40);

    auto clip = tvg::Shape::gen();
    clip->appendCircle(200, 230, 130, 130);
    clip->fill(255, 255, 255, 255); // clip object must have alpha.
    clip->translate(10, 10);

    scene->push(move(star1));
    scene->push(move(star2));

    //Clipping scene to shape
    scene->composite(move(clip), tvg::CompMethod::ClipPath);

    canvas->push(move(scene));

    //////////////////////////////////////////////
    auto star3 = tvg::Shape::gen();
    tvgDrawStar(star3.get());
    star3->translate(400, 0);
    star3->fill(255, 255, 0, 255);                    //r, g, b, a
    star3->stroke(255 ,0, 0, 128);
    star3->stroke(10);

    auto clipRect = tvg::Shape::gen();
    clipRect->appendRect(480, 110, 200, 200, 0, 0);          //x, y, w, h, rx, ry
    clipRect->fill(255, 255, 255, 255); // clip object must have alpha.
    clipRect->translate(20, 20);

    //Clipping scene to rect(shape)
    star3->composite(move(clipRect), tvg::CompMethod::ClipPath);

    canvas->push(move(star3));



    //////////////////////////////////////////////
    auto picture = tvg::Picture::gen();
    char buf[PATH_MAX];
    sprintf(buf,"%s/cartman.svg", EXAMPLE_DIR);

    if (picture->load(buf) != tvg::Result::Success) return;

    picture->scale(3);
    picture->translate(200, 400);

    auto clipPath = tvg::Shape::gen();
    clipPath->appendCircle(350, 510, 110, 110);          //x, y, w, h, rx, ry
    clipPath->appendCircle(350, 650, 50, 50);          //x, y, w, h, rx, ry
    clipPath->fill(255, 255, 255, 255); // clip object must have alpha.
    clipPath->translate(20, 20);

    //Clipping picture to path
    picture->composite(move(clipPath), tvg::CompMethod::ClipPath);

    canvas->push(move(picture));


    //canvas->push(move(shape2));
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

unique_ptr<tvg::SwCanvas> swCanvas;

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
