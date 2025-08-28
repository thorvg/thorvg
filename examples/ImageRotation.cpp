/*
 * Copyright (c) 2024 - 2025 the ThorVG project. All rights reserved.

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
        return degree * (float(M_PI) / 180.0f);
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        picture = tvg::Picture::gen();
        picture->origin(0.5f, 0.5f);  //center origin
        picture->translate(w/2, h/2);

        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/image/scale.jpg"))) return false;

        canvas->push(picture);

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        picture->scale(0.8f);
        picture->rotate(tvgexam::progress(elapsed, 4.0f) * 360.0f);

        canvas->update();

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, 960 ,960);
}