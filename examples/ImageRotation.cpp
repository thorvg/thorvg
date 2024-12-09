/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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
#define _USE_MATH_DEFINES

#include <cmath>
#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    tvg::Picture* picture = nullptr;

    float deg2rad(float degree)
    {
        return degree * (M_PI / 180.0f);
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        picture = tvg::Picture::gen();

        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/image/scaledown.jpg"))) return false;

        canvas->push(picture);

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        tvg::Matrix m = {1.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f};

        //center pivoting
        m.e13 += 400;
        m.e23 += 400;

        //rotation
        auto degree = tvgexam::progress(elapsed, 4.0f) * 360.0f;
        auto radian = deg2rad(degree);
        m.e11 = cosf(radian);
        m.e12 = -sinf(radian);
        m.e21 = sinf(radian);
        m.e22 = cosf(radian);

        //scaling
        m.e11 *= 0.75f;
        m.e21 *= 0.75f;
        m.e22 *= 0.75f;
        m.e12 *= 0.75f;

        //center pivoting
        m.e13 += (-400 * m.e11 + -400 * m.e12);
        m.e23 += (-400 * m.e21 + -400 * m.e22);

        picture->transform(m);

        canvas->update();

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true);
}