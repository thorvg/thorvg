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
    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(255, 255, 255);
        canvas->push(bg);

        string path(EXAMPLE_DIR"/image/rawimage_200x300.raw");
        ifstream file(path, ios::binary);
        if (!file.is_open()) return false;
        auto data = (uint32_t*)malloc(sizeof(uint32_t) * (200*300));
        file.read(reinterpret_cast<char *>(data), sizeof (uint32_t) * 200 * 300);
        file.close();

        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
        picture->translate(400, 250);
        canvas->push(picture);

        auto picture2 = tvg::Picture::gen();
        if (!tvgexam::verify(picture2->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;

        picture2->translate(400, 200);
        picture2->rotate(47);
        picture2->scale(1.5);
        picture2->opacity(128);

        auto circle = tvg::Shape::gen();
        circle->appendCircle(350, 350, 200, 200);

        picture2->clip(circle);

        canvas->push(picture2);

        free(data);

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