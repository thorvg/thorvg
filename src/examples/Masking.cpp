#include "Common.h"
#include <fstream>

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

uint32_t *data = nullptr;

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT, 0, 0);
    shape->fill(255, 255, 255, 255);

    if (canvas->push(move(shape)) != tvg::Result::Success) return;

    string path(EXAMPLE_DIR"/rawimage_200x300.raw");

    ifstream file(path);
    if (!file.is_open()) return ;
    data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
    file.read(reinterpret_cast<char *>(data), sizeof (data) * 200 * 300);
    file.close();

    auto picture = tvg::Picture::gen();
    if (picture->load(data, 200, 300, true) != tvg::Result::Success) return;
    picture->translate(400, 250);

    //Alpha mask
    auto mask = tvg::Shape::gen();
    mask->appendCircle(500, 350, 75, 75);
    mask->fill(0, 0, 0, 50);
    if (canvas->push(move(picture)) != tvg::Result::Success) return;

    auto cover = tvg::Shape::gen();
    cover->appendRect(400, 250, 200, 300, 0, 0);
    cover->fill(255, 255, 0, 255);
    cover->composite(move(mask), tvg::CompositeMethod::AlphaMask);
    if (canvas->push(move(cover)) != tvg::Result::Success) return;

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
