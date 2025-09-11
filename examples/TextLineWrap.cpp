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
    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/NOTO-SANS-KR.ttf"))) return false;

        //fixed size
        w = 800;
        h = 800;

        //guide line
        float border = 150.0f;
        float dashPattern[2] = {10.0f, 10.0f};
        auto lines = tvg::Shape::gen();
        lines->strokeFill(100, 100, 100);
        lines->strokeWidth(1);
        lines->strokeDash(dashPattern, 2);
        lines->appendRect(border, border, w - 2 * border, h - 2 * border);
        canvas->push(lines);

        w -= border * 2.0f;
        h -= border * 2.0f;

        //top left
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(15);
            text->align(0.0f, 0.0f);
            text->layout(w, h);
            text->text("The first is a long text for testing character line wrapping.");
            text->fill(255, 255, 255);
            canvas->push(text);
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