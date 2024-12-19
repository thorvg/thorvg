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

#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Line 1 (0 length and available stroke width - draw circle 25x25)
        auto line1 = tvg::Shape::gen();
        line1->moveTo(150, 150);
        line1->lineTo(150, 150);
        line1->strokeFill(255, 255, 255);
        line1->strokeWidth(25);
        line1->strokeCap(tvg::StrokeCap::Round);

        canvas->push(line1);

        //Line 2 (10 length and available stroke width - draw ellipse 35x25)
        auto line2 = tvg::Shape::gen();
        line2->moveTo(200, 150);
        line2->lineTo(210, 150);
        line2->strokeFill(255, 255, 255);
        line2->strokeWidth(25);
        line2->strokeCap(tvg::StrokeCap::Round);

        canvas->push(line2);

        //Line 3 (0 length and available stroke width - draw rectangle 25x25)
        auto line3 = tvg::Shape::gen();
        line3->moveTo(150, 200);
        line3->lineTo(150, 200);
        line3->strokeFill(255, 255, 255);
        line3->strokeWidth(25);
        line3->strokeCap(tvg::StrokeCap::Square);

        canvas->push(line3);

        //Line 4 (10 length and available stroke width - draw rectangle 35x25)
        auto line4 = tvg::Shape::gen();
        line4->moveTo(200, 200);
        line4->lineTo(210, 200);
        line4->strokeFill(255, 255, 255);
        line4->strokeWidth(25);
        line4->strokeCap(tvg::StrokeCap::Square);

        canvas->push(line4);

        //Line 5 (0 length and available stroke width - don't draw anything)
        auto line5 = tvg::Shape::gen();
        line5->moveTo(150, 250);
        line5->lineTo(150, 250);
        line5->strokeFill(255, 255, 255);
        line5->strokeWidth(25);
        line5->strokeCap(tvg::StrokeCap::Butt);

        canvas->push(line5);

        //Line 6 (10 length and available stroke width - draw rectangle 10x25)
        auto line6 = tvg::Shape::gen();
        line6->moveTo(200, 250);
        line6->lineTo(210, 250);
        line6->strokeFill(255, 255, 255);
        line6->strokeWidth(25);
        line6->strokeCap(tvg::StrokeCap::Butt);

        canvas->push(line6);

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