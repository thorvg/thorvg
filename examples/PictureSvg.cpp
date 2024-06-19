/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

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

        //Background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);    //x, y, w, h
        bg->fill(255, 255, 255);       //r, g, b
        canvas->push(std::move(bg));

        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), EXAMPLE_DIR"/svg/logo.svg");

        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(buf))) return false;

        float scale;
        float shiftX = 0.0f, shiftY = 0.0f;
        float w2, h2;
        picture->size(&w2, &h2);
        if (w2 > h2) {
            scale = w / w2;
            shiftY = (h - h2 * scale) * 0.5f;
        } else {
            scale = h / h2;
            shiftX = (w - w2 * scale) * 0.5f;
        }
        picture->translate(shiftX, shiftY);
        picture->scale(scale);

        canvas->push(std::move(picture));

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