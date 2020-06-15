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

    //Shape 1
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(50, 50, 200, 200, 0);
    shape1->fill(50, 50, 50, 255);
    shape1->stroke(255, 255, 255, 255);       //color: r, g, b, a
    shape1->stroke(tvg::StrokeJoin::Bevel);   //default is Bevel
    shape1->stroke(10);                       //width: 10px

    canvas->push(move(shape1));

    //Shape 2
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(300, 50, 200, 200, 0);
    shape2->fill(50, 50, 50, 255);
    shape2->stroke(255, 255, 255, 255);
    shape2->stroke(tvg::StrokeJoin::Round);
    shape2->stroke(10);

    canvas->push(move(shape2));

    //Shape 3
    auto shape3 = tvg::Shape::gen();
    shape3->appendRect(550, 50, 200, 200, 0);
    shape3->fill(50, 50, 50, 255);
    shape3->stroke(255, 255, 255, 255);
    shape3->stroke(tvg::StrokeJoin::Miter);
    shape3->stroke(10);

    canvas->push(move(shape3));

    //Shape 4
    auto shape4 = tvg::Shape::gen();
    shape4->appendCircle(150, 450, 100, 100);
    shape4->fill(50, 50, 50, 255);
    shape4->stroke(255, 255, 255, 255);
    shape4->stroke(1);

    canvas->push(move(shape4));

    //Shape 5
    auto shape5 = tvg::Shape::gen();
    shape5->appendCircle(400, 450, 100, 100);
    shape5->fill(50, 50, 50, 255);
    shape5->stroke(255, 255, 255, 255);
    shape5->stroke(2);

    canvas->push(move(shape5));

    //Shape 6
    auto shape6 = tvg::Shape::gen();
    shape6->appendCircle(650, 450, 100, 100);
    shape6->fill(50, 50, 50, 255);
    shape6->stroke(255, 255, 255, 255);
    shape6->stroke(4);

    canvas->push(move(shape6));


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
