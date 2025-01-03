/*
 * Copyright (c) 2020 - 2025 the ThorVG project. All rights reserved.

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

#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

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
        shape1->strokeFill(fillStroke1);

        auto fill1 = tvg::LinearGradient::gen();
        fill1->linear(100, 100, 250, 250);
        fill1->colorStops(colorStops1, 3);
        shape1->fill(fill1);

        canvas->push(shape1);

        // radial gradient stroke + duplicate
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(600, 175, 100, 60);
        shape2->strokeWidth(80);

        auto fillStroke2 = tvg::RadialGradient::gen();
        fillStroke2->radial(600, 175, 100, 600, 175, 0);
        fillStroke2->colorStops(colorStops2, 2);
        shape2->strokeFill(fillStroke2);

        auto shape3 = static_cast<tvg::Shape*>(shape2->duplicate());
        shape3->translate(0, 200);

        auto fillStroke3 = tvg::LinearGradient::gen();
        fillStroke3->linear(500, 115, 700, 235);
        fillStroke3->colorStops(colorStops3, 2);
        shape3->strokeFill(fillStroke3);

        auto shape4 = static_cast<tvg::Shape*>(shape2->duplicate());
        shape4->translate(0, 400);

        canvas->push(shape2);
        canvas->push(shape3);
        canvas->push(shape4);

        // dashed gradient stroke
        auto shape5 = tvg::Shape::gen();
        shape5->appendRect(100, 500, 300, 300, 50, 80);

        shape5->strokeWidth(20);
        shape5->strokeDash(dashPattern1, 2);
        shape5->strokeCap(tvg::StrokeCap::Butt);
        auto fillStroke5 = tvg::LinearGradient::gen();
        fillStroke5->linear(150, 450, 450, 750);
        fillStroke5->colorStops(colorStops3, 2);
        shape5->strokeFill(fillStroke5);

        auto fill5 = tvg::LinearGradient::gen();
        fill5->linear(150, 450, 450, 750);
        fill5->colorStops(colorStops3, 2);
        shape5->fill(fill5);
        shape5->scale(0.8);

        canvas->push(shape5);

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv);
}