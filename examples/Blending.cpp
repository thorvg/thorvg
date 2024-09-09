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

        //Normal
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(0, 0, 400, 400, 50, 50);
        shape1->fill(0, 255, 255);
        shape1->blend(tvg::BlendMethod::Normal);
        canvas->push(std::move(shape1));

        //Add
        auto shape2 = tvg::Shape::gen();
        shape2->appendCircle(400, 400, 200, 200);
        shape2->fill(255, 255, 0, 170);
        shape2->blend(tvg::BlendMethod::Add);
        canvas->push(std::move(shape2));

        //Multiply
        auto shape3 = tvg::Shape::gen();
        shape3->appendCircle(400, 400, 250, 100);
        shape3->fill(255, 255, 255, 100);
        shape3->blend(tvg::BlendMethod::Multiply);
        canvas->push(std::move(shape3));

        //Overlay
        auto shape4 = tvg::Shape::gen();
        shape4->moveTo(199, 234);
        shape4->lineTo(253, 343);
        shape4->lineTo(374, 360);
        shape4->lineTo(287, 444);
        shape4->lineTo(307, 565);
        shape4->lineTo(199, 509);
        shape4->lineTo(97, 565);
        shape4->lineTo(112, 445);
        shape4->lineTo(26, 361);
        shape4->lineTo(146, 343);
        shape4->close();
        shape4->fill(255, 0, 200, 200);
        shape4->blend(tvg::BlendMethod::Overlay);
        canvas->push(std::move(shape4));

        //Difference
        auto shape5 = tvg::Shape::gen();
        shape5->appendCircle(300, 600, 200, 200);

        //LinearGradient
        auto fill = tvg::LinearGradient::gen();
        fill->linear(300, 600, 200, 200);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops[2];
        colorStops[0] = {0, 0, 0, 0, 255};
        colorStops[1] = {1, 255, 255, 255, 100};
        fill->colorStops(colorStops, 2);

        shape5->fill(std::move(fill));
        shape5->blend(tvg::BlendMethod::Difference);
        canvas->push(std::move(shape5));

        //Exclusion
        auto shape6 = tvg::Shape::gen();
        shape6->appendCircle(300, 800, 150, 150);

        //RadialGradient
        auto fill2 = tvg::RadialGradient::gen();
        fill2->radial(300, 800, 150);
        fill2->colorStops(colorStops, 2);

        shape6->fill(std::move(fill2));
        shape6->blend(tvg::BlendMethod::Exclusion);
        canvas->push(std::move(shape6));

        //Screen
        auto shape7 = tvg::Shape::gen();
        shape7->appendCircle(600, 650, 200, 150);
        shape7->blend(tvg::BlendMethod::Screen);
        shape7->fill(0, 0, 255);
        canvas->push(std::move(shape7));

        //Darken
        auto shape9 = tvg::Shape::gen();
        shape9->appendRect(600, 650, 350, 250);
        shape9->blend(tvg::BlendMethod::Darken);
        shape9->fill(10, 255, 155);
        canvas->push(std::move(shape9));

        //Prepare Transformed Image
        string path(EXAMPLE_DIR"/image/rawimage_200x300.raw");

        ifstream file(path, ios::binary);
        if (!file.is_open()) return false;
        auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
        file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
        file.close();

        //Lighten
        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(data, 200, 300, true))) return false;
        picture->translate(800, 700);
        picture->rotate(40);
        picture->blend(tvg::BlendMethod::Lighten);
        canvas->push(std::move(picture));

        //ColorDodge
        auto shape10 = tvg::Shape::gen();
        shape10->appendRect(0, 0, 200, 200, 50, 50);
        shape10->blend(tvg::BlendMethod::ColorDodge);
        shape10->fill(255, 255, 255, 250);
        canvas->push(std::move(shape10));

        //ColorBurn
        auto picture2 = tvg::Picture::gen();
        if (!tvgexam::verify(picture2->load(data, 200, 300, true))) return false;
        picture2->translate(600, 250);
        picture2->blend(tvg::BlendMethod::ColorBurn);
        picture2->opacity(150);
        canvas->push(std::move(picture2));

        //HardLight
        auto picture3 = tvg::Picture::gen();
        if (!tvgexam::verify(picture3->load(data, 200, 300, true))) return false;
        picture3->translate(700, 150);
        picture3->blend(tvg::BlendMethod::HardLight);
        canvas->push(std::move(picture3));

        //SoftLight
        auto picture4 = tvg::Picture::gen();
        if (!tvgexam::verify(picture4->load(data, 200, 300, true))) return false;
        picture4->translate(350, 600);
        picture4->rotate(90);
        picture4->blend(tvg::BlendMethod::SoftLight);
        canvas->push(std::move(picture4));

        free(data);

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, 1024, 1024);
}