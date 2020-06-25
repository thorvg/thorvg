#include <thorvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];

void tvgtest()
{
    //Initialize ThorVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

    //Create a Canvas
    auto canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    //Star
    auto shape1 = tvg::Shape::gen();

    //Appends Paths
    shape1->moveTo(199, 34);
    shape1->lineTo(253, 143);
    shape1->lineTo(374, 160);
    shape1->lineTo(287, 244);
    shape1->lineTo(307, 365);
    shape1->lineTo(199, 309);
    shape1->lineTo(97, 365);
    shape1->lineTo(112, 245);
    shape1->lineTo(26, 161);
    shape1->lineTo(146, 143);
    shape1->close();
    shape1->fill(0, 0, 255, 255);
    canvas->push(move(shape1));

    //Circle
    auto shape2 = tvg::Shape::gen();

    auto cx = 550.0f;
    auto cy = 550.0f;
    auto radius = 125.0f;
    auto halfRadius = radius * 0.552284f;

    //Append Paths
    shape2->moveTo(cx, cy - radius);
    shape2->cubicTo(cx + halfRadius, cy - radius, cx + radius, cy - halfRadius, cx + radius, cy);
    shape2->cubicTo(cx + radius, cy + halfRadius, cx + halfRadius, cy + radius, cx, cy+ radius);
    shape2->cubicTo(cx - halfRadius, cy + radius, cx - radius, cy + halfRadius, cx - radius, cy);
    shape2->cubicTo(cx - radius, cy - halfRadius, cx - halfRadius, cy - radius, cx, cy - radius);
    shape2->fill(255, 0, 0, 255);
    canvas->push(move(shape2));

    canvas->draw();
    canvas->sync();

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}

void win_del(void *data, Evas_Object *o, void *ev)
{
   elm_exit();
}

int main(int argc, char **argv)
{
    tvgtest();

    //Show the result using EFL...
    elm_init(argc, argv);

    Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test");
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
