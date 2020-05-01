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

    /* Star */

    //Prepare Path Commands
    tvg::PathCommand cmds[11];
    cmds[0] = tvg::PathCommand::MoveTo;
    cmds[1] = tvg::PathCommand::LineTo;
    cmds[2] = tvg::PathCommand::LineTo;
    cmds[3] = tvg::PathCommand::LineTo;
    cmds[4] = tvg::PathCommand::LineTo;
    cmds[5] = tvg::PathCommand::LineTo;
    cmds[6] = tvg::PathCommand::LineTo;
    cmds[7] = tvg::PathCommand::LineTo;
    cmds[8] = tvg::PathCommand::LineTo;
    cmds[9] = tvg::PathCommand::LineTo;
    cmds[10] = tvg::PathCommand::Close;

    //Prepare Path Points
    tvg::Point pts[10];
    pts[0] = {199, 34};    //MoveTo
    pts[1] = {253, 143};   //LineTo
    pts[2] = {374, 160};   //LineTo
    pts[3] = {287, 244};   //LineTo
    pts[4] = {307, 365};   //LineTo
    pts[5] = {199, 309};   //LineTo
    pts[6] = {97, 365};    //LineTo
    pts[7] = {112, 245};   //LineTo
    pts[8] = {26, 161};    //LineTo
    pts[9] = {146, 143};   //LineTo

    auto shape1 = tvg::ShapeNode::gen();
    shape1->appendPath(cmds, 11, pts, 10);     //copy path data
    shape1->fill(0, 255, 0, 255);
    canvas->push(move(shape1));


    /* Circle */
    auto cx = 550.0f;
    auto cy = 550.0f;
    auto radius = 125.0f;
    auto halfRadius = radius * 0.552284f;

    //Prepare Path Commands
    tvg::PathCommand cmds2[6];
    cmds2[0] = tvg::PathCommand::MoveTo;
    cmds2[1] = tvg::PathCommand::CubicTo;
    cmds2[2] = tvg::PathCommand::CubicTo;
    cmds2[3] = tvg::PathCommand::CubicTo;
    cmds2[4] = tvg::PathCommand::CubicTo;
    cmds2[5] = tvg::PathCommand::Close;

    //Prepare Path Points
    tvg::Point pts2[13];
    pts2[0] = {cx, cy - radius};    //MoveTo
    //CubicTo 1
    pts2[1] = {cx + halfRadius, cy - radius};      //Ctrl1
    pts2[2] = {cx + radius, cy - halfRadius};      //Ctrl2
    pts2[3] = {cx + radius, cy};                   //To
    //CubicTo 2
    pts2[4] = {cx + radius, cy + halfRadius};      //Ctrl1
    pts2[5] = {cx + halfRadius, cy + radius};      //Ctrl2
    pts2[6] = {cx, cy+ radius};                    //To
    //CubicTo 3
    pts2[7] = {cx - halfRadius, cy + radius};      //Ctrl1
    pts2[8] = {cx - radius, cy + halfRadius};      //Ctrl2
    pts2[9] = {cx - radius, cy};                   //To
    //CubicTo 4
    pts2[10] = {cx - radius, cy - halfRadius};     //Ctrl1
    pts2[11] = {cx - halfRadius, cy - radius};     //Ctrl2
    pts2[12] = {cx, cy - radius};                  //To

    auto shape2 = tvg::ShapeNode::gen();
    shape2->appendPath(cmds2, 6, pts2, 13);     //copy path data
    shape2->fill(255, 255, 0, 255);
    canvas->push(move(shape2));

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
