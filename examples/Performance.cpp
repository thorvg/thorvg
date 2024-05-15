/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

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

static tvg::Picture* pPicture = nullptr;

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    auto mask = tvg::Shape::gen();
    mask->appendCircle(WIDTH/2, HEIGHT/2, WIDTH/2, HEIGHT/2);    
    mask->fill(255, 255, 255);
    //Use the opacity for a half-translucent mask.
    mask->opacity(125);

    auto picture = tvg::Picture::gen();
    picture->load(EXAMPLE_DIR"/svg/tiger.svg");
    picture->size(WIDTH, HEIGHT);
    picture->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
    pPicture = picture.get();
    canvas->push(std::move(picture));
}

void tvgUpdateCmds(void* data, void* obj, double progress)
{
    tvg::Canvas* canvas = getCanvas();
    if (!canvas) return;

    canvas->clear(false);

    canvas->clear(false);

    pPicture->translate(WIDTH * progress * 0.05f, HEIGHT * progress * 0.05f);

    auto before = system_time_get();

    canvas->update();

    auto after = system_time_get();

    updateTime = after - before;
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
