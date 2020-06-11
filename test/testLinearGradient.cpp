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
    canvas->reserve(3);                          //reserve 3 shape nodes (optional)

    //Prepare Round Rectangle
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 400, 400, 0);      //x, y, w, h, cornerRadius

    //LinearGradient
    auto fill = tvg::LinearGradient::gen();
    fill->linear(0, 0, 400, 400);

    //Linear Gradient Color Stops
    tvg::Fill::ColorStop colorStops[2];
    colorStops[0] = {0, 0, 0, 0, 255};
    colorStops[1] = {1, 255, 255, 255, 255};

    fill->colorStops(colorStops, 2);

    shape1->fill(move(fill));
    canvas->push(move(shape1));

    //Prepare Circle
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(400, 400, 200, 200);    //cx, cy, radiusW, radiusH

    //LinearGradient
    auto fill2 = tvg::LinearGradient::gen();
    fill2->linear(400, 200, 400, 600);

    //Linear Gradient Color Stops
    tvg::Fill::ColorStop colorStops2[3];
    colorStops2[0] = {0, 255, 0, 0, 255};
    colorStops2[1] = {0.5, 255, 255, 0, 255};
    colorStops2[2] = {1, 255, 255, 255, 255};

    fill2->colorStops(colorStops2, 3);

    shape2->fill(move(fill2));
    canvas->push(move(shape2));


    //Prepare Ellipse
    auto shape3 = tvg::Shape::gen();
    shape3->appendCircle(600, 600, 150, 100);    //cx, cy, radiusW, radiusH

    //LinearGradient
    auto fill3 = tvg::LinearGradient::gen();
    fill3->linear(450, 600, 750, 600);

    //Linear Gradient Color Stops
    tvg::Fill::ColorStop colorStops3[4];
    colorStops3[0] = {0, 0, 127, 0, 127};
    colorStops3[1] = {0.25, 0, 170, 170, 170};
    colorStops3[2] = {0.5, 200, 0, 200, 200};
    colorStops3[3] = {1, 255, 255, 255, 255};

    fill3->colorStops(colorStops3, 4);

    shape3->fill(move(fill3));
    canvas->push(move(shape3));

    //Draw the Shapes onto the Canvas
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
