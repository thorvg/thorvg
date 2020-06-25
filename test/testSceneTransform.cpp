#include <thorvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];
unique_ptr<tvg::SwCanvas> canvas = nullptr;
tvg::Scene* pScene1 = nullptr;
tvg::Scene* pScene2 = nullptr;

void tvgtest()
{
    //Create a Canvas
    canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    //Create a Scene1
    auto scene = tvg::Scene::gen();
    pScene1 = scene.get();
    scene->reserve(3);   //reserve 3 shape nodes (optional)

    //Prepare Round Rectangle (Scene1)
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(-235, -250, 400, 400, 50);      //x, y, w, h, cornerRadius
    shape1->fill(0, 255, 0, 255);                      //r, g, b, a
    shape1->stroke(5);                                 //width
    shape1->stroke(255, 255, 255, 255);                //r, g, b, a
    scene->push(move(shape1));

    //Prepare Circle (Scene1)
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(-165, -150, 200, 200);    //cx, cy, radiusW, radiusH
    shape2->fill(255, 255, 0, 255);                //r, g, b, a
    scene->push(move(shape2));

    //Prepare Ellipse (Scene1)
    auto shape3 = tvg::Shape::gen();
    shape3->appendCircle(265, 250, 150, 100);      //cx, cy, radiusW, radiusH
    shape3->fill(0, 255, 255, 255);                //r, g, b, a
    scene->push(move(shape3));

    scene->translate(350, 350);
    scene->scale(0.5);

    //Create Scene2
    auto scene2 = tvg::Scene::gen();
    pScene2 = scene2.get();
    scene2->reserve(2);   //reserve 2 shape nodes (optional)

    //Star (Scene2)
    auto shape4 = tvg::Shape::gen();

    //Appends Paths
    shape4->moveTo(0, -114.5);
    shape4->lineTo(54, -5.5);
    shape4->lineTo(175, 11.5);
    shape4->lineTo(88, 95.5);
    shape4->lineTo(108, 216.5);
    shape4->lineTo(0, 160.5);
    shape4->lineTo(-102, 216.5);
    shape4->lineTo(-87, 96.5);
    shape4->lineTo(-173, 12.5);
    shape4->lineTo(-53, -5.5);
    shape4->close();
    shape4->fill(0, 0, 127, 127);
    shape4->stroke(3);                             //width
    shape4->stroke(0, 0, 255, 255);                //r, g, b, a
    scene2->push(move(shape4));

    //Circle (Scene2)
    auto shape5 = tvg::Shape::gen();

    auto cx = -150.0f;
    auto cy = -150.0f;
    auto radius = 100.0f;
    auto halfRadius = radius * 0.552284f;

    //Append Paths
    shape5->moveTo(cx, cy - radius);
    shape5->cubicTo(cx + halfRadius, cy - radius, cx + radius, cy - halfRadius, cx + radius, cy);
    shape5->cubicTo(cx + radius, cy + halfRadius, cx + halfRadius, cy + radius, cx, cy+ radius);
    shape5->cubicTo(cx - halfRadius, cy + radius, cx - radius, cy + halfRadius, cx - radius, cy);
    shape5->cubicTo(cx - radius, cy - halfRadius, cx - halfRadius, cy - radius, cx, cy - radius);
    shape5->fill(127, 0, 0, 127);
    scene2->push(move(shape5));

    scene2->translate(500, 350);

    //Push scene2 onto the scene
    scene->push(move(scene2));

    //Draw the Scene onto the Canvas
    canvas->push(move(scene));

    canvas->draw();
    canvas->sync();
}

void transit_cb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    /* Update scene directly.
       You can update only necessary properties of this scene,
       while retaining other properties. */

    pScene1->rotate(360 * progress);
    pScene2->rotate(360 * progress);

    //Update shape for drawing (this may work asynchronously)
    canvas->update(pScene1);

    //Draw Next frames
    canvas->draw();
    canvas->sync();

    //Update Efl Canvas
    Eo* img = (Eo*) effect;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
}

void win_del(void *data, Evas_Object *o, void *ev)
{
    elm_exit();
}

int main(int argc, char **argv)
{
    //Initialize ThorVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

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

    Elm_Transit *transit = elm_transit_add();
    elm_transit_effect_add(transit, transit_cb, img, nullptr);
    elm_transit_duration_set(transit, 2);
    elm_transit_repeat_times_set(transit, -1);
    elm_transit_go(transit);

    elm_run();
    elm_shutdown();

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}
