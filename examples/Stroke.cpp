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
        //Shape 1
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(50, 50, 200, 200);
        shape1->fill(50, 50, 50);
        shape1->strokeFill(255, 255, 255);            //color: r, g, b
        shape1->strokeJoin(tvg::StrokeJoin::Bevel);   //default is Bevel
        shape1->strokeWidth(10);                       //width: 10px

        canvas->push(shape1);

        //Shape 2
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(300, 50, 200, 200);
        shape2->fill(50, 50, 50);
        shape2->strokeFill(255, 255, 255);
        shape2->strokeJoin(tvg::StrokeJoin::Round);
        shape2->strokeWidth(10);

        canvas->push(shape2);

        //Shape 3
        auto shape3 = tvg::Shape::gen();
        shape3->appendRect(550, 50, 200, 200);
        shape3->fill(50, 50, 50);
        shape3->strokeFill(255, 255, 255);
        shape3->strokeJoin(tvg::StrokeJoin::Miter);
        shape3->strokeWidth(10);

        canvas->push(shape3);

        //Shape 4
        auto shape4 = tvg::Shape::gen();
        shape4->appendCircle(150, 400, 100, 100);
        shape4->fill(50, 50, 50);
        shape4->strokeFill(255, 255, 255);
        shape4->strokeWidth(1);

        canvas->push(shape4);

        //Shape 5
        auto shape5 = tvg::Shape::gen();
        shape5->appendCircle(400, 400, 100, 100);
        shape5->fill(50, 50, 50);
        shape5->strokeFill(255, 255, 255);
        shape5->strokeWidth(2);

        canvas->push(shape5);

        //Shape 6
        auto shape6 = tvg::Shape::gen();
        shape6->appendCircle(650, 400, 100, 100);
        shape6->fill(50, 50, 50);
        shape6->strokeFill(255, 255, 255);
        shape6->strokeWidth(4);

        canvas->push(shape6);

        //Stroke width test
        for (int i = 0; i < 10; ++i) {
            auto hline = tvg::Shape::gen();
            hline->moveTo(50, 550 + (25 * i));
            hline->lineTo(300, 550 + (25 * i));
            hline->strokeFill(255, 255, 255);            //color: r, g, b
            hline->strokeWidth(i + 1);                   //stroke width
            hline->strokeCap(tvg::StrokeCap::Round);     //default is Square
            canvas->push(hline);

            auto vline = tvg::Shape::gen();
            vline->moveTo(500 + (25 * i), 550);
            vline->lineTo(500 + (25 * i), 780);
            vline->strokeFill(255, 255, 255);            //color: r, g, b
            vline->strokeWidth(i + 1);                   //stroke width
            vline->strokeCap(tvg::StrokeCap::Round);     //default is Square
            canvas->push(vline);
        }

        //Stroke cap test
        auto line1 = tvg::Shape::gen();
        line1->moveTo(360, 580);
        line1->lineTo(450, 580);
        line1->strokeFill(255, 255, 255);                //color: r, g, b
        line1->strokeWidth(15);
        line1->strokeCap(tvg::StrokeCap::Round);

        auto line2 = static_cast<tvg::Shape*>(line1->duplicate());
        auto line3 = static_cast<tvg::Shape*>(line1->duplicate());
        canvas->push(line1);

        line2->strokeCap(tvg::StrokeCap::Square);
        line2->translate(0, 50);
        canvas->push(line2);

        line3->strokeCap(tvg::StrokeCap::Butt);
        line3->translate(0, 100);
        canvas->push(line3);

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