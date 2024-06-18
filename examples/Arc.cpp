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

        //Arc Line
        auto shape1 = tvg::Shape::gen();
        shape1->appendArc(150, 150, 80, 10, 180, false);
        shape1->strokeFill(255, 255, 255);
        shape1->strokeWidth(2);
        canvas->push(std::move(shape1));

        auto shape2 = tvg::Shape::gen();
        shape2->appendArc(400, 150, 80, 0, 300, false);
        shape2->strokeFill(255, 255, 255);
        shape2->strokeWidth(2);
        canvas->push(std::move(shape2));

        auto shape3 = tvg::Shape::gen();
        shape3->appendArc(600, 150, 80, 300, 60, false);
        shape3->strokeFill(255, 255, 255);
        shape3->strokeWidth(2);
        canvas->push(std::move(shape3));

        //Pie Line
        auto shape4 = tvg::Shape::gen();
        shape4->appendArc(150, 400, 80, 10, 180, true);
        shape4->strokeFill(255, 255, 255);
        shape4->strokeWidth(2);
        canvas->push(std::move(shape4));

        auto shape5 = tvg::Shape::gen();
        shape5->appendArc(400, 400, 80, 0, 300, true);
        shape5->strokeFill(255, 255, 255);
        shape5->strokeWidth(2);
        canvas->push(std::move(shape5));

        auto shape6 = tvg::Shape::gen();
        shape6->appendArc(600, 400, 80, 300, 60, true);
        shape6->strokeFill(255, 255, 255);
        shape6->strokeWidth(2);
        canvas->push(std::move(shape6));

        //Pie Fill
        auto shape7 = tvg::Shape::gen();
        shape7->appendArc(150, 650, 80, 10, 180, true);
        shape7->fill(255, 255, 255);
        shape7->strokeFill(255, 0, 0);
        shape7->strokeWidth(2);
        canvas->push(std::move(shape7));

        auto shape8 = tvg::Shape::gen();
        shape8->appendArc(400, 650, 80, 0, 300, true);
        shape8->fill(255, 255, 255);
        shape8->strokeFill(255, 0, 0);
        shape8->strokeWidth(2);
        canvas->push(std::move(shape8));

        auto shape9 = tvg::Shape::gen();
        shape9->appendArc(600, 650, 80, 300, 60, true);
        shape9->fill(255, 255, 255);
        shape9->strokeFill(255, 0, 0);
        shape9->strokeWidth(2);
        canvas->push(std::move(shape9));

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