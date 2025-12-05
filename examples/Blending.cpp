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
    void blender(tvg::Scene* root, const char* name, tvg::BlendMethod method, float x, float y, uint32_t* data)
    {
        auto text = tvg::Text::gen();
        text->font("Arial");
        text->size(15);
        text->text(name);
        text->fill(255, 255, 255);
        text->translate(x + 20, y);
        root->push(text);

        //solid
        {
            auto bottom = tvg::Shape::gen();
            bottom->appendRect(20.0f + x, 25.0f + y, 100.0f, 100.0f, 10.0f, 10.0f);
            bottom->fill(255, 255, 0);
            root->push(bottom);

            auto top = tvg::Shape::gen();
            top->appendRect(45.0f + x, 50.0f + y, 100.0f, 100.0f, 10.0f, 10.0f);
            top->fill(0, 255, 255);
            top->blend(method);
            root->push(top);
        }

        //solid (half transparent)
        {
            auto bottom = tvg::Shape::gen();
            bottom->appendRect(170.0f + x, 25.0f + y, 100.0f, 100.0f, 10.0f, 10.0f);
            bottom->fill(255, 255, 0, 127);
            root->push(bottom);

            auto top = tvg::Shape::gen();
            top->appendRect(195.0f + x, 50.0f + y, 100.0f, 100.0f, 10.0f, 10.0f);
            top->fill(0, 255, 255, 127);
            top->blend(method);
            root->push(top);
        }

        //gradient blending
        {
            tvg::Fill::ColorStop colorStops[2];
            colorStops[0] = {0, 255, 0, 255, 255};
            colorStops[1] = {1, 0, 255, 0, 127};

            auto fill = tvg::LinearGradient::gen();
            fill->linear(325.0f + x, 25.0f + y, 425.0f + x, 125.0f + y);
            fill->colorStops(colorStops, 2);

            auto bottom = tvg::Shape::gen();
            bottom->appendRect(325.0f + x, 25.0f + y, 100.0f, 100.0f, 10.0f, 10.0f);
            bottom->fill(fill);
            root->push(bottom);

            auto fill2 = tvg::LinearGradient::gen();
            fill2->linear(350.0f + x, 50.0f + y, 450.0f + x, 150.0f + y);
            fill2->colorStops(colorStops, 2);

            auto top = tvg::Shape::gen();
            top->appendRect(350.0f + x, 50.0f + y, 100.0f, 100.0f, 10.0f, 10.0f);
            top->fill(fill2);
            top->blend(method);
            root->push(top);
        }

        //image
        {
            auto bottom = tvg::Picture::gen();
            bottom->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true);
            bottom->translate(475 + x, 25.0f + y);
            bottom->scale(0.35f);
            root->push(bottom);

            auto top = bottom->duplicate();
            top->translate(500.0f + x, 50.0f + y);
            top->rotate(-10.0f);
            top->blend(method);
            root->push(top);
        }

        //scene
        {
            auto bottom = tvg::Picture::gen();
            bottom->load(EXAMPLE_DIR"/svg/tiger.svg");
            bottom->translate(600.0f + x, 25.0f + y);
            bottom->scale(0.11f);
            root->push(bottom);

            auto top = bottom->duplicate();
            top->translate(625.0f + x, 50.0f + y);
            top->blend(method);
            root->push(top);
        }

        //scene (half transparent)
        {
            auto bottom = tvg::Picture::gen();
            bottom->load(EXAMPLE_DIR"/svg/tiger.svg");
            bottom->translate(750.0f + x, 25.0f + y);
            bottom->scale(0.11f);
            bottom->opacity(127);
            root->push(bottom);

            auto top = bottom->duplicate();
            top->translate(775.0f + x, 50.0f + y);
            top->blend(method);
            root->push(top);
        }
    }


    bool content(tvg::Canvas* canvas, tvg::Scene* root, uint32_t w, uint32_t h) override
    {
        if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf"))) return false;

        //Prepare Image
        string path(EXAMPLE_DIR"/image/rawimage_200x300.raw");
        ifstream file(path, ios::binary);
        if (!file.is_open()) return false;
        auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
        file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
        file.close();

        blender(root, "Normal", tvg::BlendMethod::Normal, 0.0f, 0.0f, data);
        blender(root, "Multiply", tvg::BlendMethod::Multiply, 0.0f, 150.0f, data);
        blender(root, "Screen", tvg::BlendMethod::Screen, 0.0f, 300.0f, data);
        blender(root, "Overlay", tvg::BlendMethod::Overlay, 0.0f, 450.0f, data);
        blender(root, "Darken", tvg::BlendMethod::Darken, 0.f, 600.0f, data);
        blender(root, "Lighten", tvg::BlendMethod::Lighten, 0.0f, 750.0f, data);
        blender(root, "ColorDodge", tvg::BlendMethod::ColorDodge, 0.0f, 900.0f, data);
        blender(root, "ColorBurn", tvg::BlendMethod::ColorBurn, 0.0f, 1050.0f, data);
        blender(root, "HardLight", tvg::BlendMethod::HardLight, 0.0f, 1200.0f, data);

        blender(root, "SoftLight", tvg::BlendMethod::SoftLight, 900.0f, 0.0f, data);
        blender(root, "Difference", tvg::BlendMethod::Difference, 900.0f, 150.0f, data);
        blender(root, "Exclusion", tvg::BlendMethod::Exclusion, 900.0f, 300.0f, data);
        blender(root, "Hue", tvg::BlendMethod::Hue, 900.0f, 450.0f, data);
        blender(root, "Saturation", tvg::BlendMethod::Saturation, 900.0f, 600.0f, data);
        blender(root, "Color", tvg::BlendMethod::Color, 900.0f, 750.0f, data);
        blender(root, "Luminosity", tvg::BlendMethod::Luminosity, 900.0f, 900.0f, data);
        blender(root, "Add", tvg::BlendMethod::Add, 900.0f, 1050.0f, data);

        free(data);

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, true, 1800, 1380);
}