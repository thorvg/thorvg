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
    void compose(tvg::Shape* star)
    {
        star->moveTo(199, 34);
        star->lineTo(253, 143);
        star->lineTo(374, 160);
        star->lineTo(287, 244);
        star->lineTo(307, 365);
        star->lineTo(199, 309);
        star->lineTo(97, 365);
        star->lineTo(112, 245);
        star->lineTo(26, 161);
        star->lineTo(146, 143);
        star->close();
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        if (!canvas) return false;

        //Background
        auto shape = tvg::Shape::gen();
        shape->appendRect(0, 0, w, h);
        shape->fill(255, 255, 255);
        canvas->push(std::move(shape));

        //////////////////////////////////////////////
        auto scene = tvg::Scene::gen();

        auto star1 = tvg::Shape::gen();
        compose(star1.get());
        star1->fill(255, 255, 0);
        star1->stroke(255 ,0, 0);
        star1->stroke(10);

        //Move Star1
        star1->translate(-10, -10);

        // color/alpha/opacity are ignored for a clip object - no need to set them
        auto clipStar = tvg::Shape::gen();
        clipStar->appendCircle(200, 230, 110, 110);
        clipStar->translate(10, 10);

        star1->clip(std::move(clipStar));

        auto star2 = tvg::Shape::gen();
        compose(star2.get());
        star2->fill(0, 255, 255);
        star2->stroke(0 ,255, 0);
        star2->stroke(10);
        star2->opacity(100);

        //Move Star2
        star2->translate(10, 40);

        // color/alpha/opacity are ignored for a clip object - no need to set them
        auto clip = tvg::Shape::gen();
        clip->appendCircle(200, 230, 130, 130);
        clip->translate(10, 10);

        scene->push(std::move(star1));
        scene->push(std::move(star2));

        //Clipping scene to shape
        scene->clip(std::move(clip));

        canvas->push(std::move(scene));

        //////////////////////////////////////////////
        auto star3 = tvg::Shape::gen();
        compose(star3.get());

        //Fill Gradient
        auto fill = tvg::LinearGradient::gen();
        fill->linear(100, 100, 300, 300);
        tvg::Fill::ColorStop colorStops[2];
        colorStops[0] = {0, 0, 0, 0, 255};
        colorStops[1] = {1, 255, 255, 255, 255};
        fill->colorStops(colorStops, 2);
        star3->fill(std::move(fill));

        star3->stroke(255 ,0, 0);
        star3->stroke(10);
        star3->translate(400, 0);

        // color/alpha/opacity are ignored for a clip object - no need to set them
        auto clipRect = tvg::Shape::gen();
        clipRect->appendRect(500, 120, 200, 200);          //x, y, w, h
        clipRect->translate(20, 20);

        //Clipping scene to rect(shape)
        star3->clip(std::move(clipRect));

        canvas->push(std::move(star3));

        //////////////////////////////////////////////
        auto picture = tvg::Picture::gen();
        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/svg/cartman.svg"))) return false;

        picture->scale(3);
        picture->translate(50, 400);

        // color/alpha/opacity are ignored for a clip object - no need to set them
        auto clipPath = tvg::Shape::gen();
        clipPath->appendCircle(200, 510, 50, 50);          //x, y, w, h, rx, ry
        clipPath->appendCircle(200, 650, 50, 50);          //x, y, w, h, rx, ry
        clipPath->translate(20, 20);

        //Clipping picture to path
        picture->clip(std::move(clipPath));

        canvas->push(std::move(picture));

        //////////////////////////////////////////////
        auto shape1 = tvg::Shape::gen();
        shape1->appendRect(500, 420, 100, 100, 20, 20);
        shape1->fill(255, 0, 255, 160);

        // color/alpha/opacity are ignored for a clip object - no need to set them
        auto clipShape = tvg::Shape::gen();
        clipShape->appendRect(600, 420, 100, 100);

        //Clipping shape1 to clipShape
        shape1->clip(std::move(clipShape));

        canvas->push(std::move(shape1));

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