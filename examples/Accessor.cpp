/*
 * Copyright (c) 2021 - 2025 the ThorVG project. All rights reserved.

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

        //load the tvg file
        auto picture = tvg::Picture::gen();
        auto result = picture->load(EXAMPLE_DIR"/svg/favorite_on.svg");
        if (!tvgexam::verify(result)) return false;
        picture->size(w, h);

        auto accessor = unique_ptr<tvg::Accessor>(tvg::Accessor::gen());

        //The callback function from lambda expression.
        //This function will be called for every paint nodes of the picture tree.
        auto f = [](const tvg::Paint* paint, void* data) -> bool
        {
            if (paint->type() == tvg::Type::Shape) {
                auto shape = (tvg::Shape*) paint;
                //override color?
                uint8_t r, g, b;
                shape->fill(&r, &g, &b);
                if (r == 255 && g == 180 && b == 0)
                    shape->fill(0, 0, 255);

            }

            //You can return false, to stop traversing immediately.
            return true;
        };

        if (!tvgexam::verify(accessor->set(picture, f, nullptr))) return false;

        // Try to retrieve the shape that corresponds to the SVG node with the unique ID "star".
        if (auto paint = picture->paint(tvg::Accessor::id("star"))) {
            auto shape = static_cast<tvg::Shape*>(const_cast<tvg::Paint*>(paint));
            shape->strokeFill(255, 255, 0);
            shape->strokeWidth(5);
        }

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
