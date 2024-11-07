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

        //Background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);    //x, y, w, h
        bg->fill(255, 255, 255);                //r, g, b
        canvas->push(bg);

        //Load png file from path
        auto opacity = 31;

        for (int i = 0; i < 7; ++i) {
            auto picture = tvg::Picture::gen();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/image/test.png"))) return false;
            picture->translate(i* 150, i * 150);
            picture->rotate(30 * i);
            picture->size(200, 200);
            picture->opacity(opacity + opacity * i);
            canvas->push(picture);
        }

        //Open file manually
        ifstream file(EXAMPLE_DIR"/image/test.png", ios::binary);
        if (!file.is_open()) return false;
        auto begin = file.tellg();
        file.seekg(0, std::ios::end);
        auto size = file.tellg() - begin;
        auto data = (char*)malloc(size);
        file.seekg(0, std::ios::beg);
        file.read(data, size);
        file.close();

        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(data, size, "png", "", true))) return false;
        free(data);
        picture->translate(400, 0);
        picture->scale(0.8);
        canvas->push(picture);

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