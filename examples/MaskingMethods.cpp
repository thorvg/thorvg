/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

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

        //Image source
        ifstream file(EXAMPLE_DIR"/image/rawimage_200x300.raw", ios::binary);
        if (!file.is_open()) return false;
        auto data = (uint32_t*) malloc(sizeof(uint32_t) * (200 * 300));
        file.read(reinterpret_cast<char*>(data), sizeof (uint32_t) * 200 * 300);
        file.close();

        //background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, 625, h);
        bg->fill(50, 50, 50);
        canvas->push(std::move(bg));

        {
            //Shape + Shape Mask Add
            auto shape = tvg::Shape::gen();
            shape->appendCircle(125, 100, 150, 150);
            shape->fill(255, 255, 255);

            auto mask = tvg::Shape::gen();
            mask->appendCircle(125, 100, 50, 50);
            mask->fill(255, 255, 255);

            auto add = tvg::Shape::gen();
            add->appendCircle(175, 100, 50, 50);
            add->fill(255, 255, 255);
            mask->mask(std::move(add), tvg::MaskMethod::Add);
            shape->mask(std::move(mask), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape));

            //Shape + Shape Mask Subtract
            auto shape2 = tvg::Shape::gen();
            shape2->appendCircle(375, 100, 150, 150);
            shape2->fill(255, 255, 255, 255);

            auto mask2 = tvg::Shape::gen();
            mask2->appendCircle(375, 100, 50, 50);
            mask2->fill(255, 255, 255, 127);

            auto sub = tvg::Shape::gen();
            sub->appendCircle(400, 100, 50, 50);
            sub->fill(255, 255, 255);
            mask2->mask(std::move(sub), tvg::MaskMethod::Subtract);
            shape2->mask(std::move(mask2), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape2));

            //Shape + Shape Mask Intersect
            auto shape3 = tvg::Shape::gen();
            shape3->appendCircle(625, 100, 50, 50);
            shape3->fill(255, 255, 255, 127);

            auto mask3 = tvg::Shape::gen();
            mask3->appendCircle(625, 100, 50, 50);
            mask3->fill(255, 255, 255, 127);

            auto inter = tvg::Shape::gen();
            inter->appendCircle(650, 100, 50, 50);
            inter->fill(255, 255, 255);
            mask3->mask(std::move(inter), tvg::MaskMethod::Intersect);
            shape3->mask(std::move(mask3), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape3));

            //Shape + Shape Mask Difference
            auto shape4 = tvg::Shape::gen();
            shape4->appendCircle(875, 100, 150, 150);
            shape4->fill(255, 255, 255);

            auto mask4 = tvg::Shape::gen();
            mask4->appendCircle(875, 100, 50, 50);
            mask4->fill(255, 255, 255);

            auto diff = tvg::Shape::gen();
            diff->appendCircle(900, 100, 50, 50);
            diff->fill(255, 255, 255);
            mask4->mask(std::move(diff), tvg::MaskMethod::Difference);
            shape4->mask(std::move(mask4), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape4));

            //Shape + Shape Mask Lighten
            auto shape5 = tvg::Shape::gen();
            shape5->appendCircle(1125, 100, 150, 150);
            shape5->fill(255, 255, 255);

            auto mask5 = tvg::Shape::gen();
            mask5->appendCircle(1125, 100, 50, 50);
            mask5->fill(255, 255, 255, 200);

            auto light = tvg::Shape::gen();
            light->appendCircle(1150, 100, 50, 50);
            light->fill(255, 255, 255);
            mask5->mask(std::move(light), tvg::MaskMethod::Lighten);
            shape5->mask(std::move(mask5), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape5));

            //Shape + Shape Mask Darken
            auto shape6 = tvg::Shape::gen();
            shape6->appendCircle(1375, 100, 150, 150);
            shape6->fill(255, 255, 255);

            auto mask6 = tvg::Shape::gen();
            mask6->appendCircle(1375, 100, 50, 50);
            mask6->fill(255, 255, 255, 200);

            auto dark = tvg::Shape::gen();
            dark->appendCircle(1400, 100, 50, 50);
            dark->fill(255, 255, 255);
            mask6->mask(std::move(dark), tvg::MaskMethod::Darken);
            shape6->mask(std::move(mask6), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape6));
        }
        {
            //Shape + Shape Mask Add
            auto shape = tvg::Shape::gen();
            shape->appendCircle(125, 300, 100, 100);
            shape->fill(255, 255, 255);

            auto mask = tvg::Shape::gen();
            mask->appendCircle(125, 300, 50, 50);
            mask->fill(255, 255, 255);

            auto add = tvg::Shape::gen();
            add->appendCircle(175, 300, 50, 50);
            add->fill(255, 255, 255);
            mask->mask(std::move(add), tvg::MaskMethod::Add);
            shape->mask(std::move(mask), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(shape));

            //Shape + Shape Mask Subtract
            auto shape2 = tvg::Shape::gen();
            shape2->appendCircle(375, 300, 100, 100);
            shape2->fill(255, 255, 255, 255);

            auto mask2 = tvg::Shape::gen();
            mask2->appendCircle(375, 300, 50, 50);
            mask2->fill(255, 255, 255, 127);

            auto sub = tvg::Shape::gen();
            sub->appendCircle(400, 300, 50, 50);
            sub->fill(255, 255, 255);
            mask2->mask(std::move(sub), tvg::MaskMethod::Subtract);
            shape2->mask(std::move(mask2), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(shape2));

            //Shape + Shape Mask Intersect
            auto shape3 = tvg::Shape::gen();
            shape3->appendCircle(625, 300, 100, 100);
            shape3->fill(255, 255, 255, 127);

            auto mask3 = tvg::Shape::gen();
            mask3->appendCircle(625, 300, 50, 50);
            mask3->fill(255, 255, 255, 127);

            auto inter = tvg::Shape::gen();
            inter->appendCircle(650, 300, 50, 50);
            inter->fill(255, 255, 255);
            mask3->mask(std::move(inter), tvg::MaskMethod::Intersect);
            shape3->mask(std::move(mask3), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(shape3));

            //Shape + Shape Mask Difference
            auto shape4 = tvg::Shape::gen();
            shape4->appendCircle(875, 300, 100, 100);
            shape4->fill(255, 255, 255);

            auto mask4 = tvg::Shape::gen();
            mask4->appendCircle(875, 300, 50, 50);
            mask4->fill(255, 255, 255);

            auto diff = tvg::Shape::gen();
            diff->appendCircle(900, 300, 50, 50);
            diff->fill(255, 255, 255);
            mask4->mask(std::move(diff), tvg::MaskMethod::Difference);
            shape4->mask(std::move(mask4), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(shape4));

            //Shape + Shape Mask Lighten
            auto shape5 = tvg::Shape::gen();
            shape5->appendCircle(1125, 300, 100, 100);
            shape5->fill(255, 255, 255);

            auto mask5 = tvg::Shape::gen();
            mask5->appendCircle(1125, 300, 50, 50);
            mask5->fill(255, 255, 255, 200);

            auto light = tvg::Shape::gen();
            light->appendCircle(1150, 300, 50, 50);
            light->fill(255, 255, 255);
            mask5->mask(std::move(light), tvg::MaskMethod::Lighten);
            shape5->mask(std::move(mask5), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(shape5));

            //Shape + Shape Mask Darken
            auto shape6 = tvg::Shape::gen();
            shape6->appendCircle(1375, 300, 100, 100);
            shape6->fill(255, 255, 255);

            auto mask6 = tvg::Shape::gen();
            mask6->appendCircle(1375, 300, 50, 50);
            mask6->fill(255, 255, 255, 200);

            auto dark = tvg::Shape::gen();
            dark->appendCircle(1400, 300, 50, 50);
            dark->fill(255, 255, 255);
            mask6->mask(std::move(dark), tvg::MaskMethod::Darken);
            shape6->mask(std::move(mask6), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(shape6));
        }
        {
            //Rect + Rect Mask Add
            auto shape = tvg::Shape::gen();
            shape->appendRect(75, 450, 150, 150);
            shape->fill(255, 255, 255);

            auto mask = tvg::Shape::gen();
            mask->appendRect(75, 500, 100, 100);
            mask->fill(255, 255, 255);

            auto add = tvg::Shape::gen();
            add->appendRect(125, 450, 100, 100);
            add->fill(255, 255, 255);
            mask->mask(std::move(add), tvg::MaskMethod::Add);
            shape->mask(std::move(mask), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape));

            //Rect + Rect Mask Subtract
            auto shape2 = tvg::Shape::gen();
            shape2->appendRect(325, 450, 150, 150);
            shape2->fill(255, 255, 255);

            auto mask2 = tvg::Shape::gen();
            mask2->appendRect(325, 500, 100, 100);
            mask2->fill(255, 255, 255, 127);

            auto sub = tvg::Shape::gen();
            sub->appendRect(375, 450, 100, 100);
            sub->fill(255, 255, 255);
            mask2->mask(std::move(sub), tvg::MaskMethod::Subtract);
            shape2->mask(std::move(mask2), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape2));

            //Rect + Rect Mask Intersect
            auto shape3 = tvg::Shape::gen();
            shape3->appendRect(575, 450, 150, 150);
            shape3->fill(255, 255, 255);

            auto mask3 = tvg::Shape::gen();
            mask3->appendRect(575, 500, 100, 100);
            mask3->fill(255, 255, 255, 127);

            auto inter = tvg::Shape::gen();
            inter->appendRect(625, 450, 100, 100);
            inter->fill(255, 255, 255);
            mask3->mask(std::move(inter), tvg::MaskMethod::Intersect);
            shape3->mask(std::move(mask3), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape3));

            //Rect + Rect Mask Difference
            auto shape4 = tvg::Shape::gen();
            shape4->appendRect(825, 450, 150, 150);
            shape4->fill(255, 255, 255);

            auto mask4 = tvg::Shape::gen();
            mask4->appendRect(825, 500, 100, 100);
            mask4->fill(255, 255, 255);

            auto diff = tvg::Shape::gen();
            diff->appendRect(875, 450, 100, 100);
            diff->fill(255, 255, 255);
            mask4->mask(std::move(diff), tvg::MaskMethod::Difference);
            shape4->mask(std::move(mask4), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape4));

            //Rect + Rect Mask Lighten
            auto shape5 = tvg::Shape::gen();
            shape5->appendRect(1125, 450, 150, 150);
            shape5->fill(255, 255, 255);

            auto mask5 = tvg::Shape::gen();
            mask5->appendRect(1125, 500, 100, 100);
            mask5->fill(255, 255, 255, 200);

            auto light = tvg::Shape::gen();
            light->appendRect(1175, 450, 100, 100);
            light->fill(255, 255, 255);
            mask5->mask(std::move(light), tvg::MaskMethod::Lighten);
            shape5->mask(std::move(mask5), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape5));

            //Rect + Rect Mask Darken
            auto shape6 = tvg::Shape::gen();
            shape6->appendRect(1375, 450, 150, 150);
            shape6->fill(255, 255, 255);

            auto mask6 = tvg::Shape::gen();
            mask6->appendRect(1375, 500, 100, 100);
            mask6->fill(255, 255, 255, 200);

            auto dark = tvg::Shape::gen();
            dark->appendRect(1400, 450, 100, 100);
            dark->fill(255, 255, 255);
            mask6->mask(std::move(dark), tvg::MaskMethod::Darken);
            shape6->mask(std::move(mask6), tvg::MaskMethod::Alpha);
            canvas->push(std::move(shape6));
        }
        {
            //Transformed Image + Shape Mask Add
            auto image = tvg::Picture::gen();
            if (!tvgexam::verify(image->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image->translate(150, 650);
            image->scale(0.5f);
            image->rotate(45);

            auto mask = tvg::Shape::gen();
            mask->appendCircle(125, 700, 50, 50);
            mask->fill(255, 255, 255);

            auto add = tvg::Shape::gen();
            add->appendCircle(150, 750, 50, 50);
            add->fill(255, 255, 255);
            mask->mask(std::move(add), tvg::MaskMethod::Add);
            image->mask(std::move(mask), tvg::MaskMethod::Alpha);
            canvas->push(std::move(image));

            //Transformed Image + Shape Mask Subtract
            auto image2 = tvg::Picture::gen();
            if (!tvgexam::verify(image2->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image2->translate(400, 650);
            image2->scale(0.5f);
            image2->rotate(45);

            auto mask2 = tvg::Shape::gen();
            mask2->appendCircle(375, 700, 50, 50);
            mask2->fill(255, 255, 255, 127);

            auto sub = tvg::Shape::gen();
            sub->appendCircle(400, 750, 50, 50);
            sub->fill(255, 255, 255);
            mask2->mask(std::move(sub), tvg::MaskMethod::Subtract);
            image2->mask(std::move(mask2), tvg::MaskMethod::Alpha);
            canvas->push(std::move(image2));

            //Transformed Image + Shape Mask Intersect
            auto image3 = tvg::Picture::gen();
            if (!tvgexam::verify(image3->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image3->translate(650, 650);
            image3->scale(0.5f);
            image3->rotate(45);

            auto mask3 = tvg::Shape::gen();
            mask3->appendCircle(625, 700, 50, 50);
            mask3->fill(255, 255, 255, 127);

            auto inter = tvg::Shape::gen();
            inter->appendCircle(650, 750, 50, 50);
            inter->fill(255, 255, 255, 127);
            mask3->mask(std::move(inter), tvg::MaskMethod::Intersect);
            image3->mask(std::move(mask3), tvg::MaskMethod::Alpha);
            canvas->push(std::move(image3));

            //Transformed Image + Shape Mask Difference
            auto image4 = tvg::Picture::gen();
            if (!tvgexam::verify(image4->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image4->translate(900, 650);
            image4->scale(0.5f);
            image4->rotate(45);

            auto mask4 = tvg::Shape::gen();
            mask4->appendCircle(875, 700, 50, 50);
            mask4->fill(255, 255, 255);

            auto diff = tvg::Shape::gen();
            diff->appendCircle(900, 750, 50, 50);
            diff->fill(255, 255, 255);
            mask4->mask(std::move(diff), tvg::MaskMethod::Difference);
            image4->mask(std::move(mask4), tvg::MaskMethod::Alpha);
            canvas->push(std::move(image4));

            //Transformed Image + Shape Mask Lighten
            auto image5 = tvg::Picture::gen();
            if (!tvgexam::verify(image5->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image5->translate(1150, 650);
            image5->scale(0.5f);
            image5->rotate(45);

            auto mask5 = tvg::Shape::gen();
            mask5->appendCircle(1125, 700, 50, 50);
            mask5->fill(255, 255, 255, 200);

            auto light = tvg::Shape::gen();
            light->appendCircle(1150, 750, 50, 50);
            light->fill(255, 255, 255);
            mask5->mask(std::move(light), tvg::MaskMethod::Lighten);
            image5->mask(std::move(mask5), tvg::MaskMethod::Alpha);
            canvas->push(std::move(image5));

            //Transformed Image + Shape Mask Darken
            auto image6 = tvg::Picture::gen();
            if (!tvgexam::verify(image6->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image6->translate(1400, 650);
            image6->scale(0.5f);
            image6->rotate(45);

            auto mask6 = tvg::Shape::gen();
            mask6->appendCircle(1375, 700, 50, 50);
            mask6->fill(255, 255, 255, 200);

            auto dark = tvg::Shape::gen();
            dark->appendCircle(1400, 750, 50, 50);
            dark->fill(255, 255, 255);
            mask6->mask(std::move(dark), tvg::MaskMethod::Darken);
            image6->mask(std::move(mask6), tvg::MaskMethod::Alpha);
            canvas->push(std::move(image6));
        }
        {
            //Transformed Image + Shape Mask Add
            auto image = tvg::Picture::gen();
            if (!tvgexam::verify(image->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image->translate(150, 850);
            image->scale(0.5f);
            image->rotate(45);

            auto mask = tvg::Shape::gen();
            mask->appendCircle(125, 900, 50, 50);
            mask->fill(255, 255, 255);

            auto add = tvg::Shape::gen();
            add->appendCircle(150, 950, 50, 50);
            add->fill(255, 255, 255);
            mask->mask(std::move(add), tvg::MaskMethod::Add);
            image->mask(std::move(mask), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(image));

            //Transformed Image + Shape Mask Subtract
            auto image2 = tvg::Picture::gen();
            if (!tvgexam::verify(image2->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image2->translate(400, 850);
            image2->scale(0.5f);
            image2->rotate(45);

            auto mask2 = tvg::Shape::gen();
            mask2->appendCircle(375, 900, 50, 50);
            mask2->fill(255, 255, 255, 127);

            auto sub = tvg::Shape::gen();
            sub->appendCircle(400, 950, 50, 50);
            sub->fill(255, 255, 255);
            mask2->mask(std::move(sub), tvg::MaskMethod::Subtract);
            image2->mask(std::move(mask2), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(image2));

            //Transformed Image + Shape Mask Intersect
            auto image3 = tvg::Picture::gen();
            if (!tvgexam::verify(image3->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image3->translate(650, 850);
            image3->scale(0.5f);
            image3->rotate(45);

            auto mask3 = tvg::Shape::gen();
            mask3->appendCircle(625, 900, 50, 50);
            mask3->fill(255, 255, 255, 127);

            auto inter = tvg::Shape::gen();
            inter->appendCircle(650, 950, 50, 50);
            inter->fill(255, 255, 255, 127);
            mask3->mask(std::move(inter), tvg::MaskMethod::Intersect);
            image3->mask(std::move(mask3), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(image3));

            //Transformed Image + Shape Mask Difference
            auto image4 = tvg::Picture::gen();
            if (!tvgexam::verify(image4->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image4->translate(900, 850);
            image4->scale(0.5f);
            image4->rotate(45);

            auto mask4 = tvg::Shape::gen();
            mask4->appendCircle(875, 900, 50, 50);
            mask4->fill(255, 255, 255);

            auto diff = tvg::Shape::gen();
            diff->appendCircle(900, 950, 50, 50);
            diff->fill(255, 255, 255);
            mask4->mask(std::move(diff), tvg::MaskMethod::Difference);
            image4->mask(std::move(mask4), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(image4));

            //Transformed Image + Shape Mask Lighten
            auto image5 = tvg::Picture::gen();
            if (!tvgexam::verify(image5->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image5->translate(1150, 850);
            image5->scale(0.5f);
            image5->rotate(45);

            auto mask5 = tvg::Shape::gen();
            mask5->appendCircle(1125, 900, 50, 50);
            mask5->fill(255, 255, 255, 200);

            auto light = tvg::Shape::gen();
            light->appendCircle(1150, 950, 50, 50);
            light->fill(255, 255, 255);
            mask5->mask(std::move(light), tvg::MaskMethod::Lighten);
            image5->mask(std::move(mask5), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(image5));

            //Transformed Image + Shape Mask Darken
            auto image6 = tvg::Picture::gen();
            if (!tvgexam::verify(image6->load(data, 200, 300, tvg::ColorSpace::ARGB8888, true))) return false;
            image6->translate(1400, 850);
            image6->scale(0.5f);
            image6->rotate(45);

            auto mask6 = tvg::Shape::gen();
            mask6->appendCircle(1375, 900, 50, 50);
            mask6->fill(255, 255, 255, 200);

            auto dark = tvg::Shape::gen();
            dark->appendCircle(1400, 950, 50, 50);
            dark->fill(255, 255, 255);
            mask6->mask(std::move(dark), tvg::MaskMethod::Darken);
            image6->mask(std::move(mask6), tvg::MaskMethod::InvAlpha);
            canvas->push(std::move(image6));
        }
        free(data);
        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, 1500, 1024);
}