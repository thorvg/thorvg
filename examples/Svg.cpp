/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

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

#include <limits.h>
#include <vector>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

#define NUM_PER_ROW 8
#define NUM_PER_COL 8
#define SIZE (WIDTH/NUM_PER_ROW)

static int counter = 0;

static std::vector<unique_ptr<tvg::Picture>> pictures;

void svgDirCallback(const char* name, const char* path, void* data)
{
    if (counter >= NUM_PER_ROW * NUM_PER_COL) return;

    //ignore if not svgs.
    const char *ext = name + strlen(name) - 3;
    if (strcmp(ext, "svg")) return;

    auto picture = tvg::Picture::gen();

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "/%s/%s", path, name);

    if (picture->load(buf) != tvg::Result::Success) return;

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

    pictures.push_back(std::move(picture));

    cout << "SVG: " << buf << endl;

    counter++;
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Background
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, WIDTH, HEIGHT);          //x, y, w, h
    shape->fill(255, 255, 255);                      //r, g, b

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    file_dir_list(EXAMPLE_DIR"/svg", true, svgDirCallback, canvas);

    /* This showcase shows you asynchrounous loading of svg.
       For this, pushing pictures at a certian sync time.
       This means it earns the time to finish loading svg resources,
       otherwise you can push pictures immediately. */
    for (auto& paint : pictures) {
        canvas->push(std::move(paint));
    }

    pictures.clear();
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
            createSwView();
        } else {
            createGlView();
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
