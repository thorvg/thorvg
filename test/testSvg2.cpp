#include <tizenvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];

void tvgtest()
{
    //Initialize TizenVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

    //Create a Canvas
    auto canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    //Create a SVG scene, keep aspect ratio to width/2 with lazy loading
    auto scene = tvg::Scene::gen();
    scene->load("sample.svg", WIDTH/2, 0, true);
    canvas->push(move(scene));

    //Create a SVG scene, keep aspect ratio to height/2 with lazy loading
    auto scene2 = tvg::Scene::gen();
    scene2->load("sample.svg", 0, HEIGHT/2, true);
    scene2->translate(WIDTH/2, HEIGHT/2);
    canvas->push(move(scene2));

    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}

void
win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}


int main(int argc, char **argv)
{
    tvgtest();

    //Show the result using EFL...
    elm_init(argc, argv);

    Eo* win = elm_win_util_standard_add(NULL, "TizenVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Eo* img = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(img, WIDTH, HEIGHT);
    evas_object_image_data_set(img, buffer);
    evas_object_size_hint_weight_set(img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(img);

    elm_win_resize_object_add(win, img);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    elm_run();
    elm_shutdown();
}
