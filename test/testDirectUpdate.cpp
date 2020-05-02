#include <tizenvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];
unique_ptr<tvg::SwCanvas> canvas = nullptr;
tvg::Shape* pShape = nullptr;

void tvgtest()
{
    //Create a Canvas
    canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    //Shape
    auto shape = tvg::Shape::gen();

    /* Acquire shape pointer to access it again.
       instead, you should consider not to interrupt this pointer life-cycle. */
    pShape = shape.get();

    shape->appendRect(-100, -100, 200, 200, 0);
    shape->fill(127, 255, 255, 255);
    canvas->push(move(shape));

    //Draw first frame
    canvas->draw();
    canvas->sync();
}

void transit_cb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    /* Update shape directly.
       You can update only necessary properties of this shape,
       while retaining other properties. */

    pShape->reset();    //reset path

    pShape->appendRect(-100 + (800 * progress), -100 + (800 * progress), 200, 200, (100 * progress));

    //Update shape for drawing (this may work asynchronously)
    pShape->update(canvas->engine());

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
