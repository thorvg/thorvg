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
#include <fstream>

/************************************************************************/
/* Drawing Commands                                                     */
/************************************************************************/

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    //Solid Rectangle
    auto shape = tvg::Shape::gen();
    shape->appendRect(0, 0, 400, 400);
    shape->fill(0, 0, 255);

    //Mask
    auto mask = tvg::Shape::gen();
    mask->appendCircle(200, 200, 125, 125);
    mask->fill(255, 255, 255);        //AlphaMask RGB channels are unused.

    //Nested Mask
    auto nMask = tvg::Shape::gen();
    nMask->appendCircle(220, 220, 125, 125);
    nMask->fill(255, 255, 255);       //AlphaMask RGB channels are unused.

    mask->composite(std::move(nMask), tvg::CompositeMethod::AlphaMask);
    shape->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
    canvas->push(std::move(shape));

    //SVG
    auto svg = tvg::Picture::gen();
    if (svg->load(EXAMPLE_DIR"/svg/cartman.svg") != tvg::Result::Success) return;
    svg->opacity(100);
    svg->scale(3);
    svg->translate(50, 400);

    //Mask2
    auto mask2 = tvg::Shape::gen();
    mask2->appendCircle(150, 500, 75, 75);
    mask2->appendRect(150, 500, 200, 200, 30, 30);
    mask2->fill(255, 255, 255);       //AlphaMask RGB channels are unused.
    svg->composite(std::move(mask2), tvg::CompositeMethod::AlphaMask);
    if (canvas->push(std::move(svg)) != tvg::Result::Success) return;

    //Star
    auto star = tvg::Shape::gen();
    star->fill(80, 80, 80);
    star->moveTo(599, 34);
    star->lineTo(653, 143);
    star->lineTo(774, 160);
    star->lineTo(687, 244);
    star->lineTo(707, 365);
    star->lineTo(599, 309);
    star->lineTo(497, 365);
    star->lineTo(512, 245);
    star->lineTo(426, 161);
    star->lineTo(546, 143);
    star->close();
    star->strokeWidth(30);
    star->strokeJoin(tvg::StrokeJoin::Miter);
    star->strokeFill(255, 255, 255);

    //Mask3
    auto mask3 = tvg::Shape::gen();
    mask3->appendCircle(600, 200, 125, 125);
    mask3->fill(255, 255, 255);       //AlphaMask RGB channels are unused.
    mask3->opacity(200);
    star->composite(std::move(mask3), tvg::CompositeMethod::AlphaMask);
    if (canvas->push(std::move(star)) != tvg::Result::Success) return;

    //Image
    ifstream file(EXAMPLE_DIR"/image/rawimage_200x300.raw", ios::binary);
    if (!file.is_open()) return;
    auto data = (uint32_t*) malloc(sizeof(uint32_t) * (200 * 300));
    file.read(reinterpret_cast<char*>(data), sizeof (uint32_t) * 200 * 300);
    file.close();

    auto image = tvg::Picture::gen();
    if (image->load(data, 200, 300, true, true) != tvg::Result::Success) return;
    image->translate(500, 400);

    //Mask4
    auto mask4 = tvg::Shape::gen();
    mask4->moveTo(599, 384);
    mask4->lineTo(653, 493);
    mask4->lineTo(774, 510);
    mask4->lineTo(687, 594);
    mask4->lineTo(707, 715);
    mask4->lineTo(599, 659);
    mask4->lineTo(497, 715);
    mask4->lineTo(512, 595);
    mask4->lineTo(426, 511);
    mask4->lineTo(546, 493);
    mask4->close();
    mask4->fill(255, 255, 255);        //AlphaMask RGB channels are unused.
    mask4->opacity(70);
    image->composite(std::move(mask4), tvg::CompositeMethod::AlphaMask);
    if (canvas->push(std::move(image)) != tvg::Result::Success) return;

    free(data);
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
