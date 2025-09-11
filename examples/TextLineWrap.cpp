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
    float size = 200.0f;

    void guide(tvg::Canvas* canvas, float x, float y)
    {
        float dashPattern[2] = {10.0f, 10.0f};
        auto lines = tvg::Shape::gen();
        lines->strokeFill(100, 100, 100);
        lines->strokeWidth(1);
        lines->strokeDash(dashPattern, 2);
        lines->appendRect(x, y, size, size);
        canvas->push(lines);
    }

    void text(tvg::Canvas* canvas, const char* content, const tvg::Point& pos, const tvg::Point& align, tvg::TextWrap wrapMode)
    {
        auto txt = tvg::Text::gen();
        txt->font("NOTO-SANS-KR");
        txt->translate(pos.x, pos.y);
        txt->layout(size, size);
        txt->size(15);
        txt->text(content);
        txt->align(align.x, align.y);
        txt->wrap(wrapMode);
        txt->fill(255, 255, 255);
        canvas->push(txt);
    }


    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/NOTO-SANS-KR.ttf"))) return false;

        guide(canvas, 25.0f, 25.0f);
        text(canvas, "This is a long text for testing character line wrapping with align left.", {25.0f, 25.0f}, {0.0f, 0.0f}, tvg::TextWrap::Character);

        guide(canvas, 250.0f, 25.0f);
        text(canvas, "This is a long text for testing character line wrapping with align center.", {250.0f, 25.0f}, {0.0f, 0.0f}, tvg::TextWrap::Character);

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