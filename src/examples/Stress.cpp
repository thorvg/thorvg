#include <vector>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

#define NUM_PER_LINE 16
#define SIZE 50

static bool rendered = false;
static int count = 0;
static int frame = 0;
static std::vector<tvg::Picture*> pictures;
static double t1, t2, t3, t4;

void svgDirCallback(const char* name, const char* path, void* data)
{
    auto picture = tvg::Picture::gen();

    char buf[PATH_MAX];
    sprintf(buf, "/%s/%s", path, name);

    if (picture->load(buf) != tvg::Result::Success) return;

    float x, y, w, h;
    picture->viewbox(&x, &y, &w, &h);

    float rate = (SIZE/(w > h ? w : h));
    picture->scale(rate);

    x *= rate;
    y *= rate;
    w *= rate;
    h *= rate;

    //Center Align ?
    if (w > h) {
         y -= (SIZE - h) * 0.5f;
    } else {
         x -= (SIZE - w) * 0.5f;
    }

    picture->translate((count % NUM_PER_LINE) * SIZE - x, SIZE * (count / NUM_PER_LINE) - y);
    ++count;

    //Duplicates
    for (int i = 0; i < NUM_PER_LINE - 1; i++) {
        tvg::Picture* dup = static_cast<tvg::Picture*>(picture->duplicate());
        dup->translate((count % NUM_PER_LINE) * SIZE - x, SIZE * (count / NUM_PER_LINE) - y);
        pictures.push_back(dup);
        ++count;
    }

    cout << "SVG: " << buf << endl;
    pictures.push_back(picture.release());
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
    for (auto picture : pictures) {
        canvas->push(unique_ptr<tvg::Picture>(picture));
    }
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
    t3 = ecore_time_get();

    //Drawing task can be performed asynchronously.
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }

    t4 = ecore_time_get();
    printf("[%5d]: total[%fs] update[%fs], render[%fs]\n", ++frame, t4 - t1, t2 - t1, t4 - t3);

    rendered = true;
}

void transitSwCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    if (!rendered) return;

    t1 = ecore_time_get();

    for (auto picture : pictures) {
        picture->rotate(progress * 360);
        swCanvas->update(picture);
    }

    t2 = ecore_time_get();

    //Update Efl Canvas
    auto img = (Eo*) effect;
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);

    rendered = false;
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

void transitGlCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    for (auto picture : pictures) {
        picture->rotate(progress * 360);
        glCanvas->update(picture);
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

        Elm_Transit *transit = elm_transit_add();

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView();
            elm_transit_effect_add(transit, transitSwCb, view, nullptr);
        } else {
            auto view = createGlView();
            elm_transit_effect_add(transit, transitGlCb, view, nullptr);
        }

        elm_transit_duration_set(transit, 2);
        elm_transit_repeat_times_set(transit, -1);
        elm_transit_go(transit);

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
