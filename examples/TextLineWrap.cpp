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
    tvg::Point size = {230.0f, 120.0f};

    void guide(tvg::Canvas* canvas, const char* title, float x, float y)
    {
        auto txt = tvg::Text::gen();
        txt->font("NOTO-SANS-KR");
        txt->translate(x, y);
        txt->size(12);
        txt->text(title);
        txt->fill(200, 200, 200);
        canvas->push(txt);

        auto lines = tvg::Shape::gen();
        lines->strokeFill(100, 100, 100);
        lines->strokeWidth(1);
        lines->appendRect(x, y + 30.0f, size.x, size.y);
        canvas->push(lines);
    }

    void text(tvg::Canvas* canvas, const char* content, const tvg::Point& pos, const tvg::Point& align, tvg::TextWrap wrapMode)
    {
        auto txt = tvg::Text::gen();
        txt->font("NOTO-SANS-KR");
        txt->translate(pos.x, pos.y + 30.0f);
        txt->layout(size.x, size.y);
        txt->size(14.5f);
        txt->text(content);
        txt->align(align.x, align.y);
        txt->wrap(wrapMode);
        txt->fill(255, 255, 255);
        canvas->push(txt);
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/NOTO-SANS-KR.ttf"))) return false;

        auto character = "TextWrap::Character";
        guide(canvas, character, 25.0f, 25.0f);
        text(canvas, "This is a lengthy text used to test line wrapping with top-left.", {25.0f, 25.0f}, {0.0f, 0.0f}, tvg::TextWrap::Character);

        guide(canvas, character, 290.0f, 25.0f);
        text(canvas, "This is a lengthy text used to test line wrapping with middle-center.", {290.0f, 25.0f}, {0.5f, 0.5f}, tvg::TextWrap::Character);

        guide(canvas, character, 550.0f, 25.0f);
        text(canvas, "This is a lengthy text used to test line wrapping with bottom-right.", {550.0f, 25.0f}, {1.0f, 1.0f}, tvg::TextWrap::Character);

        auto word = "TextWrap::Word";
        guide(canvas, word, 25.0f, 195.0f);
        text(canvas, "An extreame-long-length-word to test with top-left.", {25.0f, 195.0f}, {0.0f, 0.0f}, tvg::TextWrap::Word);

        guide(canvas, word, 290.0f, 195.0f);
        text(canvas, "An extreame-long-length-word to test with middle-center.", {290.0f, 195.0f}, {0.5f, 0.5f}, tvg::TextWrap::Word);

        guide(canvas, word, 550.0f, 195.0f);
        text(canvas, "An extreame-long-length-word to test with bottom-right.", {550.0f, 195.0f}, {1.0f, 1.0f}, tvg::TextWrap::Word);

        auto smart = "TextWrap::Smart";
        guide(canvas, smart, 25.0f, 365.0f);
        text(canvas, "An extreame-long-length-word to test with top-left.", {25.0f, 365.0f}, {0.0f, 0.0f}, tvg::TextWrap::Smart);

        guide(canvas, smart, 290.0f, 365.0f);
        text(canvas, "An extreame-long-length-word to test with middle-center.", {290.0f, 365.0f}, {0.5f, 0.5f}, tvg::TextWrap::Smart);

        guide(canvas, smart, 550.0f, 365.0f);
        text(canvas, "An extreame-long-length-word to test with bottom-right.", {550.0f, 365.0f}, {1.0f, 1.0f}, tvg::TextWrap::Smart);

        auto ellipsis = "TextWrap::Ellipsis";
        guide(canvas, ellipsis, 25.0f, 535.0f);
        text(canvas, "This is a lengthy text used to test line wrapping with top-left.", {25.0f, 535.0f}, {0.0f, 0.0f}, tvg::TextWrap::Ellipsis);

        guide(canvas, ellipsis, 290.0f, 535.0f);
        text(canvas, "This is a lengthy text used to test line wrapping with middle-center.", {290.0f, 535.0f}, {0.5f, 0.5f}, tvg::TextWrap::Ellipsis);

        guide(canvas, ellipsis, 550.0f, 535.0f);
        text(canvas, "This is a lengthy text used to test line wrapping with bottom-right.", {550.0f, 535.0f}, {1.0f, 1.0f}, tvg::TextWrap::Ellipsis);

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