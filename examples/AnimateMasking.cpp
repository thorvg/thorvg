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

static tvg::Shape *pMaskShape = nullptr;
static tvg::Shape *pMask = nullptr;


void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    // background
    auto bg = tvg::Shape::gen();
    bg->appendRect(0, 0, WIDTH, HEIGHT);
    bg->fill(255, 255, 255);
    canvas->push(std::move(bg));

    // image
    auto picture1 = tvg::Picture::gen();
    picture1->load(EXAMPLE_DIR"/svg/cartman.svg");
    picture1->size(400, 400);
    canvas->push(std::move(picture1));

    auto picture2 = tvg::Picture::gen();
    picture2->load(EXAMPLE_DIR"/svg/logo.svg");
    picture2->size(400, 400);

    //mask
    auto maskShape = tvg::Shape::gen();
    pMaskShape = maskShape.get();
    maskShape->appendCircle(180, 180, 75, 75);
    maskShape->fill(125, 125, 125);
    maskShape->strokeFill(25, 25, 25);
    maskShape->strokeJoin(tvg::StrokeJoin::Round);
    maskShape->strokeWidth(10);
    canvas->push(std::move(maskShape));

    auto mask = tvg::Shape::gen();
    pMask = mask.get();
    mask->appendCircle(180, 180, 75, 75);
    mask->fill(255, 255, 255);         //AlphaMask RGB channels are unused.

    picture2->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
    if (canvas->push(std::move(picture2)) != tvg::Result::Success) return;

    setUpdate(true);
}

void tvgUpdateCmds(void* data, void* obj, double progress)
{
    if (!getCanvas()) return;

    /* Update shape directly.
       You can update only necessary properties of this shape,
       while retaining other properties. */

    // Translate mask object with its stroke & update
    pMaskShape->translate(0 , progress * 300 - 100);
    pMask->translate(0 , progress * 300 - 100);


    getCanvas()->update();
    setUpdate(true);
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

        auto transit = addAnimatorTransit(5, -1, tvgUpdateCmds, NULL);
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
