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

    //Initialize TizenVG Engine
    tvg::Engine::init();

    //Create a Canvas
    auto canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    //Prepare a Shape (Rectangle + Rectangle + Circle + Circle)
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 200, 200, 0);          //x, y, w, h, cornerRadius
    shape1->appendRect(100, 100, 300, 300, 100);    //x, y, w, h, cornerRadius
    shape1->appendCircle(400, 400, 100, 100);       //cx, cy, radiusW, radiusH
    shape1->appendCircle(400, 500, 170, 100);       //cx, cy, radiusW, radiusH
    shape1->fill(255, 255, 0, 255);                 //r, g, b, a

    //Stroke Style
    shape1->stroke(255, 255, 255, 255);   //color: r, g, b, a
    shape1->stroke(5);                    //width: 5px
//    shape1->strokeJoin(tvg::StrokeJoin::Miter);
//    shape1->strokeLineCap(tvg::StrokeLineCap::Butt);

//   uint32_t dash[] = {3, 1, 5, 1};        //dash pattern
//   shape1->strokeDash(dash, 4);

    canvas->push(move(shape1));

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
