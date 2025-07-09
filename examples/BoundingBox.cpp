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
        //aabb
        {
            float x, y, w, h;
            paint->bounds(&x, &y, &w, &h);

            auto bound = tvg::Shape::gen();
            bound->moveTo(x, y);
            bound->lineTo(x + w, y);
            bound->lineTo(x + w, y + h);
            bound->lineTo(x, y + h);
            bound->close();
            bound->strokeWidth(2.0f);
            bound->strokeFill(255, 0, 0, 255);

            canvas->push(bound);
        }

        //obb
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
            float dash[] = {3.0f, 10.0f};
            bound->strokeDash(dash, 2);
            bound->strokeFill(255, 255, 255, 255);

            canvas->push(bound);
        }
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        {
            auto shape = tvg::Shape::gen();
            shape->appendCircle(50, 100, 40, 100);
            shape->fill(0, 30, 255);
            canvas->push(shape);
            bbox(canvas, shape);
        }

        {
            if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf"))) return false;
            auto text = tvg::Text::gen();
            text->font("Arial", 30);
            text->text("Text Test");
            text->fill(255, 255, 0);
            text->translate(100, 20);
            text->rotate(16.0f);
            canvas->push(text);
            bbox(canvas, text);
        }

        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(200, 30, 100, 20);
            shape->fill(200, 150, 55);
            shape->rotate(30);
            canvas->push(shape);
            bbox(canvas, shape);
        }

        {
            auto shape = tvg::Shape::gen();
            shape->appendRect(450, -100, 150, 100, 40, 50);
            shape->appendCircle(450, 50, 100, 50);
            shape->fill(50, 50, 155);
            shape->rotate(20.0f);
            canvas->push(shape);
            bbox(canvas, shape);
        }

        {
            auto svg = tvg::Picture::gen();
            svg->load(EXAMPLE_DIR"/svg/tiger.svg");
            svg->scale(0.3f);
            svg->translate(620, 50);
            canvas->push(svg);
            bbox(canvas, svg);
        }

        {
            auto svg = tvg::Picture::gen();
            svg->load(EXAMPLE_DIR"/svg/tiger.svg");
            svg->scale(0.2f);
            svg->translate(140, 215);
            svg->rotate(45);
            canvas->push(svg);
            bbox(canvas, svg);

        }

        {
            auto scene = tvg::Scene::gen();
            scene->scale(0.3f);
            scene->translate(280, 330);

            auto img = tvg::Picture::gen();
            img->load(EXAMPLE_DIR"/image/test.png");
            scene->push(img);

            canvas->push(scene);
            bbox(canvas, scene);
        }

        {
            auto scene = tvg::Scene::gen();
            scene->scale(0.3f);
            scene->rotate(80);
            scene->translate(200, 480);

            auto img = tvg::Picture::gen();
            img->load(EXAMPLE_DIR"/image/test.jpg");
            scene->push(img);

            canvas->push(scene);
            bbox(canvas, scene);
        }

        {
            auto line = tvg::Shape::gen();            
            line->moveTo(470, 350);
            line->lineTo(770, 350);
            line->strokeWidth(20);
            line->strokeFill(55, 55, 0);
            canvas->push(line);
            bbox(canvas, line);
        }

        {
            auto curve = tvg::Shape::gen();            
            curve->moveTo(0, 0);
            curve->cubicTo(40.0f, -10.0f, 120.0f, -150.0f, 80.0f, 0.0f);
            curve->translate(50, 770);
            curve->strokeWidth(2.0f);
            curve->strokeFill(255, 255, 255);
            canvas->push(curve);
            bbox(canvas, curve);
        }

        {
            auto curve = tvg::Shape::gen();            
            curve->moveTo(0, 0);
            curve->cubicTo(40.0f, -10.0f, 120.0f, -150.0f, 80.0f, 0.0f);
            curve->translate(150, 750);
            curve->rotate(20.0f);
            curve->strokeWidth(2.0f);
            curve->strokeFill(255, 0, 255);
            canvas->push(curve);
            bbox(canvas, curve);
        }

        {
            auto scene = tvg::Scene::gen();
            scene->translate(550, 370);
            scene->scale(0.7f);

            auto shape = tvg::Shape::gen();
            shape->moveTo(0, 0);
            shape->lineTo(300, 200);
            shape->lineTo(0, 200);
            shape->fill(255, 0, 0);
            shape->close();
            shape->rotate(20);
            scene->push(shape);

            canvas->push(scene);
            bbox(canvas, scene);
        }

        {
            auto scene = tvg::Scene::gen();
            scene->translate(350, 590);
            scene->scale(0.7f);

            auto shape = tvg::Shape::gen();
            shape->moveTo(0, 0);
            shape->lineTo(300, 200);
            shape->lineTo(0, 200);
            shape->fill(0, 255, 0);
            shape->close();
            scene->push(shape);

            canvas->push(scene);
            bbox(canvas, scene);
        }

        {
            auto scene = tvg::Scene::gen();
            scene->translate(650, 590);
            scene->scale(0.7f);
            scene->rotate(20);

            auto shape = tvg::Shape::gen();
            shape->moveTo(0, 0);
            shape->lineTo(300, 200);
            shape->lineTo(0, 200);
            shape->fill(0, 255, 255);
            shape->close();
            scene->push(shape);

            canvas->push(scene);
            bbox(canvas, scene);
        }

        {
            auto scene = tvg::Scene::gen();
            scene->translate(790, 390);
            scene->scale(0.5f);
            scene->rotate(20);

            auto shape = tvg::Shape::gen();
            shape->moveTo(0, 0);
            shape->lineTo(300, 200);
            shape->lineTo(0, 200);
            shape->fill(255, 0, 255);
            shape->close();
            shape->rotate(20);
            scene->push(shape);

            canvas->push(scene);
            bbox(canvas, scene);
        }

        {
            auto scene = tvg::Scene::gen();
            scene->translate(250, 490);
            scene->scale(0.7f);

            auto text = tvg::Text::gen();
            text->font("Arial", 50);
            text->text("Text Test");
            text->fill(255, 255, 0);
            text->translate(0, 0);
            text->rotate(16.0f);
            scene->push(text);

            canvas->push(scene);
            bbox(canvas, scene);
        }

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, 900, 900);
}