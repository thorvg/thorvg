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

#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

struct UserExample : tvgexam::Example
{
    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Shape 1
        auto shape1 = tvg::Shape::gen();
        shape1->appendCircle(245, 125, 50, 120);
        shape1->appendCircle(245, 365, 50, 120);
        shape1->appendCircle(125, 245, 120, 50);
        shape1->appendCircle(365, 245, 120, 50);
        shape1->fill(0, 50, 155, 100);
        shape1->strokeFill(0, 0, 255);
        shape1->strokeJoin(tvg::StrokeJoin::Round);
        shape1->strokeCap(tvg::StrokeCap::Round);
        shape1->strokeWidth(12);
        shape1->strokeTrim(0.0f, 0.5f, false);

        auto shape2 = static_cast<tvg::Shape*>(shape1->duplicate());
        shape2->translate(300, 300);
        shape2->fill(0, 155, 50, 100);
        shape2->strokeFill(0, 255, 0);
        shape2->strokeTrim(0.0f, 0.5f, true);

        canvas->push(shape1);
        canvas->push(shape2);

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