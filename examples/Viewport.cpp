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
    static constexpr uint32_t VPORT_SIZE = 300;
    uint32_t w, h;
    tvg::Picture* pPicture = nullptr;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        auto mask = tvg::Shape::gen();
        mask->appendCircle(w/2, h/2, w/2, h/2);
        mask->fill(255, 255, 255);
        //Use the opacity for a half-translucent mask.
        mask->opacity(125);

        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/svg/tiger.svg"))) return false;
        picture->size(w, h);
        picture->mask(std::move(mask), tvg::MaskMethod::Alpha);
        pPicture = picture.get();
        canvas->push(std::move(picture));

        this->w = w;
        this->h = h;

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        canvas->clear(false);

        auto progress = tvgexam::progress(elapsed, 2.0f, true);  //play time 2 sec.

        if (!tvgexam::verify(canvas->viewport((w - VPORT_SIZE) * progress, (h - VPORT_SIZE) * progress, VPORT_SIZE, VPORT_SIZE))) return false;

        canvas->update();

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, 1024, 1024, 4, true);
}