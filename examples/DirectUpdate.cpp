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
    tvg::Shape* pShape = nullptr;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Shape (for BG)
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);

        //fill property will be retained
        bg->fill(255, 255, 255);

        canvas->push(std::move(bg));

        //Shape
        auto shape = tvg::Shape::gen();

        /* Acquire shape pointer to access it again.
        instead, you should consider not to interrupt this pointer life-cycle. */
        pShape = shape.get();

        shape->appendRect(-100, -100, 200, 200);

        //fill property will be retained
        shape->fill(127, 255, 255);
        shape->strokeFill(0, 0, 255);
        shape->strokeWidth(1);

        canvas->push(std::move(shape));

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        auto progress = tvgexam::progress(elapsed, 2.0f, true);  //play time 2 sec.

        //Reset Shape
        if (tvgexam::verify(pShape->reset())) {
            pShape->appendRect(-100 + (800 * progress), -100 + (800 * progress), 200, 200, (100 * progress), (100 * progress));
            pShape->fill(127, 255, 255);
            pShape->strokeFill(0, 0, 255);
            pShape->strokeWidth(30 * progress);

            //Update shape for drawing (this may work asynchronously)
            canvas->update(pShape);

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
    return tvgexam::main(new UserExample, argc, argv);
}