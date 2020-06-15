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
    canvas->reserve(5);             //reserve 5 shape nodes (optional)

    //Prepare Shape1
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(-100, -100, 1000, 1000, 50);
    shape1->fill(255, 255, 255, 255);
    canvas->push(move(shape1));

    //Prepare Shape2
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(-100, -100, 250, 250, 50);
    shape2->fill(0, 0, 255, 255);
    canvas->push(move(shape2));

    //Prepare Shape3
    auto shape3 = tvg::Shape::gen();
    shape3->appendRect(500, 500, 550, 550, 0);
    shape3->fill(0, 255, 255, 255);
    canvas->push(move(shape3));

    //Prepare Shape4
    auto shape4 = tvg::Shape::gen();
    shape4->appendCircle(800, 100, 200, 200);
    shape4->fill(255, 255, 0, 255);
    canvas->push(move(shape4));

    //Prepare Shape5
    auto shape5 = tvg::Shape::gen();
    shape5->appendCircle(200, 650, 250, 200);
    shape5->fill(0, 0, 0, 255);
    canvas->push(move(shape5));

    //Draw the Shapes onto the Canvas
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
