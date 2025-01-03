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
        if (!canvas) return false;

        //Duplicate Shapes
        {
            //Original Shape
            auto shape1 = tvg::Shape::gen();
            shape1->appendRect(10, 10, 200, 200);
            shape1->appendRect(220, 10, 100, 100);

            shape1->strokeWidth(3);
            shape1->strokeFill(0, 255, 0);

            float dashPattern[2] = {4, 4};
            shape1->strokeDash(dashPattern, 2);
            shape1->fill(255, 0, 0);

            //Duplicate Shape, Switch fill method
            auto shape2 = static_cast<tvg::Shape*>(shape1->duplicate());
            shape2->translate(0, 220);

            auto fill = tvg::LinearGradient::gen();
            fill->linear(10, 10, 440, 200);

            tvg::Fill::ColorStop colorStops[2];
            colorStops[0] = {0, 0, 0, 0, 255};
            colorStops[1] = {1, 255, 255, 255, 255};
            fill->colorStops(colorStops, 2);

            shape2->fill(fill);

            //Duplicate Shape 2
            auto shape3 = shape2->duplicate();
            shape3->translate(0, 440);

            canvas->push(shape1);
            canvas->push(shape2);
            canvas->push(shape3);
        }

        //Duplicate Scene
        {
            //Create a Scene1
            auto scene1 = tvg::Scene::gen();

            auto shape1 = tvg::Shape::gen();
            shape1->appendRect(0, 0, 400, 400, 50, 50);
            shape1->fill(0, 255, 0);
            scene1->push(shape1);

            auto shape2 = tvg::Shape::gen();
            shape2->appendCircle(400, 400, 200, 200);
            shape2->fill(255, 255, 0);
            scene1->push(shape2);

            auto shape3 = tvg::Shape::gen();
            shape3->appendCircle(600, 600, 150, 100);
            shape3->fill(0, 255, 255);
            scene1->push(shape3);

            scene1->scale(0.25);
            scene1->translate(400, 0);

            //Duplicate Scene1
            auto scene2 = scene1->duplicate();
            scene2->translate(600, 0);

            canvas->push(scene1);
            canvas->push(scene2);
        }

        //Duplicate Picture - svg
        {
            auto picture1 = tvg::Picture::gen();
            if (!tvgexam::verify(picture1->load(EXAMPLE_DIR"/svg/tiger.svg"))) return false;
            picture1->translate(350, 200);
            picture1->scale(0.25);

            auto picture2 = picture1->duplicate();
            picture2->translate(550, 250);

            canvas->push(picture1);
            canvas->push(picture2);
        }

        //Duplicate Picture - raw
        {
            string path(EXAMPLE_DIR"/image/rawimage_200x300.raw");
            ifstream file(path, ios::binary);
            if (!file.is_open()) return false;
            uint32_t* data = (uint32_t*)malloc(sizeof(uint32_t) * 200 * 300);
            file.read(reinterpret_cast<char*>(data), sizeof(uint32_t) * 200 * 300);
            file.close();

            auto picture1 = tvg::Picture::gen();
            if (!tvgexam::verify(picture1->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            picture1->scale(0.8);
            picture1->translate(400, 450);

            auto picture2 = picture1->duplicate();
            picture2->translate(600, 550);
            picture2->scale(0.7);
            picture2->rotate(8);

            canvas->push(picture1);
            canvas->push(picture2);

            free(data);
        }

        //Duplicate Text
        {
            auto text = tvg::Text::gen();
            if (!tvgexam::verify(text->load(EXAMPLE_DIR"/font/Arial.ttf"))) return false;
            text->font("Arial", 50);
            text->translate(0, 650);
            text->text("ThorVG Text");
            text->fill(100, 100, 255);

            auto text2 = text->duplicate();
            text2->translate(0, 700);

            canvas->push(text);
            canvas->push(text2);
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