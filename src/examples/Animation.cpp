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

#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgUpdateCmds(tvg::Canvas* canvas, tvg::Animation* animation, float progress)
{
    if (!canvas) return;

    //Update animation frame
    animation->frame(roundf(animation->totalFrame() * progress));

    canvas->update(animation->picture());
}


/************************************************************************/
/* Sw Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::SwCanvas> swCanvas;
static unique_ptr<tvg::Animation> animation;
static Elm_Transit *transit;

void tvgSwTest(uint32_t* buffer)
{
    //Create a Canvas
    swCanvas = tvg::SwCanvas::gen();
    swCanvas->target(buffer, WIDTH, WIDTH, HEIGHT, tvg::SwCanvas::ARGB8888);

    //Animation Controller
    animation = tvg::Animation::gen();
    auto picture = animation->picture();

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);
    shape->fill(50, 50, 50);

    if (swCanvas->push(std::move(shape)) != tvg::Result::Success) return;

    if (picture->load(EXAMPLE_DIR"/sample.json") != tvg::Result::Success) {
        cout << "Lottie is not supported. Did you enable Lottie Loader?" << endl;
        return;
    }

    //image scaling preserving its aspect ratio
    float scale;
    float shiftX = 0.0f, shiftY = 0.0f;
    float w, h;
    picture->size(&w, &h);

    if (w > h) {
        scale = WIDTH / w;
        shiftY = (HEIGHT - h * scale) * 0.5f;
    } else {
        scale = HEIGHT / h;
        shiftX = (WIDTH - w * scale) * 0.5f;
    }

    picture->scale(scale);
    picture->translate(shiftX, shiftY);

    swCanvas->push(tvg::cast<tvg::Picture>(picture));

    //Run animation loop
    elm_transit_duration_set(transit, animation->duration());
    elm_transit_repeat_times_set(transit, -1);
    elm_transit_go(transit);
}

void drawSwView(void* data, Eo* obj)
{
    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
    }
}

void transitSwCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    tvgUpdateCmds(swCanvas.get(), animation.get(), progress);

    //Update Efl Canvas
    Eo* img = (Eo*) effect;
    evas_object_image_data_update_add(img, 0, 0, WIDTH, HEIGHT);
    evas_object_image_pixels_dirty_set(img, EINA_TRUE);
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

        transit = elm_transit_add();
        auto view = createSwView();
        elm_transit_effect_add(transit, transitSwCb, view, nullptr);

        elm_run();
        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term(tvg::CanvasEngine::Sw);
    }

    return 0;
}
