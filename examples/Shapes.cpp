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

        //Prepare a Composite Shape (Rectangle + Rectangle + Circle + Circle)
        auto shape4 = tvg::Shape::gen();
        shape4->appendRect(0, 0, 300, 300, 50, 50);     //x, y, w, h, rx, ry
        shape4->appendCircle(400, 150, 150, 150);       //cx, cy, radiusW, radiusH
        shape4->appendCircle(600, 150, 150, 100);       //cx, cy, radiusW, radiusH
        shape4->fill(255, 255, 0);                      //r, g, b
        canvas->push(shape4);

        //Prepare Round Rectangle
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(0, 450, 300, 300, 50, 50);  //x, y, w, h, rx, ry
        shape1->fill(0, 255, 0);                       //r, g, b
        canvas->push(shape1);

        //Prepare Circle
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(400, 600, 150, 150);    //cx, cy, radiusW, radiusH
        shape2->fill(255, 255, 0);                   //r, g, b
        canvas->push(shape2);

        //Prepare Ellipse
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(600, 600, 150, 100);    //cx, cy, radiusW, radiusH
        shape3->fill(0, 255, 255);                   //r, g, b
        canvas->push(shape3);

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