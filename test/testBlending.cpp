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
    canvas->reserve(5);

    //Prepare Round Rectangle
    auto shape1 = tvg::ShapeNode::gen();
    shape1->appendRect(0, 0, 400, 400, 50);      //x, y, w, h, cornerRadius
    shape1->fill(0, 255, 0, 255);                //r, g, b, a
    canvas->push(move(shape1));

    //Prepare Circle
    auto shape2 = tvg::ShapeNode::gen();
    shape2->appendCircle(400, 400, 200, 200);    //cx, cy, radiusW, radiusH
    shape2->fill(170, 170, 0, 170);              //r, g, b, a
    canvas->push(move(shape2));

    //Prepare Ellipse
    auto shape3 = tvg::ShapeNode::gen();
    shape3->appendCircle(400, 400, 250, 100);    //cx, cy, radiusW, radiusH
    shape3->fill(100, 100, 100, 100);            //r, g, b, a
    canvas->push(move(shape3));

    //Prepare Star
    auto shape4 = tvg::ShapeNode::gen();
    shape4->moveTo(199, 234);
    shape4->lineTo(253, 343);
    shape4->lineTo(374, 360);
    shape4->lineTo(287, 444);
    shape4->lineTo(307, 565);
    shape4->lineTo(199, 509);
    shape4->lineTo(97, 565);
    shape4->lineTo(112, 445);
    shape4->lineTo(26, 361);
    shape4->lineTo(146, 343);
    shape4->close();
    shape4->fill(200, 0, 200, 200);
    canvas->push(move(shape4));

    //Prepare Opaque Ellipse
    auto shape5 = tvg::ShapeNode::gen();
    shape5->appendCircle(600, 650, 200, 150);
    shape5->fill(0, 0, 255, 255);
    canvas->push(move(shape5));

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
