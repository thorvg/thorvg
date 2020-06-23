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

    //Create a Scene
    auto scene = tvg::Scene::gen();
    scene->reserve(3);   //reserve 3 shape nodes (optional)

    //Prepare Round Rectangle
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, 400, 400, 50);      //x, y, w, h, cornerRadius
    shape1->fill(0, 255, 0, 255);                //r, g, b, a
    scene->push(move(shape1));

    //Prepare Circle
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(400, 400, 200, 200);    //cx, cy, radiusW, radiusH
    shape2->fill(255, 255, 0, 255);              //r, g, b, a
    scene->push(move(shape2));

    //Prepare Ellipse
    auto shape3 = tvg::Shape::gen();
    shape3->appendCircle(600, 600, 150, 100);    //cx, cy, radiusW, radiusH
    shape3->fill(0, 255, 255, 255);              //r, g, b, a
    scene->push(move(shape3));

    //Create another Scene
    auto scene2 = tvg::Scene::gen();
    scene2->reserve(2);   //reserve 2 shape nodes (optional)

    //Star
    auto shape4 = tvg::Shape::gen();

    //Appends Paths
    shape4->moveTo(199, 34);
    shape4->lineTo(253, 143);
    shape4->lineTo(374, 160);
    shape4->lineTo(287, 244);
    shape4->lineTo(307, 365);
    shape4->lineTo(199, 309);
    shape4->lineTo(97, 365);
    shape4->lineTo(112, 245);
    shape4->lineTo(26, 161);
    shape4->lineTo(146, 143);
    shape4->close();
    shape4->fill(0, 0, 255, 255);
    scene2->push(move(shape4));

    //Circle
    auto shape5 = tvg::Shape::gen();

    auto cx = 550.0f;
    auto cy = 550.0f;
    auto radius = 125.0f;
    auto halfRadius = radius * 0.552284f;

    //Append Paths
    shape5->moveTo(cx, cy - radius);
    shape5->cubicTo(cx + halfRadius, cy - radius, cx + radius, cy - halfRadius, cx + radius, cy);
    shape5->cubicTo(cx + radius, cy + halfRadius, cx + halfRadius, cy + radius, cx, cy+ radius);
    shape5->cubicTo(cx - halfRadius, cy + radius, cx - radius, cy + halfRadius, cx - radius, cy);
    shape5->cubicTo(cx - radius, cy - halfRadius, cx - halfRadius, cy - radius, cx, cy - radius);
    shape5->fill(255, 0, 0, 255);
    scene2->push(move(shape5));

    //Push scene2 onto the scene
    scene->push(move(scene2));

    //Draw the Scene onto the Canvas
    canvas->push(move(scene));
    canvas->draw();
    canvas->sync();

    //Terminate TizenVG Engine
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
