#include "Common.h"
#include <fstream>

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

uint32_t *data = nullptr;

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Solid Rectangle
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, 400, 400, 0, 0);
    shape->fill(0, 0, 255, 255);

    //Mask
    auto mask = tvg::Shape::gen();
    mask->appendCircle(200, 200, 125, 125);
    mask->fill(255, 0, 0, 255);
    shape->composite(move(mask), tvg::CompositeMethod::AlphaMask);
    canvas->push(move(shape));

    //SVG
    auto svg = tvg::Picture::gen();
    if (svg->load(EXAMPLE_DIR"/cartman.svg") != tvg::Result::Success) return;
    svg->scale(3);
    svg->translate(50, 400);

    //Mask2
    auto mask2 = tvg::Shape::gen();
    mask2->appendCircle(150, 500, 75, 75);
    mask2->appendRect(150, 500, 200, 200, 30, 30);
    mask2->fill(255, 255, 255, 255);
    svg->composite(move(mask2), tvg::CompositeMethod::AlphaMask);
    canvas->push(move(svg));

    //Star

#if 0
    //Appends Paths
    shape->moveTo(199, 34);
    shape->lineTo(253, 143);
    shape->lineTo(374, 160);
    shape->lineTo(287, 244);
    shape->lineTo(307, 365);
    shape->lineTo(199, 309);
    shape->lineTo(97, 365);
    shape->lineTo(112, 245);
    shape->lineTo(26, 161);
    shape->lineTo(146, 143);
    shape->close();
#endif

    //shape->opacity(127);

//    scene->push(move(shape));

#if 0
    scene->composite(move(mask2), tvg::CompositeMethod::AlphaMask);
    if (canvas->push(move(scene)) != tvg::Result::Success) return;


    ifstream file(EXAMPLE_DIR"/rawimage_200x300.raw");
    if (!file.is_open()) return;
    data = (uint32_t*) malloc(sizeof(uint32_t) * (200 * 300));
    file.read(reinterpret_cast<char *>(data), sizeof (data) * 200 * 300);
    file.close();

    //Mask Target 1
    auto picture = tvg::Picture::gen();
    if (picture->load(data, 200, 300, true) != tvg::Result::Success) return;
    picture->translate(500, 400);

    //Alpha Mask
    auto mask = tvg::Shape::gen();
    mask->appendCircle(500, 500, 125, 125);
    mask->fill(255, 255, 255, 100);

    picture->composite(move(mask), tvg::CompositeMethod::AlphaMask);
    if (canvas->push(move(picture)) != tvg::Result::Success) return;
#endif
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
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

        if (data) free(data);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
