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

    //Shape1
    auto shape = tvg::Shape::gen();

    /* Acquire shape pointer to access it again.
       instead, you should consider not to interrupt this pointer life-cycle. */
    pShape = shape.get();

    shape->moveTo(0, -114.5);
    shape->lineTo(54, -5.5);
    shape->lineTo(175, 11.5);
    shape->lineTo(88, 95.5);
    shape->lineTo(108, 216.5);
    shape->lineTo(0, 160.5);
    shape->lineTo(-102, 216.5);
    shape->lineTo(-87, 96.5);
    shape->lineTo(-173, 12.5);
    shape->lineTo(-53, -5.5);
    shape->close();
    shape->fill(0, 0, 255, 255);
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

    //Transform Matrix
    tvg::Matrix m = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    //scale x
    m.e11 = 1 - (progress * 0.5f);

    //scale y
    m.e22 = 1 + (progress * 2.0f);

    //rotation
    constexpr auto PI = 3.141592f;
    auto degree = 45.0f;
    auto radian = degree / 180.0f * PI;
    auto cosVal = cosf(radian);
    auto sinVal = sinf(radian);

    auto t11 = m.e11 * cosVal + m.e12 * sinVal;
    auto t12 = m.e11 * -sinVal + m.e12 * cosVal;
    auto t21 = m.e21 * cosVal + m.e22 * sinVal;
    auto t22 = m.e21 * -sinVal + m.e22 * cosVal;
    auto t31 = m.e31 * cosVal + m.e32 * sinVal;
    auto t32 = m.e31 * -sinVal + m.e32 * cosVal;

    m.e11 = t11;
    m.e12 = t12;
    m.e21 = t21;
    m.e22 = t22;
    m.e31 = t31;
    m.e32 = t32;

    //translate
    m.e31 = progress * 300.0f + 300.0f;
    m.e32 = progress * -100.0f + 300.0f;

    pShape->transform(m);

    //Update shape for drawing (this may work asynchronously)
    canvas->update(pShape);

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
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

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
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}
