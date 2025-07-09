/*
 * Copyright (c) 2023 - 2025 the ThorVG project. All rights reserved.

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
        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(75, 75, 75);
        canvas->push(shape);

        //Load a necessary font data.
        //The loaded font will be released when the Initializer::term() is called.
        //Otherwise, you can immediately unload the font data.
        //Please check Text::unload() APIs.
        if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf"))) return false;
        if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/NanumGothicCoding.ttf"))) return false;

        //Load from memory
        ifstream file(EXAMPLE_DIR"/font/SentyCloud.ttf", ios::binary);
        if (!file.is_open()) return false;
        file.seekg(0, std::ios::end);
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        auto data = (char*)malloc(size);
        if (data && file.read(data, size)) {
            if (!tvgexam::verify(tvg::Text::load("SentyCloud", data, size, "ttf", true))) return false;
        }
        file.close();
        free(data);

        auto text = tvg::Text::gen();
        text->font("Arial", 80);
        text->text("THORVG Text");
        text->fill(255, 255, 255);
        canvas->push(text);

        auto text2 = tvg::Text::gen();
        text2->font("Arial", 30, "italic");
        text2->text("Font = \"Arial\", Size = 40, Style=Italic");
        text2->translate(0, 150);
        text2->fill(255, 255, 255);
        canvas->push(text2);

        auto text3 = tvg::Text::gen();
        text3->font(nullptr, 40);  //Use any font
        text3->text("Kerning Test: VA, AV, TJ, JT");
        text3->fill(255, 255, 255);
        text3->translate(0, 225);
        canvas->push(text3);

        auto text4 = tvg::Text::gen();
        text4->font("Arial", 25);
        text4->text("Purple Text");
        text4->fill(255, 0, 255);
        text4->translate(0, 310);
        canvas->push(text4);

        auto text5 = tvg::Text::gen();
        text5->font("Arial", 25);
        text5->text("Gray Text");
        text5->fill(150, 150, 150);
        text5->translate(220, 310);
        canvas->push(text5);

        auto text6 = tvg::Text::gen();
        text6->font("Arial", 25);
        text6->text("Yellow Text");
        text6->fill(255, 255, 0);
        text6->translate(400, 310);
        canvas->push(text6);

        auto text7 = tvg::Text::gen();
        text7->font("Arial", 15);
        text7->text("Transformed Text - 30'");
        text7->fill(0, 0, 0);
        text7->translate(600, 400);
        text7->rotate(30);
        canvas->push(text7);

        auto text8 = tvg::Text::gen();
        text8->font("Arial", 15);
        text8->fill(0, 0, 0);
        text8->text("Transformed Text - 90'");
        text8->translate(600, 400);
        text8->rotate(90);
        canvas->push(text8);

        auto text9 = tvg::Text::gen();
        text9->font("Arial", 15);
        text9->fill(0, 0, 0);
        text9->text("Transformed Text - 180'");
        text9->translate(800, 400);
        text9->rotate(180);
        canvas->push(text9);

        //gradient texts
        float x, y, w2, h2;

        auto text10 = tvg::Text::gen();
        text10->font("Arial", 50);
        text10->text("Linear Text");
        text10->bounds(&x, &y, &w2, &h2);

        //LinearGradient
        auto fill = tvg::LinearGradient::gen();
        fill->linear(x, y + h2 * 0.5f, x + w2, y + h2 * 0.5f);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops[3];
        colorStops[0] = {0, 255, 0, 0, 255};
        colorStops[1] = {0.5, 255, 255, 0, 255};
        colorStops[2] = {1, 255, 255, 255, 255};
        fill->colorStops(colorStops, 3);
        text10->fill(fill);

        text10->translate(0, 350);

        canvas->push(text10);

        auto text11 = tvg::Text::gen();
        text11->font("NanumGothicCoding", 40);
        text11->text("\xeb\x82\x98\xeb\x88\x94\xea\xb3\xa0\xeb\x94\x95\xec\xbd\x94\xeb\x94\xa9\x28\x55\x54\x46\x2d\x38\x29");
        text11->bounds(&x, &y, &w2, &h2);

        //RadialGradient
        auto fill2 = tvg::RadialGradient::gen();
        fill2->radial(x + w2 * 0.5f, y + h2 * 0.5f, w2 * 0.5f, x + w2 * 0.5f, y + h2 * 0.5f, 0.0f);

        //Gradient Color Stops
        tvg::Fill::ColorStop colorStops2[3];
        colorStops2[0] = {0, 0, 255, 255, 255};
        colorStops2[1] = {0.5, 255, 255, 0, 255};
        colorStops2[2] = {1, 255, 255, 255, 255};

        fill2->colorStops(colorStops2, 3);

        text11->fill(fill2);

        text11->translate(0, 450);

        canvas->push(text11);

        auto text12 = tvg::Text::gen();
        text12->font("SentyCloud", 50);
        text12->fill(255, 25, 25);
        text12->text("\xe4\xb8\x8d\xe5\x88\xb0\xe9\x95\xbf\xe5\x9f\x8e\xe9\x9d\x9e\xe5\xa5\xbd\xe6\xb1\x89\xef\xbc\x81");
        text12->translate(0, 525);
        canvas->push(text12);

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