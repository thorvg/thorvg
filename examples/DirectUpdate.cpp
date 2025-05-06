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
    tvg::Shape* solid = nullptr;
    tvg::Shape* gradient = nullptr;

    uint32_t w, h;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Shape (for BG)
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(255, 255, 255);
        canvas->push(bg);

        //Solid Shape
        {
            solid = tvg::Shape::gen();
            solid->appendRect(-100, -100, 200, 200);

            //fill property will be retained
            solid->fill(127, 255, 255);
            solid->strokeFill(0, 0, 255);
            solid->strokeWidth(1);

            canvas->push(solid);
        }

        //Gradient Shape
        {
            gradient = tvg::Shape::gen();
            gradient->appendRect(w - 200, 0, 200, 200);

            //LinearGradient
            auto fill = tvg::LinearGradient::gen();
            fill->linear(w - 200, 0, w - 200 + 285, 300);

            //Gradient Color Stops
            tvg::Fill::ColorStop colorStops[3];
            colorStops[0] = {0, 255, 0, 0, 127};
            colorStops[1] = {0.5, 255, 255, 0, 127};
            colorStops[2] = {1, 255, 255, 255, 127};

            fill->colorStops(colorStops, 3);
            gradient->fill(fill);

            canvas->push(gradient);
        }

        this->w = w;
        this->h = h;

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        auto progress = tvgexam::progress(elapsed, 2.0f, true);  //play time 2 sec.

        //Reset Shape
        if (tvgexam::verify(solid->reset())) {
            //Solid Shape
            solid->appendRect(-100 + (w * progress), -100 + (h * progress), 200, 200, (100 * progress), (100 * progress));
            solid->strokeWidth(30 * progress);

            //Gradient Shape
            gradient->translate(-(w * progress), (h * progress));

            canvas->update();

            return true;
        }

        return false;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 960, 960);
}