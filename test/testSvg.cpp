#include <thorvg.h>
#include <Elementary.h>

using namespace std;

#define WIDTH 800
#define HEIGHT 800

static uint32_t buffer[WIDTH * HEIGHT];

unique_ptr<tvg::SwCanvas> canvas = nullptr;
int count = 0;
static const int numberPerLine = 3;

static int x = 30;
static int y = 30;


void svgDirCallback(const char* name, const char* path, void* data)
{
    auto scene = tvg::Scene::gen();
    char buf[255];
    sprintf(buf,"%s/%s", path, name);
    scene->load(buf);
    printf("SVG Load : %s\n", buf);
    scene->translate(((WIDTH - (x * 2)) / numberPerLine) * (count % numberPerLine) + x, ((HEIGHT - (y * 2))/ numberPerLine) * (int)((float)count / (float)numberPerLine) + y);
    canvas->push(move(scene));
    count++;
}


void win_del(void* data, Evas_Object* o, void* ev)
{
   elm_exit();
}


int main(int argc, char** argv)
{
    //Initialize ThorVG Engine
    tvg::Initializer::init(tvg::CanvasEngine::Sw);

    //Create a Canvas
    canvas = tvg::SwCanvas::gen();
    canvas->target(buffer, WIDTH, WIDTH, HEIGHT);

    //Draw white background
    auto scene = tvg::Scene::gen();
    auto shape1 = tvg::Shape::gen();
    shape1->appendRect(0, 0, WIDTH, HEIGHT, 0);
    shape1->fill(255, 255, 255, 255);
    scene->push(move(shape1));
    canvas->push(move(scene));

    eina_file_dir_list("./svgs", EINA_TRUE, svgDirCallback, NULL);

    canvas->draw();
    canvas->sync();

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

    //Terminate ThorVG Engine
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
}
