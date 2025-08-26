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
    static constexpr int WIDTH = 1100;
    static constexpr int HEIGHT = 800;


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
        lines->moveTo(w/2, 0);
        lines->lineTo(w/2, h);
        lines->moveTo(0, h/2);
        lines->lineTo(w, h/2);
        lines->moveTo(border, border);
        lines->lineTo(w - border, border);
        lines->lineTo(w - border, h - border);
        lines->lineTo(border, h - border);
        lines->close();
        lines->moveTo(900, 0);
        lines->lineTo(900, h);
        canvas->push(lines);

        auto fontSize = 15.0f;
        w -= border * 2.0f;
        h -= border * 2.0f;

        //top left
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(0.0f, 0.0f);
            text->layout(w, h);
            text->text("Top-Left");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //top center
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(0.5f, 0.0f);
            text->layout(w, h);
            text->text("Top-Center");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //top right
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(1.0f, 0.0f);
            text->layout(w, h);
            text->text("Top-End");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //middle left
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(0.0f, 0.5f);
            text->layout(w, h);
            text->text("Middle-Left");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //middle center
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(0.5f, 0.5f);
            text->layout(w, h);
            text->text("Middle-Center");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //middle right
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(1.0f, 0.5f);
            text->layout(w, h);
            text->text("Middle-End");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //bottom left
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(0.0f, 1.0f);
            text->layout(w, h);
            text->text("Bottom-Left");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //bottom center
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(0.5f, 1.0f);
            text->layout(w, h);
            text->text("Bottom-Center");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //bottom right
        {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->translate(border, border);
            text->size(fontSize);
            text->align(1.0f, 1.0f);
            text->layout(w, h);
            text->text("Bottom-End");
            text->fill(255, 255, 255);
            canvas->push(text);
        }

        //origin
        tvg::Point alignments[] = {{0.0f, 0.5f}, {0.25f, 0.5f}, {0.5f, 0.5f}, {0.75f, 0.5f}, {1.0f, 0.5f}};
        char buf[50];
        for (int i = 0; i < 5; ++i) {
            auto text = tvg::Text::gen();
            text->font("NOTO-SANS-KR");
            text->size(fontSize);
            snprintf(buf, sizeof(buf), "Alignment = %0.2f", 0.25 * double(i));
            text->text(buf);
            text->fill(255, 255, 255);
            text->translate(900, 200 + i * 100);
            text->align(alignments[i].x, alignments[i].y);
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
    return tvgexam::main(new UserExample, argc, argv, true, UserExample::WIDTH, UserExample::HEIGHT);
}