#include <tizenvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];
unique_ptr<tvg::SwCanvas> canvas = nullptr;
tvg::Shape* pShape = nullptr;
tvg::Shape* pShape2 = nullptr;
tvg::Shape* pShape3 = nullptr;

void tvgtest()
{
    //Create a Canvas
    canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    //Shape1
    auto shape = tvg::Shape::gen();

    /* Acquire shape pointer to access it again.
       instead, you should consider not to interrupt this pointer life-cycle. */
    pShape = shape.get();

    shape->appendRect(-285, -300, 200, 200, 0);
    shape->appendRect(-185, -200, 300, 300, 100);
    shape->appendCircle(115, 100, 100, 100);
    shape->appendCircle(115, 200, 170, 100);

    //LinearGradient
    auto fill = tvg::LinearGradient::gen();
    fill->linear(-285, -300, 285, 300);

    //Gradient Color Stops
    tvg::Fill::ColorStop colorStops[3];
    colorStops[0] = {0, 255, 0, 0, 255};
    colorStops[1] = {0.5, 255, 255, 0, 255};
    colorStops[2] = {1, 255, 255, 255, 255};

    fill->colorStops(colorStops, 3);
    shape->fill(move(fill));
    shape->translate(385, 400);
    canvas->push(move(shape));

    //Shape2
    auto shape2 = tvg::Shape::gen();
    pShape2 = shape2.get();
    shape2->appendRect(-50, -50, 100, 100, 0);
    shape2->translate(400, 400);

    //LinearGradient
    auto fill2 = tvg::LinearGradient::gen();
    fill2->linear(-50, -50, 50, 50);

    //Gradient Color Stops
    tvg::Fill::ColorStop colorStops2[2];
    colorStops2[0] = {0, 0, 0, 0, 255};
    colorStops2[1] = {1, 255, 255, 255, 255};

    fill2->colorStops(colorStops2, 2);
    shape2->fill(move(fill2));
    canvas->push(move(shape2));

    //Shape3
    auto shape3 = tvg::Shape::gen();
    pShape3 = shape3.get();

    /* Look, how shape3's origin is different with shape2
       The center of the shape is the anchor point for transformation. */
    shape3->appendRect(100, 100, 150, 100, 20);

    //RadialGradient
    auto fill3 = tvg::RadialGradient::gen();
    fill3->radial(175, 150, 75);

    //Gradient Color Stops
    tvg::Fill::ColorStop colorStops3[4];
    colorStops3[0] = {0, 0, 127, 0, 127};
    colorStops3[1] = {0.25, 0, 170, 170, 170};
    colorStops3[2] = {0.5, 200, 0, 200, 200};
    colorStops3[3] = {1, 255, 255, 255, 255};

    fill3->colorStops(colorStops3, 4);

    shape3->fill(move(fill3));
    shape3->translate(400, 400);
    canvas->push(move(shape3));

    //Draw first frame
    canvas->draw();
    canvas->sync();
}

void transit_cb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    /* Update shape directly.
       You can update only necessary properties of this shape,
       while retaining other properties. */

    //Update Shape1
    pShape->scale(1 - 0.75 * progress);
    pShape->rotate(360 * progress);

    //Update shape for drawing (this may work asynchronously)
    canvas->update(pShape);

    //Update Shape2
    pShape2->rotate(360 * progress);
    pShape2->translate(400 + progress * 300, 400);
    canvas->update(pShape2);

    //Update Shape3
    pShape3->rotate(-360 * progress);
    pShape3->scale(0.5 + progress);
    canvas->update(pShape3);

    //Draw Next frames
    canvas->draw();
    canvas->sync();

    //Update Efl Canvas
    Eo* img = (Eo*) effect;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
}

void
win_del(void *data, Evas_Object *o, void *ev)
{
    elm_exit();
}

int main(int argc, char **argv)
{
    //Initialize TizenVG Engine
    tvg::Engine::init();

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

    Elm_Transit *transit = elm_transit_add();
    elm_transit_effect_add(transit, transit_cb, img, nullptr);
    elm_transit_duration_set(transit, 2);
    elm_transit_repeat_times_set(transit, -1);
    elm_transit_auto_reverse_set(transit, EINA_TRUE);
    elm_transit_go(transit);

    elm_run();
    elm_shutdown();

    //Terminate TizenVG Engine
    tvg::Engine::term();
}
