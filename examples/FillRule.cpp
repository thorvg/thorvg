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

        //Star
        auto shape1 = tvg::Shape::gen();
        shape1->moveTo(205, 35);
        shape1->lineTo(330, 355);
        shape1->lineTo(25, 150);
        shape1->lineTo(385, 150);
        shape1->lineTo(80, 355);
        shape1->close();
        shape1->fill(255, 255, 255);
        shape1->fill(tvg::FillRule::Winding);  //Fill all winding shapes

        canvas->push(shape1);

        //Star 2
        auto shape2 = tvg::Shape::gen();
        shape2->moveTo(535, 335);
        shape2->lineTo(660, 655);
        shape2->lineTo(355, 450);
        shape2->lineTo(715, 450);
        shape2->lineTo(410, 655);
        shape2->close();
        shape2->fill(255, 255, 255);
        shape2->fill(tvg::FillRule::EvenOdd); //Fill polygons with even odd pattern

        canvas->push(shape2);

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