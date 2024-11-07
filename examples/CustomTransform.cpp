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

        //Shape
        auto shape = tvg::Shape::gen();

        shape->moveTo(0, -114.5);
        shape->lineTo(54, -5.5);
        shape->lineTo(175, 11.5);
        shape->lineTo(88, 95.5);
        shape->lineTo(108, 216.5);
        shape->lineTo(0, 160.5);
        shape->lineTo(-102, 216.5);
        shape->lineTo(-87, 96.5);
        shape->lineTo(-173, 12.5);
        shape->lineTo(-53, -5.5);
        shape->close();
        shape->fill(0, 0, 255);
        shape->strokeWidth(3);
        shape->strokeFill(255, 255, 255);

        //Transform Matrix
        tvg::Matrix m = {1, 0, 0, 0, 1, 0, 0, 0, 1};

        //scale x
        m.e11 = 1 - (progress * 0.5f);

        //scale y
        m.e22 = 1 + (progress * 2.0f);

        //rotation
        constexpr auto PI = 3.141592f;
        auto degree = 45.0f;
        auto radian = degree / 180.0f * PI;
        auto cosVal = cosf(radian);
        auto sinVal = sinf(radian);

        auto t11 = m.e11 * cosVal + m.e12 * sinVal;
        auto t12 = m.e11 * -sinVal + m.e12 * cosVal;
        auto t21 = m.e21 * cosVal + m.e22 * sinVal;
        auto t22 = m.e21 * -sinVal + m.e22 * cosVal;
        auto t13 = m.e31 * cosVal + m.e32 * sinVal;
        auto t23 = m.e31 * -sinVal + m.e32 * cosVal;

        m.e11 = t11;
        m.e12 = t12;
        m.e21 = t21;
        m.e22 = t22;
        m.e13 = t13;
        m.e23 = t23;

        //translate
        m.e13 = progress * 300.0f + 300.0f;
        m.e23 = progress * -100.0f + 300.0f;

        shape->transform(m);

        canvas->push(shape);

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