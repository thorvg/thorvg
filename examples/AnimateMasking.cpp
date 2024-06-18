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
    tvg::Shape *pMaskShape = nullptr;
    tvg::Shape *pMask = nullptr;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        // background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(255, 255, 255);
        canvas->push(std::move(bg));

        //image
        auto picture1 = tvg::Picture::gen();
        if (!tvgexam::verify(picture1->load(EXAMPLE_DIR"/svg/cartman.svg"))) return false;
        picture1->size(400, 400);
        canvas->push(std::move(picture1));

        auto picture2 = tvg::Picture::gen();
        picture2->load(EXAMPLE_DIR"/svg/logo.svg");
        picture2->size(400, 400);

        //mask
        auto maskShape = tvg::Shape::gen();
        pMaskShape = maskShape.get();
        maskShape->appendCircle(180, 180, 75, 75);
        maskShape->fill(125, 125, 125);
        maskShape->stroke(25, 25, 25);
        maskShape->stroke(tvg::StrokeJoin::Round);
        maskShape->stroke(10);
        canvas->push(std::move(maskShape));

        auto mask = tvg::Shape::gen();
        pMask = mask.get();
        mask->appendCircle(180, 180, 75, 75);
        mask->fill(255, 255, 255);         //AlphaMask RGB channels are unused.

        picture2->composite(std::move(mask), tvg::CompositeMethod::AlphaMask);
        canvas->push(std::move(picture2));

        return true;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!canvas) return false;

        /* Update shape directly.
           You can update only necessary properties of this shape,
           while retaining other properties. */
        auto progress = tvgexam::progress(elapsed, 3.0f, true);  //play time 3 sec.

        // Translate mask object with its stroke & update
        pMaskShape->translate(0 , progress * 300 - 100);
        pMask->translate(0 , progress * 300 - 100);

        canvas->update();

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