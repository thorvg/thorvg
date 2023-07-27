/*
 * Copyright (c) 2023 the ThorVG project. All rights reserved.

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

#define NO_GL_EXAMPLE

#include <vector>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

#define NUM_PER_ROW 7
#define NUM_PER_COL 7
#define SIZE (WIDTH/NUM_PER_ROW)

static int counter = 0;
static Eo* view = nullptr;
static std::vector<unique_ptr<tvg::Animation>> animations;
static std::vector<Elm_Transit*> transitions;
static unique_ptr<tvg::SwCanvas> swCanvas;


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

    //Update animation frame
    animation->frame(roundf(animation->totalFrame() * progress));

    swCanvas->update(animation->picture());
}

void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);
    shape->fill(75, 75, 75);

    if (swCanvas->push(std::move(shape)) != tvg::Result::Success) return;

    eina_file_dir_list(EXAMPLE_DIR, EINA_TRUE, lottieDirCallback, swCanvas.get());
   
    //Run animation loop
    for (auto& animation : animations) {
        Elm_Transit* transit = elm_transit_add();
        elm_transit_effect_add(transit, tvgUpdateCmds, animation.get(), nullptr);
        elm_transit_duration_set(transit, animation->duration());
        elm_transit_repeat_times_set(transit, -1);
        elm_transit_go(transit);

        swCanvas->push(tvg::cast<tvg::Picture>(animation->picture()));
    }
}

void drawSwView(void* data, Eo* obj)
{
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }
}

Eina_Bool animatorCb(void *data)
{
    Eo* img = (Eo*) data;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);

    return ECORE_CALLBACK_RENEW;
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    //Threads Count
    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(tvg::CanvasEngine::Sw, threads) == tvg::Result::Success) {

        elm_init(argc, argv);

        view = createSwView(1440, 1440);
        ecore_animator_add(animatorCb, view);

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvg::CanvasEngine::Sw);

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
