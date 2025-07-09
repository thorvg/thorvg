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
        //Prepare Round Rectangle
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(0, 0, 400, 400);    //x, y, w, h

        //RadialGradient
        auto fill = tvg::RadialGradient::gen();
        fill->radial(200, 200, 200, 200, 200, 0);   //cx, cy, r, fx, fy, fr

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops[2];
        colorStops[0] = {0, 255, 255, 255, 255};
        colorStops[1] = {1, 0, 0, 0, 255};

        fill->colorStops(colorStops, 2);

        shape1->fill(fill);
        canvas->push(shape1);

        //Prepare Circle
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(400, 400, 200, 200);    //cx, cy, radiusW, radiusH

        //RadialGradient
        auto fill2 = tvg::RadialGradient::gen();
        fill2->radial(400, 400, 200, 400, 400, 0);   //cx, cy, r, fx, fy, fr

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops2[3];
        colorStops2[0] = {0, 255, 0, 0, 255};
        colorStops2[1] = {0.5, 255, 255, 0, 255};
        colorStops2[2] = {1, 255, 255, 255, 255};

        fill2->colorStops(colorStops2, 3);

        shape2->fill(fill2);
        canvas->push(shape2);

        //Prepare Ellipse
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(600, 600, 150, 100);    //cx, cy, radiusW, radiusH

        //RadialGradient
        auto fill3 = tvg::RadialGradient::gen();
        fill3->radial(600, 600, 150, 700, 600, 20);   //cx, cy, r, fx, fy, fr

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops3[4];
        colorStops3[0] = {0, 0, 127, 0, 127};
        colorStops3[1] = {0.25, 0, 170, 170, 170};
        colorStops3[2] = {0.5, 200, 0, 200, 200};
        colorStops3[3] = {1, 255, 255, 255, 255};

        fill3->colorStops(colorStops3, 4);

        shape3->fill(fill3);
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