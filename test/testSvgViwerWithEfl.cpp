#include "testCommon.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

tvg::Canvas* pCanvas = nullptr;
Eo *view, *eflView;
int count = 0;

void tvgDrawCmds(const char* path)
{
   float w, h, x, y;
   if (!pCanvas) return;

   //Clear canvas
   pCanvas->clear();

   //Background
   auto shape = tvg::Shape::gen();
   shape->appendRect(0, 0, WIDTH, HEIGHT, 0, 0);
   shape->fill(65, 65, 65, 255);;

   if (pCanvas->push(move(shape)) != tvg::Result::Success) return;

   if (!path) return;

   auto picture = tvg::Picture::gen();
   if (picture->load(path) != tvg::Result::Success) return;

   picture->viewbox(&x, &y, &w, &h);
   auto sx = WIDTH / w;
   auto sy = HEIGHT / h;
   auto scale = sx < sy ? sx : sy;
   auto tx =  (WIDTH/ 2) - (w * (scale)) / 2 - (x * scale);
   auto ty =  (HEIGHT/ 2) - (h * (scale)) / 2 - (y * scale);
   picture->translate(tx, ty);
   picture->scale(scale);

   if (pCanvas->push(move(picture)) != tvg::Result::Success) return;

   //Update image
   evas_object_image_pixels_dirty_set(view, EINA_TRUE);
   evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);

   //Update efl svg
   elm_animation_view_file_set(eflView, path, NULL);
}


static void svgItemSelectedCallback(void *data EINA_UNUSED, Eo *obj, void *event_info)
{
   char *item = (char*)elm_object_item_text_get((Elm_Widget_Item *)event_info);
   printf("SVG : %s\n", item);
   while(item[0] != ':') item++;
   item++;
   tvgDrawCmds(item);
}


void svgDirCallback(const char* name, const char* path, void* data)
{
   Eo* list = (Eo*)data;
   char buf[PATH_MAX];
   sprintf(buf,"%d:%s/%s",++count, path, name);

   elm_list_item_append(list, buf, NULL, NULL, svgItemSelectedCallback, buf);
}


static Eo* svgListCreate(Eo *parent)
{
   Eo *list;

   list = elm_list_add(parent);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);

   eina_file_dir_list("./svgs", EINA_TRUE, svgDirCallback, list);
   return list;
}


Eo* createTable(Eo *obj, Eo *parent, Evas_Coord w, Evas_Coord h)
{
   Eo *table, *rect;

   table = elm_table_add(parent);

   rect = evas_object_rectangle_add(evas_object_evas_get(table));
   evas_object_size_hint_min_set(rect, w, h);
   evas_object_size_hint_weight_set(rect, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(rect, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, rect, 0, 0, 1, 1);

   evas_object_size_hint_align_set(obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, obj, 0, 0, 1, 1);

   return table;
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;

void tvgSwTest(uint32_t* buffer)
{
   //Create a Canvas
   swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);

   /* Push the shape into the Canvas drawing list
      When this shape is into the canvas list, the shape could update & prepare
      internal data asynchronously for coming rendering.
      Canvas keeps this shape node unless user call canvas->clear() */
   pCanvas = swCanvas.get();
   tvgDrawCmds(NULL);
}


void drawSwView(void* data, Eo* obj)
{
   if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
   }
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(Eo *obj)
{
   static constexpr auto BPP = 4;

   //Create a Canvas
   glCanvas = tvg::GlCanvas::gen();
   glCanvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

   /* Push the shape into the Canvas drawing list
      When this shape is into the canvas list, the shape could update & prepare
      internal data asynchronously for coming rendering.
      Canvas keeps this shape node unless user call canvas->clear() */
   pCanvas = glCanvas.get();
   tvgDrawCmds(NULL);
}

void drawGLview(Eo *obj)
{
   auto gl = elm_glview_gl_api_get(obj);
   gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   gl->glClear(GL_COLOR_BUFFER_BIT);

   if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
   }
}


static Eo* createSwView(Eo *parent)
{
   static uint32_t buffer[WIDTH * HEIGHT];


   Eo* view = evas_object_image_filled_add(evas_object_evas_get(parent));
   evas_object_image_size_set(view, WIDTH, HEIGHT);
   evas_object_image_data_set(view, buffer);
   evas_object_image_pixels_get_callback_set(view, drawSwView, nullptr);
   evas_object_image_pixels_dirty_set(view, EINA_TRUE);
   evas_object_image_data_update_add(view, 0, 0, WIDTH, HEIGHT);
   evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(view);

   tvgSwTest(buffer);

   return view;
}


static Eo* createGlView(Eo *parent)
{
   elm_config_accel_preference_set("gl");

   Eo* view = elm_glview_add(parent);
   evas_object_size_hint_weight_set(view, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_glview_mode_set(view, ELM_GLVIEW_ALPHA);
   elm_glview_resize_policy_set(view, ELM_GLVIEW_RESIZE_POLICY_RECREATE);
   elm_glview_render_policy_set(view, ELM_GLVIEW_RENDER_POLICY_ON_DEMAND);
   elm_glview_init_func_set(view, initGLview);
   elm_glview_render_func_set(view, drawGLview);
   evas_object_show(view);

   return view;
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
   setenv("ECTOR_BACKEND", "default", 1);
   //setenv("ELM_ACCEL", "gl", 1);
   tvg::CanvasEngine tvgEngine = tvg::CanvasEngine::Sw;

   if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
   }

   //Initialize ThorVG Engine
   if (tvgEngine == tvg::CanvasEngine::Sw) {
        cout << "tvg engine: software" << endl;
   } else {
        cout << "tvg engine: opengl" << endl;
   }
    //Threads Count
    auto threads = std::thread::hardware_concurrency();

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine, threads) == tvg::Result::Success) {
        Eo *box, *list, *table;

        elm_init(argc, argv);

        Eo* win = elm_win_util_standard_add(NULL, "ThorVG Test with EFL");
        evas_object_smart_callback_add(win, "delete,request", win_del, 0);

        box = elm_box_add(win);
        elm_box_horizontal_set(box, EINA_TRUE);
        evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_win_resize_object_add(win, box);
        evas_object_show(box);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
             view = createSwView(win);
        } else {
             view = createGlView(win);
        }

        table = createTable(view, box, 800, 0);
        evas_object_size_hint_weight_set(table, 0.0, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_box_pack_end(box, table);
        evas_object_show(table);

        eflView = elm_animation_view_add(win);
        evas_object_size_hint_min_set(eflView, ELM_SCALE_SIZE(WIDTH), ELM_SCALE_SIZE(HEIGHT));
        evas_object_size_hint_align_set(eflView, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_show(eflView);

        table = createTable(eflView, box, 800, 0);
        evas_object_size_hint_weight_set(table, 0.0, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_box_pack_end(box, table);
        evas_object_show(table);

        list = svgListCreate(box);
        evas_object_show(list);

        table = createTable(list, box, 400, 0);
        evas_object_size_hint_weight_set(table, 0.0, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_box_pack_end(box, table);
        evas_object_show(table);

        evas_object_geometry_set(win, 0, 0, 2000, HEIGHT);
        evas_object_show(win);

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

   } else {
        cout << "engine is not supported" << endl;
   }
   return 0;
}
