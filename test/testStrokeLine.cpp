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

    //Test for Stroke Width
    for (int i = 0; i < 10; ++i) {
        auto shape = tvg::Shape::gen();
        shape->moveTo(50, 50 + (25 * i));
        shape->lineTo(750, 50 + (25 * i));
        shape->stroke(255, 255, 255, 255);       //color: r, g, b, a
        shape->stroke(i + 1);                    //stroke width
        shape->stroke(tvg::StrokeCap::Round);    //default is Square
        canvas->push(move(shape));
    }


    //Test for StrokeJoin & StrokeCap
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

    //Test for Stroke Dash
    auto shape4 = tvg::Shape::gen();
    shape4->moveTo(20, 600);
    shape4->lineTo(250, 600);
    shape4->lineTo(220, 750);
    shape4->lineTo(70, 720);
    shape4->lineTo(70, 580);
    shape4->stroke(255, 0, 0, 255);
    shape4->stroke(5);
    shape4->stroke(tvg::StrokeJoin::Round);
    shape4->stroke(tvg::StrokeCap::Round);

    float dashPattern1[2] = {10, 10};
    shape4->stroke(dashPattern1, 2);
    canvas->push(move(shape4));

    auto shape5 = tvg::Shape::gen();
    shape5->moveTo(270, 600);
    shape5->lineTo(500, 600);
    shape5->lineTo(470, 750);
    shape5->lineTo(320, 720);
    shape5->lineTo(320, 580);
    shape5->stroke(255, 255, 0, 255);
    shape5->stroke(5);
    shape5->stroke(tvg::StrokeJoin::Bevel);
    shape5->stroke(tvg::StrokeCap::Butt);

    float dashPattern2[4] = {10, 10};
    shape5->stroke(dashPattern2, 4);
    canvas->push(move(shape5));

    auto shape6 = tvg::Shape::gen();
    shape6->moveTo(520, 600);
    shape6->lineTo(750, 600);
    shape6->lineTo(720, 750);
    shape6->lineTo(570, 720);
    shape6->lineTo(570, 580);
    shape6->stroke(255, 255, 255, 255);
    shape6->stroke(5);
    shape6->stroke(tvg::StrokeJoin::Miter);
    shape6->stroke(tvg::StrokeCap::Square);

    float dashPattern3[2] = {10, 10};
    shape6->stroke(dashPattern3, 2);
    canvas->push(move(shape6));

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
