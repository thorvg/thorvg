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
        return update(canvas, 0);
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        if (!tvgexam::verify(canvas->clear())) return false;

        auto progress = tvgexam::progress(elapsed, 2.0f, true);  //play time 2 sec.

        //Shape1
        auto shape = tvg::Shape::gen();
        shape->appendRect(-285, -300, 200, 200);
        shape->appendRect(-185, -200, 300, 300, 100, 100);
        shape->appendCircle(115, 100, 100, 100);
        shape->appendCircle(115, 200, 170, 100);

        //LinearGradient
        auto fill = tvg::LinearGradient::gen();
        fill->linear(-285, -300, 285, 300);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops[3];
        colorStops[0] = {0, 255, 0, 0, 255};
        colorStops[1] = {0.5, 255, 255, 0, 255};
        colorStops[2] = {1, 255, 255, 255, 255};

        fill->colorStops(colorStops, 3);
        shape->fill(std::move(fill));
        shape->translate(385, 400);

        //Update Shape1
        shape->scale(1 - 0.75 * progress);
        shape->rotate(360 * progress);

        canvas->push(std::move(shape));

        //Shape2
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(-50, -50, 100, 100);
        shape2->translate(400, 400);

        //LinearGradient
        auto fill2 = tvg::LinearGradient::gen();
        fill2->linear(-50, -50, 50, 50);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops2[2];
        colorStops2[0] = {0, 0, 0, 0, 255};
        colorStops2[1] = {1, 255, 255, 255, 255};

        fill2->colorStops(colorStops2, 2);
        shape2->fill(std::move(fill2));

        shape2->rotate(360 * progress);
        shape2->translate(400 + progress * 300, 400);

        canvas->push(std::move(shape2));

        //Shape3
        auto shape3 = tvg::Shape::gen();
        /* Look, how shape3's origin is different with shape2
        The center of the shape is the anchor point for transformation. */
        shape3->appendRect(100, 100, 150, 100, 20, 20);

        //RadialGradient
        auto fill3 = tvg::RadialGradient::gen();
        fill3->radial(175, 150, 75, 175, 150, 0);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops3[4];
        colorStops3[0] = {0, 0, 127, 0, 127};
        colorStops3[1] = {0.25, 0, 170, 170, 170};
        colorStops3[2] = {0.5, 200, 0, 200, 200};
        colorStops3[3] = {1, 255, 255, 255, 255};

        fill3->colorStops(colorStops3, 4);

        shape3->fill(std::move(fill3));
        shape3->translate(400, 400);

        //Update Shape3
        shape3->rotate(-360 * progress);
        shape3->scale(0.5 + progress);

        canvas->push(std::move(shape3));

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