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
    auto canvas = tvg::SwCanvas::gen(buffer, WIDTH, HEIGHT);
    //canvas->reserve(2);      //reserve 2 shape nodes (optional)

    //Prepare Rectangle
    auto shape1 = tvg::ShapeNode::gen();
    shape1->appendRect(0, 0, 400, 400, 0);       //x, y, w, h, corner_radius
    shape1->fill(0, 255, 0, 255);                //r, g, b, a
    canvas->push(move(shape1));

    //Prepare Circle
    auto shape2 = tvg::ShapeNode::gen();
    shape2->appendCircle(400, 400, 200);         //cx, cy, radius
    shape2->fill(255, 255, 0, 255);              //r, g, b, a
    canvas->push(move(shape2));

    //Draw the Shapes onto the Canvas
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}

int main(int argc, char **argv)
{
    tvgtest();

    //Show the result using EFL...
    elm_init(argc, argv);

    Eo* win = elm_win_util_standard_add(NULL, "TizenVG Test");

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
