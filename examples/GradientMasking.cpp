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

        //Solid Rectangle
        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(0, 0, 400, 400);

            //Mask
            auto mask = tvg::Shape::gen();
            mask->appendCircle(200, 200, 125, 125);
            mask->fill(255, 0, 0);

            auto fill = tvg::LinearGradient::gen();
            fill->linear(0, 0, 400, 400);
            tvg::Fill::ColorStop colorStops[2];
            colorStops[0] = {0,0,0,0,255};
            colorStops[1] = {1,255,255,255,255};
            fill->colorStops(colorStops,2);
            shape->fill(fill);

            shape->mask(mask, tvg::MaskMethod::Alpha);
            canvas->push(shape);
        }

        //Star
        {
            auto shape1 = tvg::Shape::gen();
            shape1->moveTo(599, 34);
            shape1->lineTo(653, 143);
            shape1->lineTo(774, 160);
            shape1->lineTo(687, 244);
            shape1->lineTo(707, 365);
            shape1->lineTo(599, 309);
            shape1->lineTo(497, 365);
            shape1->lineTo(512, 245);
            shape1->lineTo(426, 161);
            shape1->lineTo(546, 143);
            shape1->close();

            //Mask
            auto mask1 = tvg::Shape::gen();
            mask1->appendCircle(600, 200, 125, 125);
            mask1->fill(255, 0, 0);

            auto fill1 = tvg::LinearGradient::gen();
            fill1->linear(400, 0, 800, 400);
            tvg::Fill::ColorStop colorStops1[2];
            colorStops1[0] = {0,0,0,0,255};
            colorStops1[1] = {1,1,255,255,255};
            fill1->colorStops(colorStops1,2);
            shape1->fill(fill1);

            shape1->mask(mask1, tvg::MaskMethod::Alpha);
            canvas->push(shape1);
        }

        //Solid Rectangle
        {
            auto shape2 = tvg::Shape::gen();
            shape2->appendRect(0, 400, 400, 400);

            //Mask
            auto mask2 = tvg::Shape::gen();
            mask2->appendCircle(200, 600, 125, 125);
            mask2->fill(255, 0, 0);

            auto fill2 = tvg::LinearGradient::gen();
            fill2->linear(0, 400, 400, 800);
            tvg::Fill::ColorStop colorStops2[2];
            colorStops2[0] = {0,0,0,0,255};
            colorStops2[1] = {1,255,255,255,255};
            fill2->colorStops(colorStops2,2);
            shape2->fill(fill2);

            shape2->mask(mask2, tvg::MaskMethod::InvAlpha);
            canvas->push(shape2);
        }

        // Star
        {
            auto shape3 = tvg::Shape::gen();
            shape3->moveTo(599, 434);
            shape3->lineTo(653, 543);
            shape3->lineTo(774, 560);
            shape3->lineTo(687, 644);
            shape3->lineTo(707, 765);
            shape3->lineTo(599, 709);
            shape3->lineTo(497, 765);
            shape3->lineTo(512, 645);
            shape3->lineTo(426, 561);
            shape3->lineTo(546, 543);
            shape3->close();

            //Mask
            auto mask3 = tvg::Shape::gen();
            mask3->appendCircle(600, 600, 125, 125);
            mask3->fill(255, 0, 0);

            auto fill3 = tvg::LinearGradient::gen();
            fill3->linear(400, 400, 800, 800);
            tvg::Fill::ColorStop colorStops3[2];
            colorStops3[0] = {0,0,0,0,255};
            colorStops3[1] = {1,1,255,255,255};
            fill3->colorStops(colorStops3,2);
            shape3->fill(fill3);

            shape3->mask(mask3, tvg::MaskMethod::InvAlpha);
            canvas->push(shape3);
        }

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