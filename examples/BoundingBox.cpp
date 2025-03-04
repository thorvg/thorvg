/*
 * Copyright (c) 2025 the ThorVG project. All rights reserved.

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
    void bbox(tvg::Canvas* canvas, tvg::Paint* paint)
    {
        tvg::Point pts[4];
        paint->bounds(pts);

        auto bound = tvg::Shape::gen();
        bound->moveTo(pts[0].x, pts[0].y);
        bound->lineTo(pts[1].x, pts[1].y);
        bound->lineTo(pts[2].x, pts[2].y);
        bound->lineTo(pts[3].x, pts[3].y);
        bound->close();
        bound->strokeWidth(2.0f);
        bound->strokeFill(255, 255, 0);

        canvas->push(bound);
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        {
            auto shape = tvg::Shape::gen();
            shape->appendCircle(50, 100, 40, 100);
            shape->fill(0, 30, 255);
            canvas->push(shape);
            bbox(canvas, shape);
        }

        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(120, 70, 100, 20);
            shape->fill(100, 250, 155);
            canvas->push(shape);
            bbox(canvas, shape);
        }

        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(120, 170, 100, 20);
            shape->fill(200, 150, 55);
            shape->rotate(30);
            canvas->push(shape);
            bbox(canvas, shape);
        }

        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(450, 50, 150, 100, 40, 50);
            shape->appendCircle(550, 300, 200, 100);
            shape->fill(50, 50, 155);
            canvas->push(shape);
            bbox(canvas, shape);
        }

        {
            auto svg = tvg::Picture::gen();
            svg->load(EXAMPLE_DIR"/svg/tiger.svg");
            svg->scale(0.3f);
            svg->translate(500, 500);
            canvas->push(svg);
            bbox(canvas, svg);
        }

        {
            auto svg = tvg::Picture::gen();
            svg->load(EXAMPLE_DIR"/svg/tiger.svg");
            svg->scale(0.2f);
            svg->translate(170, 300);
            svg->rotate(45);
            canvas->push(svg);
            bbox(canvas, svg);

        }

        {
            auto img = tvg::Picture::gen();
            img->load(EXAMPLE_DIR"/image/test.png");
            img->scale(0.3f);
            img->translate(150, 100);
            canvas->push(img);
            bbox(canvas, img);
        }

        {
            auto img = tvg::Picture::gen();
            img->load(EXAMPLE_DIR"/image/test.jpg");
            img->scale(0.3f);
            img->rotate(80);
            img->translate(200, 600);
            canvas->push(img);
            bbox(canvas, img);
        }

        {
            auto line = tvg::Shape::gen();            
            line->moveTo(400, 450);
            line->lineTo(700, 450);
            line->strokeWidth(20);
            line->strokeFill(55, 55, 0);
            canvas->push(line);
            bbox(canvas, line);
        }

        {
            auto curve = tvg::Shape::gen();            
            curve->moveTo(0, 0);
            curve->cubicTo(40.0f, -10.0f, 120.0f, -150.0f, 80.0f, 0.0f);
            curve->translate(250, 700);
            curve->strokeWidth(2.0f);
            curve->strokeFill(255, 255, 255);
            canvas->push(curve);
            bbox(canvas, curve);
        }

        {
            auto curve = tvg::Shape::gen();            
            curve->moveTo(0, 0);
            curve->cubicTo(40.0f, -10.0f, 120.0f, -150.0f, 80.0f, 0.0f);
            curve->translate(350, 700);
            curve->rotate(20.0f);
            curve->strokeWidth(2.0f);
            curve->strokeFill(255, 0, 255);
            canvas->push(curve);
            bbox(canvas, curve);
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