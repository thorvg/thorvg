/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <vector>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

#define NUM_PER_ROW 10
#define NUM_PER_COL 10
#define SIZE (WIDTH/NUM_PER_ROW)

static int counter = 0;
static std::vector<unique_ptr<tvg::Animation>> animations;
static std::vector<Elm_Transit*> transitions;
static tvg::Canvas* canvas;

//performance measure
static double updateTime = 0;
static double accumUpdateTime = 0;
static double accumRasterTime = 0;
static double accumTotalTime = 0;
static uint32_t cnt = 0;

void lottieDirCallback(const char* name, const char* path, void* data)
{
    if (counter >= NUM_PER_ROW * NUM_PER_COL) return;

    //ignore if not lottie file.
    const char *ext = name + strlen(name) - 4;
    if (strcmp(ext, "json")) return;

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "/%s/%s", path, name);

    //Animation Controller
    auto animation = tvg::Animation::gen();
    auto picture = animation->picture();

    if (picture->load(buf) != tvg::Result::Success) {
        cout << "Lottie is not supported. Did you enable Lottie Loader?" << endl;
        return;
    }

    //image scaling preserving its aspect ratio
    float scale;
    float shiftX = 0.0f, shiftY = 0.0f;
    float w, h;
    picture->size(&w, &h);

    if (w > h) {
        scale = SIZE / w;
        shiftY = (SIZE - h * scale) * 0.5f;
    } else {
        scale = SIZE / h;
        shiftX = (SIZE - w * scale) * 0.5f;
    }

    picture->scale(scale);
    picture->translate((counter % NUM_PER_ROW) * SIZE + shiftX, (counter / NUM_PER_ROW) * (HEIGHT / NUM_PER_COL) + shiftY);

    animations.push_back(std::move(animation));

    cout << "Lottie: " << buf << endl;

    counter++;
}

void tvgUpdateCmds(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    auto animation = static_cast<tvg::Animation*>(effect);

    //Update animation frame only when it's changed
    auto before = ecore_time_get();
    animation->frame(animation->totalFrame() * progress);
    auto after = ecore_time_get();
    updateTime += after - before;
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);
    shape->fill(75, 75, 75);

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    eina_file_dir_list(EXAMPLE_DIR"/lottie", EINA_FALSE, lottieDirCallback, canvas);

    //Run animation loop
    for (auto& animation : animations) {
        Elm_Transit* transit = elm_transit_add();
        elm_transit_effect_add(transit, tvgUpdateCmds, animation.get(), nullptr);
        elm_transit_duration_set(transit, animation->duration());
        elm_transit_repeat_times_set(transit, -1);
        elm_transit_go(transit);

        canvas->push(tvg::cast(animation->picture()));
    }
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

    tvgDrawCmds(swCanvas.get());

    canvas = swCanvas.get();
}

void drawSwView(void* data, Eo* obj)
{
    //It's not necessary to clear buffer since it has a solid background
    //swCanvas->clear(false);

    //canvas update
    auto before = ecore_time_get();

    swCanvas->update();

    auto after = ecore_time_get();

    updateTime += (after - before);

    //canvas draw
    before = ecore_time_get();

    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }

    after = ecore_time_get();

    auto rasterTime = after - before;

    ++cnt;

    accumUpdateTime += updateTime;
    accumRasterTime += rasterTime;
    accumTotalTime += (updateTime + rasterTime);

    printf("[%5d]: update = %fs,   raster = %fs,  total = %fs\n", cnt, accumUpdateTime / cnt, accumRasterTime / cnt, accumTotalTime / cnt);

    updateTime = 0;
}

Eina_Bool animatorSwCb(void *data)
{
    Eo* img = (Eo*) data;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);

    return ECORE_CALLBACK_RENEW;
}


/************************************************************************/
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(Evas_Object *obj)
{
    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();

    //Get the drawing target id
    int32_t targetId;
    auto gl = elm_glview_gl_api_get(obj);
    gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &targetId);

    glCanvas->target(targetId, WIDTH, HEIGHT);

    tvgDrawCmds(glCanvas.get());

    canvas = glCanvas.get();
}

void drawGLview(Evas_Object *obj)
{
    auto before = ecore_time_get();

    auto gl = elm_glview_gl_api_get(obj);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
    }

    auto after = ecore_time_get();

    auto rasterTime = after - before;

    ++cnt;

    accumUpdateTime += updateTime;
    accumRasterTime += rasterTime;
    accumTotalTime += (updateTime + rasterTime);

    printf("[%5d]: update = %fs,   raster = %fs,  total = %fs\n", cnt, accumUpdateTime / cnt, accumRasterTime / cnt, accumTotalTime / cnt);

    updateTime = 0;
}

Eina_Bool animatorGlCb(void *data)
{
    elm_glview_changed_set((Evas_Object*)data);

    return ECORE_CALLBACK_RENEW;
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
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
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvgEngine, 4) == tvg::Result::Success) {

        elm_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView(1280, 1280);
            ecore_animator_add(animatorSwCb, view);
        } else {
            auto view = createGlView(1280, 1280);
            ecore_animator_add(animatorGlCb, view);
        }

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

    } else {
        cout << "engine is not supported" << endl;
    }

    return 0;
}
