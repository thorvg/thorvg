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

#include "Common.h"

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgUpdateCmds(void* data, void* obj, double progress)
{
    tvg::Canvas * canvas = getCanvas();

    if (!canvas) return;

    if (canvas->clear() != tvg::Result::Success) return;

    //Shape1
    auto shape = tvg::Shape::gen();
    shape->appendRect(-285, -300, 200, 200);
    shape->appendRect(-185, -200, 300, 300, 100, 100);
    shape->appendCircle(115, 100, 100, 100);
    shape->appendCircle(115, 200, 170, 100);
    shape->fill(255, 255, 255);
    shape->translate(385, 400);
    shape->scale(1 - 0.75 * progress);
    shape->rotate(360 * progress);

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    //Shape2
    auto shape2 = tvg::Shape::gen();
    shape2->appendRect(-50, -50, 100, 100);
    shape2->fill(0, 255, 255);
    shape2->translate(400, 400);
    shape2->rotate(360 * progress);
    shape2->translate(400 + progress * 300, 400);
    if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;

    //Shape3
    auto shape3 = tvg::Shape::gen();

    /* Look, how shape3's origin is different with shape2
       The center of the shape is the anchor point for transformation. */
    shape3->appendRect(100, 100, 150, 50, 20, 20);
    shape3->fill(255, 0, 255);
    shape3->translate(400, 400);
    shape3->rotate(-360 * progress);
    shape3->scale(0.5 + progress);
    if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;
}

void tvgDrawCmds(tvg::Canvas* canvas)
{
    tvgUpdateCmds(NULL, NULL, 0);
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

        auto transit = addAnimatorTransit(2, -1, tvgUpdateCmds, NULL);
        setAnimatorTransitAutoReverse(transit, true);

        plat_run();

        delAnimatorTransit(transit);

        plat_shutdown();

        //Terminate ThorVG Engine
        tvg::Initializer::term();

    } else {
        cout << "engine is not supported" << endl;
    }
    return 0;
}
