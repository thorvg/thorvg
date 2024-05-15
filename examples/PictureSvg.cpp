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

#include <limits.h>
#include <fstream>
#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Background
    auto bg = tvg::Shape::gen();
    bg->appendRect(0, 0, WIDTH, HEIGHT);    //x, y, w, h
    bg->fill(255, 255, 255);                //r, g, b
    canvas->push(std::move(bg));

    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), EXAMPLE_DIR"/svg/logo.svg");

    auto picture = tvg::Picture::gen();
    tvg::Result ret = picture->load(buf);
    if (ret == tvg::Result::InvalidArguments) {
        cout << "Path is invalid." << endl;
        return;
    } else if (ret == tvg::Result::Unknown) {
        cout << "Unknown error occurs." << endl;
        return;
    }

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
    picture->translate(shiftX, shiftY);
    picture->scale(scale);

    if (canvas->push(std::move(picture)) != tvg::Result::Success) return;
    cout << "SVG: " << buf << endl;
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
