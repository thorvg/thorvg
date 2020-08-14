#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

static const char* svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" stroke-linejoin=\"round\" viewBox=\"50 -100 500 500\"><path fill=\"none\" stroke=\"black\" stroke-width=\"10\" d=\"M 212,220 C 197,171 156,153 123,221 109,157 120,109  159,63.6 190,114  234,115  254,89.8 260,82.3 268,69.6 270,60.3 273,66.5 275,71.6 280,75.6 286,79.5 294,79.8 300,79.8 306,79.8 314,79.5 320,75.6 325,71.6 327,66.5 330,60.3 332,69.6 340,82.3 346,89.8 366,115  410,114  441,63.6 480,109  491,157 477,221 444,153 403,171 388,220 366,188 316,200 300,248 284,200 234,188 212,220 Z\"/></svg>";


void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT, 0, 0);    //x, y, w, h, rx, ry
    shape->fill(255, 255, 255, 255);                 //r, g, b, a

    if (canvas->push(move(shape)) != tvg::Result::Success) return;

    auto picture = tvg::Picture::gen();
    if (picture->load(svg, strlen(svg)) != tvg::Result::Success) return;

    float x, y, w, h;
    picture->viewbox(&x, &y, &w, &h);

    float rate = (WIDTH/(w > h ? w : h));
    picture->scale(rate);

    x *= rate;
    y *= rate;
    w *= rate;
    h *= rate;

    //Center Align ?
    if (w > h) {
         y -= (WIDTH - h) * 0.5f;
    } else {
         x -= (WIDTH - w) * 0.5f;
    }

    picture->translate(-x, -y);

    canvas->push(move(picture));
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

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine) == tvg::Result::Success) {

        elm_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            createSwView();
        } else {
            createGlView();
        }

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
