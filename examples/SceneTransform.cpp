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

        //Create a Scene1
        auto scene = tvg::Scene::gen();

        //Prepare Round Rectangle (Scene1)
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(-235, -250, 400, 400, 50, 50);  //x, y, w, h, rx, ry
        shape1->fill(0, 255, 0);                           //r, g, b
        shape1->strokeWidth(5);                            //width
        shape1->strokeFill(255, 255, 255);                 //r, g, b
        scene->push(shape1);

        //Prepare Circle (Scene1)
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(-165, -150, 200, 200);    //cx, cy, radiusW, radiusH
        shape2->fill(255, 255, 0);                     //r, g, b
        scene->push(shape2);

        //Prepare Ellipse (Scene1)
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(265, 250, 150, 100);      //cx, cy, radiusW, radiusH
        shape3->fill(0, 255, 255);                     //r, g, b
        scene->push(shape3);

        scene->translate(430, 430);
        scene->scale(0.7f);
        scene->rotate(360 * progress);

        //Create Scene2
        auto scene2 = tvg::Scene::gen();

        //Star (Scene2)
        auto shape4 = tvg::Shape::gen();

        //Appends Paths
        shape4->moveTo(0, -114.5);
        shape4->lineTo(54, -5.5);
        shape4->lineTo(175, 11.5);
        shape4->lineTo(88, 95.5);
        shape4->lineTo(108, 216.5);
        shape4->lineTo(0, 160.5);
        shape4->lineTo(-102, 216.5);
        shape4->lineTo(-87, 96.5);
        shape4->lineTo(-173, 12.5);
        shape4->lineTo(-53, -5.5);
        shape4->close();
        shape4->fill(0, 0, 255, 127);
        shape4->strokeWidth(3);                        //width
        shape4->strokeFill(0, 0, 255);                 //r, g, b
        scene2->push(shape4);

        //Circle (Scene2)
        auto shape5 = tvg::Shape::gen();

        auto cx = -150.0f;
        auto cy = -150.0f;
        auto radius = 100.0f;
        auto halfRadius = radius * 0.552284f;

        //Append Paths
        shape5->moveTo(cx, cy - radius);
        shape5->cubicTo(cx + halfRadius, cy - radius, cx + radius, cy - halfRadius, cx + radius, cy);
        shape5->cubicTo(cx + radius, cy + halfRadius, cx + halfRadius, cy + radius, cx, cy+ radius);
        shape5->cubicTo(cx - halfRadius, cy + radius, cx - radius, cy + halfRadius, cx - radius, cy);
        shape5->cubicTo(cx - radius, cy - halfRadius, cx - halfRadius, cy - radius, cx, cy - radius);
        shape5->close();
        shape5->fill(255, 0, 0, 127);
        scene2->push(shape5);

        scene2->translate(500, 350);
        scene2->rotate(360 * progress);

        //Push scene2 onto the scene
        scene->push(scene2);

        //Draw the Scene onto the Canvas
        canvas->push(scene);

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, 960, 960);
}