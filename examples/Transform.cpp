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
        return update(canvas, 0);
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        if (!tvgexam::verify(canvas->remove())) return false;

        auto progress = tvgexam::progress(elapsed, 2.0f, true);  //play time 2 sec.

        //Shape1
        auto shape = tvg::Shape::gen();
        shape->appendRect(-285, -300, 200, 200);
        shape->appendRect(-185, -200, 300, 300, 100, 100);
        shape->appendCircle(115, 100, 100, 100);
        shape->appendCircle(115, 200, 170, 100);
        shape->fill(255, 255, 255);
        shape->translate(385, 400);
        shape->scale(1 - 0.75 * progress);
        shape->rotate(360 * progress);

        canvas->push(shape);

        //Shape2
        auto shape2 = tvg::Shape::gen();
        shape2->appendRect(-50, -50, 100, 100);
        shape2->fill(0, 255, 255);
        shape2->translate(400, 400);
        shape2->rotate(360 * progress);
        shape2->translate(400 + progress * 300, 400);
        canvas->push(shape2);

        //Shape3
        auto shape3 = tvg::Shape::gen();

        /* Look, how shape3's origin is different with shape2
        The center of the shape is the anchor point for transformation. */
        shape3->appendRect(100, 100, 150, 50, 20, 20);
        shape3->fill(255, 0, 255);
        shape3->translate(400, 400);
        shape3->rotate(-360 * progress);
        shape3->scale(0.5 + progress);
        canvas->push(shape3);

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