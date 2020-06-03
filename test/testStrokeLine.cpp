#include <tizenvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];

void tvgtest()
{
    //Initialize TizenVG Engine
    tvg::Engine::init();

    //Create a Canvas
    auto canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    for (int i = 0; i < 10; ++i) {
        auto shape = tvg::Shape::gen();
        shape->moveTo(50, 50 + (25 * i));
        shape->lineTo(750, 50 + (25 * i));
        shape->stroke(255, 255, 255, 255);       //color: r, g, b, a
        shape->stroke(i + 1);                    //stroke width
        shape->stroke(tvg::StrokeCap::Round);    //default is Square
        canvas->push(move(shape));
    }

    auto shape1 = tvg::Shape::gen();
    shape1->moveTo(20, 350);
    shape1->lineTo(250, 350);
    shape1->lineTo(220, 500);
    shape1->lineTo(70, 470);
    shape1->lineTo(70, 330);
    shape1->stroke(255, 0, 0, 255);
    shape1->stroke(10);
    shape1->stroke(tvg::StrokeJoin::Round);
    shape1->stroke(tvg::StrokeCap::Round);
    canvas->push(move(shape1));

    auto shape2 = tvg::Shape::gen();
    shape2->moveTo(270, 350);
    shape2->lineTo(500, 350);
    shape2->lineTo(470, 500);
    shape2->lineTo(320, 470);
    shape2->lineTo(320, 330);
    shape2->stroke(255, 255, 0, 255);
    shape2->stroke(10);
    shape2->stroke(tvg::StrokeJoin::Bevel);
    shape2->stroke(tvg::StrokeCap::Square);
    canvas->push(move(shape2));

    auto shape3 = tvg::Shape::gen();
    shape3->moveTo(520, 350);
    shape3->lineTo(750, 350);
    shape3->lineTo(720, 500);
    shape3->lineTo(570, 470);
    shape3->lineTo(570, 330);
    shape3->stroke(0, 255, 0, 255);
    shape3->stroke(10);
    shape3->stroke(tvg::StrokeJoin::Miter);
    shape3->stroke(tvg::StrokeCap::Butt);
    canvas->push(move(shape3));

    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
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
