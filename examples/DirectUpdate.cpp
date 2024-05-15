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
static tvg::Shape* pShape = nullptr;

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Shape (for BG)
    auto bg = tvg::Shape::gen();
    bg->appendRect(0, 0, WIDTH, HEIGHT);

    //fill property will be retained
    bg->fill(255, 255, 255);

    if (canvas->push(std::move(bg)) != tvg::Result::Success) return;

    //Shape
    auto shape = tvg::Shape::gen();

    /* Acquire shape pointer to access it again.
       instead, you should consider not to interrupt this pointer life-cycle. */
    pShape = shape.get();

    shape->appendRect(-100, -100, 200, 200);

    //fill property will be retained
    shape->fill(127, 255, 255);
    shape->strokeFill(0, 0, 255);
    shape->strokeWidth(1);

    if (canvas->push(std::move(shape)) != tvg::Result::Success) return;

    setUpdate(true);
}

void tvgUpdateCmds(void* data, void* obj, double progress)
{
    if (getUpdate() || !getCanvas()) return;

    //It's not necessary to clear canvas since it has a solid background and retaining the paints.
    //canvas->clear(false);

    //Reset Shape
    if (pShape->reset() == tvg::Result::Success) {
        pShape->appendRect(-100 + (800 * progress), -100 + (800 * progress), 200, 200, (100 * progress), (100 * progress));
        pShape->fill(127, 255, 255);
        pShape->strokeFill(0, 0, 255);
        pShape->strokeWidth(30 * progress);

        //Update shape for drawing (this may work asynchronously)
        getCanvas()->update(pShape);
        setUpdate(true);
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
