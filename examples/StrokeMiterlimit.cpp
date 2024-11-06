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

        //background
        {
            auto bg = tvg::Shape::gen();
            bg->appendRect(0, 0, w, h);    //x, y, w, h
            bg->fill(200, 200, 255);       //r, g, b
            canvas->push(std::move(bg));
        }

        //wild
        {
            float top = 100.0f;
            float bot = 700.0f;

            auto path = tvg::Shape::gen();
            path->moveTo(300, top / 2);
            path->lineTo(100, bot);
            path->lineTo(350, 400);
            path->lineTo(420, bot);
            path->lineTo(430, top * 2);
            path->lineTo(500, bot);
            path->lineTo(460, top * 2);
            path->lineTo(750, bot);
            path->lineTo(460, top / 2);
            path->close();

            path->fill(150, 150, 255);
            path->strokeWidth(20);
            path->strokeFill(120, 120, 255);

            path->strokeJoin(tvg::StrokeJoin::Miter);

            path->strokeMiterlimit(10);
            static float ml = path->strokeMiterlimit();
            cout << "stroke miterlimit = " << ml << endl;

            canvas->push(std::move(path));
        }

        //blueprint
        {
            // Load png file from path.
            const char* path = EXAMPLE_DIR"/image/stroke-miterlimit.png";

            auto picture = tvg::Picture::gen();
            if (!tvgexam::verify(picture->load(path))) return false;

            picture->opacity(42);
            picture->translate(24, 0);
            canvas->push(std::move(picture));
        }

        //svg
        {
            //SVG stroke-miterlimit test.
            std::string svgText (R"SVG(
        <svg viewBox="0 0 38 30">
        <!-- Impact of the default miter limit -->
        <path
            stroke="black"
            fill="none"
            stroke-linejoin="miter"
            id="p1"
            d="M1,9 l7   ,-3 l7   ,3
            m2,0 l3.5 ,-3 l3.5 ,3
            m2,0 l2   ,-3 l2   ,3
            m2,0 l0.75,-3 l0.75,3
            m2,0 l0.5 ,-3 l0.5 ,3" />

        <!-- Impact of the smallest miter limit (1) -->
        <path
            stroke="black"
            fill="none"
            stroke-linejoin="miter"
            stroke-miterlimit="1"
            id="p2"
            d="M1,19 l7   ,-3 l7   ,3
            m2, 0 l3.5 ,-3 l3.5 ,3
            m2, 0 l2   ,-3 l2   ,3
            m2, 0 l0.75,-3 l0.75,3
            m2, 0 l0.5 ,-3 l0.5 ,3" />

        <!-- Impact of a large miter limit (8) -->
        <path
            stroke="black"
            fill="none"
            stroke-linejoin="miter"
            stroke-miterlimit="8"
            id="p3"
            d="M1,29 l7   ,-3 l7   ,3
            m2, 0 l3.5 ,-3 l3.5 ,3
            m2, 0 l2   ,-3 l2   ,3
            m2, 0 l0.75,-3 l0.75,3
            m2, 0 l0.5 ,-3 l0.5 ,3" />

        <!-- the following pink lines highlight the position of the path for each stroke -->
        <path
            stroke="pink"
            fill="none"
            stroke-width="0.05"
            d="M1, 9 l7,-3 l7,3 m2,0 l3.5,-3 l3.5,3 m2,0 l2,-3 l2,3 m2,0 l0.75,-3 l0.75,3 m2,0 l0.5,-3 l0.5,3
            M1,19 l7,-3 l7,3 m2,0 l3.5,-3 l3.5,3 m2,0 l2,-3 l2,3 m2,0 l0.75,-3 l0.75,3 m2,0 l0.5,-3 l0.5,3
            M1,29 l7,-3 l7,3 m2,0 l3.5,-3 l3.5,3 m2,0 l2,-3 l2,3 m2,0 l0.75,-3 l0.75,3 m2,0 l0.5,-3 l0.5,3" />
        </svg>
        )SVG");

            auto picture = tvg::Picture::gen();
            if (!tvgexam::verify(picture->load(svgText.data(), svgText.size(), "svg", "", true))) return false;
            picture->scale(20);
            canvas->push(std::move(picture));
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