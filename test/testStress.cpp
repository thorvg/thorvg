#include <tizenvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 1920
#define HEIGHT 1080
#define COUNT 50

static uint32_t buffer[WIDTH * HEIGHT];
unique_ptr<tvg::SwCanvas> canvas = nullptr;
static double t1, t2, t3, t4;
static unsigned cnt = 0;

void tvgtest()
{
    //Create a Canvas
    canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);
}

Eina_Bool anim_cb(void *data)
{
    //Explicitly clear all retained paint nodes.
    t1 = ecore_time_get();
    canvas->clear();
    t2 = ecore_time_get();

    for (int i = 0; i < COUNT; i++) {
        auto shape = tvg::Shape::gen();

        float x = rand() % (WIDTH/2);
        float y = rand() % (HEIGHT/2);
        float w = 1 + rand() % 1200;
        float h = 1 + rand() %  800;

        shape->appendRect(x, y, w, h, rand() % 400);

        if (rand() % 2) {
            //LinearGradient
            auto fill = tvg::LinearGradient::gen();
            fill->linear(x, y, x + w, y + h);

            //Gradient Color Stops
            tvg::Fill::ColorStop colorStops[3];
            colorStops[0] = {0, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};
            colorStops[1] = {1, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};
            colorStops[2] = {2, uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255};

            fill->colorStops(colorStops, 3);
            shape->fill(move(fill));
        } else {
            shape->fill(uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255);
        }
#if 0
        if (rand() % 2) {
           shape->stroke(float(rand() % 10));
           shape->stroke(uint8_t(rand() % 255), uint8_t(rand() % 255), uint8_t(rand() % 255), 255);
        }
#endif
        canvas->push(move(shape));
    }

    //Update Efl Canvas
    Eo* img = (Eo*) data;
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);

    return ECORE_CALLBACK_RENEW;
}

void render_cb(void* data, Eo* obj)
{
    t3 = ecore_time_get();

    //Draw Next frames
    canvas->draw();
    canvas->sync();

    t4 = ecore_time_get();

    printf("[%5d]: total[%fms] = clear[%fms], update[%fms], render[%fms]\n", ++cnt, t4 - t1, t2 - t1, t3 - t2, t4 - t3);
}

void win_del(void *data, Evas_Object *o, void *ev)
{
    elm_exit();
}

int main(int argc, char **argv)
{
    //Initialize TizenVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

    tvgtest();

    //Show the result using EFL...
    elm_init(argc, argv);

    Eo* win = elm_win_util_standard_add(NULL, "TizenVG Test");
    evas_object_smart_callback_add(win, "delete,request", win_del, 0);

    Eo* img = evas_object_image_filled_add(evas_object_evas_get(win));
    evas_object_image_size_set(img, WIDTH, HEIGHT);
    evas_object_image_data_set(img, buffer);
    evas_object_image_pixels_get_callback_set(img, render_cb, nullptr);
    evas_object_size_hint_weight_set(img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(img);

    elm_win_resize_object_add(win, img);
    evas_object_geometry_set(win, 0, 0, WIDTH, HEIGHT);
    evas_object_show(win);

    ecore_animator_add(anim_cb, img);

    elm_run();
    elm_shutdown();

    //Terminate TizenVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}
