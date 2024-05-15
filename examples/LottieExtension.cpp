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

#include <stdio.h>
#include <limits.h>
#include "Common.h"
#include <thorvg_lottie.h>
#include <vector>

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

#define NUM_PER_ROW 2
#define NUM_PER_COL 1
#define SIZE (WIDTH/NUM_PER_ROW)

static int counter = 0;
static std::vector<unique_ptr<tvg::LottieAnimation>> animations;

void lottieSlotCmds(tvg::LottieAnimation* animation)
{
    const char* slotJson = R"({"gradient_fill":{"p":{"a":0,"k":[0,0.1,0.1,0.2,1,1,0.1,0.2,0.1,1]}}})";
    if (animation->override(slotJson) == tvg::Result::Success) {
        getCanvas()->update();
    } else {
        cout << "Failed to override the slot" << endl;
    }
}

void lottieMarkerCmds(tvg::LottieAnimation* animation)
{
    if (animation->segment("sectionC") != tvg::Result::Success) {
        cout << "Failed to segment by the marker" << endl;
    }
}

void lottieDirCallback(const char* name, const char* path, void* data)
{
    if (counter >= NUM_PER_ROW * NUM_PER_COL) return;

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "/%s/%s", path, name);

    //Animation Controller
    auto animation = tvg::LottieAnimation::gen();
    auto picture = animation->picture();

    if (picture->load(buf) != tvg::Result::Success) {
        cout << "Lottie is not supported. Did you enable Lottie Loader?" << endl;
        return;
    }
    
    getCanvas()->push(tvg::cast(picture));

    if (!strcmp(name, "slotsample.json")) lottieSlotCmds(animation.get());
    else if (!strcmp(name, "marker_sample.json")) lottieMarkerCmds(animation.get());
    else return;
    
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

void tvgUpdateCmds(void* data, void* obj, double progress)
{
    auto animation = static_cast<tvg::Animation*>(data);
    animation->frame(animation->totalFrame() * progress);
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);
    shape->fill(75, 75, 75);

    if (getCanvas()->push(std::move(shape)) != tvg::Result::Success) return;

    file_dir_list(EXAMPLE_DIR"/lottie/extensions", false, lottieDirCallback, canvas);

    //Run animation loop
    for (auto& animation : animations) {
        addAnimatorTransit(animation->duration(), -1, tvgUpdateCmds, animation.get());
    }
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

        plat_init(argc, argv);

        if (tvgEngine == tvg::CanvasEngine::Sw) {
            auto view = createSwView();
            setAnimatorSw(view);
        } else {
            auto view = createGlView();
            setAnimatorGl(view);
        }

        plat_run();
        plat_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }

    return 0;
}
