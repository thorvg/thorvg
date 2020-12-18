#include <vector>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

#define NUM_PER_LINE 4
#define SIZE 200

static int count = 0;

static std::vector<unique_ptr<tvg::Picture>> pictures;

void svgDirCallback(const char* name, const char* path, void* data)
{
    //ignore if not svgs.
    const char *ext = name + strlen(name) - 3;
    if (strcmp(ext, "svg")) return;

    auto picture = tvg::Picture::gen();

    char buf[PATH_MAX];
    sprintf(buf, "/%s/%s", path, name);

    if (picture->load(buf) != tvg::Result::Success) return;

    picture->size(SIZE, SIZE);
    picture->translate((count % NUM_PER_LINE) * SIZE, SIZE * (count / NUM_PER_LINE));

    pictures.push_back(move(picture));

    cout << "SVG: " << buf << endl;

    count++;
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT, 0, 0);    //x, y, w, h, rx, ry
    shape->fill(255, 255, 255, 255);                 //r, g, b, a

    if (canvas->push(move(shape)) != tvg::Result::Success) return;

    eina_file_dir_list(EXAMPLE_DIR, EINA_TRUE, svgDirCallback, canvas);

    /* This showcase shows you asynchrounous loading of svg.
       For this, pushing pictures at a certian sync time.
       This means it earns the time to finish loading svg resources,
       otherwise you can push pictures immediately. */
    for (auto& paint : pictures) {
        canvas->push(move(paint));
    }

    pictures.clear();
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

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
