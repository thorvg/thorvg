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

    //Load webp file from path
    auto opacity = 31;

    for (int i = 0; i < 7; ++i) {
        auto picture = tvg::Picture::gen();
        if (picture->load(EXAMPLE_DIR"/image/test.webp") != tvg::Result::Success) {
             cout << "WEBP is not supported. Did you enable WEBP Loader?" << endl;
             return;
        }
        picture->translate(i* 150, i * 150);
        picture->rotate(30 * i);
        picture->size(200, 200);
        picture->opacity(opacity + opacity * i);
        if (canvas->push(std::move(picture)) != tvg::Result::Success) return;
    }

    //Open file manually
    ifstream file(EXAMPLE_DIR"/image/test.webp", ios::binary);
    if (!file.is_open()) return;
    auto begin = file.tellg();
    file.seekg(0, std::ios::end);
    auto size = file.tellg() - begin;
    auto data = (char*)malloc(size);
    if (!data) return;
    file.seekg(0, std::ios::beg);
    file.read(data, size);
    file.close();

    auto picture = tvg::Picture::gen();
    if (picture->load(data, size, "webp", "", true) != tvg::Result::Success) {
        cout << "Couldn't load WEBP file from data." << endl;
        return;
    }

    free(data);
    picture->translate(400, 0);
    picture->scale(0.8);
    canvas->push(std::move(picture));
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
