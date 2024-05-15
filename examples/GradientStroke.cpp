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

void tvgDrawCmds(tvg::Canvas* canvas)
{
    if (!canvas) return;

    tvg::Fill::ColorStop colorStops1[3];
    colorStops1[0] = {0, 255, 0, 0, 150};
    colorStops1[1] = {0.5, 0, 0, 255, 150};
    colorStops1[2] = {1, 127, 0, 127, 150};

    tvg::Fill::ColorStop colorStops2[2];
    colorStops2[0] = {0.3, 255, 0, 0, 255};
    colorStops2[1] = {1, 50, 0, 255, 155};

    tvg::Fill::ColorStop colorStops3[2];
    colorStops3[0] = {0, 0, 0, 255, 155};
    colorStops3[1] = {1, 0, 255, 0, 155};

    float dashPattern1[2] = {15, 15};

    // linear gradient stroke + linear gradient fill
    auto shape1 = tvg::Shape::gen();
    shape1->moveTo(150, 100);
    shape1->lineTo(200, 100);
    shape1->lineTo(200, 150);
    shape1->lineTo(300, 150);
    shape1->lineTo(250, 200);
    shape1->lineTo(200, 200);
    shape1->lineTo(200, 250);
    shape1->lineTo(150, 300);
    shape1->lineTo(150, 200);
    shape1->lineTo(100, 200);
    shape1->lineTo(100, 150);
    shape1->close();

    shape1->strokeFill(0, 255, 0);
    shape1->strokeWidth(20);
    shape1->strokeJoin(tvg::StrokeJoin::Miter);
    shape1->strokeCap(tvg::StrokeCap::Butt);

    auto fillStroke1 = tvg::LinearGradient::gen();
    fillStroke1->linear(100, 100, 250, 250);
    fillStroke1->colorStops(colorStops1, 3);
    shape1->strokeFill(std::move(fillStroke1));

    auto fill1 = tvg::LinearGradient::gen();
    fill1->linear(100, 100, 250, 250);
    fill1->colorStops(colorStops1, 3);
    shape1->fill(std::move(fill1));

    if (canvas->push(std::move(shape1)) != tvg::Result::Success) return;

    // radial gradient stroke + duplicate
    auto shape2 = tvg::Shape::gen();
    shape2->appendCircle(600, 175, 100, 60);
    shape2->strokeWidth(80);

    auto fillStroke2 = tvg::RadialGradient::gen();
    fillStroke2->radial(600, 175, 100);
    fillStroke2->colorStops(colorStops2, 2);
    shape2->strokeFill(std::move(fillStroke2));

    auto shape3 = tvg::cast<tvg::Shape>(shape2->duplicate());
    shape3->translate(0, 200);

    auto fillStroke3 = tvg::LinearGradient::gen();
    fillStroke3->linear(500, 115, 700, 235);
    fillStroke3->colorStops(colorStops3, 2);
    shape3->strokeFill(std::move(fillStroke3));

    auto shape4 = tvg::cast<tvg::Shape>(shape2->duplicate());
    shape4->translate(0, 400);

    if (canvas->push(std::move(shape2)) != tvg::Result::Success) return;
    if (canvas->push(std::move(shape3)) != tvg::Result::Success) return;
    if (canvas->push(std::move(shape4)) != tvg::Result::Success) return;

    // dashed gradient stroke
    auto shape5 = tvg::Shape::gen();
    shape5->appendRect(100, 500, 300, 300, 50, 80);

    shape5->strokeWidth(20);
    shape5->strokeDash(dashPattern1, 2);
    shape5->strokeCap(tvg::StrokeCap::Butt);
    auto fillStroke5 = tvg::LinearGradient::gen();
    fillStroke5->linear(150, 450, 450, 750);
    fillStroke5->colorStops(colorStops3, 2);
    shape5->strokeFill(std::move(fillStroke5));

    auto fill5 = tvg::LinearGradient::gen();
    fill5->linear(150, 450, 450, 750);
    fill5->colorStops(colorStops3, 2);
    shape5->fill(std::move(fill5));
    shape5->scale(0.8);

    if (canvas->push(std::move(shape5)) != tvg::Result::Success) return;
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
