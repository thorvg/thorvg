/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.
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

#include "Common.h"
#include <thorvg_lottie.h>
#include "iostream"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

static unique_ptr<tvg::LottieAnimation> animation;
static Elm_Transit *transit;
static bool updated = false;

void tvgUpdateCmds(tvg::Canvas* canvas, tvg::LottieAnimation* animation, float progress)
{
    if (updated || !canvas) return;

    //Update animation frame only when it's changed
    if (animation->frame(animation->totalFrame() * progress) == tvg::Result::Success) {
        canvas->update();
        updated = true;
    }
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    //Animation Controller
    animation = tvg::LottieAnimation::gen();
    auto picture = animation->picture();

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);
    shape->fill(50, 50, 50);

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;


    // const char* slotJson = R"({"gradient_fill":{"p":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}})";
    const char* slotJson = R"({"text_document_data":{"p":{"k":[{"s":{"s":71,"f":"OmnesMedium","t":"AAAA","j":2,"tr":0,"lh":85.2,"ls":0,"fc":[0.549,0.549,0.549]},"t":0}]}}})";

    // const char* slotJson = R"({"gradient_fill":{"p":{"a":0,"k":[0,0.1,0.1,0.1,0.2,1,1,0.1,0.2,0.1,11]}}})";
    // const char* slotJson2 = R"({"gradient_fill":{"p":{"a":0,"k":[0,0.514,0.373,0.984,0.141,0.478,0.412,0.984,0.283,0.443,0.451,0.984,0.379,0.408,0.49,0.984,0.475,0.373,0.529,0.984,0.606,0.278,0.647,0.925,0.737,0.184,0.765,0.867,0.868,0.092,0.882,0.808,1,0,1,0.749]}}})";

    if (picture->load(EXAMPLE_DIR"/slotsampletext.json") != tvg::Result::Success) {
        cout << "Lottie is not supported. Did you enable Lottie Loader?" << endl;
        return;
    }

    std::cout << "[SLOT LOG] Animation Loaded" << std::endl;
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

    canvas->push(tvg::cast(picture));
  
    //Override slot data
    std::cout << "[SLOT LOG] Override() called" << std::endl;
    if (animation->override(slotJson) == tvg::Result::Success) {
        canvas->update();
    } else {
        cout << "Failed to override the slot" << endl;
    }

    //Revert
    // std::cout << "[SLOT LOG] Revert - override(nullptr) called" << std::endl;
    // if (animation->override(nullptr) == tvg::Result::Success) {
    //     canvas->update();
    // }
  
    //Run animation loop
    elm_transit_duration_set(transit, animation->duration());
    elm_transit_repeat_times_set(transit, -1);
    elm_transit_go(transit);
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
}

void drawSwView(void* data, Eo* obj)
{
    //It's not necessary to clear buffer since it has a solid background
    //swCanvas->clear(false);

    if (swCanvas->draw() == tvg::Result::Success) {
        swCanvas->sync();
        updated = false;
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
/* GL Engine Test Code                                                  */
/************************************************************************/

static unique_ptr<tvg::GlCanvas> glCanvas;

void initGLview(Evas_Object *obj)
{
    static constexpr auto BPP = 4;

    //Create a Canvas
    glCanvas = tvg::GlCanvas::gen();
    glCanvas->target(nullptr, WIDTH * BPP, WIDTH, HEIGHT);

    tvgDrawCmds(glCanvas.get());
}

void drawGLview(Evas_Object *obj)
{
    auto gl = elm_glview_gl_api_get(obj);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    if (glCanvas->draw() == tvg::Result::Success) {
        glCanvas->sync();
    }
}

void transitGlCb(Elm_Transit_Effect *effect, Elm_Transit* transit, double progress)
{
    tvgUpdateCmds(glCanvas.get(), animation.get(), progress);
    elm_glview_changed_set((Evas_Object*)effect);
}


/************************************************************************/
/* Main Code                                                            */
/************************************************************************/

int main(int argc, char **argv)
{
    auto tvgEngine = tvg::CanvasEngine::Sw;

    if (argc > 1) {
        if (!strcmp(argv[1], "gl")) tvgEngine = tvg::CanvasEngine::Gl;
    }

    //Threads Count
    auto threads = std::thread::hardware_concurrency();
    if (threads > 0) --threads;    //Allow the designated main thread capacity

    //Initialize ThorVG Engine
    if (tvg::Initializer::init(threads) == tvg::Result::Success) {

        elm_init(argc, argv);

        transit = elm_transit_add();

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView(1024, 1024);
            elm_transit_effect_add(transit, transitSwCb, view, nullptr);
        } else {
            auto view = createGlView(1024, 1024);
            elm_transit_effect_add(transit, transitGlCb, view, nullptr);
        }

        elm_run();

        elm_transit_del(transit);

        elm_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term();
    } else {
        cout << "engine is not supported" << endl;
    }

    return 0;
}
