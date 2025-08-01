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
    //Use a scene for selection effect
    tvg::Scene* shape;
    tvg::Scene* picture;
    tvg::Scene* text;
    tvg::Scene* tiger;

    tvg::Shape* marquee;
    int mx = 0, my = 0, mw = 20, mh = 20;
    bool updated = false;

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        //dash stroke filled shape
        {
            auto shape = tvg::Shape::gen();
            shape->moveTo(255, 85);
            shape->lineTo(380, 405);
            shape->lineTo(75, 200);
            shape->lineTo(435, 200);
            shape->lineTo(130, 405);
            shape->close();
            shape->fill(255, 255, 255);
            shape->fillRule(tvg::FillRule::EvenOdd);

            shape->strokeWidth(20);
            shape->strokeFill(0, 255, 0);
            float dashPattern[] = {40, 40};
            shape->strokeCap(tvg::StrokeCap::Butt);
            shape->strokeDash(dashPattern, 2);

            shape->scale(1.25f);

            auto scene = tvg::Scene::gen();
            scene->push(shape);

            canvas->push(scene);

            this->shape = scene;
        }

        //clipped, rotated image
        {
            auto picture = tvg::Picture::gen();
            if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/image/test.jpg"))) return false;

            picture->translate(800, 100);
            picture->rotate(47);

            auto clip = tvg::Shape::gen();
            clip->appendCircle(900, 350, 200, 200);
            picture->clip(clip);

            //Use a scene for selection effect
            auto scene = tvg::Scene::gen();
            scene->push(picture);

            canvas->push(scene);

            this->picture = scene;
        }

        //normal text
        {
            if (!tvgexam::verify(tvg::Text::load(EXAMPLE_DIR"/font/Arial.ttf"))) return false;
            auto text = tvg::Text::gen();
            text->font("Arial", 100);
            text->text("Intersect?!");
            text->translate(25, 800);
            text->fill(255, 255, 255);

            //Use a scene for selection effect
            auto scene = tvg::Scene::gen();
            scene->push(text);

            canvas->push(scene);

            this->text = scene;
        }

        //vector scene
        {
            auto tiger = tvg::Picture::gen();
            if (!tvgexam::verify(tiger->load(EXAMPLE_DIR"/svg/tiger.svg"))) return false;
            tiger->translate(700, 640);
            tiger->scale(0.5f);

            //Use a scene for selection effect
            auto scene = tvg::Scene::gen();
            scene->push(tiger);

            canvas->push(scene);

            this->tiger = scene;
        }

        //marquee
        {
            marquee = tvg::Shape::gen();
            marquee->appendRect(mx, my, mw, mh);
            marquee->strokeWidth(2);
            marquee->strokeFill(255, 255, 0);
            marquee->fill(255, 255, 0, 50);
            canvas->push(marquee);
        }

        return true;
    }

    bool motion(tvg::Canvas* canvas, int32_t x, int32_t y) override
    {
        //center align
        mx = x - (mw / 2);
        my = y - (mh / 2);

        return false;
    }

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        //reset
        shape->push(tvg::SceneEffect::ClearAll);
        picture->push(tvg::SceneEffect::ClearAll);
        text->push(tvg::SceneEffect::ClearAll);
        tiger->push(tvg::SceneEffect::ClearAll);

        marquee->translate(mx, my);

        if (shape->intersects(mx, my, mw, mh)) {
            shape->push(tvg::SceneEffect::Fill, 255, 255, 0, 255);
        } else if (picture->intersects(mx, my, mw, mh)) {
            picture->push(tvg::SceneEffect::Fill, 255, 255, 0, 255);
        } else if (text->intersects(mx, my, mw, mh)) {
            text->push(tvg::SceneEffect::Fill, 255, 255, 0, 255);
        } else if (tiger->intersects(mx, my, mw, mh)) {
            tiger->push(tvg::SceneEffect::Fill, 255, 255, 0, 255);
        }

        canvas->update();

        updated = false;

        return true;
    }
};

/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 1200, 1200);
}