/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

    /* Using a single shape is sufficient to clip multiple regions.
    The disabled code below performs the same function. Please try it. */
    #if 1
        //A scene clipper
        auto clipper = tvg::Scene::gen();

        auto shape1 = tvg::Shape::gen();
        shape1->appendCircle(200, 200, 100, 100);
        clipper->push(std::move(shape1));

        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(400, 400, 150, 150);
        clipper->push(std::move(shape2));

        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(150, 300, 60, 60);
        clipper->push(std::move(shape3));

        auto shape4 = tvg::Shape::gen();
        shape4->appendCircle(400, 100, 125, 125);
        clipper->push(std::move(shape4));

        auto shape5 = tvg::Shape::gen();
        shape5->appendCircle(150, 500, 100, 100);
        clipper->push(std::move(shape5));
    #else
        //A single clipper with multiple regions
        auto clipper = tvg::Shape::gen();
        clipper->appendCircle(200, 200, 100, 100);
        clipper->appendCircle(400, 400, 150, 150);
        clipper->appendCircle(150, 300, 60, 60);
        clipper->appendCircle(400, 100, 125, 125);
        clipper->appendCircle(150, 500, 100, 100);
    #endif

        //Source
        auto shape = tvg::Shape::gen();
        shape->appendRect(100, 100, 400, 400, 50, 50);
        shape->stroke(0, 0, 255);
        shape->stroke(10);
        shape->fill(255, 255, 255);
        shape->composite(std::move(clipper), tvg::CompositeMethod::ClipPath);
        canvas->push(std::move(shape));

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